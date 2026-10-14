#include "core/render/CpuRenderer.h"
#include "core/color/DisplayTransform.h"
#include "core/image/Bayer.h"
#include "core/lens/LensCorrection.h"
#include "core/render/CropGeometry.h"
#include "core/render/DevelopPipeline.h"
#include "core/render/NoiseReduction.h"
#include <algorithm>
#include <cmath>

namespace luma {
float applyExposure(float linearValue, double ev) {
    EditDocument{ev}.validate();
    return linearValue * static_cast<float>(std::exp2(ev));
}

FloatImage standardPreview(const FloatImage &input, int maximumDimension,
                           luma::CancellationToken stop) {
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

DisplayImage renderCPU(const FloatImage &source, EditDocument edits, int orientation,
                       std::span<const std::uint8_t> displayProfile, luma::CancellationToken stop,
                       RawColorCalibration calibration, DetailDiagnostic diagnostic,
                       double detailScale, PhotoMetadata metadata) {
    const LensCorrectionPass optics(source, edits, metadata, stop);
    const auto &input = optics.image();
    edits.validate();
    if (orientation < 0 || orientation > 7 ||
        input.rgb.size() != pixelCount(input.width, input.height) * 3) {
        throw std::runtime_error("Invalid render input or orientation.");
    }
    const CropGeometry geometry(input.width, input.height, orientation, edits);
    const int width = geometry.width(), height = geometry.height();
    DisplayImage result{width, height, std::vector<std::uint8_t>(pixelCount(width, height) * 4)};
    const DevelopPipeline pipeline(edits, calibration);
    const NoiseReductionPass noise(input, pipeline, edits, stop, detailScale, diagnostic);
    const SharpeningPass sharpening(input, pipeline, edits, stop, detailScale, diagnostic, {},
                                    &noise);
    DisplayTransform transform(displayProfile);
    std::vector<float> row(static_cast<std::size_t>(width) * 3);
    for (int y = 0; y < height; ++y) {
        checkCancellation(stop);
        for (int x = 0; x < width; ++x) {
            const auto rgb = geometry.sample(x, y, [&](int sx, int sy) {
                const auto offset = (static_cast<std::size_t>(sy) * input.width + sx) * 3;
                const auto developed =
                    noise.active() ? noise.pixel({}, sx, sy)
                                   : pipeline.pixel({input.rgb[offset], input.rgb[offset + 1],
                                                     input.rgb[offset + 2]});
                return sharpening.pixel(developed, sx, sy);
            });
            for (std::size_t c = 0; c < 3; ++c)
                row[static_cast<std::size_t>(x) * 3 + c] = rgb[c];
        }
        auto destination = std::span(result.rgba)
                               .subspan(static_cast<std::size_t>(y) * width * 4,
                                        static_cast<std::size_t>(width) * 4);
        transform.convert(row, destination);
        if (!geometry.opaque())
            for (int x = 0; x < width; ++x)
                if (!geometry.covered(x, y))
                    destination[static_cast<std::size_t>(x) * 4 + 3] = 0;
    }
    return result;
}
} // namespace luma
