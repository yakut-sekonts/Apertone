#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace luma {
struct DescriptiveMetadata {
    std::string title, caption, copyright, creator, location;
    std::vector<std::string> keywords;
    int rating = 0;
    std::string label;
    void validate() const;
    bool operator==(const DescriptiveMetadata &) const = default;
};
struct MetadataSnapshot {
    std::string id, checksum;
    std::filesystem::path path;
    DescriptiveMetadata values;
    std::int64_t revision = 0, writtenRevision = 0;
    std::string sidecarHash;
    bool initialized = false;
    std::string readError;
};
struct MetadataOverlay { DescriptiveMetadata values; bool includeText = false; };
}
