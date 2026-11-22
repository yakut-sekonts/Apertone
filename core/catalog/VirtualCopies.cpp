#include "core/catalog/Catalog.h"
#include "core/catalog/Identity.h"
#include "core/catalog/MetadataStorage.h"
#include "core/catalog/SqliteStatement.h"
#include "core/edits/EditSnapshot.h"

namespace luma {
namespace {
using detail::Statement;

void validateCopyName(const std::string &name) {
    if (name.size() > 512 || name.find_first_not_of(' ') == name.npos)
        throw std::runtime_error("Virtual copy names must contain 1–512 UTF-8 bytes.");
    DescriptiveMetadata metadata;
    metadata.title = name;
    metadata.validate();
}

DescriptiveMetadata organization(const AssetRecord &asset) {
    return asset.descriptive ? asset.descriptive->values : DescriptiveMetadata{};
}
} // namespace

void Catalog::migrateVirtualCopies() {
    execute(R"sql(
        ALTER TABLE assets ADD COLUMN original_asset_id TEXT REFERENCES assets(id) ON DELETE CASCADE;
        ALTER TABLE assets ADD COLUMN copy_name TEXT NOT NULL DEFAULT '';
        CREATE INDEX assets_virtual_original ON assets(original_asset_id);
        CREATE TRIGGER validate_virtual_original BEFORE INSERT ON assets
        WHEN NEW.original_asset_id IS NOT NULL AND NOT EXISTS(
            SELECT 1 FROM assets original WHERE original.id=NEW.original_asset_id
              AND original.original_asset_id IS NULL AND original.checksum=NEW.checksum)
        BEGIN SELECT RAISE(ABORT,'Invalid virtual copy original'); END;
        CREATE TRIGGER sync_virtual_filename AFTER UPDATE OF filename,extension ON assets
        WHEN NEW.original_asset_id IS NULL
        BEGIN
            UPDATE assets SET filename=NEW.filename,extension=NEW.extension
            WHERE original_asset_id=NEW.id;
        END;
        INSERT INTO schema_migrations(version) VALUES(25);
        PRAGMA user_version=25;
    )sql");
    // Version 24 represented Lightroom copies as snapshots on the original.
    // Read the stored copy document so later changes to the Lightroom catalog
    // or original photo cannot affect the upgrade.
    struct PreviousCopy {
        std::string catalog, source, id, version, name, label;
        int rating, flag;
    };
    std::vector<PreviousCopy> previous;
    {
        Statement copies(db_, "SELECT source_catalog,source_photo,asset_id,version_id,copy_name,"
                              "label,rating,flag FROM lightroom_migration_photos "
                              "WHERE source_master<>'' AND version_id IS NOT NULL");
        while (copies.row())
            previous.push_back({copies.string(0), copies.string(1), copies.string(2),
                                copies.string(3), copies.string(4), copies.string(5),
                                static_cast<int>(copies.integer(6)),
                                static_cast<int>(copies.integer(7))});
    }
    for (const auto &copy : previous) {
        const auto asset = assetByID(copy.id);
        Statement stored(db_, "SELECT document FROM edit_versions WHERE id=?1 AND asset_id=?2");
        stored.text(1, copy.version);
        stored.text(2, copy.id);
        if (!asset || !stored.row())
            throw std::runtime_error("Missing Lightroom copy data during catalog upgrade.");
        auto metadata = organization(*asset);
        metadata.rating = copy.rating;
        metadata.flag = copy.flag;
        metadata.label = copy.label;
        const auto id =
            insertVirtualCopy(*asset, copy.name, decodeEditSnapshot(stored.string(0)), metadata);
        Statement link(db_, "UPDATE lightroom_migration_photos SET asset_id=?1 "
                            "WHERE source_catalog=?2 AND source_photo=?3");
        link.text(1, id);
        link.text(2, copy.catalog);
        link.text(3, copy.source);
        link.row();
    }
}

std::string Catalog::insertVirtualCopy(const AssetRecord &source, const std::string &name,
                                       const EditDocument &edits,
                                       const DescriptiveMetadata &metadata) {
    validateCopyName(name);
    const auto &original = source.originalAssetID.empty() ? source.id : source.originalAssetID;
    const auto id = newID();
    Statement create(db_,
                     "INSERT INTO assets(id,original_path,filename,extension,filesize,checksum,"
                     "capture_date,width,height,orientation,camera,lens,focal_length,aperture,"
                     "shutter_seconds,iso,gps_latitude,gps_longitude,original_asset_id,copy_name) "
                     "SELECT ?1,?2,filename,extension,filesize,checksum,capture_date,width,height,"
                     "orientation,camera,lens,focal_length,aperture,shutter_seconds,iso,"
                     "gps_latitude,gps_longitude,id,?3 FROM assets WHERE id=?4 "
                     "AND original_asset_id IS NULL");
    create.text(1, id);
    create.text(2, "apertone-copy:" + id);
    create.text(3, name);
    create.text(4, original);
    create.row();
    if (sqlite3_changes(db_) != 1)
        throw std::runtime_error("The original for this virtual copy is no longer in the catalog.");
    Statement edit(db_, "INSERT INTO edit_documents(asset_id) VALUES(?1)");
    edit.text(1, id);
    edit.row();
    PhotoMetadata description;
    description.descriptive = metadata;
    detail::initializeMetadata(db_, id, source.path, description);
    writeEdits(id, edits);
    return id;
}

std::string Catalog::createVirtualCopy(const std::string &assetID, const std::string &name) {
    execute("BEGIN IMMEDIATE;");
    try {
        const auto source = assetByID(assetID);
        if (!source)
            throw std::runtime_error("The photo is no longer in the catalog.");
        auto id = insertVirtualCopy(*source, name, source->edits, organization(*source));
        execute("COMMIT;");
        return id;
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}

void Catalog::renameVirtualCopy(const std::string &id, const std::string &name) {
    validateCopyName(name);
    Statement rename(
        db_, "UPDATE assets SET copy_name=?1 WHERE id=?2 AND original_asset_id IS NOT NULL");
    rename.text(1, name);
    rename.text(2, id);
    rename.row();
    if (sqlite3_changes(db_) != 1)
        throw std::runtime_error("The virtual copy is no longer in the catalog.");
}

void Catalog::deleteVirtualCopy(const std::string &id) {
    execute("BEGIN IMMEDIATE;");
    try {
        Statement origin(
            db_,
            "SELECT original_asset_id FROM assets WHERE id=?1 AND original_asset_id IS NOT NULL");
        origin.text(1, id);
        if (!origin.row())
            throw std::runtime_error("The virtual copy is no longer in the catalog.");
        Statement selected(db_, "UPDATE session SET asset_id=?1 WHERE asset_id=?2");
        selected.text(1, origin.string(0));
        selected.text(2, id);
        selected.row();
        Statement remove(db_, "DELETE FROM assets WHERE id=?1 AND original_asset_id IS NOT NULL");
        remove.text(1, id);
        remove.row();
        execute("COMMIT;");
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}
} // namespace luma
