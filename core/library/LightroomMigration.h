#pragma once

#include "core/edits/EditDocument.h"
#include "core/metadata/PhotoMetadata.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace luma {
struct LightroomMigrationPhoto {
    std::string sourceID;
    std::string masterID;
    std::string copyName;
    std::filesystem::path path;
    std::string checksum;
    PhotoMetadata metadata;
    std::optional<EditDocument> edits;
    std::string developDocument;
};

struct LightroomMigrationCollection {
    std::int64_t sourceID = 0;
    std::string name;
    std::vector<std::string> members;
};

struct LightroomMigrationResult {
    std::size_t added = 0;
    std::size_t reused = 0;
    std::size_t versions = 0;
    std::size_t unchanged = 0;
    std::size_t collections = 0;
};
} // namespace luma
