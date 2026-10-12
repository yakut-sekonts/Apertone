#include "core/export/JpegICC.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace luma {
std::vector<std::uint8_t> embedJpegICC(std::span<const std::uint8_t> jpeg,
                                       std::span<const std::uint8_t> profile) {
    if (jpeg.size() < 4 || jpeg[0] != 255 || jpeg[1] != 216 || profile.empty())
        throw std::runtime_error("Invalid JPEG or ICC data.");
    constexpr std::array<std::uint8_t, 12> signature{'I', 'C', 'C', '_', 'P', 'R',
                                                     'O', 'F', 'I', 'L', 'E', 0};
    constexpr std::size_t maximumChunk = 65519;
    const auto count = (profile.size() + maximumChunk - 1) / maximumChunk;
    if (count > 255)
        throw std::runtime_error("ICC profile exceeds JPEG APP2 budget.");
    std::vector<std::uint8_t> segments;
    segments.reserve(profile.size() + count * 18);
    for (std::size_t chunk = 0; chunk < count; ++chunk) {
        const auto offset = chunk * maximumChunk;
        const auto size = std::min(maximumChunk, profile.size() - offset);
        const auto length = size + 16;
        segments.insert(segments.end(), {255, 226, static_cast<std::uint8_t>(length >> 8),
                                         static_cast<std::uint8_t>(length & 255)});
        segments.insert(segments.end(), signature.begin(), signature.end());
        segments.push_back(static_cast<std::uint8_t>(chunk + 1));
        segments.push_back(static_cast<std::uint8_t>(count));
        segments.insert(segments.end(), profile.begin() + offset, profile.begin() + offset + size);
    }
    std::vector<std::uint8_t> result{255, 216};
    result.reserve(jpeg.size() + segments.size());
    bool inserted = false;
    std::size_t offset = 2;
    while (offset + 4 <= jpeg.size()) {
        if (jpeg[offset] != 255)
            throw std::runtime_error("Invalid JPEG marker.");
        const auto marker = jpeg[offset + 1];
        if (marker == 218) {
            if (!inserted)
                result.insert(result.end(), segments.begin(), segments.end());
            result.insert(result.end(), jpeg.begin() + offset, jpeg.end());
            return result;
        }
        const std::size_t length =
            static_cast<std::size_t>(jpeg[offset + 2]) * 256 + jpeg[offset + 3];
        if (length < 2 || offset + 2 + length > jpeg.size())
            throw std::runtime_error("Truncated JPEG segment.");
        const bool oldICC =
            marker == 226 && length >= 16 &&
            std::equal(signature.begin(), signature.end(), jpeg.begin() + offset + 4);
        if (!inserted && marker != 224 && marker != 225) {
            result.insert(result.end(), segments.begin(), segments.end());
            inserted = true;
        }
        if (!oldICC)
            result.insert(result.end(), jpeg.begin() + offset, jpeg.begin() + offset + 2 + length);
        offset += 2 + length;
    }
    throw std::runtime_error("JPEG has no scan data.");
}
} // namespace luma
