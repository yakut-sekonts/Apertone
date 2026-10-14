#pragma once
#include "core/math/Homography.h"
#include <string>
#include <string_view>
#include <vector>

namespace luma {
inline constexpr std::array<std::string_view, 17> transformParameters{
    "transform.rotate", "transform.horizontal", "transform.vertical", "transform.aspect",
    "transform.scale",  "transform.xOffset",    "transform.yOffset",  "transform.constrainCrop",
    "transform.guided", "transform.m11",        "transform.m12",      "transform.m13",
    "transform.m21",    "transform.m22",        "transform.m23",      "transform.m31",
    "transform.m32"};
struct TransformSettings {
    double rotate = 0, horizontal = 0, vertical = 0, aspect = 0, scale = 100, xOffset = 0,
           yOffset = 0;
    bool constrainCrop = true, guided = false;
    std::array<double, 8> guide{1, 0, 0, 0, 1, 0, 0, 0};
    bool operator==(const TransformSettings &) const = default;
    void validate() const;
    double parameter(std::string_view name) const;
    void setParameters(const std::vector<std::pair<std::string, double>> &parameters);
    Homography guideMatrix() const;
    bool neutral() const;
};
} // namespace luma
