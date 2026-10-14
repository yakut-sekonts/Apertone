#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace luma {
using ColorMatrix3 = std::array<double, 9>;
std::vector<std::uint8_t> linearSRGBProfile();
ColorMatrix3 rgbToXYZD50(std::span<const std::uint8_t> profile);
ColorMatrix3 inverseColorMatrix(const ColorMatrix3 &matrix);
ColorMatrix3 multiplyColorMatrices(const ColorMatrix3 &left, const ColorMatrix3 &right);
} // namespace luma
