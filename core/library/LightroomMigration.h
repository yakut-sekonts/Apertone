#pragma once

#include "core/edits/EditDocument.h"
#include "core/library/SmartCollection.h"
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
    std::optional<std::int64_t> editedAt;
};

struct LightroomMigrationCollection {
    std::int64_t sourceID = 0;
    std::string name;
    std::vector<std::string> members;
    bool smart = false;
    std::optional<SmartRule> rule;
    std::string sourceRule;
};

struct LightroomMigrationResult {
    std::size_t added = 0;
    std::size_t reused = 0;
    std::size_t versions = 0;
    std::size_t copies = 0;
    std::size_t unchanged = 0;
    std::size_t collections = 0;
    std::size_t smartCollections = 0;
};
} // namespace luma
