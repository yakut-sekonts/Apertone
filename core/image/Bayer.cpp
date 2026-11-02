#include "core/image/Bayer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace luma {
namespace {
void validateBayer(const BayerFrame &frame) {
    if (frame.width < 2 || frame.height < 2 ||
        frame.samples.size() != pixelCount(frame.width, frame.height))
        throw std::runtime_error("Invalid Bayer frame.");
    std::array<int, 3> counts{};
    for (const int color : frame.pattern) {
        if (color < 0 || color > 2)
            throw std::runtime_error("Invalid Bayer CFA.");
        ++counts[color];
    }
    if (counts != std::array<int, 3>{1, 2, 1})
        throw std::runtime_error("Unsupported CFA pattern.");
}
float sample(const BayerFrame &frame, int x, int y) {
    return frame.samples[static_cast<std::size_t>(y) * frame.width + x];
}
bool inside(const BayerFrame &frame, int x, int y) {
    return x >= 0 && y >= 0 && x < frame.width && y < frame.height;
}
struct DirectionalEstimate {
    float value = 0;
    double gradient = std::numeric_limits<double>::infinity();
    int samples = 0;
};
DirectionalEstimate greenEstimate(const BayerFrame &frame, int x, int y, int dx, int dy) {
    DirectionalEstimate estimate;
    std::array<float, 2> adjacent{};
    for (const int direction : {-1, 1}) {
        const int sx = x + direction * dx, sy = y + direction * dy;
        if (inside(frame, sx, sy) && frame.colorAt(sx, sy) == 1)
            adjacent[static_cast<std::size_t>(estimate.samples++)] = sample(frame, sx, sy);
    }
    if (!estimate.samples)
        return estimate;
    estimate.value = adjacent[0];
    // At a frame boundary the single inward sample is the only direction that
    // cannot cross an unseen edge. Do not penalize it against a two-sided
    // estimate from the orthogonal direction.
    estimate.gradient = estimate.samples == 2 ? std::abs(adjacent[0] - adjacent[1]) : 0;
    if (estimate.samples == 2)
        estimate.value = (adjacent[0] + adjacent[1]) / 2;
    const int ax = x - 2 * dx, ay = y - 2 * dy;
    const int bx = x + 2 * dx, by = y + 2 * dy;
    if (inside(frame, ax, ay) && inside(frame, bx, by) &&
        frame.colorAt(ax, ay) == frame.colorAt(x, y) &&
        frame.colorAt(bx, by) == frame.colorAt(x, y))
        estimate.gradient +=
            std::abs(2 * sample(frame, x, y) - sample(frame, ax, ay) - sample(frame, bx, by));
    return estimate;
}
} // namespace

void checkCancellation(luma::CancellationToken stop) {
    if (stop.isCancelled()) {
        throw std::runtime_error("Operation cancelled.");
    }
}

FloatImage demosaicBilinear(const BayerFrame &frame, luma::CancellationToken stop) {
    validateBayer(frame);
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

FloatImage demosaicEdgeAware(const BayerFrame &frame, luma::CancellationToken stop) {
    validateBayer(frame);
    const auto count = pixelCount(frame.width, frame.height);
    std::vector<float> green(count);
    FloatImage result(frame.width, frame.height);

    for (int y = 0; y < frame.height; ++y) {
        checkCancellation(stop);
        for (int x = 0; x < frame.width; ++x) {
            const auto index = static_cast<std::size_t>(y) * frame.width + x;
            const float center = frame.samples[index];
            if (!std::isfinite(center))
                throw std::runtime_error("Non-finite Bayer sample.");
            if (frame.colorAt(x, y) == 1) {
                green[index] = center;
                continue;
            }
            const auto horizontal = greenEstimate(frame, x, y, 1, 0);
            const auto vertical = greenEstimate(frame, x, y, 0, 1);
            if (!horizontal.samples && !vertical.samples)
                throw std::runtime_error("Bayer frame has no adjacent green samples.");
            if (horizontal.gradient < vertical.gradient)
                green[index] = horizontal.value;
            else if (vertical.gradient < horizontal.gradient)
                green[index] = vertical.value;
            else
                green[index] = (horizontal.value + vertical.value) / 2;
        }
    }

    for (int y = 0; y < frame.height; ++y) {
        checkCancellation(stop);
        for (int x = 0; x < frame.width; ++x) {
            const auto index = static_cast<std::size_t>(y) * frame.width + x;
            const int known = frame.colorAt(x, y);
            result.rgb[index * 3 + 1] = green[index];
            result.rgb[index * 3 + static_cast<std::size_t>(known)] = frame.samples[index];
            for (const int color : {0, 2}) {
                if (known == color)
                    continue;
                double difference = 0;
                int neighbors = 0;
                const auto accumulate = [&](int sx, int sy) {
                    if (!inside(frame, sx, sy) || frame.colorAt(sx, sy) != color)
                        return;
                    const auto source = static_cast<std::size_t>(sy) * frame.width + sx;
                    difference += frame.samples[source] - green[source];
                    ++neighbors;
                };
                if (known == 1) {
                    accumulate(x - 1, y);
                    accumulate(x + 1, y);
                    accumulate(x, y - 1);
                    accumulate(x, y + 1);
                } else {
                    accumulate(x - 1, y - 1);
                    accumulate(x + 1, y - 1);
                    accumulate(x - 1, y + 1);
                    accumulate(x + 1, y + 1);
                }
                if (!neighbors)
                    throw std::runtime_error("Bayer frame has no chroma neighbors.");
                result.rgb[index * 3 + static_cast<std::size_t>(color)] =
                    std::max(0.F, static_cast<float>(green[index] + difference / neighbors));
            }
        }
    }
    return result;
}

std::uint64_t recoverClippedHighlights(FloatImage &image, luma::CancellationToken stop) {
    image.validateCoverage();
    constexpr float clip = 1.F;
    constexpr float reliable = .98F;
    constexpr float minimumAnchor = .01F;
    constexpr float maximumRatio = 16.F;
    constexpr int radius = 3;
    // Recovery writes only channels that were already outside the reliable
    // range. Such samples remain excluded from neighboring ratio estimates, so
    // in-place processing is deterministic and avoids a second full image copy.
    const auto &source = image.rgb;
    std::uint64_t recoveredPixels = 0;
    std::vector<float> estimates;
    estimates.reserve((radius * 2 + 1) * (radius * 2 + 1) * 2);

    for (int y = 0; y < image.height; ++y) {
        checkCancellation(stop);
        for (int x = 0; x < image.width; ++x) {
            const auto offset = (static_cast<std::size_t>(y) * image.width + x) * 3;
            std::array<bool, 3> clipped{};
            int clippedChannels = 0;
            for (std::size_t channel = 0; channel < 3; ++channel) {
                const float value = source[offset + channel];
                if (!std::isfinite(value) || value < 0)
                    throw std::runtime_error("Invalid camera-linear highlight sample.");
                clipped[channel] = value >= clip;
                clippedChannels += clipped[channel] ? 1 : 0;
            }
            // A fully clipped pixel has no local chromatic anchor. Preserve its
            // measured sensor headroom instead of inventing a color.
            if (clippedChannels == 0 || clippedChannels == 3)
                continue;

            bool recovered = false;
            for (std::size_t target = 0; target < 3; ++target) {
                if (!clipped[target])
                    continue;
                estimates.clear();
                for (int sy = std::max(0, y - radius); sy <= std::min(image.height - 1, y + radius);
                     ++sy) {
                    for (int sx = std::max(0, x - radius);
                         sx <= std::min(image.width - 1, x + radius); ++sx) {
                        if (sx == x && sy == y)
                            continue;
                        const auto neighbor = (static_cast<std::size_t>(sy) * image.width + sx) * 3;
                        const float targetSample = source[neighbor + target];
                        if (!std::isfinite(targetSample) || targetSample < minimumAnchor ||
                            targetSample >= reliable)
                            continue;
                        for (std::size_t anchor = 0; anchor < 3; ++anchor) {
                            if (clipped[anchor])
                                continue;
                            const float currentAnchor = source[offset + anchor];
                            const float neighborAnchor = source[neighbor + anchor];
                            if (currentAnchor < minimumAnchor || currentAnchor >= reliable ||
                                !std::isfinite(neighborAnchor) || neighborAnchor < minimumAnchor ||
                                neighborAnchor >= reliable)
                                continue;
                            const float ratio = targetSample / neighborAnchor;
                            const float estimate = currentAnchor * ratio;
                            if (std::isfinite(estimate) && ratio > 0 && ratio <= maximumRatio &&
                                estimate > 0)
                                estimates.push_back(estimate);
                        }
                    }
                }
                if (estimates.size() < 3)
                    continue;
                const auto middle = estimates.begin() + estimates.size() / 2;
                std::nth_element(estimates.begin(), middle, estimates.end());
                const float estimate = std::clamp(*middle, clip, maximumRatio);
                if (std::abs(estimate - source[offset + target]) > 1e-6F) {
                    image.rgb[offset + target] = estimate;
                    recovered = true;
                }
            }
            recoveredPixels += recovered ? 1 : 0;
        }
    }
    return recoveredPixels;
}

void cameraToLinearSRGB(FloatImage &image, const std::array<float, 9> &matrix,
                        luma::CancellationToken stop) {
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
