#include "core/color/RGBProfiles.h"
#include <cmath>
#include <lcms2.h>
#include <limits>
#include <stdexcept>

namespace luma {
std::vector<std::uint8_t> linearSRGBProfile() {
    const cmsCIExyY white{0.3127, 0.3290, 1};
    const cmsCIExyYTRIPLE primaries{{0.64, 0.33, 1}, {0.30, 0.60, 1}, {0.15, 0.06, 1}};
    auto *gamma = cmsBuildGamma(nullptr, 1);
    if (!gamma)
        throw std::runtime_error("Cannot allocate linear RGB curve.");
    cmsToneCurve *curves[]{gamma, gamma, gamma};
    auto profile = cmsCreateRGBProfile(&white, &primaries, curves);
    cmsFreeToneCurve(gamma);
    if (!profile)
        throw std::runtime_error("Cannot create linear sRGB profile.");
    cmsUInt32Number size = 0;
    if (!cmsSaveProfileToMem(profile, nullptr, &size)) {
        cmsCloseProfile(profile);
        throw std::runtime_error("Cannot measure linear RGB profile.");
    }
    std::vector<std::uint8_t> result(size);
    const bool saved = cmsSaveProfileToMem(profile, result.data(), &size);
    cmsCloseProfile(profile);
    if (!saved || result.size() < 128)
        throw std::runtime_error("Cannot serialize linear RGB profile.");
    constexpr std::array<std::uint16_t, 6> date{2026, 1, 1, 0, 0, 0};
    for (std::size_t i = 0; i < date.size(); ++i) {
        result[24 + i * 2] = static_cast<std::uint8_t>(date[i] >> 8);
        result[25 + i * 2] = static_cast<std::uint8_t>(date[i] & 255);
    }
    return result;
}
ColorMatrix3 rgbToXYZD50(std::span<const std::uint8_t> bytes) {
    if (bytes.size() > std::numeric_limits<cmsUInt32Number>::max())
        throw std::runtime_error("ICC profile is too large.");
    auto profile = cmsOpenProfileFromMem(bytes.data(), static_cast<cmsUInt32Number>(bytes.size()));
    if (!profile)
        throw std::runtime_error("Cannot open RGB profile.");
    ColorMatrix3 result{};
    constexpr std::array tags{cmsSigRedColorantTag, cmsSigGreenColorantTag, cmsSigBlueColorantTag};
    for (std::size_t column = 0; column < 3; ++column) {
        const auto *value = static_cast<const cmsCIEXYZ *>(cmsReadTag(profile, tags[column]));
        if (!value || !std::isfinite(value->X) || !std::isfinite(value->Y) ||
            !std::isfinite(value->Z)) {
            cmsCloseProfile(profile);
            throw std::runtime_error("RGB profile lacks finite matrix colorants.");
        }
        result[column] = value->X;
        result[column + 3] = value->Y;
        result[column + 6] = value->Z;
    }
    cmsCloseProfile(profile);
    (void)inverseColorMatrix(result);
    return result;
}
ColorMatrix3 inverseColorMatrix(const ColorMatrix3 &m) {
    for (double value : m)
        if (!std::isfinite(value) || std::abs(value) > 1000)
            throw std::runtime_error("Invalid RGB color matrix.");
    const double determinant = m[0] * (m[4] * m[8] - m[5] * m[7]) -
                               m[1] * (m[3] * m[8] - m[5] * m[6]) +
                               m[2] * (m[3] * m[7] - m[4] * m[6]);
    if (std::abs(determinant) < 1e-10)
        throw std::runtime_error("Singular RGB color matrix.");
    return {(m[4] * m[8] - m[5] * m[7]) / determinant, (m[2] * m[7] - m[1] * m[8]) / determinant,
            (m[1] * m[5] - m[2] * m[4]) / determinant, (m[5] * m[6] - m[3] * m[8]) / determinant,
            (m[0] * m[8] - m[2] * m[6]) / determinant, (m[2] * m[3] - m[0] * m[5]) / determinant,
            (m[3] * m[7] - m[4] * m[6]) / determinant, (m[1] * m[6] - m[0] * m[7]) / determinant,
            (m[0] * m[4] - m[1] * m[3]) / determinant};
}
ColorMatrix3 multiplyColorMatrices(const ColorMatrix3 &left, const ColorMatrix3 &right) {
    ColorMatrix3 result{};
    for (std::size_t row = 0; row < 3; ++row)
        for (std::size_t column = 0; column < 3; ++column)
            for (std::size_t k = 0; k < 3; ++k)
                result[row * 3 + column] += left[row * 3 + k] * right[k * 3 + column];
    return result;
}
} // namespace luma
