#ifdef LUMA_UX_PERSISTENCE_PROBE
#include "tests/ux/PersistenceProbe.h"
#endif
#include "core/catalog/Catalog.h"
#include "core/catalog/SqliteStatement.h"
#include "core/edits/EditSnapshot.h"
#include <limits>
#include <set>
#include <sqlite3.h>

namespace luma {
namespace {
using detail::Statement;
constexpr std::size_t maximumHistoryEntries = 201;
constexpr std::size_t maximumHistoryBytes = 256 * 1024 * 1024;

void validateHistory(const PersistedEditHistory &history) {
    if (history.entries.empty() || history.entries.size() > maximumHistoryEntries ||
        history.currentIndex >= history.entries.size())
        throw std::runtime_error("Invalid processing history.");
    std::set<std::uint64_t> identities;
    for (const auto &entry : history.entries) {
        entry.document.validate();
        if (entry.id > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
            entry.action.size() > 256 || entry.action.find('\0') != std::string::npos ||
            !identities.insert(entry.id).second)
            throw std::runtime_error("Invalid processing history.");
    }
}
} // namespace

void Catalog::migratePersistentWorkflow() {
    execute(R"sql(
        CREATE TABLE edit_history_states(
            asset_id TEXT NOT NULL REFERENCES assets(id) ON DELETE CASCADE,
            position INTEGER NOT NULL CHECK(position BETWEEN 0 AND 200),
            state_id INTEGER NOT NULL CHECK(state_id>=0),
            action TEXT NOT NULL DEFAULT '' CHECK(length(action)<=256),
            document TEXT NOT NULL CHECK(typeof(document)='text' AND length(document) BETWEEN 1 AND 20971520),
            is_current INTEGER NOT NULL CHECK(is_current IN (0,1)),
            PRIMARY KEY(asset_id,position),UNIQUE(asset_id,state_id));
        CREATE UNIQUE INDEX edit_history_current ON edit_history_states(asset_id) WHERE is_current=1;
        INSERT INTO schema_migrations(version) VALUES(23);
        PRAGMA user_version=23;
    )sql");
}

void Catalog::writeEditHistory(const std::string &assetID, const PersistedEditHistory &history) {
    validateHistory(history);
    std::vector<std::string> documents;
    documents.reserve(history.entries.size());
    std::size_t totalBytes = 0;
    for (const auto &entry : history.entries) {
        documents.push_back(encodeEditSnapshot(entry.document));
        if (documents.back().size() > maximumHistoryBytes - totalBytes)
            throw std::runtime_error("Processing history exceeds its storage limit.");
        totalBytes += documents.back().size();
    }

    Statement clear(db_, "DELETE FROM edit_history_states WHERE asset_id=?1");
    clear.text(1, assetID);
    clear.row();
    for (std::size_t index = 0; index < history.entries.size(); ++index) {
        const auto &entry = history.entries[index];
        Statement insert(
            db_, "INSERT INTO edit_history_states(asset_id,position,state_id,action,document,"
                 "is_current) VALUES(?1,?2,?3,?4,?5,?6)");
        insert.text(1, assetID);
        insert.integer(2, static_cast<std::int64_t>(index));
        insert.integer(3, static_cast<std::int64_t>(entry.id));
        insert.text(4, entry.action);
        insert.text(5, documents[index]);
        insert.integer(6, index == history.currentIndex ? 1 : 0);
        insert.row();
    }
}

void Catalog::clearEditHistory(const std::string &assetID) {
    Statement clear(db_, "DELETE FROM edit_history_states WHERE asset_id=?1");
    clear.text(1, assetID);
    clear.row();
}

void Catalog::saveEditsAndHistory(const std::string &assetID, const EditDocument &edits,
                                  const PersistedEditHistory &history) {
    edits.validate();
    validateHistory(history);
    if (history.entries[history.currentIndex].document != edits)
        throw std::runtime_error("Processing history does not match the current edit.");
    execute("BEGIN IMMEDIATE;");
    try {
        writeEdits(assetID, edits);
        writeEditHistory(assetID, history);
#ifdef LUMA_UX_PERSISTENCE_PROBE
        ux::observePersistenceCheckpoint(sqlite3_db_filename(db_, "main"), "catalog.before-commit",
                                         {}, {});
#endif
        execute("COMMIT;");
#ifdef LUMA_UX_PERSISTENCE_PROBE
        ux::observePersistenceCheckpoint(sqlite3_db_filename(db_, "main"), "catalog.after-commit",
                                         {}, {});
#endif
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}

std::optional<PersistedEditHistory> Catalog::editHistory(const std::string &assetID) const {
    PersistedEditHistory result;
    std::size_t totalBytes = 0;
    std::size_t position = 0;
    bool foundCurrent = false;
    Statement query(db_, "SELECT position,state_id,action,document,is_current FROM "
                         "edit_history_states WHERE asset_id=?1 ORDER BY position");
    query.text(1, assetID);
    while (query.row()) {
        if (position >= maximumHistoryEntries ||
            query.integer(0) != static_cast<std::int64_t>(position) || query.integer(1) < 0)
            throw std::runtime_error("Invalid stored processing history.");
        const auto document = query.string(3);
        if (document.empty() || document.size() > maximumEditSnapshotBytes ||
            document.size() > maximumHistoryBytes - totalBytes)
            throw std::runtime_error("Invalid stored processing history.");
        totalBytes += document.size();
        const bool current = query.integer(4) == 1;
        if (current) {
            if (foundCurrent)
                throw std::runtime_error("Invalid stored processing history.");
            foundCurrent = true;
            result.currentIndex = position;
        } else if (query.integer(4) != 0) {
            throw std::runtime_error("Invalid stored processing history.");
        }
        result.entries.push_back({static_cast<std::uint64_t>(query.integer(1)), query.string(2),
                                  decodeEditSnapshot(document)});
        ++position;
    }
    if (result.entries.empty())
        return std::nullopt;
    if (!foundCurrent)
        throw std::runtime_error("Invalid stored processing history.");
    validateHistory(result);
    return result;
}
} // namespace luma
