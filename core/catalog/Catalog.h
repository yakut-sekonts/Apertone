#pragma once
#include "core/edits/EditDocument.h"
#include "core/library/LibraryTypes.h"
#include "core/metadata/PhotoMetadata.h"
#include <filesystem>
#include <optional>
#include <string>

struct sqlite3;
namespace luma {
struct AssetRecord {
    std::string id;
    std::filesystem::path path;
    std::string checksum;
    PhotoMetadata metadata;
    EditDocument edits;
};

/// Each instance owns a connection on the calling worker thread.
class Catalog {
  public:
    explicit Catalog(const std::filesystem::path &path);
    ~Catalog();
    Catalog(const Catalog &) = delete;
    Catalog &operator=(const Catalog &) = delete;
    AssetRecord addAndSelect(const std::filesystem::path &source, const std::string &checksum,
                             const PhotoMetadata &metadata);
    std::optional<AssetRecord> activeAsset() const;
    void saveEdits(const std::string &assetID, EditDocument edits);
    void saveExposure(const std::string &assetID, double ev);
    std::optional<AssetRecord> assetByID(const std::string &id) const;
    std::optional<AssetRecord> duplicate(const std::string &checksum, std::int64_t size) const;
    std::int64_t libraryCount(const LibraryFilter &filter = {}) const;
    std::vector<LibraryItem> libraryPage(const LibraryFilter &filter, int offset, int limit) const;
    std::vector<std::string> libraryIDs(const LibraryFilter &filter, int offset = 0,
                                        int limit = 2147483647) const;
    void setRating(const std::vector<std::string> &ids, int rating);
    void setFlag(const std::vector<std::string> &ids, int flag);
    void setLabel(const std::vector<std::string> &ids, const std::string &label);
    std::vector<CollectionRecord> collections() const;
    std::string createCollection(const std::string &name);
    void renameCollection(const std::string &id, const std::string &name);
    void deleteCollection(const std::string &id);
    void collectionMembership(const std::string &collectionID, const std::vector<std::string> &ids,
                              bool add);
    std::string beginImport(ImportMode mode, const std::filesystem::path &destination,
                            const std::vector<std::filesystem::path> &sources,
                            std::int64_t ownerPID, bool skipDuplicates = true);
    bool claimImport(const std::string &id, std::int64_t previousPID, std::int64_t newPID);
    void stageImport(const ImportJournalItem &item);
    bool commitImport(const ImportJournalItem &item, const PhotoMetadata &metadata,
                      bool skipDuplicates);
    void finishImportItem(const std::string &sessionID, int index, const std::string &state,
                          const std::string &error = "", const std::string &assetID = "");
    void finishImport(const std::string &sessionID, const std::string &state);
    std::vector<ImportJournal> unfinishedImports() const;
    ImportSummary importSummary(const std::string &sessionID) const;
    std::vector<std::string> importProblems(const std::string &sessionID) const;
    void verify() const;

  private:
    sqlite3 *db_ = nullptr;
    void execute(const char *sql) const;
    AssetRecord addAssetRecord(const std::filesystem::path &source, const std::string &checksum,
                               const PhotoMetadata &metadata, bool selectActive);
    void updateMetadata(const std::vector<std::string> &ids, const char *sql,
                        std::optional<int> number, const std::string &text = "");
};
} // namespace luma
