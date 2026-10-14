#include "platform/macos/ExportColor.h"
#include <CoreGraphics/CoreGraphics.h>
#include <cmath>
#include <lcms2.h>
#include <limits>
#include <stdexcept>

namespace luma::macos {
ExportColor::ExportColor(ExportColorSpace space) {
    CFStringRef name = nullptr;
    switch (space) {
    case ExportColorSpace::srgb:
        name = kCGColorSpaceSRGB;
        break;
    case ExportColorSpace::displayP3:
        name = kCGColorSpaceDisplayP3;
        break;
    case ExportColorSpace::adobeRGB:
        name = kCGColorSpaceAdobeRGB1998;
        break;
    case ExportColorSpace::proPhoto:
        name = kCGColorSpaceROMMRGB;
        break;
    }
    if (!name)
        throw std::runtime_error("Unsupported export color space.");
    auto color = CGColorSpaceCreateWithName(name);
    auto data = color ? CGColorSpaceCopyICCData(color) : nullptr;
    if (color)
        CGColorSpaceRelease(color);
    if (!data)
        throw std::runtime_error("macOS cannot provide the selected ICC profile.");
    profile_.assign(CFDataGetBytePtr(data), CFDataGetBytePtr(data) + CFDataGetLength(data));
    CFRelease(data);
    colorants_ = rgbToXYZD50(profile_);
    const auto inputBytes = linearSRGBProfile();
    auto input =
        cmsOpenProfileFromMem(inputBytes.data(), static_cast<cmsUInt32Number>(inputBytes.size()));
    auto encoded =
        cmsOpenProfileFromMem(profile_.data(), static_cast<cmsUInt32Number>(profile_.size()));
    auto linear =
        cmsOpenProfileFromMem(profile_.data(), static_cast<cmsUInt32Number>(profile_.size()));
    auto gamma = cmsBuildGamma(nullptr, 1);
    bool prepared = input && encoded && linear && gamma;
    if (prepared) {
        for (const auto tag : {cmsSigRedTRCTag, cmsSigGreenTRCTag, cmsSigBlueTRCTag})
            prepared = cmsWriteTag(linear, tag, gamma) && prepared;
        if (prepared) {
            encoded_ =
                cmsCreateTransform(input, TYPE_RGB_FLT, encoded, TYPE_RGB_FLT,
                                   INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_BLACKPOINTCOMPENSATION);
            linear_ =
                cmsCreateTransform(input, TYPE_RGB_FLT, linear, TYPE_RGB_FLT,
                                   INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_BLACKPOINTCOMPENSATION);
        }
    }
    if (gamma)
        cmsFreeToneCurve(gamma);
    if (input)
        cmsCloseProfile(input);
    if (encoded)
        cmsCloseProfile(encoded);
    if (linear)
        cmsCloseProfile(linear);
    if (!encoded_ || !linear_) {
        if (encoded_)
            cmsDeleteTransform(encoded_);
        if (linear_)
            cmsDeleteTransform(linear_);
        throw std::runtime_error("Cannot create export ICC transforms.");
    }
}
ExportColor::~ExportColor() {
    if (encoded_)
        cmsDeleteTransform(encoded_);
    if (linear_)
        cmsDeleteTransform(linear_);
}
void ExportColor::convert(std::span<const float> rgb, std::span<float> output, bool linear) const {
    if (rgb.size() % 3 || output.size() != rgb.size() ||
        rgb.size() / 3 > std::numeric_limits<cmsUInt32Number>::max())
        throw std::runtime_error("Invalid export color buffers.");
    cmsDoTransform(linear ? linear_ : encoded_, rgb.data(), output.data(),
                   static_cast<cmsUInt32Number>(rgb.size() / 3));
    for (float value : output)
        if (!std::isfinite(value))
            throw std::runtime_error("Non-finite ICC output sample.");
}
} // namespace luma::macos
