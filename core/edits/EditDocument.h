#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

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
    saturation = 256,
    curves = 512,
    hsl = 1024
};
struct CurvePoint {
    double x = 0;
    double y = 0;
    bool operator==(const CurvePoint &) const = default;
};
struct PointCurve {
    std::vector<CurvePoint> points{{0, 0}, {1, 1}};
    bool operator==(const PointCurve &) const = default;
    void validate() const;
};
struct HSLBand {
    double hue = 0;
    double saturation = 0;
    double luminance = 0;
    bool operator==(const HSLBand &) const = default;
};
inline constexpr std::array<std::string_view, 4> curveChannels{"rgb", "red", "green", "blue"};
inline constexpr std::array<std::string_view, 8> hslBands{"red",  "orange", "yellow", "green",
                                                          "aqua", "blue",   "purple", "magenta"};
struct EditDocument {
    static constexpr int schemaVersion = 3;
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
    std::uint32_t enabledCorrections = 2047;
    double curveShadows = 0;
    double curveDarks = 0;
    double curveLights = 0;
    double curveHighlights = 0;
    std::array<PointCurve, 4> curves{};
    std::array<HSLBand, 8> hsl{};
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
    static std::size_t curveChannel(std::string_view name);
    static std::size_t hslBand(std::string_view name);
};
} // namespace luma
