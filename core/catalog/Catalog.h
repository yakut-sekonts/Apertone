#pragma once
#include "core/edits/EditDocument.h"
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
    void verify() const;

  private:
    sqlite3 *db_ = nullptr;
    void execute(const char *sql) const;
};
} // namespace luma
