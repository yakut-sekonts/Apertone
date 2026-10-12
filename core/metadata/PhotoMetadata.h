#pragma once
#include <cstdint>
#include <optional>
#include <string>

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
};
} // namespace luma
