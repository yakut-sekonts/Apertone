#include "core/edits/LensSettings.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace luma {
void LensSettings::validate() const {
    if ((!profile.empty() &&
         (profile.size() != 32 ||
          !std::all_of(profile.begin(), profile.end(),
                       [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }))) ||
        !std::isfinite(distortion) || distortion < 0 || distortion > 200 ||
        !std::isfinite(vignetting) || vignetting < 0 || vignetting > 200)
        throw std::runtime_error("Invalid lens profile or correction amount.");
}
double LensSettings::parameter(std::string_view name) const {
    if (name == "lens.distortion")
        return distortion;
    if (name == "lens.vignetting")
        return vignetting;
    if (name == "lens.chromaticAberration")
        return chromaticAberration;
    throw std::runtime_error("Unknown lens correction parameter.");
}
void LensSettings::setParameter(std::string_view name, double value) {
    auto next = *this;
    if (name == "lens.distortion")
        next.distortion = value;
    else if (name == "lens.vignetting")
        next.vignetting = value;
    else if (name == "lens.chromaticAberration" && (value == 0 || value == 1))
        next.chromaticAberration = value != 0;
    else
        throw std::runtime_error("Unknown lens parameter or invalid boolean.");
    next.validate();
    *this = next;
}
} // namespace luma
