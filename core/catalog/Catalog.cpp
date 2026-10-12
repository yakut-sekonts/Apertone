#include "core/catalog/Catalog.h"
#include "core/catalog/Identity.h"
#include "core/catalog/SqliteStatement.h"
#include <array>
#include <chrono>
#include <iomanip>
#include <random>
#include <sqlite3.h>
#include <sstream>
#include <thread>

namespace luma {
namespace {
using detail::Statement;
AssetRecord readAsset(const Statement &query) {
    PhotoMetadata metadata;
    metadata.camera = query.string(3);
    metadata.lens = query.string(4);
    metadata.iso = query.optional(5);
    metadata.shutterSeconds = query.optional(6);
    metadata.aperture = query.optional(7);
    metadata.focalLength = query.optional(8);
    metadata.width = static_cast<int>(query.value(9));
    metadata.height = static_cast<int>(query.value(10));
    metadata.orientation = static_cast<int>(query.value(11));
    if (auto timestamp = query.optional(12)) {
        metadata.captureTimestamp = static_cast<std::int64_t>(*timestamp);
    }
    EditDocument edits{query.value(13)};
    edits.temperature = query.value(14);
    edits.tint = query.value(15);
    edits.customWhiteBalance = query.value(16) != 0;
    edits.contrast = query.value(17);
    edits.highlights = query.value(18);
    edits.shadows = query.value(19);
    edits.whites = query.value(20);
    edits.blacks = query.value(21);
    edits.vibrance = query.value(22);
    edits.saturation = query.value(23);
    edits.enabledCorrections = static_cast<std::uint32_t>(query.value(24));
    if (query.value(25) != EditDocument::schemaVersion)
        throw std::runtime_error("Unsupported edit document version.");
    edits.validate();
    return {query.string(0), query.string(1), query.string(2), metadata, edits};
}
constexpr auto selectFields =
    "SELECT a.id,a.original_path,a.checksum,a.camera,a.lens,a.iso,"
    "a.shutter_seconds,a.aperture,a.focal_length,a.width,a.height,a."
    "orientation,a.capture_date,e.exposure,e.temperature,e.tint,e.custom_wb,e.contrast,"
    "e.highlights,e.shadows,e.whites,e.blacks,e.vibrance,e.saturation,e.enabled,e.schema_version "
    "FROM assets a JOIN edit_documents e ON e.asset_id=a.id ";
} // namespace

Catalog::Catalog(const std::filesystem::path &path) {
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }
    if (sqlite3_open_v2(path.c_str(), &db_,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX,
                        nullptr) != SQLITE_OK) {
        const std::string error =
            db_ ? sqlite3_errmsg(db_) : "Unable to allocate SQLite connection";
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        throw std::runtime_error(error);
    }
    try {
        sqlite3_busy_timeout(db_, 3000);
        execute("PRAGMA foreign_keys=ON; PRAGMA synchronous=FULL;");
        // SQLite can return BUSY immediately for concurrent first-time WAL setup,
        // without invoking the configured busy handler. Retry only that operation.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (true) {
            try {
                Statement wal(db_, "PRAGMA journal_mode=WAL");
                if (!wal.row() || wal.string(0) != "wal")
                    throw std::runtime_error("Catalog requires WAL journaling.");
                break;
            } catch (const std::exception &) {
                const int code = sqlite3_errcode(db_);
                if ((code != SQLITE_BUSY && code != SQLITE_LOCKED) ||
                    std::chrono::steady_clock::now() >= deadline)
                    throw;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        const auto schemaVersion = [this] {
            Statement version(db_, "PRAGMA user_version");
            if (!version.row()) {
                throw std::runtime_error("Cannot read catalog schema version.");
            }
            return static_cast<int>(version.value(0));
        };
        int current = schemaVersion();
        // Current-schema readers must remain usable during WAL writes/imports.
        if (current == 3)
            return;
        execute("BEGIN IMMEDIATE;");
        // Another connection may have migrated while this one awaited the lock.
        current = schemaVersion();
        if (current > 3) {
            throw std::runtime_error("Catalog was created by a newer version of Luma.");
        }
        if (current == 0) {
            execute(R"sql(
                CREATE TABLE schema_migrations(version INTEGER PRIMARY KEY, applied_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);
                CREATE TABLE assets(
                    id TEXT PRIMARY KEY, original_path TEXT NOT NULL UNIQUE, filename TEXT NOT NULL,
                    extension TEXT NOT NULL, filesize INTEGER NOT NULL CHECK(filesize>=0), checksum TEXT NOT NULL,
                    capture_date INTEGER, import_date TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                    width INTEGER NOT NULL CHECK(width>0), height INTEGER NOT NULL CHECK(height>0),
                    orientation INTEGER NOT NULL, camera TEXT, lens TEXT, focal_length REAL,
                    aperture REAL, shutter_seconds REAL, iso REAL, gps_latitude REAL, gps_longitude REAL,
                    rating INTEGER NOT NULL DEFAULT 0 CHECK(rating BETWEEN 0 AND 5),
                    flag INTEGER NOT NULL DEFAULT 0 CHECK(flag IN (-1,0,1)), color_label TEXT);
                CREATE INDEX assets_checksum ON assets(checksum,filesize);
                CREATE INDEX assets_capture_date ON assets(capture_date);
                CREATE INDEX assets_rating ON assets(rating,id);
                CREATE TABLE edit_documents(
                    asset_id TEXT PRIMARY KEY REFERENCES assets(id) ON DELETE CASCADE,
                    schema_version INTEGER NOT NULL DEFAULT 1 CHECK(schema_version=1),
                    exposure REAL NOT NULL DEFAULT 0 CHECK(exposure BETWEEN -5 AND 5),
                    revision INTEGER NOT NULL DEFAULT 0,
                    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);
                CREATE TABLE session(slot INTEGER PRIMARY KEY CHECK(slot=1), asset_id TEXT NOT NULL REFERENCES assets(id));
                INSERT INTO schema_migrations(version) VALUES(1);
                PRAGMA user_version=1;
            )sql");
            current = 1;
        }
        if (current == 1) {
            execute(R"sql(
                CREATE TABLE edit_documents_v2(
                    asset_id TEXT PRIMARY KEY REFERENCES assets(id) ON DELETE CASCADE,
                    schema_version INTEGER NOT NULL DEFAULT 2 CHECK(schema_version=2),
                    exposure REAL NOT NULL DEFAULT 0 CHECK(exposure BETWEEN -5 AND 5),
                    temperature REAL NOT NULL DEFAULT 6500 CHECK(temperature BETWEEN 2000 AND 25000),
                    tint REAL NOT NULL DEFAULT 0 CHECK(tint BETWEEN -150 AND 150),
                    custom_wb INTEGER NOT NULL DEFAULT 0 CHECK(custom_wb IN (0,1)),
                    contrast REAL NOT NULL DEFAULT 0 CHECK(contrast BETWEEN -100 AND 100),
                    highlights REAL NOT NULL DEFAULT 0 CHECK(highlights BETWEEN -100 AND 100),
                    shadows REAL NOT NULL DEFAULT 0 CHECK(shadows BETWEEN -100 AND 100),
                    whites REAL NOT NULL DEFAULT 0 CHECK(whites BETWEEN -100 AND 100),
                    blacks REAL NOT NULL DEFAULT 0 CHECK(blacks BETWEEN -100 AND 100),
                    vibrance REAL NOT NULL DEFAULT 0 CHECK(vibrance BETWEEN -100 AND 100),
                    saturation REAL NOT NULL DEFAULT 0 CHECK(saturation BETWEEN -100 AND 100),
                    enabled INTEGER NOT NULL DEFAULT 511 CHECK(typeof(enabled)='integer' AND enabled BETWEEN 0 AND 511),
                    revision INTEGER NOT NULL DEFAULT 0,
                    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);
                INSERT INTO edit_documents_v2(asset_id,exposure,revision,updated_at)
                    SELECT asset_id,exposure,revision,updated_at FROM edit_documents;
                DROP TABLE edit_documents;
                ALTER TABLE edit_documents_v2 RENAME TO edit_documents;
                INSERT INTO schema_migrations(version) VALUES(2);
                PRAGMA user_version=2;
            )sql");
            current = 2;
        }
        if (current == 2) {
            execute(R"sql(
                CREATE TABLE collections(id TEXT PRIMARY KEY,name TEXT NOT NULL UNIQUE
                    CHECK(length(trim(name)) BETWEEN 1 AND 128),created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);
                CREATE TABLE collection_assets(collection_id TEXT NOT NULL REFERENCES collections(id) ON DELETE CASCADE,
                    asset_id TEXT NOT NULL REFERENCES assets(id) ON DELETE CASCADE,PRIMARY KEY(collection_id,asset_id));
                CREATE INDEX collection_assets_asset ON collection_assets(asset_id,collection_id);
                CREATE INDEX assets_filename ON assets(filename COLLATE NOCASE,id);
                CREATE INDEX assets_flag ON assets(flag,id);
                CREATE TABLE import_sessions(id TEXT PRIMARY KEY,mode TEXT NOT NULL CHECK(mode IN ('add','copy')),
                    destination TEXT NOT NULL,state TEXT NOT NULL CHECK(state IN ('running','completed','cancelled','interrupted')),
                    owner_pid INTEGER NOT NULL,skip_duplicates INTEGER NOT NULL DEFAULT 1 CHECK(skip_duplicates IN (0,1)),created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                    finished_at TEXT);
                CREATE TABLE import_items(session_id TEXT NOT NULL REFERENCES import_sessions(id),
                    position INTEGER NOT NULL CHECK(position>=0),source_path TEXT NOT NULL,target_path TEXT NOT NULL DEFAULT '',
                    temporary_path TEXT NOT NULL DEFAULT '',checksum TEXT NOT NULL DEFAULT '',filesize INTEGER NOT NULL DEFAULT 0,
                    state TEXT NOT NULL CHECK(state IN ('queued','staged','committed','duplicate','failed','cancelled')),
                    asset_id TEXT REFERENCES assets(id),error TEXT NOT NULL DEFAULT '',PRIMARY KEY(session_id,position));
                INSERT INTO schema_migrations(version) VALUES(3);
                PRAGMA user_version=3;
            )sql");
        }
        execute("COMMIT;");
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        sqlite3_close(db_);
        db_ = nullptr;
        throw;
    }
}
Catalog::~Catalog() {
    if (db_) {
        sqlite3_close(db_);
    }
}
void Catalog::execute(const char *sql) const {
    if (sqlite3_exec(db_, sql, nullptr, nullptr, nullptr) != SQLITE_OK) {
        throw std::runtime_error(std::string("Catalog: ") + sqlite3_errmsg(db_));
    }
}

AssetRecord Catalog::addAndSelect(const std::filesystem::path &source, const std::string &checksum,
                                  const PhotoMetadata &metadata) {
    execute("BEGIN IMMEDIATE;");
    try {
        auto result = addAssetRecord(source, checksum, metadata, true);
        execute("COMMIT;");
        return result;
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}
AssetRecord Catalog::addAssetRecord(const std::filesystem::path &source,
                                    const std::string &checksum, const PhotoMetadata &metadata,
                                    bool selectActive) {
    const auto path = std::filesystem::canonical(source);
    Statement query(db_, (std::string(selectFields) + "WHERE a.original_path=?1").c_str());
    query.text(1, path.string());
    AssetRecord result;
    if (query.row()) {
        result = readAsset(query);
        if (result.checksum != checksum) {
            throw std::runtime_error("Original file changed. Existing edits were preserved; "
                                     "automatic replacement is disabled.");
        }
    } else {
        result = {newID(), path, checksum, metadata, {}};
        Statement insert(
            db_, "INSERT INTO "
                 "assets(id,original_path,filename,extension,filesize,checksum,capture_date,"
                 "width,height,orientation,camera,lens,focal_length,aperture,shutter_seconds,"
                 "iso) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16)");
        insert.text(1, result.id);
        insert.text(2, path.string());
        insert.text(3, path.filename().string());
        insert.text(4, path.extension().string());
        insert.number(5, static_cast<double>(std::filesystem::file_size(path)));
        insert.text(6, checksum);
        insert.number(7, metadata.captureTimestamp
                             ? std::optional<double>(*metadata.captureTimestamp)
                             : std::nullopt);
        insert.number(8, metadata.width);
        insert.number(9, metadata.height);
        insert.number(10, metadata.orientation);
        insert.text(11, metadata.camera);
        insert.text(12, metadata.lens);
        insert.number(13, metadata.focalLength);
        insert.number(14, metadata.aperture);
        insert.number(15, metadata.shutterSeconds);
        insert.number(16, metadata.iso);
        insert.row();
        Statement edit(db_, "INSERT INTO edit_documents(asset_id) VALUES(?1)");
        edit.text(1, result.id);
        edit.row();
    }
    if (selectActive) {
        Statement select(db_, "INSERT INTO session(slot,asset_id) VALUES(1,?1) ON CONFLICT(slot) "
                              "DO UPDATE SET asset_id=excluded.asset_id");
        select.text(1, result.id);
        select.row();
    }
    return result;
}

std::optional<AssetRecord> Catalog::activeAsset() const {
    Statement query(
        db_,
        (std::string(selectFields) + "JOIN session s ON s.asset_id=a.id WHERE s.slot=1").c_str());
    return query.row() ? std::optional<AssetRecord>(readAsset(query)) : std::nullopt;
}
std::optional<AssetRecord> Catalog::assetByID(const std::string &id) const {
    Statement query(db_, (std::string(selectFields) + "WHERE a.id=?1").c_str());
    query.text(1, id);
    return query.row() ? std::optional<AssetRecord>(readAsset(query)) : std::nullopt;
}
std::optional<AssetRecord> Catalog::duplicate(const std::string &checksum,
                                              std::int64_t size) const {
    Statement query(db_, (std::string(selectFields) +
                          "WHERE a.checksum=?1 AND a.filesize=?2 ORDER BY a.id LIMIT 1")
                             .c_str());
    query.text(1, checksum);
    query.integer(2, size);
    return query.row() ? std::optional<AssetRecord>(readAsset(query)) : std::nullopt;
}
void Catalog::saveEdits(const std::string &assetID, EditDocument edits) {
    edits.validate();
    execute("BEGIN IMMEDIATE;");
    try {
        Statement query(
            db_,
            "UPDATE edit_documents SET exposure=?1,temperature=?2,tint=?3,custom_wb=?4,"
            "contrast=?5,highlights=?6,shadows=?7,whites=?8,blacks=?9,vibrance=?10,saturation=?11,"
            "enabled=?12,revision=revision+1,updated_at=CURRENT_TIMESTAMP WHERE asset_id=?13");
        query.number(1, edits.exposure);
        query.number(2, edits.temperature);
        query.number(3, edits.tint);
        query.number(4, edits.customWhiteBalance ? 1 : 0);
        query.number(5, edits.contrast);
        query.number(6, edits.highlights);
        query.number(7, edits.shadows);
        query.number(8, edits.whites);
        query.number(9, edits.blacks);
        query.number(10, edits.vibrance);
        query.number(11, edits.saturation);
        query.number(12, edits.enabledCorrections);
        query.text(13, assetID);
        query.row();
        if (sqlite3_changes(db_) != 1)
            throw std::runtime_error("Cannot save edits: asset is missing from catalog.");
        execute("COMMIT;");
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}
void Catalog::saveExposure(const std::string &assetID, double ev) {
    EditDocument{ev}.validate();
    execute("BEGIN IMMEDIATE;");
    try {
        Statement query(
            db_, "UPDATE edit_documents SET "
                 "exposure=?1,revision=revision+1,updated_at=CURRENT_TIMESTAMP WHERE asset_id=?2");
        query.number(1, ev);
        query.text(2, assetID);
        query.row();
        if (sqlite3_changes(db_) != 1) {
            throw std::runtime_error("Cannot save edits: asset is missing from catalog.");
        }
        execute("COMMIT;");
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}
void Catalog::verify() const {
    Statement integrity(db_, "PRAGMA integrity_check");
    if (!integrity.row() || integrity.string(0) != "ok") {
        throw std::runtime_error("Catalog integrity check failed.");
    }
    Statement foreignKeys(db_, "PRAGMA foreign_key_check");
    if (foreignKeys.row()) {
        throw std::runtime_error("Catalog foreign key check failed.");
    }
}
} // namespace luma
