#include "core/color/DcpProfile.h"
#include "core/color/DngColorCalibration.h"
#include "core/color/DngIlluminant.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char *reason) {
    if (!condition)
        throw std::runtime_error(reason);
}

struct CameraProfileFixture {
    struct Field {
        std::uint16_t tag, type;
        std::uint32_t count;
        std::string bytes;
    };
    bool little = true;
    std::vector<Field> fields;

    void append(std::string &bytes, std::uint32_t value, unsigned size) const {
        for (unsigned i = 0; i < size; ++i)
            bytes.push_back(static_cast<char>(value >> (8 * (little ? i : size - 1 - i))));
    }
    void numbers(std::uint16_t tag, std::uint16_t type, const std::vector<std::uint32_t> &values,
                 unsigned size, unsigned denominator = 0) {
        Field field{tag, type, static_cast<std::uint32_t>(values.size()), {}};
        for (const auto value : values) {
            append(field.bytes, value, size);
            if (denominator != 0)
                append(field.bytes, denominator, 4);
        }
        fields.push_back(std::move(field));
    }
    void text(std::uint16_t tag, std::string data) {
        data.push_back(0);
        fields.push_back({tag, 2, static_cast<std::uint32_t>(data.size()), std::move(data)});
    }
    void customWhite(bool spectral) {
        std::string data;
        append(data, spectral ? 1 : 0, 2);
        if (spectral)
            append(data, 2, 4);
        // An equal-energy spectrum and explicit 1/3, 1/3 must agree.
        for (const auto number :
             spectral ? std::vector{360, 1, 470, 1, 1, 1, 1, 1} : std::vector{1, 3, 1, 3})
            append(data, number, 4);
        fields.push_back({52535, 7, static_cast<std::uint32_t>(data.size()), std::move(data)});
    }
    std::string bytes() {
        std::ranges::sort(fields, {}, &Field::tag);
        std::string header = little ? "II" : "MM", data;
        append(header, 0x4352, 2);
        append(header, 8, 4);
        append(header, fields.size(), 2);
        const auto firstValueOffset = 8 + 2 + 12 * fields.size() + 4;
        for (const auto &field : fields) {
            append(header, field.tag, 2);
            append(header, field.type, 2);
            append(header, field.count, 4);
            if (field.bytes.size() <= 4) {
                header.append(field.bytes);
                header.append(4 - field.bytes.size(), '\0');
            } else {
                append(header, firstValueOffset + data.size(), 4);
                data.append(field.bytes);
            }
        }
        append(header, 0, 4);
        return header + data;
    }
};

CameraProfileFixture fixture(bool little, bool spectral) {
    CameraProfileFixture data{little, {}};
    data.text(50708, "Test Camera");
    data.text(50936, "Triplet Fixture");
    data.numbers(50778, 3, {17}, 2);
    data.numbers(50779, 3, {21}, 2);
    data.numbers(52529, 3, {255}, 2);
    data.customWhite(spectral);
    data.numbers(50721, 10, {100, 0, 0, 0, 100, 0, 0, 0, 100}, 4, 100);
    data.numbers(50722, 10, {125, 0, 0, 0, 100, 0, 0, 0, 90}, 4, 100);
    data.numbers(52531, 10, {85, 0, 0, 0, 100, 0, 0, 0, 110}, 4, 100);
    data.numbers(50937, 4, {1, 2, 1}, 4);
    const auto lut = [&](std::uint16_t tag, float saturation) {
        data.numbers(tag, 11,
                     {0, std::bit_cast<std::uint32_t>(1.F), std::bit_cast<std::uint32_t>(1.F), 0,
                      std::bit_cast<std::uint32_t>(saturation), std::bit_cast<std::uint32_t>(1.F)},
                     4);
    };
    lut(50938, 1.F);
    lut(50939, .7F);
    lut(52537, .2F);
    return data;
}

void reject(CameraProfileFixture data) {
    try {
        (void)luma::readDcpProfile(data.bytes());
    } catch (const std::exception &) {
        return;
    }
    throw std::runtime_error("Malformed triple profile was accepted");
}

void verify(bool little, bool spectral) {
    auto input = fixture(little, spectral);
    const auto profile = luma::readDcpProfile(input.bytes());
    require(profile->illuminantCount == 3, "Missing third profile calibration");
    const std::array lights{profile->illuminants[0].calibrationWhite,
                            profile->illuminants[1].calibrationWhite,
                            profile->illuminants[2].calibrationWhite};
    require(std::abs(lights[2][0] - 1.0 / 3) < 1e-12 && std::abs(lights[2][1] - 1.0 / 3) < 1e-12,
            "Custom illuminant white point differs from specified xy/spectrum");
    const luma::DngChromaticity white{.4, .37};
    // Reference values computed independently from the Adobe SDK weighting math.
    const luma::DngIlluminantWeights expected{.5688601723096092, .05451062290234223,
                                              .3766292047880486};
    const auto weights = luma::dngTripleIlluminantWeights(white, lights);
    for (std::size_t i = 0; i < weights.size(); ++i) {
        require(std::abs(weights[i] - expected[i]) < 1e-10, "Wrong DNG SDK interpolation weight");
        require(luma::dngTripleIlluminantWeights(lights[i], lights)[i] > .999999,
                "Nominal calibration illuminant failed to select its matrix");
    }
    luma::RawColorCalibration capture;
    // Shared matrices give an unambiguous neutral while the illuminants still
    // have independent hue/saturation correction tables.
    auto neutralProfile = *profile;
    for (auto &illuminant : neutralProfile.illuminants)
        illuminant.colorMatrix = profile->illuminants[0].colorMatrix;
    capture.asShotGains = {static_cast<float>(white[1] / white[0]), 1,
                           static_cast<float>(white[1] / (1 - white[0] - white[1]))};
    const auto calibrated = luma::applyDcpCalibration(capture, neutralProfile);
    for (std::size_t i = 0; i < weights.size(); ++i)
        require(std::abs(calibrated.asShotProfileWeights[i] - weights[i]) < 1e-5,
                "As Shot solve lost the chromaticity or calibration weights");
    const auto varied = luma::applyDcpCalibration(capture, *profile);
    const auto cool = luma::resolveRawColorCalibration(varied, 5200, 45);
    const auto warm = luma::resolveRawColorCalibration(varied, 5200, -45);
    require(cool.profileWeights != warm.profileWeights && cool.cameraToSRGB != warm.cameraToSRGB,
            "Manual tint did not affect third-illuminant interpolation");
    const std::array<float, 3> rgb{.3F, .17F, .09F};
    const auto mixed = luma::applyDcpHueSaturation(*profile, rgb, 5200, weights);
    std::array<double, 3> expectedColor{};
    for (std::size_t i = 0; i < weights.size(); ++i) {
        luma::DngIlluminantWeights single{};
        single[i] = 1;
        const auto color = luma::applyDcpHueSaturation(*profile, rgb, 5200, single);
        for (std::size_t j = 0; j < color.size(); ++j)
            expectedColor[j] += weights[i] * color[j];
    }
    for (std::size_t i = 0; i < mixed.size(); ++i)
        require(std::abs(mixed[i] - expectedColor[i]) < 1e-6,
                "Third hue/saturation table did not use the same weights");
}
} // namespace

int main() {
    try {
        verify(true, true);
        verify(false, false);
        auto invalid = fixture(true, false);
        std::erase_if(invalid.fields, [](const auto &field) { return field.tag == 52537; });
        reject(std::move(invalid));
        invalid = fixture(true, false);
        invalid.numbers(52532, 10, {100, 0, 0, 0, 100, 0, 0, 0, 100}, 4, 100);
        reject(std::move(invalid));
        auto dual = fixture(true, false);
        std::erase_if(dual.fields, [](const auto &field) { return field.tag >= 52529; });
        const auto previousProfile = luma::readDcpProfile(dual.bytes());
        require(previousProfile->illuminantCount == 2 &&
                    previousProfile->illuminants[0].calibrationTemperature == 2856 &&
                    previousProfile->illuminants[1].calibrationTemperature == 6504,
                "Dual calibration behavior changed");
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
