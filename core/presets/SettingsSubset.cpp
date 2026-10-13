#include "core/presets/SettingsSubset.h"
#include <stdexcept>

namespace luma {
void validateSettingsGroups(std::uint32_t groups) {
    if (!groups || (groups & ~allSettingsGroups))
        throw std::invalid_argument("Select at least one supported settings group.");
}
std::uint32_t settingsCorrections(std::uint32_t groups) {
    validateSettingsGroups(groups);
    const auto flag = [](Correction correction) { return static_cast<std::uint32_t>(correction); };
    std::uint32_t mask = 0;
    if (groups & 1)
        mask |= flag(Correction::exposure) | flag(Correction::contrast) |
                flag(Correction::highlights) | flag(Correction::shadows) |
                flag(Correction::whites) | flag(Correction::blacks);
    if (groups & 2)
        mask |= flag(Correction::whiteBalance) | flag(Correction::vibrance) |
                flag(Correction::saturation);
    if (groups & 4)
        mask |= flag(Correction::curves);
    if (groups & 8)
        mask |= flag(Correction::hsl);
    if (groups & 16)
        mask |= flag(Correction::sharpening) | flag(Correction::luminanceNoise) |
                flag(Correction::colorNoise);
    if (groups & 32)
        mask |= flag(Correction::crop);
    return mask;
}
EditDocument mergeSettings(const EditDocument &current, const EditDocument &source,
                           std::uint32_t groups) {
    const auto mask = settingsCorrections(groups);
    current.validate();
    source.validate();
    auto result = current;
    if (groups & 1) {
        result.exposure = source.exposure;
        result.contrast = source.contrast;
        result.highlights = source.highlights;
        result.shadows = source.shadows;
        result.whites = source.whites;
        result.blacks = source.blacks;
    }
    if (groups & 2) {
        result.temperature = source.temperature;
        result.tint = source.tint;
        result.customWhiteBalance = source.customWhiteBalance;
        result.vibrance = source.vibrance;
        result.saturation = source.saturation;
    }
    if (groups & 4) {
        result.curveShadows = source.curveShadows;
        result.curveDarks = source.curveDarks;
        result.curveLights = source.curveLights;
        result.curveHighlights = source.curveHighlights;
        result.curves = source.curves;
    }
    if (groups & 8)
        result.hsl = source.hsl;
    if (groups & 16) {
        result.sharpenAmount = source.sharpenAmount;
        result.sharpenRadius = source.sharpenRadius;
        result.sharpenDetail = source.sharpenDetail;
        result.sharpenMasking = source.sharpenMasking;
        result.noiseLuminance = source.noiseLuminance;
        result.noiseLuminanceDetail = source.noiseLuminanceDetail;
        result.noiseLuminanceContrast = source.noiseLuminanceContrast;
        result.noiseColor = source.noiseColor;
        result.noiseColorDetail = source.noiseColorDetail;
        result.noiseColorSmoothness = source.noiseColorSmoothness;
    }
    if (groups & 32)
        result.crop = source.crop;
    result.enabledCorrections =
        (current.enabledCorrections & ~mask) | (source.enabledCorrections & mask);
    result.validate();
    return result;
}
} // namespace luma
