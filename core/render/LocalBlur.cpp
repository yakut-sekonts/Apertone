#include "core/render/LocalBlur.h"
#include <algorithm>
#include <cmath>

namespace luma {
namespace {
void boxBlur(std::vector<float> &pixels, int width, int height, int radius,
             CancellationToken stop) {
    std::vector<float> scratch(pixels.size());
    for (int y = 0; y < height; ++y) {
        checkCancellation(stop);
        for (int channel = 0; channel < 3; ++channel) {
            double sum = 0;
            for (int x = 0; x <= std::min(width - 1, radius); ++x)
                sum += pixels[(static_cast<std::size_t>(y) * width + x) * 3 + channel];
            for (int x = 0; x < width; ++x) {
                const int first = std::max(0, x - radius);
                const int last = std::min(width - 1, x + radius);
                scratch[(static_cast<std::size_t>(y) * width + x) * 3 + channel] =
                    static_cast<float>(sum / (last - first + 1));
                const int remove = x - radius;
                const int add = x + radius + 1;
                if (remove >= 0)
                    sum -= pixels[(static_cast<std::size_t>(y) * width + remove) * 3 + channel];
                if (add < width)
                    sum += pixels[(static_cast<std::size_t>(y) * width + add) * 3 + channel];
            }
        }
    }
    for (int x = 0; x < width; ++x) {
        checkCancellation(stop);
        for (int channel = 0; channel < 3; ++channel) {
            double sum = 0;
            for (int y = 0; y <= std::min(height - 1, radius); ++y)
                sum += scratch[(static_cast<std::size_t>(y) * width + x) * 3 + channel];
            for (int y = 0; y < height; ++y) {
                const int first = std::max(0, y - radius);
                const int last = std::min(height - 1, y + radius);
                pixels[(static_cast<std::size_t>(y) * width + x) * 3 + channel] =
                    static_cast<float>(sum / (last - first + 1));
                const int remove = y - radius;
                const int add = y + radius + 1;
                if (remove >= 0)
                    sum -= scratch[(static_cast<std::size_t>(remove) * width + x) * 3 + channel];
                if (add < height)
                    sum += scratch[(static_cast<std::size_t>(add) * width + x) * 3 + channel];
            }
        }
    }
}
} // namespace
LocalBlurPass::LocalBlurPass(int width, int height, bool active,
                             const std::function<std::array<float, 3>(int, int)> &source,
                             CancellationToken stop)
    : sourceWidth_(width), sourceHeight_(height) {
    if (!active)
        return;
    const double scale = std::min(1.0, 2048.0 / std::max(width, height));
    width_ = std::max(1, static_cast<int>(std::lround(width * scale)));
    height_ = std::max(1, static_cast<int>(std::lround(height * scale)));
    pixels_.resize(pixelCount(width_, height_) * 3);
    for (int y = 0; y < height_; ++y) {
        checkCancellation(stop);
        const int sourceY = std::min(height - 1, static_cast<int>((y + 0.5) * height / height_));
        for (int x = 0; x < width_; ++x) {
            const int sourceX = std::min(width - 1, static_cast<int>((x + 0.5) * width / width_));
            const auto color = source(sourceX, sourceY);
            const auto offset = (static_cast<std::size_t>(y) * width_ + x) * 3;
            std::copy(color.begin(), color.end(),
                      pixels_.begin() + static_cast<std::ptrdiff_t>(offset));
        }
    }
    const int radius =
        std::clamp(static_cast<int>(std::lround(std::min(width_, height_) * 0.012)), 2, 36);
    // Three box passes approximate a Gaussian while keeping memory bounded.
    for (int pass = 0; pass < 3; ++pass)
        boxBlur(pixels_, width_, height_, radius, stop);
}
std::array<float, 3> LocalBlurPass::pixel(int x, int y) const {
    const int sampleX = std::clamp(x * width_ / sourceWidth_, 0, width_ - 1);
    const int sampleY = std::clamp(y * height_ / sourceHeight_, 0, height_ - 1);
    const auto offset = (static_cast<std::size_t>(sampleY) * width_ + sampleX) * 3;
    return {pixels_[offset], pixels_[offset + 1], pixels_[offset + 2]};
}
} // namespace luma
