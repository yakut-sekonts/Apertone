#pragma once
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace luma {
struct CropSettings {
    double x = 0, y = 0, width = 1, height = 1;
    double angle = 0, aspect = 0;
    int quarterTurns = 0;
    bool flipHorizontal = false, flipVertical = false;
    bool operator==(const CropSettings &) const = default;
    void validate() const;
    double parameter(std::string_view name) const;
    void setParameters(const std::vector<std::pair<std::string, double>> &values);
};
} // namespace luma
