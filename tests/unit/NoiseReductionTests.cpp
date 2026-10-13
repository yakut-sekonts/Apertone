#include "core/render/CpuRenderer.h"
#include "core/render/NoiseReduction.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <class F> void rejects(F action) {
    bool failed = false;
    try {
        action();
    } catch (const std::exception &) {
        failed = true;
    }
    require(failed, "Expected validation/cancellation failure.");
}
double Y(const std::array<float, 3> &rgb) {
    return 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2];
}
std::array<float, 3> sourcePixel(const luma::FloatImage &image, int x, int y) {
    const auto i = (static_cast<std::size_t>(y) * image.width + x) * 3;
    return {image.rgb[i], image.rgb[i + 1], image.rgb[i + 2]};
}
luma::FloatImage fixture(bool chroma = false) {
    luma::FloatImage image(64, 40);
    for (int y = 0; y < image.height; ++y)
        for (int x = 0; x < image.width; ++x) {
            const float n = static_cast<float>(((x * 37 + y * 61) % 17 - 8) * 0.0015);
            const float base = x < 32 ? 0.25F : 0.75F;
            const float u = chroma ? n : 0, v = chroma ? -n : 0;
            const float luminance = base + (chroma ? 0 : n);
            const auto i = (static_cast<std::size_t>(y) * image.width + x) * 3;
            image.rgb[i] = luminance + u;
            image.rgb[i + 1] = luminance - (0.2126F * u + 0.0722F * v) / 0.7152F;
            image.rgb[i + 2] = luminance + v;
        }
    return image;
}
luma::FloatImage processed(const luma::FloatImage &input, luma::EditDocument edits,
                           double scale = 1,
                           luma::DetailDiagnostic mode = luma::DetailDiagnostic::none) {
    const luma::DevelopPipeline pipeline(edits);
    const luma::NoiseReductionPass pass(input, pipeline, edits, {}, scale, mode);
    luma::FloatImage result(input.width, input.height);
    for (int y = 0; y < input.height; ++y)
        for (int x = 0; x < input.width; ++x) {
            const auto rgb = pass.pixel(pipeline.pixel(sourcePixel(input, x, y)), x, y);
            const auto i = (static_cast<std::size_t>(y) * input.width + x) * 3;
            std::copy(rgb.begin(), rgb.end(), result.rgb.begin() + static_cast<std::ptrdiff_t>(i));
        }
    return result;
}
double error(const luma::FloatImage &image, bool chroma = false) {
    double total = 0;
    for (int y = 5; y < 35; ++y)
        for (int x = 5; x < 26; ++x) {
            const auto rgb = sourcePixel(image, x, y);
            const auto value = chroma ? rgb[0] - Y(rgb) : Y(rgb) - 0.25;
            total += value * value;
        }
    return total;
}
void processing() {
    const auto input = fixture();
    luma::EditDocument edits;
    require(!luma::NoiseReductionPass(input, luma::DevelopPipeline(edits), edits).active(),
            "Neutral noise allocated buffers.");
    const auto baseline = luma::renderCPU(input, edits).rgba;
    edits.noiseLuminance = 100;
    const auto denoised = processed(input, edits);
    require(error(denoised) < error(input) * 0.2, "Luminance noise variance was not reduced.");
    require(Y(sourcePixel(denoised, 32, 20)) - Y(sourcePixel(denoised, 31, 20)) > 0.47,
            "Noise filter blurred a strong edge.");
    auto halfAmount = edits;
    halfAmount.noiseLuminance = 50;
    const auto halfFiltered = processed(input, halfAmount);
    for (std::size_t i = 0; i < input.rgb.size(); ++i)
        require(std::abs(halfFiltered.rgb[i] - (input.rgb[i] + denoised.rgb[i]) * 0.5F) < 1e-6,
                "Luminance Amount blend is not linear.");
    edits.noiseLuminanceDetail = 100;
    require(error(processed(input, edits)) > error(denoised) * 1.3,
            "Luminance Detail did not retain texture.");
    edits.noiseLuminanceDetail = 50;
    edits.noiseLuminanceContrast = 100;
    require(error(processed(input, edits)) > error(denoised) * 1.3,
            "Luminance Contrast did not retain residual contrast.");
    edits.setEnabled(luma::Correction::luminanceNoise, false);
    require(luma::renderCPU(input, edits).rgba == baseline, "Disabled luminance changed pixels.");
    const auto colored = fixture(true);
    edits = {};
    edits.noiseColor = 100;
    const auto colorFiltered = processed(colored, edits);
    require(error(colorFiltered, true) < error(colored, true) * 0.25,
            "Color noise variance was not reduced.");
    for (int y = 0; y < colored.height; ++y)
        for (int x = 0; x < colored.width; ++x)
            require(std::abs(Y(sourcePixel(colored, x, y)) - Y(sourcePixel(colorFiltered, x, y))) <
                        1e-6,
                    "Color noise changed luminance.");
    luma::FloatImage colorEdge(32, 20);
    for (int y = 0; y < colorEdge.height; ++y)
        for (int x = 0; x < colorEdge.width; ++x) {
            const float u = x < 16 ? -0.3F : 0.3F;
            const auto i = (static_cast<std::size_t>(y) * colorEdge.width + x) * 3;
            colorEdge.rgb[i] = 0.5F + u;
            colorEdge.rgb[i + 1] = 0.5F - 0.2126F * u / 0.7152F;
            colorEdge.rgb[i + 2] = 0.5F;
        }
    const auto preservedColor = processed(colorEdge, edits);
    require(sourcePixel(preservedColor, 16, 10)[0] - sourcePixel(preservedColor, 15, 10)[0] > 0.5F,
            "Color denoise blurred a strong equal-luminance chroma edge.");
    auto colorOff = edits;
    colorOff.setEnabled(luma::Correction::colorNoise, false);
    require(luma::renderCPU(colored, colorOff).rgba == luma::renderCPU(colored, {}).rgba,
            "Disabled Color changed pixels.");
    edits.noiseColorDetail = 100;
    require(error(processed(colored, edits), true) > error(colorFiltered, true) * 1.5,
            "Color Detail did not protect chroma detail.");
    edits.noiseColorDetail = 50;
    edits.noiseColorSmoothness = 0;
    const auto narrow = processed(colored, edits);
    edits.noiseColorSmoothness = 100;
    require(error(processed(colored, edits), true) < error(narrow, true),
            "Color Smoothness did not expand smoothing.");
    const auto scaled = processed(colored, edits, 1.0 / 6);
    require(scaled.rgb == narrow.rgb, "Preview spatial radius scale mismatch.");
    edits = {};
    edits.noiseLuminance = 100;
    const auto luminanceFiltered = processed(colored, edits);
    for (int y = 0; y < colored.height; ++y)
        for (int x = 0; x < colored.width; ++x) {
            const auto a = sourcePixel(colored, x, y), b = sourcePixel(luminanceFiltered, x, y);
            require(std::abs((a[0] - a[1]) - (b[0] - b[1])) < 1e-6,
                    "Luminance noise changed channel differences.");
        }
    for (const auto constant : {std::array{-0.2F, -0.1F, -0.3F}, std::array{3.F, 0.5F, 1.2F}}) {
        luma::FloatImage flat(1, 12);
        for (std::size_t i = 0; i < flat.rgb.size(); ++i)
            flat.rgb[i] = constant[i % 3];
        edits.noiseColor = 100;
        const auto output = processed(flat, edits);
        for (std::size_t i = 0; i < output.rgb.size(); ++i)
            require(std::abs(output.rgb[i] - flat.rgb[i]) < 1e-6,
                    "Constant signed/HDR color changed.");
    }
    const auto gray = processed(colored, edits, 1, luma::DetailDiagnostic::noiseLuminance);
    for (std::size_t i = 0; i < gray.rgb.size(); i += 3)
        require(gray.rgb[i] == gray.rgb[i + 1] && gray.rgb[i + 1] == gray.rgb[i + 2],
                "Luminance diagnostic has color.");
    const auto chromaView = processed(colored, edits, 1, luma::DetailDiagnostic::noiseColor);
    for (int y = 0; y < colored.height; ++y)
        for (int x = 0; x < colored.width; ++x)
            require(std::abs(Y(sourcePixel(chromaView, x, y)) - 0.5) < 1e-6,
                    "Chroma diagnostic has varying luminance.");
    // Composition: exported/rendered sharpening must consume reduced noise.
    edits.noiseLuminanceContrast = 0;
    edits.sharpenAmount = 100;
    edits.sharpenDetail = 100;
    const auto clean = processed(input, edits);
    luma::EditDocument sharpOnly;
    sharpOnly.sharpenAmount = 100;
    sharpOnly.sharpenDetail = 100;
    for (int orientation = 0; orientation < 8; ++orientation)
        require(luma::renderCPU(input, edits, orientation).rgba ==
                    luma::renderCPU(clean, sharpOnly, orientation).rgba,
                "Noise/sharpening order or orientation mismatch.");
    const auto diagnostic =
        luma::renderCPU(input, edits, 0, {}, {}, {}, luma::DetailDiagnostic::noiseLuminance).rgba;
    edits.sharpenAmount = 0;
    require(
        luma::renderCPU(input, edits, 0, {}, {}, {}, luma::DetailDiagnostic::noiseLuminance).rgba ==
            diagnostic,
        "Sharpening entered noise diagnostic.");
}
void validation() {
    const auto input = fixture();
    luma::EditDocument edits;
    edits.noiseLuminance = 100;
    edits.noiseColor = 100;
    const luma::DevelopPipeline pipeline(edits);
    for (const auto name : {"noise.luminance", "noise.luminanceDetail", "noise.luminanceContrast",
                            "noise.color", "noise.colorDetail", "noise.colorSmoothness"})
        for (double value : {-1., 101., std::numeric_limits<double>::quiet_NaN(),
                             std::numeric_limits<double>::infinity()})
            rejects([&] {
                auto invalid = edits;
                invalid.setParameter(name, value);
            });
    for (double scale : {0., -1., 1.01, std::numeric_limits<double>::quiet_NaN()})
        rejects([&] { luma::NoiseReductionPass pass(input, pipeline, edits, {}, scale); });
    auto bad = input;
    bad.rgb[5] = std::numeric_limits<float>::quiet_NaN();
    rejects([&] { luma::NoiseReductionPass pass(bad, pipeline, edits); });
    for (double phase : {0., 0.05, 0.2, 0.5, 0.8, 1.}) {
        luma::CancellationSource source;
        if (phase == 0)
            source.cancel();
        rejects([&] {
            luma::NoiseReductionPass pass(input, pipeline, edits, source.token(), 1,
                                          luma::DetailDiagnostic::none, [&](double value) {
                                              if (value >= phase)
                                                  source.cancel();
                                          });
        });
    }
    luma::NoiseReductionPass pass(input, pipeline, edits);
    rejects([&] { pass.pixel({}, input.width, 0); });
}
} // namespace
int main() {
    try {
        processing();
        validation();
        std::cout << "Noise variance, edge/component preservation, six controls, diagnostics, "
                     "composition, scale and cancellation passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
