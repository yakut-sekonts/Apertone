#pragma once
// This product includes DNG technology under license by Adobe.
#include <array>
#include <cstdint>
#include <span>

namespace luma {
using DngChromaticity = std::array<double, 2>;
using DngIlluminantWeights = std::array<double, 3>;

DngChromaticity dngStandardIlluminant(unsigned lightSource);
DngChromaticity dngCustomIlluminant(std::span<const std::uint8_t> payload, bool littleEndian);
DngChromaticity dngWhiteChromaticity(double temperature, double tint);
std::array<double, 2> dngWhiteTemperatureTint(DngChromaticity white);
DngIlluminantWeights dngTripleIlluminantWeights(DngChromaticity white,
                                                const std::array<DngChromaticity, 3> &illuminants);
} // namespace luma
