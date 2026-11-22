#include "core/catalog/Catalog.h"
#include "core/catalog/SqliteStatement.h"
#include "core/image/Bayer.h"
#include "core/io/Path.h"
#include <set>

namespace luma {
using detail::Statement;

void Catalog::migrateLightroomMigration() {
    execute(R"sql(
        CREATE TABLE lightroom_migration_photos(
            source_catalog TEXT NOT NULL,
            source_photo TEXT NOT NULL,
            asset_id TEXT NOT NULL REFERENCES assets(id) ON DELETE CASCADE,
            version_id TEXT REFERENCES edit_versions(id) ON DELETE SET NULL,
            source_master TEXT NOT NULL,
            copy_name TEXT NOT NULL,
            develop_document TEXT NOT NULL,
            rating INTEGER NOT NULL,
            flag INTEGER NOT NULL,
            label TEXT NOT NULL,
            PRIMARY KEY(source_catalog,source_photo));
        CREATE INDEX lightroom_migration_assets ON lightroom_migration_photos(asset_id);
        CREATE TABLE lightroom_migration_collections(
            source_catalog TEXT NOT NULL,
            source_collection INTEGER NOT NULL,
            collection_id TEXT NOT NULL REFERENCES collections(id) ON DELETE CASCADE,
            PRIMARY KEY(source_catalog,source_collection));
        INSERT INTO schema_migrations(version) VALUES(24);
        PRAGMA user_version=24;
    )sql");
}

void Catalog::migrateLightroomSmartRules() {
    execute(R"sql(
        CREATE TABLE lightroom_smart_rules(
            source_catalog TEXT NOT NULL,
            source_collection INTEGER NOT NULL,
            name TEXT NOT NULL,
            document TEXT NOT NULL,
            PRIMARY KEY(source_catalog,source_collection));
    )sql");
}

LightroomMigrationResult Catalog::applyLightroomMigration(
    const std::string &sourceKey, const std::vector<LightroomMigrationPhoto> &photos,
    const std::vector<LightroomMigrationCollection> &collections, CancellationToken stop,
    const std::vector<CatalogKeyword> &keywords) {
    if (sourceKey.size() != 64 || sourceKey.find_first_not_of("0123456789abcdef") != sourceKey.npos)
        throw std::runtime_error("Invalid Lightroom migration source identity.");
    const auto mappedAsset = [&](const std::string &id) {
        Statement mapped(db_, "SELECT asset_id FROM lightroom_migration_photos "
                              "WHERE source_catalog=?1 AND source_photo=?2");
        mapped.text(1, sourceKey);
        mapped.text(2, id);
        return mapped.row() ? mapped.string(0) : std::string();
    };
    LightroomMigrationResult result;
    const auto restoreHistory = [&](const std::string &id, const LightroomMigrationPhoto &photo,
                                    bool existing) {
        if (!photo.history || !photo.edits)
            return;
        if (existing) {
            Statement count(db_, "SELECT 1 FROM edit_history_states WHERE asset_id=?1 LIMIT 1");
            count.text(1, id);
            if (count.row())
                return;
            const auto asset = assetByID(id);
            if (!asset || asset->edits != *photo.edits)
                return;
        }
        const auto &history = *photo.history;
        if (history.currentIndex >= history.entries.size() ||
            history.entries[history.currentIndex].document != *photo.edits)
            throw std::runtime_error("Imported Develop history does not match the active edit.");
        writeEditHistory(id, history);
        result.historySteps += history.entries.size();
    };
    const auto restoreSnapshots = [&](const std::string &id, const LightroomMigrationPhoto &photo,
                                      bool existing) {
        const auto asset = existing && photo.localAdjustments ? assetByID(id) : std::nullopt;
        const bool addAdjustments = asset && asset->edits != photo.localAdjustments->edits;
        if (photo.snapshots.empty() && !addAdjustments)
            return;
        std::set<std::string> names;
        for (const auto &version : editVersions(id))
            names.insert(version.name);
        const auto addSnapshot = [&](const LightroomMigrationSnapshot &snapshot) {
            checkCancellation(stop);
            Statement known(db_, "SELECT 1 FROM lightroom_migration_snapshots WHERE "
                                 "source_catalog=?1 AND source_photo=?2 AND source_snapshot=?3");
            known.text(1, sourceKey);
            known.text(2, photo.sourceID);
            known.text(3, snapshot.sourceID);
            if (known.row())
                return;
            auto name = snapshot.name;
            for (int suffix = 2; names.contains(name); ++suffix)
                name = snapshot.name + " (" + std::to_string(suffix) + ")";
            if (insertEditVersion(id, name, snapshot.edits, true).empty()) {
                ++result.snapshotsWithoutRoom;
                return;
            }
            names.insert(std::move(name));
            Statement mapping(db_,
                              "INSERT INTO lightroom_migration_snapshots "
                              "(source_catalog,source_photo,source_snapshot) VALUES(?1,?2,?3)");
            mapping.text(1, sourceKey);
            mapping.text(2, photo.sourceID);
            mapping.text(3, snapshot.sourceID);
            mapping.row();
            ++result.snapshots;
        };
        for (const auto &snapshot : photo.snapshots)
            addSnapshot(snapshot);
        if (addAdjustments)
            addSnapshot(*photo.localAdjustments);
    };
    std::set<std::string> newlyMapped;
    execute("BEGIN IMMEDIATE;");
    try {
        for (const auto &keyword : keywords) {
            checkCancellation(stop);
            recordImportedKeyword(keyword);
        }
        for (const auto &photo : photos) {
            checkCancellation(stop);
            if (const auto existing = mappedAsset(photo.sourceID); !existing.empty()) {
                restoreHistory(existing, photo, true);
                restoreSnapshots(existing, photo, true);
                ++result.unchanged;
                continue;
            }
            if (photo.sourceID.size() != 36 || photo.checksum.size() != 64 ||
                !photo.path.is_absolute() || photo.developDocument.size() > 1'048'576)
                throw std::runtime_error("Invalid Lightroom photo migration record.");
            const bool existing = assetByPath(photo.path).has_value();
            const auto asset = addAssetRecord(photo.path, photo.checksum, photo.metadata, false);
            if (existing)
                ++result.reused;
            else
                ++result.added;
            const auto metadata = photo.metadata.descriptive.value_or(DescriptiveMetadata{});
            auto destination = asset.id;
            std::string version;
            if (!photo.masterID.empty()) {
                destination = insertVirtualCopy(asset, photo.copyName,
                                                photo.edits.value_or(EditDocument{}), metadata);
                ++result.copies;
            } else if (existing && photo.edits && *photo.edits != asset.edits) {
                auto name = photo.copyName.empty() ? "Lightroom" : "Lightroom · " + photo.copyName;
                name += " [" + photo.sourceID.substr(0, 8) + "]";
                version = insertEditVersion(asset.id, name, photo.edits.value_or(EditDocument{}));
                ++result.versions;
            } else if (photo.edits && !existing) {
                writeEdits(asset.id, *photo.edits);
            }
            restoreHistory(destination, photo, existing && photo.masterID.empty());
            if (photo.editedAt && (!existing || !photo.masterID.empty())) {
                Statement timestamp(db_,
                                    "UPDATE edit_documents SET updated_at=datetime(?1,'unixepoch') "
                                    "WHERE asset_id=?2");
                timestamp.integer(1, *photo.editedAt);
                timestamp.text(2, destination);
                timestamp.row();
            }
            Statement record(db_, "INSERT INTO lightroom_migration_photos("
                                  "source_catalog,source_photo,asset_id,version_id,source_master,"
                                  "copy_name,develop_document,rating,flag,label) "
                                  "VALUES(?1,?2,?3,NULLIF(?4,''),?5,?6,?7,?8,?9,?10)");
            record.text(1, sourceKey);
            record.text(2, photo.sourceID);
            record.text(3, destination);
            record.text(4, version);
            record.text(5, photo.masterID);
            record.text(6, photo.copyName);
            record.text(7, photo.developDocument);
            record.integer(8, metadata.rating);
            record.integer(9, metadata.flag);
            record.text(10, metadata.label);
            record.row();
            restoreSnapshots(destination, photo, false);
            newlyMapped.insert(photo.sourceID);
        }
        std::map<std::int64_t, const LightroomMigrationCollection *> sourceSets;
        for (const auto &node : collections)
            if (node.group && node.sourceID > 0)
                sourceSets.emplace(node.sourceID, &node);
        std::map<std::int64_t, std::string> importedSets;
        std::set<std::int64_t> ancestors;
        const auto ensureSet = [&](const auto &self, std::int64_t sourceID) -> std::string {
            checkCancellation(stop);
            if (const auto known = importedSets.find(sourceID); known != importedSets.end())
                return known->second;
            const auto source = sourceSets.find(sourceID);
            if (source == sourceSets.end() || ancestors.size() >= 16 ||
                !ancestors.insert(sourceID).second)
                return {};
            Statement mapped(db_, "SELECT set_id FROM lightroom_migration_sets "
                                  "WHERE source_catalog=?1 AND source_set=?2");
            mapped.text(1, sourceKey);
            mapped.integer(2, sourceID);
            std::string id;
            if (mapped.row()) {
                id = mapped.string(0);
            } else {
                const auto &node = *source->second;
                const auto parent = self(self, node.parentSourceID);
                const auto base = parent.empty() ? "Lightroom · " + node.name : node.name;
                auto name = base;
                for (int suffix = 2;; ++suffix) {
                    Statement exists(db_, "SELECT 1 FROM collection_sets WHERE "
                                          "COALESCE(parent_id,'')=?1 AND name=?2 COLLATE NOCASE");
                    exists.text(1, parent);
                    exists.text(2, name);
                    if (!exists.row())
                        break;
                    checkCancellation(stop);
                    name = base + " (" + std::to_string(suffix) + ")";
                }
                id = createCollectionSet(name, parent);
                Statement record(db_, "INSERT INTO lightroom_migration_sets "
                                      "(source_catalog,source_set,set_id) VALUES(?1,?2,?3)");
                record.text(1, sourceKey);
                record.integer(2, sourceID);
                record.text(3, id);
                record.row();
                ++result.collectionSets;
            }
            ancestors.erase(sourceID);
            importedSets.emplace(sourceID, id);
            return id;
        };
        for (const auto &source : sourceSets)
            ensureSet(ensureSet, source.first);
        for (const auto &collection : collections) {
            checkCancellation(stop);
            if (collection.group)
                continue;
            if (collection.smart) {
                Statement source(
                    db_, "INSERT OR IGNORE INTO lightroom_smart_rules"
                         "(source_catalog,source_collection,name,document) VALUES(?1,?2,?3,?4)");
                source.text(1, sourceKey);
                source.integer(2, collection.sourceID);
                source.text(3, collection.name);
                source.text(4, collection.sourceRule);
                source.row();
                if (!collection.rule)
                    continue;
            }
            Statement mapped(db_, "SELECT collection_id FROM lightroom_migration_collections "
                                  "WHERE source_catalog=?1 AND source_collection=?2");
            mapped.text(1, sourceKey);
            mapped.integer(2, collection.sourceID);
            std::string id;
            bool created = false;
            const auto set = importedSets.find(collection.parentSourceID);
            const auto setID = set == importedSets.end() ? std::string{} : set->second;
            const auto nameAvailable = [&](const std::string &name, const std::string &except) {
                Statement exists(db_, setID.empty()
                                          ? "SELECT 1 FROM collections WHERE "
                                            "name=?1 COLLATE NOCASE AND id<>?2"
                                          : "SELECT 1 FROM collection_set_entries WHERE "
                                            "name=?1 COLLATE NOCASE AND collection_id<>?2 "
                                            "AND set_id=?3");
                exists.text(1, name);
                exists.text(2, except);
                if (!setID.empty())
                    exists.text(3, setID);
                return !exists.row();
            };
            const auto availableName = [&](const std::string &base,
                                           const std::string &except = {}) {
                auto name = base;
                for (int suffix = 2; !nameAvailable(name, except); ++suffix) {
                    checkCancellation(stop);
                    name = base + " (" + std::to_string(suffix) + ")";
                }
                return name;
            };
            if (mapped.row()) {
                id = mapped.string(0);
                if (!setID.empty()) {
                    Statement flat(db_, "SELECT 1 FROM collections c WHERE c.id=?1 AND c.name=?2 "
                                        "AND NOT EXISTS(SELECT 1 FROM collection_set_entries e "
                                        "WHERE e.collection_id=c.id)");
                    flat.text(1, id);
                    flat.text(2, "Lightroom · " + collection.legacyName);
                    if (flat.row())
                        updateCollection(id, availableName(collection.name, id), setID);
                }
            } else {
                const auto &legacy =
                    collection.legacyName.empty() ? collection.name : collection.legacyName;
                const auto name =
                    availableName(setID.empty() ? "Lightroom · " + legacy : collection.name);
                id = collection.rule
                         ? insertSmartCollection(name, encodeSmartRule(*collection.rule), setID)
                         : createCollection(name, setID);
                Statement record(
                    db_, "INSERT INTO lightroom_migration_collections("
                         "source_catalog,source_collection,collection_id) VALUES(?1,?2,?3)");
                record.text(1, sourceKey);
                record.integer(2, collection.sourceID);
                record.text(3, id);
                record.row();
                ++result.collections;
                result.smartCollections += collection.smart;
                created = true;
            }
            for (const auto &member : collection.members) {
                checkCancellation(stop);
                if (!created && !newlyMapped.contains(member))
                    continue;
                if (const auto asset = mappedAsset(member); !asset.empty()) {
                    Statement link(
                        db_, "INSERT OR IGNORE INTO collection_assets(collection_id,asset_id) "
                             "VALUES(?1,?2)");
                    link.text(1, id);
                    link.text(2, asset);
                    link.row();
                }
            }
        }
        checkCancellation(stop);
        execute("COMMIT;");
        return result;
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}
} // namespace luma
