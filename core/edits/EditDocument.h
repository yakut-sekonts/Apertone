#pragma once
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace luma {
enum class Correction : std::uint32_t {
    whiteBalance = 1,
    exposure = 2,
    contrast = 4,
    highlights = 8,
    shadows = 16,
    whites = 32,
    blacks = 64,
    vibrance = 128,
    saturation = 256
};
struct EditDocument {
    static constexpr int schemaVersion = 2;
    double exposure = 0;
    double temperature = 6500;
    double tint = 0;
    bool customWhiteBalance = false;
    double contrast = 0;
    double highlights = 0;
    double shadows = 0;
    double whites = 0;
    double blacks = 0;
    double vibrance = 0;
    double saturation = 0;
    std::uint32_t enabledCorrections = 511;
    bool operator==(const EditDocument &) const = default;
    bool enabled(Correction correction) const {
        return (enabledCorrections & static_cast<std::uint32_t>(correction)) != 0;
    }
    void setEnabled(Correction correction, bool value) {
        const auto mask = static_cast<std::uint32_t>(correction);
        enabledCorrections = value ? enabledCorrections | mask : enabledCorrections & ~mask;
    }
    void validate() const;
    double parameter(std::string_view name) const;
    void setParameter(std::string_view name, double value);
    static Correction correction(std::string_view name);
};
} // namespace luma
