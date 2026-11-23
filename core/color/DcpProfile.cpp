#include "core/color/DcpProfile.h"
#include "core/color/DngColorCalibration.h"
#include "core/color/DngIlluminant.h"
#include "core/color/RGBProfiles.h"
#include "core/io/TiffDirectory.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace luma {
namespace {
void require(bool valid) {
    if (!valid)
        throw std::runtime_error("Unsupported or damaged DNG camera profile.");
}

std::string text(const TiffDirectory &directory, std::uint16_t tag) {
    const auto bytes = directory.text(tag, 512);
    const auto end = std::find(bytes.begin(), bytes.end(), 0);
    std::string result(bytes.begin(), end);
    require(!result.empty() &&
            std::ranges::none_of(result, [](unsigned char ch) { return ch < 32 || ch == 127; }));
    return result;
}

void readMatrix(const TiffDirectory &directory, std::uint16_t tag, std::array<float, 9> &target) {
    const auto values = directory.numbers(tag, 9);
    require(values.size() == 9 &&
            std::ranges::all_of(values, [](double v) { return std::abs(v) <= 64; }));
    ColorMatrix3 checked{};
    std::copy(values.begin(), values.end(), checked.begin());
    (void)inverseColorMatrix(checked);
    std::copy(values.begin(), values.end(), target.begin());
}

DcpHueTable readTable(const TiffDirectory &directory, std::uint16_t dimensionTag,
                      std::uint16_t contentsTag, std::uint16_t encodingTag) {
    if (!directory.has(contentsTag))
        return {};
    const auto dims = directory.integers(dimensionTag, 3);
    const auto encoding = directory.integers(encodingTag, 1);
    require(dims.size() == 3 && dims[0] >= 1 && dims[0] <= 360 && dims[1] >= 2 && dims[1] <= 128 &&
            dims[2] >= 1 && dims[2] <= 128 && (encoding.empty() || encoding[0] <= 1));
    const auto count = static_cast<std::size_t>(dims[0]) * dims[1] * dims[2] * 3;
    require(count <= 1'000'000);
    const auto entries = directory.numbers(contentsTag, count);
    require(entries.size() == count);
    DcpHueTable result{
        {dims[0], dims[1], dims[2]}, !encoding.empty() && encoding[0] == 1 && dims[2] > 1, {}};
    result.samples.reserve(count);
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto value = entries[i];
        require(i % 3 == 0 ? std::abs(value) <= 360 : value >= 0 && value <= 64);
        result.samples.push_back(static_cast<float>(value));
    }
    return result;
}

constexpr ColorMatrix3 proPhotoToD50{.7976749, .1351917, .0313534, .2880402, .7118741,
                                     .0000857, 0,        0,        .8252100};
std::array<float, 3> color(const ColorMatrix3 &matrix, const std::array<float, 3> &value) {
    std::array<float, 3> result{};
    for (std::size_t i = 0; i < 3; ++i)
        result[i] = static_cast<float>(matrix[i * 3] * value[0] + matrix[i * 3 + 1] * value[1] +
                                       matrix[i * 3 + 2] * value[2]);
    return result;
}
const ColorMatrix3 &srgbToProPhoto() {
    static const auto matrix =
        multiplyColorMatrices(inverseColorMatrix(proPhotoToD50), rgbToXYZD50(linearSRGBProfile()));
    return matrix;
}
const ColorMatrix3 &proPhotoToSrgb() {
    static const auto matrix = inverseColorMatrix(srgbToProPhoto());
    return matrix;
}

struct Hsv {
    double h, s, v;
};

Hsv hsv(const std::array<float, 3> &rgb) {
    const auto [lower, upper] = std::minmax_element(rgb.begin(), rgb.end());
    const double maximum = *upper, minimum = std::max(0.F, *lower);
    const double delta = maximum - minimum;
    if (maximum <= 0 || delta <= 1e-8)
        return {0, 0, maximum};
    const auto ratio = [&](int a, int b) {
        return (std::max(0.F, rgb[a]) - std::max(0.F, rgb[b])) / delta;
    };
    const auto channel = upper - rgb.begin();
    double hue = channel == 0 ? ratio(1, 2) : channel == 1 ? 2 + ratio(2, 0) : 4 + ratio(0, 1);
    hue -= 6 * std::floor(hue / 6);
    return {hue / 6, delta / maximum, maximum};
}

std::array<float, 3> rgb(Hsv value) {
    const auto channel = [&](double shift) {
        const auto position = value.h * 6 + shift;
        const auto sector = position - 6 * std::floor(position / 6);
        return static_cast<float>(
            value.v * (1 - value.s * std::max(0.0, std::min({sector, 4 - sector, 1.0}))));
    };
    return {channel(5), channel(3), channel(1)};
}

std::array<double, 3> lookup(const DcpHueTable &table, Hsv value) {
    const auto [h, s, v] = table.divisions;
    const double hue = value.h * h, sat = std::clamp(value.s, 0.0, 1.0) * (s - 1),
                 val = std::clamp(value.v, 0.0, 1.0) * (v - 1);
    const auto h0 = static_cast<unsigned>(hue) % h, h1 = (h0 + 1) % h;
    const auto s0 = static_cast<unsigned>(sat), s1 = std::min(s0 + 1, s - 1);
    const auto v0 = static_cast<unsigned>(val), v1 = std::min(v0 + 1, v - 1);
    const double ht = hue - std::floor(hue), st = sat - s0, vt = val - v0;
    std::array<double, 3> result{};
    for (std::size_t c = 0; c < 3; ++c) {
        const auto row = [&](unsigned z, unsigned x) {
            const auto start = (static_cast<std::size_t>(z) * h + x) * s * 3;
            return std::lerp(static_cast<double>(table.samples[start + s0 * 3 + c]),
                             static_cast<double>(table.samples[start + s1 * 3 + c]), st);
        };
        result[c] = std::lerp(std::lerp(row(v0, h0), row(v0, h1), ht),
                              std::lerp(row(v1, h0), row(v1, h1), ht), vt);
    }
    return result;
}

double encode(double x) {
    return x < .0031308 ? 12.92 * x : 1.055 * std::pow(x, 1 / 2.4) - .055;
}
double decode(double x) {
    return x <= .04045 ? x / 12.92 : std::pow((x + .055) / 1.055, 2.4);
}

std::array<float, 3> tableColor(const std::array<float, 3> &input, bool gammaEncodedValue,
                                const auto &correctionAt) {
    auto value = hsv(input);
    if (value.v <= 0)
        return input;
    const double headroom = std::max(1.0, value.v);
    value.v /= headroom;
    if (gammaEncodedValue)
        value.v = encode(value.v);
    const auto correction = correctionAt(value);
    value.h += correction[0] / 360;
    value.s = std::clamp(value.s * correction[1], 0.0, 1.0);
    value.v = std::min(1.0, value.v * correction[2]);
    if (gammaEncodedValue)
        value.v = decode(value.v);
    value.v *= headroom;
    return rgb(value);
}

std::array<float, 3> tableColor(const std::array<float, 3> &input, const DcpHueTable &first,
                                const DcpHueTable *second = nullptr, double weight = 0) {
    return tableColor(input, first.gammaEncodedValue, [&](Hsv value) {
        auto correction = lookup(first, value);
        if (second) {
            const auto high = lookup(*second, value);
            for (std::size_t i = 0; i < correction.size(); ++i)
                correction[i] = std::lerp(correction[i], high[i], weight);
        }
        return correction;
    });
}

struct DcpIlluminantTags {
    std::uint16_t illuminant, color, forward, hueSaturation, custom;
};
constexpr DcpIlluminantTags tags[]{{50778, 50721, 50964, 50938, 52533},
                                   {50779, 50722, 50965, 50939, 52534},
                                   {52529, 52531, 52532, 52537, 52535}};

void readTripleIlluminant(DngColorProfile &profile, const TiffDirectory &directory,
                          const DcpIlluminantTags &set, unsigned code, bool littleEndian) {
    require(directory.hasType(set.illuminant, 3) && directory.hasType(set.color, 10));
    if (code == 255) {
        require(directory.hasType(set.custom, 7));
        profile.calibrationWhite =
            dngCustomIlluminant(directory.text(set.custom, maximumDcpBytes), littleEndian);
    } else {
        profile.calibrationWhite = dngStandardIlluminant(code);
    }
}

float toneValue(const std::vector<std::array<float, 2>> &curve, float value) {
    if (value < 0 || value > 1)
        return value;
    const auto upper = std::lower_bound(curve.begin(), curve.end(), value,
                                        [](const auto &point, float x) { return point[0] < x; });
    if (upper == curve.begin())
        return upper->at(1);
    if (upper == curve.end())
        return curve.back()[1];
    const auto &low = *(upper - 1);
    return std::lerp(low[1], (*upper)[1], (value - low[0]) / ((*upper)[0] - low[0]));
}
} // namespace

std::shared_ptr<const DcpProfile> readDcpProfile(std::string source) {
    require(source.size() <= maximumDcpBytes);
    const TiffDirectory directory(
        {reinterpret_cast<const std::uint8_t *>(source.data()), source.size()}, 0x4352);
    auto result = std::make_shared<DcpProfile>();
    result->model = text(directory, 50708);
    result->name = text(directory, 50936);
    const auto &third = tags[2];
    const bool triple = directory.has(third.illuminant) || directory.has(third.color) ||
                        directory.has(third.forward) || directory.has(third.hueSaturation);
    // DCPs here are RGB. A reduction matrix applies only to more than 3 planes.
    if (triple)
        require(!directory.has(52530) && !directory.has(52538));
    result->illuminantCount = triple ? 3 : directory.has(50722) ? 2 : 1;
    for (unsigned i = 0; i < result->illuminantCount; ++i) {
        auto &profile = result->illuminants[i];
        const auto &set = tags[i];
        const auto illuminant = directory.integers(set.illuminant, 1);
        require(illuminant.size() == 1);
        if (triple) {
            readTripleIlluminant(profile, directory, set, illuminant[0], source.front() == 'I');
        } else {
            const auto temperature = dngIlluminantTemperature(illuminant[0]);
            require(temperature.has_value());
            profile.calibrationTemperature = *temperature;
        }
        readMatrix(directory, set.color, profile.colorMatrix);
        profile.hasForwardMatrix = directory.has(set.forward);
        if (profile.hasForwardMatrix)
            readMatrix(directory, set.forward, profile.forwardMatrix);
        result->hueSaturation[i] = readTable(directory, 50937, set.hueSaturation, 51107);
        if (triple) {
            const auto &table = result->hueSaturation[i];
            require(profile.hasForwardMatrix == result->illuminants[0].hasForwardMatrix &&
                    (!profile.hasForwardMatrix || directory.hasType(set.forward, 10)) &&
                    table.samples.empty() == result->hueSaturation[0].samples.empty() &&
                    (table.samples.empty() ||
                     (directory.hasType(set.hueSaturation, 11) && directory.hasType(50937, 4))));
            for (std::size_t j = 2; j < table.samples.size(); j += table.divisions[1] * 3)
                require(table.samples[j] == 1);
            for (unsigned j = 0; j < i; ++j)
                require(profile.calibrationWhite != result->illuminants[j].calibrationWhite);
        }
    }
    if (result->illuminantCount == 2)
        require(result->illuminants[0].calibrationTemperature !=
                result->illuminants[1].calibrationTemperature);
    result->look = readTable(directory, 50981, 50982, 51108);
    const auto points = directory.numbers(50940, 8192);
    require(points.empty() || (points.size() >= 4 && points.size() % 2 == 0));
    for (std::size_t i = 0; i < points.size(); i += 2) {
        require(points[i] >= 0 && points[i] <= 1 && points[i + 1] >= 0 && points[i + 1] <= 1 &&
                (i == 0 || points[i] > points[i - 2]));
        result->tone.push_back({static_cast<float>(points[i]), static_cast<float>(points[i + 1])});
    }
    const auto exposure = directory.numbers(51109, 1);
    if (!exposure.empty()) {
        require(std::abs(exposure[0]) <= 4);
        result->baselineExposure = exposure[0];
    }
    result->source = std::move(source);
    return result;
}

RawColorCalibration applyDcpCalibration(RawColorCalibration calibration,
                                        const DcpProfile &profile) {
    if (profile.illuminantCount == 3) {
        constexpr DngColorProfile identity{};
        for (std::size_t i = 0; i < calibration.dngProfileCount; ++i)
            if (calibration.dngProfiles[i].cameraCalibration != identity.cameraCalibration)
                throw std::runtime_error(
                    "Triple-illuminant DCP requires an uncalibrated camera reference space.");
    }
    if (calibration.dngProfileCount == 0)
        for (std::size_t i = 0; i < calibration.asShotNeutral.size(); ++i)
            calibration.asShotNeutral[i] = 1 / calibration.asShotGains[i];
    calibration.dngProfiles = profile.illuminants;
    calibration.dngProfileCount = static_cast<std::uint8_t>(profile.illuminantCount);
    finalizeDngColorCalibration(calibration);
    return calibration;
}

std::array<float, 3> applyDcpHueSaturation(const DcpProfile &profile, std::array<float, 3> input,
                                           double temperature,
                                           const std::array<double, 3> &profileWeights) {
    const auto &first = profile.hueSaturation[0];
    const auto &second = profile.hueSaturation[1];
    if (first.samples.empty() && second.samples.empty())
        return input;
    const auto linear = color(srgbToProPhoto(), input);
    if (profile.illuminantCount == 3) {
        return color(proPhotoToSrgb(), tableColor(linear, first.gammaEncodedValue, [&](Hsv value) {
                         std::array<double, 3> correction{};
                         for (std::size_t i = 0; i < profileWeights.size(); ++i) {
                             if (profileWeights[i] == 0)
                                 continue;
                             const auto sample = lookup(profile.hueSaturation[i], value);
                             for (std::size_t channel = 0; channel < correction.size(); ++channel)
                                 correction[channel] += profileWeights[i] * sample[channel];
                         }
                         return correction;
                     }));
    }
    if (first.samples.empty() || second.samples.empty())
        return color(proPhotoToSrgb(), tableColor(linear, first.samples.empty() ? second : first));
    const auto a = 1 / profile.illuminants[0].calibrationTemperature;
    const auto b = 1 / profile.illuminants[1].calibrationTemperature;
    const auto weight = std::clamp((1 / temperature - a) / (b - a), 0.0, 1.0);
    return color(proPhotoToSrgb(), tableColor(linear, first, &second, weight));
}

std::array<float, 3> applyDcpLook(const DcpProfile &profile, std::array<float, 3> input) {
    if (profile.look.samples.empty() && profile.tone.empty())
        return input;
    auto linear = color(srgbToProPhoto(), input);
    if (!profile.look.samples.empty())
        linear = tableColor(linear, profile.look);
    if (!profile.tone.empty())
        for (auto &channel : linear)
            channel = toneValue(profile.tone, channel);
    return color(proPhotoToSrgb(), linear);
}
} // namespace luma
