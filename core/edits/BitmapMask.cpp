#include "core/edits/BitmapMask.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <stdexcept>

namespace luma {
namespace {
std::vector<std::uint8_t> extremaHorizontal(std::span<const std::uint8_t> source, int width,
                                            int height, int radius, bool maximum) {
    std::vector<std::uint8_t> result(source.size());
    for (int y = 0; y < height; ++y) {
        std::deque<int> candidates;
        int next = 0;
        for (int x = 0; x < width; ++x) {
            const int right = std::min(width - 1, x + radius);
            while (next <= right) {
                const auto value = source[static_cast<std::size_t>(y) * width + next];
                while (!candidates.empty()) {
                    const auto candidate =
                        source[static_cast<std::size_t>(y) * width + candidates.back()];
                    if (maximum ? candidate > value : candidate < value)
                        break;
                    candidates.pop_back();
                }
                candidates.push_back(next++);
            }
            const int left = std::max(0, x - radius);
            while (candidates.front() < left)
                candidates.pop_front();
            result[static_cast<std::size_t>(y) * width + x] =
                source[static_cast<std::size_t>(y) * width + candidates.front()];
        }
    }
    return result;
}

std::vector<std::uint8_t> extremaVertical(std::span<const std::uint8_t> source, int width,
                                          int height, int radius, bool maximum) {
    std::vector<std::uint8_t> result(source.size());
    for (int x = 0; x < width; ++x) {
        std::deque<int> candidates;
        int next = 0;
        for (int y = 0; y < height; ++y) {
            const int bottom = std::min(height - 1, y + radius);
            while (next <= bottom) {
                const auto value = source[static_cast<std::size_t>(next) * width + x];
                while (!candidates.empty()) {
                    const auto candidate =
                        source[static_cast<std::size_t>(candidates.back()) * width + x];
                    if (maximum ? candidate > value : candidate < value)
                        break;
                    candidates.pop_back();
                }
                candidates.push_back(next++);
            }
            const int top = std::max(0, y - radius);
            while (candidates.front() < top)
                candidates.pop_front();
            result[static_cast<std::size_t>(y) * width + x] =
                source[static_cast<std::size_t>(candidates.front()) * width + x];
        }
    }
    return result;
}

std::vector<std::uint8_t> blurHorizontal(std::span<const std::uint8_t> source, int width,
                                         int height, int radius) {
    std::vector<std::uint8_t> result(source.size());
    std::vector<std::uint32_t> prefix(static_cast<std::size_t>(width) + 1);
    for (int y = 0; y < height; ++y) {
        prefix[0] = 0;
        for (int x = 0; x < width; ++x)
            prefix[static_cast<std::size_t>(x) + 1] =
                prefix[static_cast<std::size_t>(x)] +
                source[static_cast<std::size_t>(y) * width + x];
        for (int x = 0; x < width; ++x) {
            const int left = std::max(0, x - radius), right = std::min(width - 1, x + radius);
            const auto count = static_cast<std::uint32_t>(right - left + 1);
            const auto sum = prefix[static_cast<std::size_t>(right) + 1] -
                             prefix[static_cast<std::size_t>(left)];
            result[static_cast<std::size_t>(y) * width + x] =
                static_cast<std::uint8_t>((sum + count / 2) / count);
        }
    }
    return result;
}

std::vector<std::uint8_t> blurVertical(std::span<const std::uint8_t> source, int width, int height,
                                       int radius) {
    std::vector<std::uint8_t> result(source.size());
    std::vector<std::uint32_t> prefix(static_cast<std::size_t>(height) + 1);
    for (int x = 0; x < width; ++x) {
        prefix[0] = 0;
        for (int y = 0; y < height; ++y)
            prefix[static_cast<std::size_t>(y) + 1] =
                prefix[static_cast<std::size_t>(y)] +
                source[static_cast<std::size_t>(y) * width + x];
        for (int y = 0; y < height; ++y) {
            const int top = std::max(0, y - radius), bottom = std::min(height - 1, y + radius);
            const auto count = static_cast<std::uint32_t>(bottom - top + 1);
            const auto sum = prefix[static_cast<std::size_t>(bottom) + 1] -
                             prefix[static_cast<std::size_t>(top)];
            result[static_cast<std::size_t>(y) * width + x] =
                static_cast<std::uint8_t>((sum + count / 2) / count);
        }
    }
    return result;
}
} // namespace

BitmapMask BitmapMask::refined(int edgeShiftPixels, int featherRadiusPixels) const {
    if (!values_ || edgeShiftPixels < -1024 || edgeShiftPixels > 1024 || featherRadiusPixels < 0 ||
        featherRadiusPixels > 1024)
        throw std::runtime_error("Invalid bitmap mask refinement.");
    if (edgeShiftPixels == 0 && featherRadiusPixels == 0)
        return *this;
    std::vector<std::uint8_t> result(values_->begin(), values_->end());
    if (edgeShiftPixels != 0) {
        const int radius = std::abs(edgeShiftPixels);
        const bool maximum = edgeShiftPixels > 0;
        result = extremaHorizontal(result, width_, height_, radius, maximum);
        result = extremaVertical(result, width_, height_, radius, maximum);
    }
    if (featherRadiusPixels > 0) {
        result = blurHorizontal(result, width_, height_, featherRadiusPixels);
        result = blurVertical(result, width_, height_, featherRadiusPixels);
    }
    return BitmapMask(width_, height_, std::move(result));
}
} // namespace luma
