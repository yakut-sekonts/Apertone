#include "core/catalog/Catalog.h"
#include <array>
#include <iomanip>
#include <random>
#include <sqlite3.h>
#include <sstream>

namespace luma {
namespace {
class Statement {
  public:
    Statement(sqlite3 *db, const char *sql) : db_(db) {
        if (sqlite3_prepare_v2(db, sql, -1, &statement_, nullptr) != SQLITE_OK) {
            fail();
        }
    }
    ~Statement() {
        sqlite3_finalize(statement_);
    }
    Statement(const Statement &) = delete;
    Statement &operator=(const Statement &) = delete;
    void text(int index, const std::string &value) {
        if (sqlite3_bind_text(statement_, index, value.c_str(), static_cast<int>(value.size()),
                              SQLITE_TRANSIENT) != SQLITE_OK) {
            fail();
        }
    }
    void number(int index, std::optional<double> value) {
        const int code = value ? sqlite3_bind_double(statement_, index, *value)
                               : sqlite3_bind_null(statement_, index);
        if (code != SQLITE_OK) {
            fail();
        }
    }
    bool row() {
        const int code = sqlite3_step(statement_);
        if (code != SQLITE_ROW && code != SQLITE_DONE) {
            fail();
        }
        return code == SQLITE_ROW;
    }
    std::string string(int column) const {
        const auto *value = sqlite3_column_text(statement_, column);
        return value ? reinterpret_cast<const char *>(value) : "";
    }
    std::optional<double> optional(int column) const {
        return sqlite3_column_type(statement_, column) == SQLITE_NULL
                   ? std::nullopt
                   : std::optional<double>(sqlite3_column_double(statement_, column));
    }
    double value(int column) const {
        return sqlite3_column_double(statement_, column);
    }

  private:
    sqlite3 *db_;
    sqlite3_stmt *statement_ = nullptr;
    [[noreturn]] void fail() const {
        throw std::runtime_error(std::string("Catalog: ") + sqlite3_errmsg(db_));
    }
};
std::string uuid() {
    std::random_device random;
    std::array<unsigned char, 16> bytes{};
    for (auto &byte : bytes) {
        byte = static_cast<unsigned char>(random());
    }
    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    bytes[8] = (bytes[8] & 0x3F) | 0x80;
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            stream << '-';
        }
        stream << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return stream.str();
}
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
    edits.validate();
    return {query.string(0), query.string(1), query.string(2), metadata, edits};
}
constexpr auto selectFields = "SELECT a.id,a.original_path,a.checksum,a.camera,a.lens,a.iso,"
                              "a.shutter_seconds,a.aperture,a.focal_length,a.width,a.height,a."
                              "orientation,a.capture_date,e.exposure "
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
        {
            Statement wal(db_, "PRAGMA journal_mode=WAL");
            if (!wal.row() || wal.string(0) != "wal") {
                throw std::runtime_error("Catalog requires WAL journaling.");
            }
        }
        int current = 0;
        {
            Statement version(db_, "PRAGMA user_version");
            if (!version.row()) {
                throw std::runtime_error("Cannot read catalog schema version.");
            }
            current = static_cast<int>(version.value(0));
        }
        if (current > 1) {
            throw std::runtime_error("Catalog was created by a newer version of Luma.");
        }
        if (current == 0) {
            execute("BEGIN IMMEDIATE;");
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
            execute("COMMIT;");
        }
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
    const auto path = std::filesystem::canonical(source);
    execute("BEGIN IMMEDIATE;");
    try {
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
            result = {uuid(), path, checksum, metadata, {}};
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
        Statement select(db_, "INSERT INTO session(slot,asset_id) VALUES(1,?1) ON CONFLICT(slot) "
                              "DO UPDATE SET asset_id=excluded.asset_id");
        select.text(1, result.id);
        select.row();
        execute("COMMIT;");
        return result;
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}

std::optional<AssetRecord> Catalog::activeAsset() const {
    Statement query(
        db_,
        (std::string(selectFields) + "JOIN session s ON s.asset_id=a.id WHERE s.slot=1").c_str());
    return query.row() ? std::optional<AssetRecord>(readAsset(query)) : std::nullopt;
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
