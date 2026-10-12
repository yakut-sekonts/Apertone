#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace luma {
/// Replace ICC APP2 segments without changing compressed JPEG pixels.
std::vector<std::uint8_t> embedJpegICC(std::span<const std::uint8_t> jpeg,
                                       std::span<const std::uint8_t> profile);
} // namespace luma
