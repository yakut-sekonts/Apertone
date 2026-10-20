#include "core/render/DevelopPipeline.h"
#include <algorithm>
#include <cmath>

namespace luma {
namespace {
std::array<double, 9> inverse(const std::array<float, 9> &m) {
    const double d = m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) +
                     m[2] * (m[3] * m[7] - m[4] * m[6]);
    if (!std::isfinite(d) || std::abs(d) < 1e-9)
        throw std::runtime_error("Camera matrix cannot support manual WB.");
    return {(m[4] * m[8] - m[5] * m[7]) / d, (m[2] * m[7] - m[1] * m[8]) / d,
            (m[1] * m[5] - m[2] * m[4]) / d, (m[5] * m[6] - m[3] * m[8]) / d,
            (m[0] * m[8] - m[2] * m[6]) / d, (m[2] * m[3] - m[0] * m[5]) / d,
            (m[3] * m[7] - m[4] * m[6]) / d, (m[1] * m[6] - m[0] * m[7]) / d,
            (m[0] * m[4] - m[1] * m[3]) / d};
}
std::array<float, 3> manualGains(double temperature, double tint,
                                 const RawColorCalibration &calibration) {
    // Kang et al. 2002 Planckian locus approximation, 2000–25000 K.
    const double t = 1000 / temperature;
    const double x = temperature <= 4000
                         ? -0.2661239 * t * t * t - 0.2343589 * t * t + 0.8776956 * t + 0.179910
                         : -3.0258469 * t * t * t + 2.1070379 * t * t + 0.2226347 * t + 0.240390;
    const double y0 =
        temperature <= 2222
            ? -1.1063814 * x * x * x - 1.34811020 * x * x + 2.18555832 * x - 0.20219683
        : temperature <= 4000
            ? -0.9549476 * x * x * x - 1.37418593 * x * x + 2.09137015 * x - 0.16748867
            : 3.0817580 * x * x * x - 5.87338670 * x * x + 3.75112997 * x - 0.37001483;
    const double y = y0 + tint * 0.0001;
    const double X = x / y, Z = (1 - x - y) / y;
    const std::array<double, 3> rgb{3.2404542 * X - 1.5371385 - 0.4985314 * Z,
                                    -0.9692660 * X + 1.8760108 + 0.0415560 * Z,
                                    0.0556434 * X - 0.2040259 + 1.0572252 * Z};
    const auto m = inverse(calibration.cameraToSRGB);
    std::array<double, 3> neutral{};
    for (std::size_t c = 0; c < 3; ++c)
        neutral[c] =
            std::max(0.01, m[c * 3] * rgb[0] + m[c * 3 + 1] * rgb[1] + m[c * 3 + 2] * rgb[2]);
    return {static_cast<float>(neutral[1] / neutral[0]), 1,
            static_cast<float>(neutral[1] / neutral[2])};
}
double amount(const EditDocument &edits, Correction correction, double value) {
    return edits.enabled(correction) ? value / 100 : 0;
}
} // namespace
DevelopPipeline::DevelopPipeline(EditDocument edits, RawColorCalibration calibration)
    : edits_(edits), calibration_(calibration), gains_(calibration.asShotGains),
      curves_{ToneCurve(edits.curves[0]), ToneCurve(edits.curves[1]), ToneCurve(edits.curves[2]),
              ToneCurve(edits.curves[3])} {
    edits.validate();
    for (auto value : calibration.cameraToSRGB)
        if (!std::isfinite(value))
            throw std::runtime_error("Invalid camera color matrix.");
    for (auto value : gains_)
        if (!std::isfinite(value) || value <= 0)
            throw std::runtime_error("Invalid As Shot WB.");
    if (edits.customWhiteBalance && edits.enabled(Correction::whiteBalance))
        gains_ = manualGains(edits.temperature, edits.tint, calibration);
    exposureGain_ =
        static_cast<float>(std::exp2(edits.enabled(Correction::exposure) ? edits.exposure : 0));
    curvesActive_ = edits.enabled(Correction::curves) &&
                    (edits.curveShadows != 0 || edits.curveDarks != 0 || edits.curveLights != 0 ||
                     edits.curveHighlights != 0 ||
                     std::any_of(edits.curves.begin(), edits.curves.end(),
                                 [](const PointCurve &curve) { return curve != PointCurve{}; }));
    hslActive_ = edits.enabled(Correction::hsl) &&
                 std::any_of(edits.hsl.begin(), edits.hsl.end(),
                             [](const HSLBand &band) { return band != HSLBand{}; });
}
std::optional<std::array<float, 20>> DevelopPipeline::basicGpuParameters() const {
    if (curvesActive_ || hslActive_)
        return std::nullopt;
    std::array<float, 20> parameters{};
    std::copy(calibration_.cameraToSRGB.begin(), calibration_.cameraToSRGB.end(),
              parameters.begin());
    std::copy(gains_.begin(), gains_.end(), parameters.begin() + 9);
    parameters[12] = exposureGain_;
    parameters[13] = static_cast<float>(amount(edits_, Correction::contrast, edits_.contrast));
    parameters[14] = static_cast<float>(amount(edits_, Correction::shadows, edits_.shadows));
    parameters[15] = static_cast<float>(amount(edits_, Correction::highlights, edits_.highlights));
    parameters[16] = static_cast<float>(amount(edits_, Correction::blacks, edits_.blacks));
    parameters[17] = static_cast<float>(amount(edits_, Correction::whites, edits_.whites));
    parameters[18] = static_cast<float>(amount(edits_, Correction::saturation, edits_.saturation));
    parameters[19] = static_cast<float>(amount(edits_, Correction::vibrance, edits_.vibrance));
    return parameters;
}
std::array<float, 3> DevelopPipeline::pixel(std::array<float, 3> camera) const {
    for (std::size_t c = 0; c < 3; ++c) {
        if (!std::isfinite(camera[c]))
            throw std::runtime_error("Non-finite input pixel.");
        camera[c] *= gains_[c];
    }
    std::array<float, 3> rgb{};
    const auto &m = calibration_.cameraToSRGB;
    for (std::size_t c = 0; c < 3; ++c)
        rgb[c] = (m[c * 3] * camera[0] + m[c * 3 + 1] * camera[1] + m[c * 3 + 2] * camera[2]) *
                 exposureGain_;
    const double luminance = 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2];
    if (luminance > 1e-8) {
        // Smooth log-domain luminance remapping preserves chromatic ratios and HDR.
        const double l = std::log2(luminance / 0.18);
        const double shadowWeight = 1 / (1 + std::exp2(l / 2));
        const double highlightWeight = 1 - shadowWeight;
        const double blackWeight = shadowWeight * shadowWeight;
        const double whiteWeight = highlightWeight * highlightWeight;
        const double contrast = amount(edits_, Correction::contrast, edits_.contrast);
        const double delta =
            contrast * 0.5 * std::tanh(l / 2) +
            amount(edits_, Correction::shadows, edits_.shadows) * shadowWeight * 2 +
            amount(edits_, Correction::highlights, edits_.highlights) * highlightWeight * 2 +
            amount(edits_, Correction::blacks, edits_.blacks) * blackWeight +
            amount(edits_, Correction::whites, edits_.whites) * whiteWeight;
        const auto gain = static_cast<float>(std::exp2(delta));
        for (auto &value : rgb)
            value *= gain;
    }
    const double gray = 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2];
    const auto [minimum, maximum] = std::minmax_element(rgb.begin(), rgb.end());
    const double chroma =
        std::clamp((*maximum - *minimum) / std::max(1e-6F, std::abs(*maximum)), 0.0F, 1.0F);
    const double saturation = amount(edits_, Correction::saturation, edits_.saturation);
    const double vibrance = amount(edits_, Correction::vibrance, edits_.vibrance);
    const double gain = std::max(0.0, (1 + saturation) * (1 + vibrance * (1 - chroma)));
    for (auto &value : rgb)
        value = static_cast<float>(gray + (value - gray) * gain);
    if (curvesActive_ || hslActive_) {
        std::array<double, 3> encoded{};
        for (std::size_t c = 0; c < 3; ++c) {
            encoded[c] = encodeSRGB(rgb[c]);
            if (curvesActive_)
                encoded[c] =
                    curves_[c + 1].value(curves_[0].value(parametricCurve(encoded[c], edits_)));
        }
        if (hslActive_)
            encoded = mixHSL(encoded, edits_.hsl);
        for (std::size_t c = 0; c < 3; ++c)
            rgb[c] = static_cast<float>(decodeSRGB(encoded[c]));
    }
    return rgb;
}
} // namespace luma
