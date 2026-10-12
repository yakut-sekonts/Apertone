#include "core/image/Bayer.h"
#include <algorithm>
#include <cmath>

namespace luma {
void checkCancellation(std::stop_token stop) {
    if (stop.stop_requested()) {
        throw std::runtime_error("Operation cancelled.");
    }
}

FloatImage demosaicBilinear(const BayerFrame &frame, std::stop_token stop) {
    if (frame.width < 2 || frame.height < 2 ||
        frame.samples.size() != pixelCount(frame.width, frame.height)) {
        throw std::runtime_error("Invalid Bayer frame.");
    }
    std::array<int, 3> counts{};
    for (const int color : frame.pattern) {
        if (color < 0 || color > 2) {
            throw std::runtime_error("Invalid Bayer CFA.");
        }
        ++counts[color];
    }
    if (counts != std::array<int, 3>{1, 2, 1}) {
        throw std::runtime_error("Unsupported CFA pattern.");
    }
    FloatImage result(frame.width, frame.height);
    for (int y = 0; y < frame.height; ++y) {
        checkCancellation(stop);
        for (int x = 0; x < frame.width; ++x) {
            const auto offset = (static_cast<std::size_t>(y) * frame.width + x) * 3;
            for (int color = 0; color < 3; ++color) {
                float sum = 0;
                float weight = 0;
                if (frame.colorAt(x, y) == color) {
                    sum = frame.samples[offset / 3];
                    weight = 1;
                } else {
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            const int sx = x + dx;
                            const int sy = y + dy;
                            if (sx < 0 || sy < 0 || sx >= frame.width || sy >= frame.height ||
                                frame.colorAt(sx, sy) != color) {
                                continue;
                            }
                            const float w = (dx == 0 || dy == 0) ? 1.0F : 0.5F;
                            sum +=
                                frame.samples[static_cast<std::size_t>(sy) * frame.width + sx] * w;
                            weight += w;
                        }
                    }
                }
                result.rgb[offset + color] = weight > 0 ? sum / weight : 0;
            }
        }
    }
    return result;
}

void cameraToLinearSRGB(FloatImage &image, const std::array<float, 9> &matrix,
                        std::stop_token stop) {
    for (const float value : matrix) {
        if (!std::isfinite(value)) {
            throw std::runtime_error("Invalid camera color matrix.");
        }
    }
    for (int y = 0; y < image.height; ++y) {
        checkCancellation(stop);
        for (int x = 0; x < image.width; ++x) {
            const auto offset = (static_cast<std::size_t>(y) * image.width + x) * 3;
            const std::array<float, 3> camera = {image.rgb[offset], image.rgb[offset + 1],
                                                 image.rgb[offset + 2]};
            for (int channel = 0; channel < 3; ++channel) {
                image.rgb[offset + channel] = matrix[channel * 3] * camera[0] +
                                              matrix[channel * 3 + 1] * camera[1] +
                                              matrix[channel * 3 + 2] * camera[2];
            }
        }
    }
}
} // namespace luma
