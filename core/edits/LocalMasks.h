#pragma once
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace luma {
enum class MaskShape { linear, radial };
inline constexpr std::array<std::string_view, 8> localParameters{
    "exposure", "contrast", "highlights", "shadows", "whites", "blacks", "vibrance", "saturation"};
struct LocalMask {
    std::string id, name;
    MaskShape shape = MaskShape::linear;
    bool enabled = true, inverted = false;
    double x1 = 0.5, y1 = 0.25, x2 = 0.5, y2 = 0.75;
    double angle = 0, feather = 100, opacity = 100;
    std::array<double, 8> adjustments{};
    bool operator==(const LocalMask &) const = default;
    void validate() const;
    bool neutral() const;
    double parameter(std::string_view name) const;
    void setParameter(std::string_view name, double value);
    // Normalized pixel-edge coordinates in the optically corrected source canvas.
    double coverage(double x, double y) const;
};
void validateLocalMasks(const std::vector<LocalMask> &masks);
std::string encodeLocalMasks(const std::vector<LocalMask> &masks);
std::vector<LocalMask> decodeLocalMasks(std::string_view text);
} // namespace luma
