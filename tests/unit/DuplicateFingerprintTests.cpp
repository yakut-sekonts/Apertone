#include "core/catalog/Catalog.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unistd.h>

namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void throws(F work) {
    try {
        work();
    } catch (const std::exception &) {
        return;
    }
    throw std::runtime_error("Expected invalid request to fail.");
}
void insert(sqlite3 *db, const std::string &id, const std::string &checksum, int size,
            const std::string &path) {
    sqlite3_stmt *raw = nullptr;
    require(sqlite3_prepare_v2(db,
                               "INSERT INTO assets(id,original_path,filename,extension,filesize,"
                               "checksum,width,height,orientation) VALUES(?1,?2,?3,'jpg',?4,"
                               "?5,1,1,1)",
                               -1, &raw, nullptr) == SQLITE_OK,
            "Prepare insert failed.");
    const auto bind = [&](int index, const std::string &value) {
        require(sqlite3_bind_text(raw, index, value.data(), static_cast<int>(value.size()),
                                  SQLITE_TRANSIENT) == SQLITE_OK,
                "Bind failed.");
    };
    bind(1, id);
    bind(2, path);
    bind(3, id);
    require(sqlite3_bind_int(raw, 4, size) == SQLITE_OK, "Bind size failed.");
    bind(5, checksum);
    const int result = sqlite3_step(raw);
    sqlite3_finalize(raw);
    require(result == SQLITE_DONE, "Insert failed.");
}
void checkPlan(sqlite3 *db, const std::string &predicate, bool seekRowID) {
    const std::string sql =
        "SELECT a.checksum,a.filesize,a.id,a.original_path,a.rowid FROM assets a "
        "WHERE length(a.checksum)=64 AND length(CAST(a.checksum AS BLOB))=64 "
        "AND a.checksum NOT GLOB '*[^0-9a-f]*' "
        "AND EXISTS(SELECT 1 FROM assets b WHERE b.checksum=a.checksum "
        "AND b.filesize=a.filesize AND b.rowid!=a.rowid) " +
        predicate + " ORDER BY a.checksum,a.filesize,a.rowid LIMIT ?4";
    for (const auto &prefix : {std::string("EXPLAIN QUERY PLAN "), std::string("EXPLAIN ")}) {
        sqlite3_stmt *statement = nullptr;
        require(sqlite3_prepare_v2(db, (prefix + sql).c_str(), -1, &statement, nullptr) ==
                    SQLITE_OK,
                "EXPLAIN failed.");
        bool indexUsed = false, seekGT = false, rowIDRange = false;
        int result = SQLITE_OK;
        while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
            const int column = prefix == "EXPLAIN " ? 1 : 3;
            const auto *text = sqlite3_column_text(statement, column);
            const std::string detail = text ? reinterpret_cast<const char *>(text) : "";
            require(detail.find("TEMP B-TREE") == std::string::npos &&
                        detail.find("SorterOpen") == std::string::npos,
                    "Page query requires an external sort.");
            indexUsed |= detail.find("assets_checksum") != std::string::npos;
            rowIDRange |= detail.find("rowid>?") != std::string::npos;
            seekGT |= detail == "SeekGT";
        }
        sqlite3_finalize(statement);
        require(result == SQLITE_DONE, "EXPLAIN step failed.");
        if (prefix == "EXPLAIN QUERY PLAN ") {
            require(indexUsed, "Fingerprint index not used.");
            require(!seekRowID || rowIDRange, "Within-group page did not use rowid range.");
        } else if (seekRowID) {
            require(seekGT, "Within-group page did not seek beyond cursor.");
        }
    }
}
luma::CancellationSource *queryCancellation = nullptr;
bool queryStarted = false;
int cancelAtQueryStart(unsigned event, void *, void *statement, void *) {
    if (event == SQLITE_TRACE_STMT && queryCancellation) {
        const char *sql = sqlite3_sql(static_cast<sqlite3_stmt *>(statement));
        if (sql && std::string(sql).starts_with("SELECT a.checksum,a.filesize")) {
            queryStarted = true;
            queryCancellation->cancel();
        }
    }
    return 0;
}
int installCancellationTrace(sqlite3 *db, char **, const sqlite3_api_routines *) {
    return sqlite3_trace_v2(db, SQLITE_TRACE_STMT, cancelAtQueryStart, nullptr);
}

void cancellationAndLegacy(const std::filesystem::path &path) {
    sqlite3 *db = nullptr;
    require(sqlite3_open(path.c_str(), &db) == SQLITE_OK, "Legacy fixture open failed.");
    const char *schema =
        "CREATE TABLE assets(id TEXT PRIMARY KEY,original_path TEXT NOT NULL,checksum TEXT NOT "
        "NULL,filesize INTEGER NOT NULL);"
        "CREATE INDEX assets_checksum ON assets(checksum,filesize);"
        "PRAGMA user_version=2;"
        "WITH RECURSIVE n(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM n WHERE x<1000) "
        "INSERT INTO assets SELECT printf('id%06d',x),printf('/missing/%d',x),"
        "printf('%064x',x),1 FROM n;";
    require(sqlite3_exec(db, schema, nullptr, nullptr, nullptr) == SQLITE_OK,
            "Legacy fixture setup failed.");
    sqlite3_close(db);
    require(luma::Catalog::duplicateFingerprintPageFrom(path).rows.empty(),
            "Legacy singleton fixture returned duplicates.");
    luma::CancellationSource midQuery;
    queryCancellation = &midQuery;
    queryStarted = false;
    require(sqlite3_auto_extension(reinterpret_cast<void (*)()>(installCancellationTrace)) ==
                SQLITE_OK,
            "Unable to install query trace.");
    bool interrupted = false;
    try {
        luma::Catalog::duplicateFingerprintPageFrom(path, std::nullopt, 40, midQuery.token());
    } catch (const std::exception &error) {
        interrupted = std::string(error.what()).find("interrupted") != std::string::npos;
    }
    sqlite3_cancel_auto_extension(reinterpret_cast<void (*)()>(installCancellationTrace));
    queryCancellation = nullptr;
    require(queryStarted && interrupted, "In-flight query was not interrupted by SQLite.");
    require(luma::Catalog::duplicateFingerprintPageFrom(path).rows.empty(),
            "Interrupted query changed the catalog or prevented a later read.");
    require(sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK,
            "Legacy fixture reopen failed.");
    sqlite3_stmt *statement = nullptr;
    require(sqlite3_prepare_v2(db, "PRAGMA user_version", -1, &statement, nullptr) == SQLITE_OK &&
                sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_int(statement, 0) == 2,
            "Read-only query migrated the catalog.");
    sqlite3_finalize(statement);
    require(sqlite3_prepare_v2(db, "SELECT count(*) FROM sqlite_master WHERE type='table'", -1,
                               &statement, nullptr) == SQLITE_OK &&
                sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_int(statement, 0) == 1,
            "Read-only query changed catalog schema.");
    sqlite3_finalize(statement);
    sqlite3_close(db);
}
} // namespace

int main() {
    char root[] = "/tmp/apertone-duplicate-fingerprints-XXXXXX";
    if (!mkdtemp(root))
        return 1;
    try {
        const auto catalogPath = std::filesystem::path(root) / "catalog.sqlite";
        const auto original = std::filesystem::path(root) / "оригинал.jpg";
        {
            std::ofstream file(original, std::ios::binary);
            file << "original remains untouched";
        }
        const auto missing = std::filesystem::path(root) / "missing.sqlite";
        throws([&] { luma::Catalog::duplicateFingerprintPageFrom(missing); });
        require(!std::filesystem::exists(missing), "Read-only open created a missing catalog.");
        luma::CancellationSource cancelled;
        cancelled.cancel();
        throws([&] {
            luma::Catalog::duplicateFingerprintPageFrom(missing, std::nullopt, 40,
                                                        cancelled.token());
        });
        require(!std::filesystem::exists(missing), "Cancelled query created a catalog.");
        luma::Catalog catalog(catalogPath);
        require(luma::Catalog::duplicateFingerprintPageFrom(catalogPath).rows.empty(),
                "Empty catalog returned rows.");
        sqlite3 *raw = nullptr;
        require(sqlite3_open(catalogPath.c_str(), &raw) == SQLITE_OK, "Fixture open failed.");
        const std::string a(64, 'a'), b(64, 'b');
        insert(raw, "a00", a, 3, original.string());
        require(luma::Catalog::duplicateFingerprintPageFrom(catalogPath).rows.empty(),
                "Singleton returned a row.");
        for (int i = 1; i < 45; ++i)
            insert(raw, "a" + std::to_string(100 + i), a, 3,
                   (std::filesystem::path(root) / ("missing" + std::to_string(i))).string());
        insert(raw, "other-size", a, 4, std::string(root) + "/other-size");
        insert(raw, "b1", b, 8, std::string(root) + "/b1");
        insert(raw, "b2", b, 8, std::string(root) + "/b2");
        const std::string invalid[] = {std::string(64, 'A'), std::string(63, 'a'),
                                       std::string(63, 'a') + "g", std::string(63, 'a') + '\0'};
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 2; ++j) {
                const auto id = "invalid" + std::to_string(i) + std::to_string(j);
                insert(raw, id, invalid[i], 3, std::string(root) + "/" + id);
            }
        checkPlan(raw, "", false);
        checkPlan(raw, "AND a.checksum=?1 AND a.filesize=?2 AND a.rowid>?3", true);
        checkPlan(raw, "AND (a.checksum,a.filesize)>(?1,?2)", false);
        sqlite3_close(raw);
        const auto first = luma::Catalog::duplicateFingerprintPageFrom(catalogPath);
        require(first.rows.size() == 40 && first.hasMore, "Default page limit failed.");
        std::optional<luma::DuplicateFingerprintCursor> cursor;
        std::set<std::string> ids;
        std::optional<std::tuple<std::string, std::int64_t, std::int64_t>> previous;
        do {
            const auto page = luma::Catalog::duplicateFingerprintPageFrom(catalogPath, cursor, 7);
            require(!page.rows.empty(), "Unexpected empty page.");
            for (const auto &row : page.rows) {
                const auto key = std::make_tuple(row.checksum, row.size, row.rowID);
                require(!previous || *previous < key, "Results out of order.");
                previous = key;
                require(ids.insert(row.assetID).second, "Pagination duplicated an asset.");
                require((row.checksum == a && row.size == 3) ||
                            (row.checksum == b && row.size == 8),
                        "Invalid fingerprint or different-size singleton returned.");
            }
            const auto &last = page.rows.back();
            cursor = luma::DuplicateFingerprintCursor{last.checksum, last.size, last.rowID};
            if (!page.hasMore)
                break;
        } while (true);
        require(ids.size() == 47, "Pagination lost rows.");
        require(luma::Catalog::duplicateFingerprintPageFrom(catalogPath, cursor, 7).rows.empty(),
                "End cursor failed.");
        const auto beforeMutation =
            luma::Catalog::duplicateFingerprintPageFrom(catalogPath, std::nullopt, 7);
        const auto lastBeforeMutation = beforeMutation.rows.back();
        const luma::DuplicateFingerprintCursor mutationCursor{
            lastBeforeMutation.checksum, lastBeforeMutation.size, lastBeforeMutation.rowID};
        require(sqlite3_open(catalogPath.c_str(), &raw) == SQLITE_OK, "Fixture reopen failed.");
        require(sqlite3_exec(raw, "DELETE FROM assets WHERE id='a00'", nullptr, nullptr, nullptr) ==
                    SQLITE_OK,
                "Delete failed.");
        insert(raw, "a999", a, 3, std::string(root) + "/insert-after-cursor");
        sqlite3_close(raw);
        std::set<std::string> later;
        auto mutationAfter = std::optional<luma::DuplicateFingerprintCursor>{mutationCursor};
        for (;;) {
            const auto page =
                luma::Catalog::duplicateFingerprintPageFrom(catalogPath, mutationAfter, 7);
            for (const auto &row : page.rows) {
                require(std::tie(row.checksum, row.size, row.rowID) >
                            std::tie(mutationCursor.checksum, mutationCursor.size,
                                     mutationCursor.rowID),
                        "Mutation repeated a row at or before the cursor.");
                require(later.insert(row.assetID).second, "Mutation produced a repeated row.");
            }
            if (!page.hasMore)
                break;
            const auto &last = page.rows.back();
            mutationAfter = luma::DuplicateFingerprintCursor{last.checksum, last.size, last.rowID};
        }
        require(!later.contains("a00") && later.contains("a999"),
                "Keyset pagination did not honor its cursor after catalog changes.");
        require(
            !luma::Catalog::duplicateFingerprintPageFrom(catalogPath, std::nullopt, 128).hasMore,
            "hasMore set on final page.");
        throws([&] { luma::Catalog::duplicateFingerprintPageFrom(catalogPath, std::nullopt, 0); });
        throws(
            [&] { luma::Catalog::duplicateFingerprintPageFrom(catalogPath, std::nullopt, 129); });
        throws([&] {
            luma::Catalog::duplicateFingerprintPageFrom(
                catalogPath, luma::DuplicateFingerprintCursor{"bad", 0, 0});
        });
        throws([&] {
            luma::Catalog::duplicateFingerprintPageFrom(catalogPath,
                                                        luma::DuplicateFingerprintCursor{a, -1, 0});
        });
        std::ifstream file(original, std::ios::binary);
        const std::string content((std::istreambuf_iterator<char>(file)), {});
        require(content == "original remains untouched", "Original was changed.");
        cancellationAndLegacy(std::filesystem::path(root) / "legacy.sqlite");
        std::filesystem::remove_all(root);
        std::cout << "Duplicate fingerprint pagination passed.\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        std::filesystem::remove_all(root);
        return 1;
    }
}
