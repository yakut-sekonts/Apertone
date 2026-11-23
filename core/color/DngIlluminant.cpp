#include "core/color/DngIlluminant.h"
// This product includes DNG technology under license by Adobe.
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace luma {
namespace {
struct IsotemperatureLine {
    double reciprocalKelvin, u, v, slope;
};

// Robertson isotemperature lines tabulated in Wyszecki & Stiles, Color Science,
// 2nd ed., Table 1(3.11). This is the table used by the Adobe DNG SDK.
constexpr IsotemperatureLine locus[]{
    {0, .18006, .26352, -.24341},   {10, .18066, .26589, -.25479},  {20, .18133, .26846, -.26876},
    {30, .18208, .27119, -.28539},  {40, .18293, .27407, -.30470},  {50, .18388, .27709, -.32675},
    {60, .18494, .28021, -.35156},  {70, .18611, .28342, -.37915},  {80, .18740, .28668, -.40955},
    {90, .18880, .28997, -.44278},  {100, .19032, .29326, -.47888}, {125, .19462, .30141, -.58204},
    {150, .19962, .30921, -.70471}, {175, .20525, .31647, -.84901}, {200, .21142, .32312, -1.0182},
    {225, .21807, .32909, -1.2168}, {250, .22511, .33439, -1.4512}, {275, .23247, .33904, -1.7298},
    {300, .24010, .34308, -2.0637}, {325, .24702, .34655, -2.4681}, {350, .25591, .34951, -2.9641},
    {375, .26400, .35200, -3.5814}, {400, .27218, .35407, -4.3633}, {425, .28039, .35577, -5.3762},
    {450, .28863, .35714, -6.7262}, {475, .29685, .35823, -8.5955}, {500, .30505, .35907, -11.324},
    {525, .31320, .35968, -15.628}, {550, .32129, .36011, -23.325}, {575, .32931, .36038, -40.770},
    {600, .33724, .36051, -116.45},
};
constexpr double tintScale = -3000;

void require(bool condition) {
    if (!condition)
        throw std::runtime_error("Invalid DNG calibration illuminant.");
}

DngChromaticity normalize(double u, double v) {
    const auto length = std::hypot(u, v);
    return {u / length, v / length};
}

DngChromaticity direction(const IsotemperatureLine &line) {
    return normalize(1, line.slope);
}

void validate(DngChromaticity white) {
    const auto [x, y] = white;
    require(std::isfinite(x) && std::isfinite(y) && x > 0 && y > 0 && x + y < 1);
}

constexpr double cieObserver[][3]{
#include "core/color/Cie1931Observer.inc"
};
static_assert(std::size(cieObserver) == 830 - 360 + 1);
} // namespace

DngChromaticity dngStandardIlluminant(unsigned lightSource) {
    switch (lightSource) {
    case 3:
    case 17:
        return {.4476, .4074}; // Standard A
    case 18:
        return {.348483, .351747}; // Standard B
    case 19:
        return {.310061, .316150}; // Standard C
    case 23:
        return {.3457, .3585}; // D50
    case 1:
    case 4:
    case 9:
    case 20:
        return {.3324, .3474}; // D55
    case 10:
    case 21:
        return {.3127, .3290}; // D65
    case 11:
    case 22:
        return {.2990, .3149}; // D75
    case 12:
        return {.31310, .33727}; // F1
    case 2:
        return {.37208, .37529}; // F2
    case 15:
        return {.40910, .39430}; // F3
    case 16:
        return {.44018, .40329}; // F4
    case 13:
        return {.34588, .35875}; // F8
    case 14:
        return {.37417, .37281}; // F9
    case 24:
        return dngWhiteChromaticity(3200, 0);
    default:
        throw std::runtime_error("Unsupported DNG calibration illuminant.");
    }
}

DngChromaticity dngCustomIlluminant(std::span<const std::uint8_t> payload, bool littleEndian) {
    const auto integer = [&](std::size_t position, std::size_t length) {
        require(position <= payload.size() && length <= payload.size() - position);
        std::uint32_t result = 0;
        for (std::size_t i = 0; i < length; ++i)
            result = (result << 8) | payload[position + (littleEndian ? length - 1 - i : i)];
        return result;
    };
    const auto rational = [&](std::size_t position) {
        const auto denominator = integer(position + 4, 4);
        require(denominator != 0);
        return double(integer(position, 4)) / denominator;
    };
    const auto type = integer(0, 2);
    if (type == 0) {
        const DngChromaticity white{rational(2), rational(10)};
        validate(white);
        return white;
    }
    require(type == 1);
    const auto samples = integer(2, 4);
    require(samples >= 2 && payload.size() >= 22 && samples <= (payload.size() - 22) / 8);
    const auto start = rational(6), spacing = rational(14);
    require(start > 0 && spacing > 0);
    for (std::size_t i = 0; i < samples; ++i)
        (void)rational(22 + 8 * i);
    std::array<double, 3> response{}, observer{};
    for (std::size_t i = 0; i < std::size(cieObserver); ++i) {
        const auto position = std::clamp((360.0 + i - start) / spacing, 0.0, double(samples - 1));
        const auto lower = std::min(static_cast<std::size_t>(position), std::size_t(samples - 2));
        const auto power =
            std::lerp(rational(22 + 8 * lower), rational(30 + 8 * lower), position - lower);
        for (std::size_t channel = 0; channel < 3; ++channel) {
            observer[channel] += cieObserver[i][channel];
            response[channel] += power * cieObserver[i][channel];
        }
    }
    for (std::size_t channel = 0; channel < 3; ++channel)
        response[channel] /= observer[channel];
    const auto total = std::accumulate(response.begin(), response.end(), 0.0);
    require(total > 0);
    const DngChromaticity white{response[0] / total, response[1] / total};
    validate(white);
    return white;
}

DngChromaticity dngWhiteChromaticity(double temperature, double tint) {
    require(std::isfinite(temperature) && temperature >= 1'000'000.0 / 600 && std::isfinite(tint));
    const auto reciprocal = 1'000'000 / temperature;
    const auto next =
        std::upper_bound(std::begin(locus), std::end(locus) - 1, reciprocal,
                         [](double r, const auto &line) { return r < line.reciprocalKelvin; });
    const auto &left = *(next - 1), &right = *next;
    const auto fraction =
        (reciprocal - left.reciprocalKelvin) / (right.reciprocalKelvin - left.reciprocalKelvin);
    const auto a = direction(left), b = direction(right);
    const auto slope = normalize(std::lerp(a[0], b[0], fraction), std::lerp(a[1], b[1], fraction));
    const auto u = std::lerp(left.u, right.u, fraction) + slope[0] * tint / tintScale;
    const auto v = std::lerp(left.v, right.v, fraction) + slope[1] * tint / tintScale;
    const auto denominator = u - 4 * v + 2;
    const DngChromaticity white{1.5 * u / denominator, v / denominator};
    validate(white);
    return white;
}

std::array<double, 2> dngWhiteTemperatureTint(DngChromaticity white) {
    validate(white);
    const auto denominator = 1.5 - white[0] + 6 * white[1];
    const auto u = 2 * white[0] / denominator, v = 3 * white[1] / denominator;
    double previousDistance = 0;
    for (std::size_t i = 1; i < std::size(locus); ++i) {
        const auto &line = locus[i];
        const auto slope = direction(line);
        const auto distance = (v - line.v) * slope[0] - (u - line.u) * slope[1];
        if (distance <= 0 || i == std::size(locus) - 1) {
            const auto &previous = locus[i - 1];
            const auto magnitude = std::max(0.0, -distance);
            const auto before = i == 1 ? 0.0 : magnitude / (previousDistance + magnitude);
            const auto oldSlope = direction(previous);
            const auto axis = normalize(std::lerp(slope[0], oldSlope[0], before),
                                        std::lerp(slope[1], oldSlope[1], before));
            const auto mired = std::lerp(line.reciprocalKelvin, previous.reciprocalKelvin, before);
            const auto du = u - std::lerp(line.u, previous.u, before);
            const auto dv = v - std::lerp(line.v, previous.v, before);
            return {1'000'000 / mired, tintScale * (du * axis[0] + dv * axis[1])};
        }
        previousDistance = distance;
    }
    throw std::runtime_error("Invalid DNG calibration illuminant.");
}

DngIlluminantWeights dngTripleIlluminantWeights(DngChromaticity white,
                                                const std::array<DngChromaticity, 3> &illuminants) {
    // Adobe DNG SDK triple-illuminant interpolation in scaled reciprocal
    // temperature and tint coordinates, including the 2% suppression step.
    const auto [temperature, tint] = dngWhiteTemperatureTint(white);
    const auto mappedTemperature = [](double kelvin) { return std::min(1500 / kelvin, 1.0); };
    DngIlluminantWeights weights{};
    for (std::size_t i = 0; i < weights.size(); ++i) {
        const auto [otherTemperature, otherTint] = dngWhiteTemperatureTint(illuminants[i]);
        const auto x = mappedTemperature(temperature) - mappedTemperature(otherTemperature);
        const auto y = (tint - otherTint) / 200;
        weights[i] = 1 / (x * x + y * y + 1e-8);
    }
    auto sum = std::accumulate(weights.begin(), weights.end(), 0.0);
    for (auto &weight : weights) {
        const auto value = weight / sum;
        const auto smooth = value * value * (3 - 2 * value);
        weight = std::clamp((smooth - .02) / .98, 0.0, 1.0);
    }
    sum = std::accumulate(weights.begin(), weights.end(), 0.0);
    weights[0] /= sum;
    weights[1] /= sum;
    weights[2] = std::max(0.0, 1 - weights[0] - weights[1]);
    return weights;
}
} // namespace luma
