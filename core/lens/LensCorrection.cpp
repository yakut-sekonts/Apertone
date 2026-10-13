#include "core/lens/LensCorrection.h"
#include "core/image/Bayer.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace luma {
namespace {
using Polynomial = std::array<double, 6>;
constexpr Polynomial identity{1, 0, 0, 0, 0, 0};
double evaluate(const Polynomial &polynomial, double radius) {
    double value = 0;
    for (auto term = polynomial.rbegin(); term != polynomial.rend(); ++term)
        value = value * radius + *term;
    return value;
}
Polynomial derivative(const Polynomial &polynomial) {
    Polynomial result{};
    for (std::size_t i = 1; i < polynomial.size(); ++i)
        result[i - 1] = polynomial[i] * static_cast<double>(i);
    return result;
}
std::array<double, 2> range(const Polynomial &polynomial, double radius) {
    if (!std::isfinite(radius) || radius < 0 || radius > 100)
        throw std::runtime_error("Invalid lens calibration sensor scale.");
    // A derivative bound provides a conservative envelope between samples,
    // unlike a sampled boundary estimate which can miss a polynomial extremum.
    auto slope = derivative(polynomial);
    for (auto &value : slope)
        value = std::abs(value);
    const double margin = evaluate(slope, radius) * radius / 1024;
    double minimum = evaluate(polynomial, 0), maximum = minimum;
    for (int i = 1; i <= 512; ++i) {
        const auto value = evaluate(polynomial, radius * i / 512);
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
    }
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || !std::isfinite(margin))
        throw std::runtime_error("Non-finite lens calibration.");
    return {minimum - margin, maximum + margin};
}
double safeMaximum(const Polynomial &polynomial, double radius) {
    const auto limits = range(polynomial, radius);
    // The radial lookup must stay positive and monotonic throughout the sensor.
    auto radialDerivative = polynomial;
    for (std::size_t i = 0; i < radialDerivative.size(); ++i)
        radialDerivative[i] *= static_cast<double>(i + 1);
    if (limits[0] <= 0 || range(radialDerivative, radius)[0] <= 0 || limits[1] > 8)
        throw std::runtime_error("Lens calibration is unsafe on this sensor.");
    return limits[1];
}
Polynomial distortionPolynomial(const LensCalibration &calibration) {
    const auto &t = calibration.terms;
    if (calibration.model == "poly3")
        return {1 - t[0], 0, t[0], 0, 0, 0};
    if (calibration.model == "poly5")
        return {1, 0, t[0], 0, t[1], 0};
    if (calibration.model == "ptlens")
        return {1 - t[0] - t[1] - t[2], t[2], t[1], t[0], 0, 0};
    throw std::runtime_error("Unsupported distortion model.");
}
} // namespace
LensMapping::LensMapping(int width, int height, const ResolvedLens &resolved,
                         const LensSettings &settings) {
    settings.validate();
    if (width < 1 || height < 1)
        throw std::runtime_error("Invalid lens image dimensions.");
    if (!resolved.camera || !resolved.profile)
        return;
    const auto &profile = *resolved.profile;
    if (!std::isfinite(profile.cropFactor) || !std::isfinite(profile.aspect) ||
        !std::isfinite(resolved.camera->cropFactor) || profile.cropFactor <= 0 ||
        profile.aspect < 1 || resolved.camera->cropFactor <= 0)
        throw std::runtime_error("Invalid lens sensor calibration.");
    // Singular dimensions are left unchanged rather than fabricating a sensor.
    if (width == 1 || height == 1)
        return;
    aspect_ = std::hypot(profile.aspect, 1);
    norm_ = 2 / std::hypot(width - 1, height - 1) * profile.cropFactor /
            resolved.camera->cropFactor * aspect_;
    centerX_ = (width - 1) * 0.5 + profile.centerX / norm_;
    centerY_ = (height - 1) * 0.5 + profile.centerY / norm_;
    if (!std::isfinite(centerX_) || !std::isfinite(centerY_) || centerX_ < 0 || centerY_ < 0 ||
        centerX_ > width - 1 || centerY_ > height - 1)
        throw std::runtime_error("Lens calibration center is outside the image.");
    if (resolved.distortion && settings.distortion != 0) {
        distortion_ = distortionPolynomial(*resolved.distortion);
        for (std::size_t i = 0; i < distortion_.size(); ++i)
            distortion_[i] =
                identity[i] + (distortion_[i] - identity[i]) * settings.distortion / 100;
    }
    if (resolved.tca && settings.chromaticAberration) {
        const auto &t = resolved.tca->terms;
        if (resolved.tca->model == "linear") {
            red_[0] = t[0];
            blue_[0] = t[1];
        } else if (resolved.tca->model == "poly3") {
            red_ = {t[2], t[1], t[0], 0, 0, 0};
            blue_ = {t[5], t[4], t[3], 0, 0, 0};
        } else
            throw std::runtime_error("Unsupported chromatic aberration model.");
    }
    const double radius = std::hypot(std::max(centerX_, width - 1 - centerX_),
                                     std::max(centerY_, height - 1 - centerY_)) *
                          norm_;
    if (geometryActive()) {
        const auto maximum = safeMaximum(distortion_, radius);
        const auto channelMaximum =
            std::max(safeMaximum(red_, radius * maximum), safeMaximum(blue_, radius * maximum));
        zoom_ = std::max(1.0, maximum * std::max(1.0, channelMaximum));
    }
    if (resolved.vignetting && settings.vignetting != 0) {
        if (resolved.vignetting->model != "pa")
            throw std::runtime_error("Unsupported vignetting model.");
        const auto &t = resolved.vignetting->terms;
        // PA is a polynomial in squared diagonal-normalized radius.
        vignette_ = {1, t[0], t[1], t[2], 0, 0};
        const auto limits = range(vignette_, std::pow(radius / aspect_, 2));
        if (limits[0] < 0.05 || limits[1] > 8)
            throw std::runtime_error("Unsafe vignette calibration on this sensor.");
        vignetteAmount_ = settings.vignetting / 100;
    }
}
bool LensMapping::geometryActive() const {
    return distortion_ != identity || red_ != identity || blue_ != identity;
}
bool LensMapping::vignetteActive() const {
    return vignetteAmount_ != 0 && vignette_ != identity;
}
std::array<double, 2> LensMapping::source(double x, double y, std::size_t channel) const {
    if (channel > 2)
        throw std::runtime_error("Invalid lens channel.");
    if (!geometryActive())
        return {x, y};
    const double nx = (x - centerX_) * norm_ / zoom_, ny = (y - centerY_) * norm_ / zoom_;
    const double radius = std::hypot(nx, ny);
    const double dist = evaluate(distortion_, radius);
    const double tca = evaluate(channel == 0   ? red_
                                : channel == 2 ? blue_
                                               : identity,
                                radius * dist);
    return {centerX_ + nx * dist * tca / norm_, centerY_ + ny * dist * tca / norm_};
}
double LensMapping::gain(double x, double y) const {
    if (!vignetteActive())
        return 1;
    const double nx = (x - centerX_) * norm_ / aspect_, ny = (y - centerY_) * norm_ / aspect_;
    const double reciprocal = 1 / evaluate(vignette_, nx * nx + ny * ny);
    return vignetteAmount_ == 1 ? reciprocal : std::pow(reciprocal, vignetteAmount_);
}
LensCorrectionPass::LensCorrectionPass(const FloatImage &input, const EditDocument &edits,
                                       const PhotoMetadata &metadata, const CancellationToken &stop)
    : input_(input) {
    edits.validate();
    if (input.rgb.size() != pixelCount(input.width, input.height) * 3)
        throw std::runtime_error("Invalid lens correction image.");
    checkCancellation(stop);
    if (!edits.enabled(Correction::lens))
        return;
    const LensMapping mapping(input.width, input.height, resolveLens(metadata, edits.lens),
                              edits.lens);
    if (!mapping.geometryActive() && !mapping.vignetteActive())
        return;
    corrected_.emplace(input.width, input.height);
    for (int y = 0; y < input.height; ++y) {
        checkCancellation(stop);
        for (int x = 0; x < input.width; ++x)
            for (std::size_t channel = 0; channel < 3; ++channel) {
                const auto point = mapping.source(x, y, channel);
                const double sx = std::clamp(point[0], 0.0, static_cast<double>(input.width - 1));
                const double sy = std::clamp(point[1], 0.0, static_cast<double>(input.height - 1));
                const int x0 = static_cast<int>(std::floor(sx)),
                          y0 = static_cast<int>(std::floor(sy));
                const int x1 = std::min(x0 + 1, input.width - 1),
                          y1 = std::min(y0 + 1, input.height - 1);
                const double fx = sx - x0, fy = sy - y0;
                const auto sample = [&](int px, int py) {
                    return input.rgb[(static_cast<std::size_t>(py) * input.width + px) * 3 +
                                     channel] *
                           mapping.gain(px, py);
                };
                const double value = std::lerp(std::lerp(sample(x0, y0), sample(x1, y0), fx),
                                               std::lerp(sample(x0, y1), sample(x1, y1), fx), fy);
                const auto offset = (static_cast<std::size_t>(y) * input.width + x) * 3 + channel;
                corrected_->rgb[offset] = static_cast<float>(value);
                if (!std::isfinite(corrected_->rgb[offset]))
                    throw std::runtime_error("Non-finite optical correction pixel.");
            }
    }
}
} // namespace luma
