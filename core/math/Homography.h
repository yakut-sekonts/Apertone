#pragma once
#include <array>

namespace luma {
struct Homography {
    std::array<double, 9> values{1, 0, 0, 0, 1, 0, 0, 0, 1};
    void validate() const;
    Homography inverse() const;
    Homography operator*(const Homography &right) const;
    std::array<double, 3> homogeneous(double x, double y, double w = 1) const;
    std::array<double, 2> point(double x, double y) const;
};
} // namespace luma
