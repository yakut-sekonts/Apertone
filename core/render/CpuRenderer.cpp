#include "core/render/CpuRenderer.h"
#include "core/color/DisplayTransform.h"
#include "core/image/Bayer.h"
#include <algorithm>
#include <cmath>

namespace luma {
float applyExposure(float linearValue, double ev) {
    EditDocument{ev}.validate();
    return linearValue * static_cast<float>(std::exp2(ev));
}

FloatImage standardPreview(const FloatImage &input, int maximumDimension, std::stop_token stop) {
    if (maximumDimension < 1 || input.rgb.size() != pixelCount(input.width, input.height) * 3) {
        throw std::runtime_error("Invalid preview dimensions or input buffer.");
    }
    const double scale =
        std::min(1.0, static_cast<double>(maximumDimension) / std::max(input.width, input.height));
    const int width = std::max(1, static_cast<int>(std::round(input.width * scale)));
    const int height = std::max(1, static_cast<int>(std::round(input.height * scale)));
    FloatImage result(width, height);
    // Area averaging prevents the aliasing produced by subsampling a Bayer-derived image.
    for (int y = 0; y < height; ++y) {
        checkCancellation(stop);
        const int firstY = y * input.height / height;
        const int lastY = std::max(firstY + 1, (y + 1) * input.height / height);
        for (int x = 0; x < width; ++x) {
            const int firstX = x * input.width / width;
            const int lastX = std::max(firstX + 1, (x + 1) * input.width / width);
            const float count = static_cast<float>((lastX - firstX) * (lastY - firstY));
            const auto output = (static_cast<std::size_t>(y) * width + x) * 3;
            for (int sy = firstY; sy < lastY; ++sy) {
                for (int sx = firstX; sx < lastX; ++sx) {
                    const auto offset = (static_cast<std::size_t>(sy) * input.width + sx) * 3;
                    for (int c = 0; c < 3; ++c) {
                        result.rgb[output + c] += input.rgb[offset + c] / count;
                    }
                }
            }
        }
    }
    return result;
}

DisplayImage renderCPU(const FloatImage &input, EditDocument edits, int orientation,
                       std::span<const std::uint8_t> displayProfile, std::stop_token stop) {
    edits.validate();
    if (orientation < 0 || orientation > 7 ||
        input.rgb.size() != pixelCount(input.width, input.height) * 3) {
        throw std::runtime_error("Invalid render input or orientation.");
    }
    const int width = orientation & 4 ? input.height : input.width;
    const int height = orientation & 4 ? input.width : input.height;
    DisplayImage result{width, height, std::vector<std::uint8_t>(pixelCount(width, height) * 4)};
    const float gain = static_cast<float>(std::exp2(edits.exposure));
    DisplayTransform transform(displayProfile);
    std::vector<float> row(static_cast<std::size_t>(width) * 3);
    for (int y = 0; y < height; ++y) {
        checkCancellation(stop);
        for (int x = 0; x < width; ++x) {
            int sx = orientation & 4 ? y : x;
            int sy = orientation & 4 ? x : y;
            if (orientation & 1) {
                sx = input.width - 1 - sx;
            }
            if (orientation & 2) {
                sy = input.height - 1 - sy;
            }
            const auto inputOffset = (static_cast<std::size_t>(sy) * input.width + sx) * 3;
            for (int c = 0; c < 3; ++c) {
                row[static_cast<std::size_t>(x) * 3 + c] = input.rgb[inputOffset + c] * gain;
            }
        }
        auto destination = std::span(result.rgba)
                               .subspan(static_cast<std::size_t>(y) * width * 4,
                                        static_cast<std::size_t>(width) * 4);
        transform.convert(row, destination);
    }
    return result;
}
} // namespace luma
