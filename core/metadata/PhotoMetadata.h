#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include "core/metadata/DescriptiveMetadata.h"

namespace luma {
struct PhotoMetadata {
    std::string camera;
    std::string lens;
    std::optional<double> iso;
    std::optional<double> shutterSeconds;
    std::optional<double> aperture;
    std::optional<double> focalLength;
    std::optional<std::int64_t> captureTimestamp;
    int width = 0;
    int height = 0;
    int orientation = 0;
    // Filled by the application decoder, independent of RAW pixel decoding.
    std::optional<DescriptiveMetadata> descriptive = std::nullopt;
    std::filesystem::path descriptiveSource{};
    std::string sidecarHash{}, descriptiveError{};
};
} // namespace luma
