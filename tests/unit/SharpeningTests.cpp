#include "core/render/CpuRenderer.h"
#include "core/render/Sharpening.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void throws(F work) {
    bool failed = false;
    try {
        work();
    } catch (const std::exception &) {
        failed = true;
    }
    require(failed, "Expected sharpening failure.");
}
luma::FloatImage image(int width, int height, bool noise = false) {
    luma::FloatImage result(width, height);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const float value = noise           ? 0.35F + (x % 2 ? 0.004F : -0.004F)
                                : x < width / 2 ? 0.1F
                                                : 0.8F;
            for (std::size_t c = 0; c < 3; ++c)
                result.rgb[(static_cast<std::size_t>(y) * width + x) * 3 + c] = value;
        }
    return result;
}
std::array<float, 3> at(const luma::FloatImage &input, const luma::DevelopPipeline &pipeline,
                        const luma::SharpeningPass &pass, int x, int y) {
    const auto i = (static_cast<std::size_t>(y) * input.width + x) * 3;
    return pass.pixel(pipeline.pixel({input.rgb[i], input.rgb[i + 1], input.rgb[i + 2]}), x, y);
}
void processing() {
    const auto edge = image(64, 48);
    luma::EditDocument edits;
    const luma::DevelopPipeline neutral(edits);
    require(!luma::SharpeningPass(edge, neutral, edits).active(),
            "Zero Amount allocated spatial buffers.");
    const auto baseline = luma::renderCPU(edge, edits).rgba;
    edits.sharpenAmount = 100;
    edits.sharpenDetail = 100;
    edits.setEnabled(luma::Correction::sharpening, false);
    require(luma::renderCPU(edge, edits).rgba == baseline,
            "Disabled sharpening changed prior output.");
    edits.setEnabled(luma::Correction::sharpening, true);
    const luma::DevelopPipeline pipeline(edits);
    const luma::SharpeningPass pass(edge, pipeline, edits);
    require(at(edge, pipeline, pass, 32, 24)[0] - at(edge, pipeline, pass, 31, 24)[0] > 0.7F,
            "Sharpening did not enhance edge contrast.");
    require(std::abs(at(edge, pipeline, pass, 10, 24)[0] - 0.1F) < 1e-6, "Flat area changed.");
    auto broadEdits = edits;
    broadEdits.sharpenRadius = 3;
    const luma::SharpeningPass broad(edge, pipeline, broadEdits);
    require(at(edge, pipeline, broad, 29, 24)[0] < at(edge, pipeline, pass, 29, 24)[0],
            "Radius did not expand edge extent.");
    auto halfEdits = edits;
    halfEdits.sharpenRadius = 0.5;
    const luma::SharpeningPass half(edge, pipeline, halfEdits);
    const luma::SharpeningPass scaled(edge, pipeline, edits, {}, 0.5);
    for (int y = 0; y < edge.height; ++y)
        for (int x = 0; x < edge.width; ++x)
            require(at(edge, pipeline, half, x, y) == at(edge, pipeline, scaled, x, y),
                    "Preview radius scale changed its meaning.");
    for (const auto color : {std::array{-0.2F, -0.1F, -0.3F}, std::array{3.F, 0.5F, 1.2F}}) {
        luma::FloatImage flat(3, 2);
        for (std::size_t i = 0; i < flat.rgb.size(); ++i)
            flat.rgb[i] = color[i % 3];
        const luma::SharpeningPass stable(flat, pipeline, edits);
        const auto expected = pipeline.pixel(color);
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 3; ++x)
                require(at(flat, pipeline, stable, x, y) == expected,
                        "Constant signed/HDR color changed.");
    }
    const auto noise = image(64, 32, true);
    const luma::SharpeningPass noisy(noise, pipeline, edits);
    auto maskedEdits = edits;
    maskedEdits.sharpenMasking = 80;
    const luma::SharpeningPass masked(noise, pipeline, maskedEdits);
    auto smoothEdits = edits;
    smoothEdits.sharpenDetail = 0;
    const luma::SharpeningPass smooth(noise, pipeline, smoothEdits);
    require(std::abs(at(noise, pipeline, noisy, 30, 16)[0] - 0.35F) >
                std::abs(at(noise, pipeline, masked, 30, 16)[0] - 0.35F),
            "Masking amplified flat noise.");
    require(std::abs(at(noise, pipeline, smooth, 30, 16)[0] - noise.rgb[(16 * 64 + 30) * 3]) < 1e-6,
            "Low Detail did not suppress low-amplitude residuals.");
    for (auto mode : {luma::DetailDiagnostic::amount, luma::DetailDiagnostic::radius,
                      luma::DetailDiagnostic::detail, luma::DetailDiagnostic::masking}) {
        const luma::SharpeningPass diagnostic(edge, pipeline, maskedEdits, {}, 1, mode);
        const auto pixel = at(edge, pipeline, diagnostic, 32, 24);
        require(pixel[0] == pixel[1] && pixel[1] == pixel[2] && pixel[0] >= 0 && pixel[0] <= 1,
                "Diagnostic is not a bounded neutral display.");
        if (mode == luma::DetailDiagnostic::masking)
            require(at(edge, pipeline, diagnostic, 10, 24)[0] == 0,
                    "Mask diagnostic did not protect a flat area.");
    }
    const auto reference = luma::renderCPU(edge, edits);
    for (int orientation = 0; orientation < 8; ++orientation) {
        const auto rotated = luma::renderCPU(edge, edits, orientation);
        for (int y = 0; y < rotated.height; ++y)
            for (int x = 0; x < rotated.width; ++x) {
                int sx = orientation & 4 ? y : x, sy = orientation & 4 ? x : y;
                if (orientation & 1)
                    sx = edge.width - 1 - sx;
                if (orientation & 2)
                    sy = edge.height - 1 - sy;
                for (int c = 0; c < 4; ++c)
                    require(rotated.rgba[(y * rotated.width + x) * 4 + c] ==
                                reference.rgba[(sy * edge.width + sx) * 4 + c],
                            "Orientation changed spatial sharpening.");
            }
    }
    const auto column = image(1, 5);
    require(luma::renderCPU(column, edits).rgba.size() == 20, "One-column image failed.");
    for (double value : {-1., 151., std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::infinity()})
        throws([&] {
            auto invalid = edits;
            invalid.setParameter("sharpen.amount", value);
        });
    throws([&] {
        auto invalid = edits;
        invalid.setParameter("sharpen.radius", 0.49);
    });
    throws([&] { luma::SharpeningPass invalid(edge, pipeline, edits, {}, 0); });
    throws([&] { luma::SharpeningPass invalid(edge, pipeline, edits, {}, 1e-300); });
    auto invalid = edge;
    invalid.rgb[2] = std::numeric_limits<float>::quiet_NaN();
    throws([&] { luma::SharpeningPass refused(invalid, pipeline, edits); });
    for (double cancellationPoint : {0.15, 0.5, 0.65}) {
        luma::CancellationSource cancellation;
        double reached = 0;
        throws([&] {
            luma::SharpeningPass cancelled(edge, pipeline, edits, cancellation.token(), 1,
                                           luma::DetailDiagnostic::none, [&](double progress) {
                                               reached = progress;
                                               if (progress >= cancellationPoint)
                                                   cancellation.cancel();
                                           });
        });
        require(reached >= cancellationPoint && reached < 1,
                "Cancellation did not stop the active spatial stage.");
    }
}
} // namespace
int main() {
    try {
        processing();
        std::cout << "Sharpening, diagnostics, masking, HDR, scale, orientation and cancellation "
                     "passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
