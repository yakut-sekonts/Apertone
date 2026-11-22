#include "core/catalog/Catalog.h"
#include "core/catalog/Identity.h"
#include "core/catalog/SqliteStatement.h"

namespace luma {
namespace {
using detail::Statement;
constexpr int maximumDepth = 24;

class CollectionChange {
  public:
    explicit CollectionChange(sqlite3 *db) : db_(db) {
        run("SAVEPOINT collection_change");
    }
    CollectionChange(const CollectionChange &) = delete;
    CollectionChange &operator=(const CollectionChange &) = delete;
    ~CollectionChange() {
        if (db_)
            sqlite3_exec(db_, "ROLLBACK TO collection_change; RELEASE collection_change", nullptr,
                         nullptr, nullptr);
    }
    void commit() {
        run("RELEASE collection_change");
        db_ = nullptr;
    }

  private:
    sqlite3 *db_;
    void run(const char *sql) const {
        Statement command(db_, sql);
        command.row();
    }
};

void validateName(const std::string &name) {
    if (name.size() > 512 || name.find_first_not_of(" \t\r\n") == std::string::npos ||
        name.find_first_of(std::string_view("\0\x1f", 2)) != std::string::npos)
        throw std::runtime_error("Collection name must contain text and fit 128 characters.");
}

int parentDepth(sqlite3 *db, std::string parent, const std::string &movingID = {}) {
    int depth = 0;
    while (!parent.empty()) {
        if (parent == movingID || ++depth >= maximumDepth)
            throw std::runtime_error("Invalid collection set hierarchy.");
        Statement row(db, "SELECT COALESCE(parent_id,'') FROM collection_sets WHERE id=?1");
        row.text(1, parent);
        if (!row.row())
            throw std::runtime_error("Collection set does not exist.");
        parent = row.string(0);
    }
    return depth;
}

std::pair<std::string, std::string> location(sqlite3 *db, const std::string &id) {
    Statement row(db, "SELECT COALESCE(e.name,c.name),COALESCE(e.set_id,'') FROM collections c "
                      "LEFT JOIN collection_set_entries e ON e.collection_id=c.id WHERE c.id=?1");
    row.text(1, id);
    if (!row.row())
        throw std::runtime_error("Collection does not exist.");
    return {row.string(0), row.string(1)};
}

void place(sqlite3 *db, const std::string &id, const std::string &name, const std::string &setID) {
    if (setID.empty()) {
        Statement remove(db, "DELETE FROM collection_set_entries WHERE collection_id=?1");
        remove.text(1, id);
        remove.row();
    } else {
        Statement entry(db, "INSERT INTO collection_set_entries(collection_id,set_id,name) "
                            "VALUES(?1,?2,?3) ON CONFLICT(collection_id) DO UPDATE SET "
                            "set_id=excluded.set_id,name=excluded.name");
        entry.text(1, id);
        entry.text(2, setID);
        entry.text(3, name);
        entry.row();
    }
    // The legacy name remains unique. Nested display names are unique inside each set.
    // Updating the collection row also invalidates open Smart Collection rule editors.
    Statement update(db, "UPDATE collections SET name=?2 WHERE id=?1");
    update.text(1, id);
    update.text(2, setID.empty() ? name : "\x1f" + id);
    update.row();
}
} // namespace

void Catalog::migrateOrganization() {
    execute(R"sql(
        CREATE TABLE collection_sets(
            id TEXT PRIMARY KEY,
            parent_id TEXT REFERENCES collection_sets(id) ON DELETE RESTRICT,
            name TEXT NOT NULL CHECK(length(trim(name)) BETWEEN 1 AND 128));
        CREATE UNIQUE INDEX collection_sets_siblings
            ON collection_sets(COALESCE(parent_id,''),name COLLATE NOCASE);
        CREATE INDEX collection_sets_parent ON collection_sets(parent_id);
        CREATE TABLE collection_set_entries(
            collection_id TEXT PRIMARY KEY REFERENCES collections(id) ON DELETE CASCADE,
            set_id TEXT NOT NULL REFERENCES collection_sets(id) ON DELETE RESTRICT,
            name TEXT NOT NULL CHECK(length(trim(name)) BETWEEN 1 AND 128),
            UNIQUE(set_id,name COLLATE NOCASE));
        CREATE TABLE lightroom_migration_sets(
            source_catalog TEXT NOT NULL,
            source_set INTEGER NOT NULL,
            set_id TEXT NOT NULL REFERENCES collection_sets(id) ON DELETE CASCADE,
            PRIMARY KEY(source_catalog,source_set));
        CREATE TABLE lightroom_migration_snapshots(
            source_catalog TEXT NOT NULL,
            source_photo TEXT NOT NULL,
            source_snapshot TEXT NOT NULL,
            PRIMARY KEY(source_catalog,source_photo,source_snapshot),
            FOREIGN KEY(source_catalog,source_photo)
                REFERENCES lightroom_migration_photos(source_catalog,source_photo) ON DELETE CASCADE);
        CREATE TABLE catalog_keywords(
            path TEXT PRIMARY KEY CHECK(length(CAST(path AS BLOB)) BETWEEN 1 AND 512));
        CREATE TABLE keyword_aliases(
            path TEXT NOT NULL REFERENCES catalog_keywords(path) ON DELETE CASCADE ON UPDATE CASCADE,
            alias TEXT NOT NULL CHECK(length(CAST(alias AS BLOB)) BETWEEN 1 AND 512),
            PRIMARY KEY(path,alias));
        CREATE INDEX keyword_alias_lookup ON keyword_aliases(alias COLLATE NOCASE,path);
        INSERT INTO schema_migrations(version) VALUES(27);
        PRAGMA user_version=27;
    )sql");
}

std::vector<CollectionSet> Catalog::collectionSets() const {
    Statement rows(db_, R"sql(
        WITH RECURSIVE sets(id,name,parent_id,path) AS (
            SELECT id,name,parent_id,name FROM collection_sets WHERE parent_id IS NULL
            UNION ALL SELECT child.id,child.name,child.parent_id,sets.path||' › '||child.name
                FROM collection_sets child JOIN sets ON child.parent_id=sets.id)
        SELECT id,name,COALESCE(parent_id,''),path FROM sets ORDER BY path COLLATE NOCASE,id
    )sql");
    std::vector<CollectionSet> result;
    while (rows.row())
        result.push_back({rows.string(0), rows.string(1), rows.string(2), rows.string(3)});
    return result;
}

std::string Catalog::createCollectionSet(const std::string &name, const std::string &parentID) {
    validateName(name);
    CollectionChange change(db_);
    parentDepth(db_, parentID);
    const auto id = newID();
    Statement add(db_,
                  "INSERT INTO collection_sets(id,name,parent_id) VALUES(?1,?2,NULLIF(?3,''))");
    add.text(1, id);
    add.text(2, name);
    add.text(3, parentID);
    add.row();
    change.commit();
    return id;
}

void Catalog::updateCollectionSet(const std::string &id, const std::string &name,
                                  const std::string &parentID) {
    validateName(name);
    CollectionChange change(db_);
    const int depth = parentDepth(db_, parentID, id);
    Statement subtree(db_, R"sql(
        WITH RECURSIVE children(id,depth) AS (
            SELECT id,1 FROM collection_sets WHERE id=?1
            UNION ALL SELECT s.id,c.depth+1 FROM collection_sets s JOIN children c
                ON s.parent_id=c.id WHERE c.depth<=24)
        SELECT COALESCE(MAX(depth),0) FROM children
    )sql");
    subtree.text(1, id);
    if (!subtree.row() || subtree.integer(0) == 0)
        throw std::runtime_error("Collection set does not exist.");
    if (depth + subtree.integer(0) > maximumDepth)
        throw std::runtime_error("Invalid collection set hierarchy.");
    Statement update(db_, "UPDATE collection_sets SET name=?2,parent_id=NULLIF(?3,'') WHERE id=?1");
    update.text(1, id);
    update.text(2, name);
    update.text(3, parentID);
    update.row();
    change.commit();
}

void Catalog::deleteCollectionSet(const std::string &id) {
    Statement remove(db_, "DELETE FROM collection_sets WHERE id=?1 AND NOT EXISTS "
                          "(SELECT 1 FROM collection_sets child WHERE child.parent_id=?1) "
                          "AND NOT EXISTS (SELECT 1 FROM collection_set_entries WHERE set_id=?1)");
    remove.text(1, id);
    remove.row();
    if (sqlite3_changes(db_) != 1)
        throw std::runtime_error("Only an empty collection set can be deleted.");
}

std::string Catalog::createCollection(const std::string &name, const std::string &setID) {
    validateName(name);
    CollectionChange change(db_);
    const auto id = newID();
    Statement add(db_, "INSERT INTO collections(id,name) VALUES(?1,?2)");
    add.text(1, id);
    add.text(2, setID.empty() ? name : "\x1f" + id);
    add.row();
    if (!setID.empty())
        place(db_, id, name, setID);
    change.commit();
    return id;
}

void Catalog::renameCollection(const std::string &id, const std::string &name) {
    validateName(name);
    CollectionChange change(db_);
    place(db_, id, name, location(db_, id).second);
    change.commit();
}

void Catalog::moveCollection(const std::string &id, const std::string &setID) {
    CollectionChange change(db_);
    const auto [name, previous] = location(db_, id);
    if (previous != setID)
        place(db_, id, name, setID);
    change.commit();
}

void Catalog::updateCollection(const std::string &id, const std::string &name,
                               const std::string &setID) {
    validateName(name);
    CollectionChange change(db_);
    const auto [previousName, previousSet] = location(db_, id);
    if (previousName != name || previousSet != setID)
        place(db_, id, name, setID);
    change.commit();
}
} // namespace luma
