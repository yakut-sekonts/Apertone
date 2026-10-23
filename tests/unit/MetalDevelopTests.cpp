#include "core/render/CpuRenderer.h"
#include "platform/macos/MetalDevelop.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
class ObservedAccelerator final : public luma::DevelopAccelerator {
  public:
    std::shared_ptr<luma::DevelopAccelerator> native = luma::macos::makeMetalDevelopAccelerator();
    unsigned calls = 0, completed = 0;
    std::vector<float> develop(const luma::FloatImage &input, const luma::DevelopPipeline &pipeline,
                               luma::CancellationToken stop) override {
        ++calls;
        auto result = native->develop(input, pipeline, stop);
        completed += !result.empty();
        return result;
    }
};
luma::FloatImage fixture() {
    luma::FloatImage image(257, 193);
    image.alpha.resize(luma::pixelCount(image.width, image.height));
    for (std::size_t index = 0; index < image.alpha.size(); ++index) {
        image.alpha[index] = static_cast<float>(index % 3) / 2;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const auto integer = (index * (37 + channel * 16) + channel * 997) % 4097;
            image.rgb[index * 3 + channel] = static_cast<float>(integer) / 1024 - .125F;
        }
    }
    // Include exact black, nearly neutral colors and transfer-function knees.
    const std::vector<float> boundaries{0, 1e-9F, 1e-5F, .0031308F, .18F, 1, 4, -.1F};
    for (std::size_t index = 0; index < boundaries.size(); ++index)
        for (std::size_t channel = 0; channel < 3; ++channel)
            image.rgb[index * 3 + channel] = boundaries[index];
    return image;
}
void compare(const std::string &name, const luma::FloatImage &input,
             const luma::EditDocument &edits, const luma::RawColorCalibration &calibration,
             ObservedAccelerator &accelerator) {
    const luma::DevelopPipeline pipeline(edits, calibration);
    const auto accelerated = accelerator.develop(input, pipeline, {});
    require(accelerated.size() == input.rgb.size(), "Actual Metal execution unavailable.");
    double maxError = 0, maxRatio = 0;
    std::size_t worst = 0;
    float worstReference = 0;
    for (std::size_t index = 0; index < input.rgb.size(); index += 3) {
        const auto reference =
            pipeline.pixel({input.rgb[index], input.rgb[index + 1], input.rgb[index + 2]});
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const auto actual = accelerated[index + channel];
            const double error = std::abs(double(actual) - reference[channel]);
            // Fixed before execution: FP32 linear-light agreement, scaled for HDR.
            const double limit = 2e-5 + 2e-5 * std::abs(reference[channel]);
            require(std::isfinite(actual), "Metal produced a non-finite sample.");
            maxError = std::max(maxError, error);
            if (error / limit > maxRatio) {
                maxRatio = error / limit;
                worst = index + channel;
                worstReference = reference[channel];
            }
        }
    }
    std::cout << name << " linear_max_error=" << maxError << " limit_ratio=" << maxRatio;
    if (maxRatio > 1) {
        const auto pixel = worst / 3 * 3;
        std::cout << " worst_index=" << worst << " cpu=" << worstReference
                  << " gpu=" << accelerated[worst] << " input=" << input.rgb[pixel] << ','
                  << input.rgb[pixel + 1] << ',' << input.rgb[pixel + 2];
    }
    const auto reference = luma::renderCPU(input, edits, 0, {}, {}, calibration);
    const auto calls = accelerator.completed;
    const auto actual = luma::renderCPU(input, edits, 0, {}, {}, calibration,
                                        luma::DetailDiagnostic::none, 1, {}, {}, &accelerator);
    require(accelerator.completed == calls + 1, "Render silently fell back from Metal.");
    require(actual.width == reference.width && actual.height == reference.height &&
                actual.rgba.size() == reference.rgba.size(),
            "Metal changed output geometry.");
    int maximum = 0;
    for (std::size_t index = 0; index < reference.rgba.size(); ++index) {
        const int error = std::abs(int(actual.rgba[index]) - int(reference.rgba[index]));
        if (index % 4 == 3)
            require(error == 0, "Metal changed alpha coverage.");
        maximum = std::max(maximum, error);
    }
    std::cout << " rgba8_max_error=" << maximum << '\n';
    // CPU double and Metal float may straddle one final quantization boundary.
    require(maximum <= 1, "Metal differs by more than one RGBA8 quantization step.");
    require(maxRatio <= 1, "Metal differs from the CPU linear-light reference.");
}
void fallback(const luma::FloatImage &input, const luma::EditDocument &edits,
              const luma::RawColorCalibration &calibration, ObservedAccelerator &accelerator) {
    const luma::DevelopPipeline pipeline(edits, calibration);
    require(!pipeline.gpuCompatible(), "Unqualified combined Curve/HSL pipeline entered Metal.");
    require(accelerator.develop(input, pipeline, {}).empty(),
            "Metal did not honor the unsupported pipeline contract.");
    const auto reference = luma::renderCPU(input, edits, 0, {}, {}, calibration);
    const auto calls = accelerator.calls;
    const auto actual = luma::renderCPU(input, edits, 0, {}, {}, calibration,
                                        luma::DetailDiagnostic::none, 1, {}, {}, &accelerator);
    require(accelerator.calls == calls && actual.width == reference.width &&
                actual.height == reference.height && actual.rgba == reference.rgba,
            "Combined Curve/HSL fallback did not preserve the exact CPU image.");
    std::cout << "combined-curves-hsl: explicit CPU fallback, exact RGBA8\n";
}
} // namespace
int main() {
    try {
        ObservedAccelerator accelerator;
        const auto input = fixture();
        const auto originalRGB = input.rgb, originalAlpha = input.alpha;
        if (accelerator.develop(input, luma::DevelopPipeline({}), {}).empty()) {
            std::cout << "SKIPPED: real Metal device/runtime unavailable; no equivalence claim.\n";
            return 77;
        }
        luma::EditDocument edits;
        compare("neutral", input, edits, {}, accelerator);
        edits.exposure = 1.7;
        edits.contrast = 28;
        edits.shadows = 65;
        edits.highlights = -70;
        edits.blacks = -35;
        edits.whites = 20;
        edits.saturation = 35;
        edits.vibrance = -20;
        compare("light-color", input, edits, {}, accelerator);
        edits.customWhiteBalance = true;
        edits.temperature = 3850;
        edits.tint = -23;
        luma::RawColorCalibration calibration;
        calibration.asShotGains = {1.8F, 1, 1.3F};
        calibration.cameraToSRGB = {1.12F, -.1F, -.02F, -.06F, 1.09F, -.03F, .01F, -.16F, 1.15F};
        compare("camera-white-balance", input, edits, calibration, accelerator);
        for (const double temperature : {2000., 25000.}) {
            for (const double tint : {-150., 0., 150.}) {
                luma::EditDocument boundary;
                boundary.customWhiteBalance = true;
                boundary.temperature = temperature;
                boundary.tint = tint;
                compare("white-balance-" + std::to_string(int(temperature)) + "-" +
                            std::to_string(int(tint)),
                        input, boundary, calibration, accelerator);
            }
        }
        edits.curves[0].points = {{0, 0}, {.2, .12}, {.7, .82}, {1, 1}};
        edits.curves[1].points = {{0, .02}, {.4, .35}, {1, .98}};
        edits.curveShadows = 31;
        edits.curveHighlights = -46;
        compare("curves", input, edits, calibration, accelerator);
        for (std::size_t band = 0; band < edits.hsl.size(); ++band)
            edits.hsl[band] = {double(band) * 10 - 35, double(band) * 12 - 40,
                               double(band) * 9 - 31};
        auto hslOnly = edits;
        hslOnly.setEnabled(luma::Correction::curves, false);
        compare("hsl-only", input, hslOnly, calibration, accelerator);
        fallback(input, edits, calibration, accelerator);
        edits.sharpenAmount = 75;
        edits.crop.angle = 7;
        edits.crop.x = .1;
        edits.crop.y = .1;
        edits.crop.width = .8;
        edits.crop.height = .8;
        fallback(input, edits, calibration, accelerator);
        auto detail = edits;
        detail.setEnabled(luma::Correction::curves, false);
        compare("detail-crop-alpha", input, detail, calibration, accelerator);
        edits.setEnabled(luma::Correction::curves, false);
        edits.setEnabled(luma::Correction::hsl, false);
        compare("disabled-groups", input, edits, calibration, accelerator);
        luma::CancellationSource cancelled;
        cancelled.cancel();
        bool rejected = false;
        try {
            accelerator.develop(input, luma::DevelopPipeline(edits), cancelled.token());
        } catch (const std::exception &) {
            rejected = true;
        }
        require(rejected, "Metal ignored cancellation before submission.");
        require(input.rgb == originalRGB && input.alpha == originalAlpha,
                "Metal mutated the input image.");
        std::cout << "Actual Metal/CPU equivalence: 13 cases, 2 explicit fallbacks, alpha/input "
                     "preservation, pre-cancel.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << '\n' << error.what() << '\n';
        return 1;
    }
}
