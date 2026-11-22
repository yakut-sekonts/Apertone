#include "core/merge/MergeAlignment.h"
#include "core/image/Bayer.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace luma {
namespace {
// Registration can return a finite but incorrect homography for a translated
// panorama. Compare image content before accepting it. ZNCC tolerates exposure
// changes; a bounded translation search also supplies a large-motion candidate
// when Vision's local homography optimizer stays near its initial position.
double correlation(ai::RgbaImage reference, ai::RgbaImage floating, const Homography &placement) {
    const auto inverse = placement.inverse();
    auto luminance = [](ai::RgbaImage input, double u, double v) {
        const double x = std::clamp(u * input.width - .5, 0.0, input.width - 1.0);
        const double y = std::clamp(v * input.height - .5, 0.0, input.height - 1.0);
        const int ix = static_cast<int>(x), iy = static_cast<int>(y);
        double result = 0;
        for (int dy = 0; dy < 2; ++dy)
            for (int dx = 0; dx < 2; ++dx) {
                const auto offset =
                    (static_cast<std::size_t>(std::min(iy + dy, input.height - 1)) * input.width +
                     std::min(ix + dx, input.width - 1)) *
                    4;
                const double weight = (dx ? x - ix : 1 - x + ix) * (dy ? y - iy : 1 - y + iy);
                result +=
                    weight * (.2126 * input.pixels[offset] + .7152 * input.pixels[offset + 1] +
                              .0722 * input.pixels[offset + 2]);
            }
        return result;
    };
    double sumA = 0, sumB = 0, squareA = 0, squareB = 0, product = 0;
    int count = 0;
    constexpr int grid = 24;
    for (int y = 0; y < grid; ++y)
        for (int x = 0; x < grid; ++x) {
            const double u = (x + .5) / grid, v = (y + .5) / grid;
            const auto mapped = inverse.point(u, v);
            if (mapped[0] <= 0 || mapped[0] >= 1 || mapped[1] <= 0 || mapped[1] >= 1)
                continue;
            const double a = luminance(reference, u, v),
                         b = luminance(floating, mapped[0], mapped[1]);
            sumA += a;
            sumB += b;
            squareA += a * a;
            squareB += b * b;
            product += a * b;
            ++count;
        }
    if (count < grid * grid / 4)
        return -1;
    const double varianceA = squareA - sumA * sumA / count,
                 varianceB = squareB - sumB * sumB / count;
    if (varianceA < count || varianceB < count)
        return -1; // Less than one RGB8 step of variation cannot establish alignment.
    return (product - sumA * sumB / count) / std::sqrt(varianceA * varianceB);
}
} // namespace

Homography verifiedMergeAlignment(ai::RgbaImage reference, ai::RgbaImage floating,
                                  const Homography &native, CancellationToken stop) {
    checkCancellation(stop);
    reference.validate();
    floating.validate();
    if (reference.width != floating.width || reference.height != floating.height)
        throw std::runtime_error("Alignment requires matching upright image dimensions.");
    if (reference.width > 1024 || reference.height > 1024)
        throw std::runtime_error("Alignment reference exceeds its budget.");
    Homography best = native;
    double bestScore = correlation(reference, floating, native);
    Homography translation;
    double translationScore = -std::numeric_limits<double>::infinity();
    auto consider = [&](double x, double y) {
        if (std::abs(x) > .75 || std::abs(y) > .75)
            return;
        const Homography candidate{{1, 0, x, 0, 1, y, 0, 0, 1}};
        const double score = correlation(reference, floating, candidate);
        if (score > translationScore) {
            translation = candidate;
            translationScore = score;
        }
    };
    for (int y = -12; y <= 12; ++y) {
        checkCancellation(stop);
        for (int x = -12; x <= 12; ++x)
            consider(x / 16.0, y / 16.0);
    }
    // Refine the strongest coarse translation to subpixel resolution, retaining
    // the candidate only when measured correspondence improves.
    for (double step = 1.0 / 64; step * std::max(reference.width, reference.height) >= .25;
         step /= 4) {
        checkCancellation(stop);
        const double centerX = translation.values[2], centerY = translation.values[5];
        for (int y = -3; y <= 3; ++y)
            for (int x = -3; x <= 3; ++x)
                consider(centerX + step * x, centerY + step * y);
    }
    if (translationScore > bestScore) {
        best = translation;
        bestScore = translationScore;
    }
    // Fit residual scale, yaw and shear around the overlap established by
    // translation. Restrict the corrections so texture cannot pull the
    // registration towards a degenerate transform with excessive cropping.
    if (translationScore > .45 && bestScore < .995) {
        const Homography center{{1, 0, .5, 0, 1, .5, 0, 0, 1}};
        const auto uncenter = center.inverse();
        const auto baseline = bestScore > .85 ? best : translation;
        std::array<double, 8> parameters{};
        constexpr std::array limit{.10, .13, .08, .13, .10, .08, .16, .16};
        constexpr std::array initialStep{.032, .046, .022, .046, .032, .022, .042, .042};
        double objective = correlation(reference, floating, baseline);
        for (double step = 1; step >= .0625; step /= 2) {
            for (int iteration = 0; iteration < 3; ++iteration) {
                checkCancellation(stop);
                bool improved = false;
                for (std::size_t axis = 0; axis < parameters.size(); ++axis) {
                    auto trial = parameters;
                    for (int direction : {-1, 1}) {
                        trial[axis] = parameters[axis] + direction * step * initialStep[axis];
                        if (std::abs(trial[axis]) > limit[axis])
                            continue;
                        const Homography correction{{1 + trial[0], trial[1], trial[2], trial[3],
                                                     1 + trial[4], trial[5], trial[6], trial[7],
                                                     1}};
                        const auto candidate = center * correction * uncenter * baseline;
                        const auto score = correlation(reference, floating, candidate);
                        double penalty = 0;
                        for (std::size_t i = 0; i < trial.size(); ++i)
                            penalty += .001 * std::pow(trial[i] / limit[i], 2);
                        if (score - penalty > objective + 1e-5) {
                            objective = score - penalty;
                            parameters = trial;
                            improved = true;
                            if (score > bestScore + .001) {
                                best = candidate;
                                bestScore = score;
                            }
                        }
                    }
                }
                if (!improved)
                    break;
            }
        }
    }
    if (!std::isfinite(bestScore) || bestScore < .85)
        throw std::runtime_error(
            "Could not align these frames. Use images with sufficient overlap.");
    return best;
}
} // namespace luma
