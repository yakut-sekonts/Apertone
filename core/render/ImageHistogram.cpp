#include "core/render/ImageHistogram.h"
#include "core/image/FloatImage.h"
#include "core/render/AdvancedColor.h"
#include <algorithm>
#include <cmath>

namespace luma {
std::uint8_t clippingFlags(const std::array<float, 3> &rgb) {
    const auto maximum = *std::max_element(rgb.begin(), rgb.end());
    return static_cast<std::uint8_t>((maximum <= 0 ? 1 : 0) | (maximum >= 1 ? 2 : 0));
}
void ImageHistogram::add(const std::array<float, 3> &rgb, float coverage) {
    if (!std::isfinite(coverage) || coverage < 0 || coverage > 1)
        throw std::runtime_error("Invalid histogram coverage.");
    if (coverage == 0)
        return;
    for (const auto value : rgb)
        if (!std::isfinite(value))
            throw std::runtime_error("Non-finite histogram pixel.");
    const auto bin = [](double linear) {
        return static_cast<std::size_t>(
            std::clamp(std::lround(encodeSRGB(std::clamp(linear, 0.0, 1.0)) * 255), 0L, 255L));
    };
    for (std::size_t channel = 0; channel < 3; ++channel)
        bins[channel][bin(rgb[channel])] += coverage;
    const double luminance = 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2];
    bins[3][bin(luminance)] += coverage;
    const auto flags = clippingFlags(rgb);
    if (flags & 1)
        shadows += coverage;
    if (flags & 2)
        highlights += coverage;
    weight += coverage;
    ++samples;
}
HistogramSamplingGrid::HistogramSamplingGrid(int width, int height) {
    pixelCount(width, height);
    const auto positions = [](int size) {
        std::vector<bool> result(static_cast<std::size_t>(size), false);
        const int count = std::min(size, 512);
        for (int index = 0; index < count; ++index)
            result[static_cast<std::size_t>((2 * index + 1) * size / (2 * count))] = true;
        return result;
    };
    columns_ = positions(width);
    rows_ = positions(height);
}
bool HistogramSamplingGrid::contains(int x, int y) const {
    return x >= 0 && y >= 0 && static_cast<std::size_t>(x) < columns_.size() &&
           static_cast<std::size_t>(y) < rows_.size() && rows_[static_cast<std::size_t>(y)] &&
           columns_[static_cast<std::size_t>(x)];
}
} // namespace luma
