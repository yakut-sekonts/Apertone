#pragma once
#include <string>
#include <string_view>

namespace luma {
struct LensSettings {
    std::string profile; // Empty means conservative automatic identification.
    double distortion = 100;
    double vignetting = 100;
    bool chromaticAberration = true;
    bool operator==(const LensSettings &) const = default;
    void validate() const;
    double parameter(std::string_view name) const;
    void setParameter(std::string_view name, double value);
};
} // namespace luma
