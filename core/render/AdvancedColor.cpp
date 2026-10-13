#include "core/render/AdvancedColor.h"
#include <algorithm>
#include <cmath>

namespace luma {
double encodeSRGB(double linear) {
    const double x = std::abs(linear);
    return std::copysign(x <= 0.0031308 ? 12.92 * x : 1.055 * std::pow(x, 1 / 2.4) - 0.055, linear);
}
double decodeSRGB(double encoded) {
    const double x = std::abs(encoded);
    return std::copysign(x <= 0.04045 ? x / 12.92 : std::pow((x + 0.055) / 1.055, 2.4), encoded);
}
namespace {
double endpoint(double h0, double h1, double d0, double d1) {
    const double slope = ((2 * h0 + h1) * d0 - h0 * d1) / (h0 + h1);
    if (slope * d0 <= 0)
        return 0;
    return d0 * d1 <= 0 && std::abs(slope) > 3 * std::abs(d0) ? 3 * d0 : slope;
}
double wrapHue(double hue) {
    const auto result = std::fmod(hue, 360);
    return result < 0 ? result + 360 : result;
}
} // namespace
ToneCurve::ToneCurve(const PointCurve &curve) : samples_(4097) {
    curve.validate();
    const auto &p = curve.points;
    std::vector<double> widths(p.size() - 1), secants(p.size() - 1), slopes(p.size());
    for (std::size_t i = 0; i < widths.size(); ++i) {
        widths[i] = p[i + 1].x - p[i].x;
        secants[i] = (p[i + 1].y - p[i].y) / widths[i];
    }
    // PCHIP (Fritsch–Butland): preserve each segment's shape, without overshoot.
    // https://docs.scipy.org/doc/scipy/reference/generated/scipy.interpolate.PchipInterpolator.html
    slopes.front() = slopes.back() = secants.front();
    if (p.size() > 2) {
        slopes.front() = endpoint(widths[0], widths[1], secants[0], secants[1]);
        const auto last = widths.size() - 1;
        slopes.back() = endpoint(widths[last], widths[last - 1], secants[last], secants[last - 1]);
        for (std::size_t i = 1; i + 1 < p.size(); ++i) {
            if (secants[i - 1] * secants[i] > 0) {
                const double w1 = 2 * widths[i] + widths[i - 1];
                const double w2 = widths[i] + 2 * widths[i - 1];
                slopes[i] = (w1 + w2) / (w1 / secants[i - 1] + w2 / secants[i]);
            }
        }
    }
    startSlope_ = slopes.front();
    endSlope_ = slopes.back();
    std::size_t segment = 0;
    for (std::size_t i = 0; i < samples_.size(); ++i) {
        const double x = static_cast<double>(i) / (samples_.size() - 1);
        while (segment + 2 < p.size() && x > p[segment + 1].x)
            ++segment;
        const double t = (x - p[segment].x) / widths[segment], t2 = t * t, t3 = t2 * t;
        samples_[i] = (2 * t3 - 3 * t2 + 1) * p[segment].y +
                      (t3 - 2 * t2 + t) * widths[segment] * slopes[segment] +
                      (-2 * t3 + 3 * t2) * p[segment + 1].y +
                      (t3 - t2) * widths[segment] * slopes[segment + 1];
    }
}
double ToneCurve::value(double x) const {
    if (!std::isfinite(x))
        throw std::runtime_error("Non-finite curve input.");
    if (x <= 0)
        return samples_.front() + x * startSlope_;
    if (x >= 1)
        return samples_.back() + (x - 1) * endSlope_;
    const double position = x * (samples_.size() - 1);
    const auto index = static_cast<std::size_t>(position);
    return std::lerp(samples_[index], samples_[index + 1], position - index);
}
HSLColor toHSL(const std::array<double, 3> &rgb) {
    const auto [low, high] = std::minmax_element(rgb.begin(), rgb.end());
    const double delta = *high - *low, l = (*high + *low) / 2;
    if (delta <= 1e-12)
        return {0, 0, l};
    const double s = delta / std::max(1e-12, 1 - std::abs(2 * l - 1));
    double hue = *high == rgb[0]   ? (rgb[1] - rgb[2]) / delta
                 : *high == rgb[1] ? (rgb[2] - rgb[0]) / delta + 2
                                   : (rgb[0] - rgb[1]) / delta + 4;
    return {wrapHue(hue * 60), s, l};
}
std::array<double, 3> fromHSL(HSLColor color) {
    const double h = wrapHue(color.hue) / 60;
    const double c = (1 - std::abs(2 * color.luminance - 1)) * color.saturation;
    const double x = c * (1 - std::abs(std::fmod(h, 2) - 1));
    const double m = color.luminance - c / 2;
    const std::array<std::array<double, 3>, 6> sectors{
        {{c, x, 0}, {x, c, 0}, {0, c, x}, {0, x, c}, {x, 0, c}, {c, 0, x}}};
    auto rgb = sectors[static_cast<std::size_t>(h)];
    for (auto &value : rgb)
        value += m;
    return rgb;
}
std::array<double, 8> hueWeights(double hue) {
    if (!std::isfinite(hue))
        throw std::runtime_error("Non-finite hue.");
    constexpr std::array<double, 9> centers{0, 30, 60, 120, 180, 240, 270, 300, 360};
    hue = wrapHue(hue);
    std::size_t band = 0;
    while (band < 7 && hue >= centers[band + 1])
        ++band;
    const double fraction = (hue - centers[band]) / (centers[band + 1] - centers[band]);
    std::array<double, 8> weights{};
    weights[band] = 1 - fraction;
    weights[(band + 1) % 8] = fraction;
    return weights;
}
double parametricCurve(double x, const EditDocument &edits) {
    const std::array<double, 4> values{edits.curveShadows, edits.curveDarks, edits.curveLights,
                                       edits.curveHighlights};
    if (x <= 0)
        return x * std::exp2(values.front() * 0.005);
    if (x >= 1)
        return 1 + (x - 1) * std::exp2(-values.back() * 0.005);
    const double position = std::clamp((x - 0.125) * 4, 0.0, 3.0);
    const auto index = std::min(static_cast<std::size_t>(position), std::size_t{2});
    const double gain =
        std::exp2(std::lerp(values[index], values[index + 1], position - index) * 0.005);
    return x * gain / (1 - x + x * gain);
}
std::array<double, 3> mixHSL(std::array<double, 3> rgb, const std::array<HSLBand, 8> &bands) {
    const double offset = std::min(0.0, *std::min_element(rgb.begin(), rgb.end()));
    const double scale = std::max(1.0, *std::max_element(rgb.begin(), rgb.end()) - offset);
    for (auto &value : rgb)
        value = (value - offset) / scale;
    auto hsl = toHSL(rgb);
    const auto weights = hueWeights(hsl.hue);
    HSLBand adjustment;
    for (std::size_t i = 0; i < bands.size(); ++i) {
        adjustment.hue += bands[i].hue * weights[i];
        adjustment.saturation += bands[i].saturation * weights[i];
        adjustment.luminance += bands[i].luminance * weights[i];
    }
    const double chromaWeight = std::min(1.0, hsl.saturation / 0.1);
    hsl.hue = wrapHue(hsl.hue + adjustment.hue * chromaWeight * 0.3);
    hsl.saturation =
        std::clamp(hsl.saturation * (1 + adjustment.saturation * chromaWeight / 100), 0.0, 1.0);
    hsl.luminance += adjustment.luminance * chromaWeight * 0.005 *
                     (adjustment.luminance >= 0 ? 1 - hsl.luminance : hsl.luminance);
    rgb = fromHSL(hsl);
    for (auto &value : rgb)
        value = value * scale + offset;
    return rgb;
}
} // namespace luma
