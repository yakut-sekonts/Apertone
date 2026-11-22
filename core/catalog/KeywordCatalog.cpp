#include "core/catalog/Catalog.h"
#include "core/catalog/SqliteStatement.h"
#include <algorithm>
#include <set>
#include <string_view>

namespace luma {
namespace {
using detail::Statement;

bool validPath(std::string_view path) {
    if (path.empty() || path.size() > 512 ||
        std::ranges::any_of(path, [](unsigned char ch) { return ch < 32 || ch == 127; }))
        return false;
    for (int depth = 0;; ++depth) {
        const auto split = path.find('|');
        const auto part = path.substr(0, split);
        if (depth >= 24 || part.empty() || part.front() == ' ' || part.back() == ' ')
            return false;
        if (split == path.npos)
            return true;
        path.remove_prefix(split + 1);
    }
}

void requirePath(const std::string &path) {
    if (!validPath(path))
        throw std::runtime_error("Use nonempty keyword names separated by |.");
}

void define(sqlite3 *db, const std::string &path) {
    Statement insert(db, "INSERT OR IGNORE INTO catalog_keywords(path) VALUES(?1)");
    insert.text(1, path);
    insert.row();
}

bool validAlias(const std::string &alias) {
    return validPath(alias) && alias.find('|') == std::string::npos;
}

void insertAliases(sqlite3 *db, const std::string &path, const std::vector<std::string> &aliases) {
    for (const auto &alias : std::set(aliases.begin(), aliases.end())) {
        if (alias == path || !validAlias(alias))
            continue;
        Statement insert(db, "INSERT INTO keyword_aliases(path,alias) VALUES(?1,?2)");
        insert.text(1, path);
        insert.text(2, alias);
        insert.row();
    }
}

void markAffected(sqlite3 *db, const std::string &path) {
    Statement mark(
        db, "UPDATE asset_metadata SET revision=revision+1,initialized=1 WHERE asset_id IN "
            "(SELECT asset_id FROM asset_keywords WHERE keyword=?1 OR instr(keyword,?1||'|')=1)");
    mark.text(1, path);
    mark.row();
}
} // namespace

void Catalog::recordImportedKeyword(const CatalogKeyword &term) {
    if (!validPath(term.path))
        return;
    define(db_, term.path);
    // Preserve aliases previously edited by the user, including an empty list.
    if (sqlite3_changes(db_) == 1)
        insertAliases(db_, term.path, term.aliases);
}

std::vector<CatalogKeyword> Catalog::catalogKeywords() const {
    std::map<std::string, CatalogKeyword> tree;
    Statement paths(db_, "SELECT keyword,COUNT(*) FROM asset_keywords GROUP BY keyword "
                         "UNION ALL SELECT path,0 FROM catalog_keywords");
    while (paths.row()) {
        auto path = paths.string(0);
        if (!validPath(path))
            continue;
        auto &entry = tree[path];
        entry.path = path;
        entry.photos += paths.integer(1);
        for (auto node = path.rfind('|'); node != path.npos; node = path.rfind('|')) {
            path.resize(node);
            tree.try_emplace(path, CatalogKeyword{path, {}, 0});
        }
    }
    Statement aliases(db_, "SELECT path,alias FROM keyword_aliases ORDER BY alias COLLATE NOCASE");
    while (aliases.row())
        if (const auto found = tree.find(aliases.string(0)); found != tree.end())
            found->second.aliases.push_back(aliases.string(1));
    std::vector<CatalogKeyword> result;
    result.reserve(tree.size());
    for (auto &entry : tree)
        result.push_back(std::move(entry.second));
    return result;
}

void Catalog::saveCatalogKeyword(const std::string &previous, const std::string &path,
                                 const std::vector<std::string> &aliases) {
    requirePath(path);
    if (!previous.empty())
        requirePath(previous);
    if (aliases.size() > 64 || !std::ranges::all_of(aliases, validAlias))
        throw std::runtime_error("Enter up to 64 nonempty keyword aliases.");
    if (!previous.empty() && std::string_view(path).starts_with(previous + '|'))
        throw std::runtime_error("A keyword cannot move below itself.");
    execute("BEGIN IMMEDIATE;");
    try {
        if (!previous.empty() && previous != path) {
            Statement affected(
                db_, "SELECT path FROM catalog_keywords WHERE path=?1 OR instr(path,?1||'|')=1 "
                     "UNION SELECT keyword FROM asset_keywords WHERE keyword=?1 OR "
                     "instr(keyword,?1||'|')=1");
            affected.text(1, previous);
            std::vector<std::string> targets;
            while (affected.row()) {
                const auto destination = path + affected.string(0).substr(previous.size());
                requirePath(destination);
                targets.push_back(destination);
            }
            if (targets.empty())
                throw std::runtime_error("Keyword no longer exists.");
            for (const auto &destination : targets) {
                Statement conflict(
                    db_, "SELECT 1 FROM catalog_keywords WHERE path=?1 OR instr(path,?1||'|')=1 "
                         "UNION ALL SELECT 1 FROM asset_keywords WHERE keyword=?1 LIMIT 1");
                conflict.text(1, destination);
                if (conflict.row())
                    throw std::runtime_error("The destination keyword already exists.");
            }
            markAffected(db_, previous);
            Statement definitions(db_,
                                  "UPDATE catalog_keywords SET path=?2||substr(path,length(?1)+1) "
                                  "WHERE path=?1 OR instr(path,?1||'|')=1");
            definitions.text(1, previous);
            definitions.text(2, path);
            definitions.row();
            Statement assignments(
                db_, "UPDATE asset_keywords SET keyword=?2||substr(keyword,length(?1)+1) "
                     "WHERE keyword=?1 OR instr(keyword,?1||'|')=1");
            assignments.text(1, previous);
            assignments.text(2, path);
            assignments.row();
        }
        define(db_, path);
        Statement clear(db_, "DELETE FROM keyword_aliases WHERE path=?1");
        clear.text(1, path);
        clear.row();
        insertAliases(db_, path, aliases);
        execute("COMMIT;");
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}

void Catalog::deleteCatalogKeyword(const std::string &path) {
    requirePath(path);
    execute("BEGIN IMMEDIATE;");
    try {
        markAffected(db_, path);
        Statement assignments(
            db_, "DELETE FROM asset_keywords WHERE keyword=?1 OR instr(keyword,?1||'|')=1");
        assignments.text(1, path);
        assignments.row();
        Statement definitions(
            db_, "DELETE FROM catalog_keywords WHERE path=?1 OR instr(path,?1||'|')=1");
        definitions.text(1, path);
        definitions.row();
        execute("COMMIT;");
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}

void Catalog::keywordMembership(const std::string &path, const std::vector<std::string> &ids,
                                bool add) {
    requirePath(path);
    if (ids.empty())
        return;
    execute("BEGIN IMMEDIATE;");
    try {
        if (add)
            define(db_, path);
        for (const auto &id : ids) {
            Statement asset(db_, "SELECT 1 FROM asset_metadata WHERE asset_id=?1");
            asset.text(1, id);
            if (!asset.row())
                throw std::runtime_error("Photo does not exist.");
            Statement existing(db_, "SELECT position,keyword FROM asset_keywords WHERE asset_id=?1 "
                                    "ORDER BY position");
            existing.text(1, id);
            std::vector<std::string> values;
            bool assigned = false;
            while (existing.row()) {
                const auto keyword = existing.string(1);
                assigned |= keyword == path;
                if (add || keyword != path)
                    values.push_back(keyword);
            }
            if (assigned == add)
                continue;
            if (add)
                values.push_back(path);
            DescriptiveMetadata metadata;
            metadata.keywords = std::move(values);
            metadata.validate();
            Statement clear(db_, "DELETE FROM asset_keywords WHERE asset_id=?1");
            clear.text(1, id);
            clear.row();
            for (std::size_t index = 0; index < metadata.keywords.size(); ++index) {
                Statement insert(
                    db_, "INSERT INTO asset_keywords(asset_id,position,keyword) VALUES(?1,?2,?3)");
                insert.text(1, id);
                insert.integer(2, static_cast<std::int64_t>(index));
                insert.text(3, metadata.keywords[index]);
                insert.row();
            }
            Statement mark(
                db_,
                "UPDATE asset_metadata SET revision=revision+1,initialized=1 WHERE asset_id=?1");
            mark.text(1, id);
            mark.row();
        }
        execute("COMMIT;");
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}
} // namespace luma
