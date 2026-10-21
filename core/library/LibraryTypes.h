#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace luma {
struct LibraryFilter {
    int minimumRating = 0;
    int flag = -2; // -2: any, -1: Reject, 0: Unflagged, 1: Pick.
    std::optional<std::string> label;
    std::string collectionID;
    std::string search;
    void validate() const;
};
struct LibraryItem {
    std::string id;
    std::filesystem::path path;
    std::string filename;
    std::string checksum;
    int width = 0, height = 0, orientation = 0, rating = 0, flag = 0;
    std::string label, camera;
    bool missing = false;
};
struct CollectionRecord {
    std::string id, name;
    std::int64_t count = 0;
};
enum class ImportMode : std::uint8_t { add, copy };
struct ImportOptions {
    ImportMode mode = ImportMode::add;
    std::filesystem::path destination;
    bool recursive = true;
    bool skipDuplicates = true;
};
struct ImportSummary {
    std::string sessionID;
    std::int64_t added = 0, duplicates = 0, failed = 0, cancelled = 0;
};
struct ImportJournalItem {
    std::string sessionID;
    int index = 0;
    std::filesystem::path source, target, temporary;
    std::string checksum, state, error;
    std::int64_t size = 0;
};
struct ImportJournal {
    std::string id, state;
    ImportMode mode = ImportMode::add;
    std::int64_t ownerPID = 0;
    std::filesystem::path destination;
    std::vector<ImportJournalItem> items;
    bool skipDuplicates = true;
};
void validateLabel(const std::string &label);
} // namespace luma
