#include "core/render/NoiseProfile.h"
#include "core/image/Bayer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace luma {
namespace {
std::array<double, 3> components(const FloatImage &image, int x, int y) {
    const auto offset = (static_cast<std::size_t>(y) * image.width + x) * 3;
    const double red = image.rgb[offset], green = image.rgb[offset + 1],
                 blue = image.rgb[offset + 2];
    const double luminance = .2126 * red + .7152 * green + .0722 * blue;
    return {luminance, red - luminance, blue - luminance};
}

double robustSigma(std::vector<double> residuals) {
    if (residuals.empty())
        return 0;
    const auto middle = residuals.begin() + static_cast<std::ptrdiff_t>(residuals.size() / 2);
    std::nth_element(residuals.begin(), middle, residuals.end());
    // Residual to a four-neighbour mean has sqrt(1 + 1/4) times the
    // independent sample deviation. MAD-to-sigma contributes 0.67449.
    return *middle / (.6744897501960817 * std::sqrt(1.25));
}
} // namespace

NoiseProfileRecommendation estimateNoiseProfile(const FloatImage &image,
                                                const PhotoMetadata &metadata,
                                                CancellationToken stop) {
    image.validateCoverage();
    const auto count = pixelCount(image.width, image.height);
    if (count < 9)
        throw std::runtime_error("Noise profile requires at least a 3×3 image.");
    const int stride =
        std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count) / 200000.0))));
    std::vector<double> luminanceResiduals, colorResiduals;
    luminanceResiduals.reserve(std::min<std::size_t>(count, 200000));
    colorResiduals.reserve(luminanceResiduals.capacity());
    constexpr std::array<std::array<int, 2>, 4> neighbors{{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}};
    for (int y = 1; y + 1 < image.height; y += stride) {
        checkCancellation(stop);
        for (int x = 1; x + 1 < image.width; x += stride) {
            const auto index = static_cast<std::size_t>(y) * image.width + x;
            if (!image.alpha.empty() && image.alpha[index] < .999F)
                continue;
            const auto center = components(image, x, y);
            if (!std::isfinite(center[0]) || center[0] < .02 || center[0] > 1)
                continue;
            std::array<double, 3> average{};
            double gradient = 0;
            bool covered = true;
            for (const auto &offset : neighbors) {
                const int sampleX = x + offset[0], sampleY = y + offset[1];
                const auto sampleIndex = static_cast<std::size_t>(sampleY) * image.width + sampleX;
                if (!image.alpha.empty() && image.alpha[sampleIndex] < .999F) {
                    covered = false;
                    break;
                }
                const auto sample = components(image, sampleX, sampleY);
                for (std::size_t component = 0; component < average.size(); ++component)
                    average[component] += sample[component] / neighbors.size();
                gradient = std::max(gradient, std::abs(sample[0] - center[0]));
            }
            const double flatLimit = .006 + .025 * std::sqrt(std::max(.02, center[0]));
            if (!covered || gradient > flatLimit)
                continue;
            luminanceResiduals.push_back(std::abs(center[0] - average[0]));
            colorResiduals.push_back(std::hypot(center[1] - average[1], center[2] - average[2]) /
                                     std::sqrt(2.0));
        }
    }

    NoiseProfileRecommendation result;
    result.sampleCount = luminanceResiduals.size();
    result.measurementAvailable = result.sampleCount >= 64;
    if (result.measurementAvailable) {
        result.measuredLuminance = robustSigma(std::move(luminanceResiduals));
        result.measuredColor = robustSigma(std::move(colorResiduals));
    }
    const double iso = metadata.iso && std::isfinite(*metadata.iso) && *metadata.iso > 0
                           ? std::clamp(*metadata.iso, 25.0, 204800.0)
                           : 100.0;
    const double stops = std::clamp(std::log2(iso / 100.0), 0.0, 11.0);
    const double priorLuminance = .0014 * std::exp2(.42 * stops);
    const double priorColor = .0018 * std::exp2(.36 * stops);
    const double measuredWeight = result.measurementAvailable ? .8 : 0;
    const double sigmaLuminance =
        measuredWeight * result.measuredLuminance + (1 - measuredWeight) * priorLuminance;
    const double sigmaColor =
        measuredWeight * result.measuredColor + (1 - measuredWeight) * priorColor;
    result.luminance = std::clamp((sigmaLuminance - .0015) * 1900, 0.0, 80.0);
    result.color = std::clamp((sigmaColor - .0015) * 2200, 0.0, 80.0);
    result.luminanceDetail = std::clamp(68 - .38 * result.luminance, 35.0, 68.0);
    result.luminanceContrast = std::clamp(.22 * result.luminance, 0.0, 20.0);
    result.colorDetail = std::clamp(68 - .3 * result.color, 35.0, 68.0);
    result.colorSmoothness = std::clamp(42 + .35 * result.color, 42.0, 72.0);
    return result;
}
} // namespace luma
