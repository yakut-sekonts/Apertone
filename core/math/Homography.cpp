#include "core/math/Homography.h"
#include <cmath>
#include <stdexcept>

namespace luma {
namespace {
double determinant(const std::array<double, 9> &m) {
    return m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) +
           m[2] * (m[3] * m[7] - m[4] * m[6]);
}
} // namespace
void Homography::validate() const {
    for (double value : values)
        if (!std::isfinite(value) || std::abs(value) > 1e6)
            throw std::runtime_error("Non-finite or excessive homography coefficient.");
    if (!std::isfinite(determinant(values)) || std::abs(determinant(values)) < 1e-10)
        throw std::runtime_error("Singular projective transform.");
}
Homography Homography::inverse() const {
    validate();
    const auto &m = values;
    Homography result{
        {m[4] * m[8] - m[5] * m[7], m[2] * m[7] - m[1] * m[8], m[1] * m[5] - m[2] * m[4],
         m[5] * m[6] - m[3] * m[8], m[0] * m[8] - m[2] * m[6], m[2] * m[3] - m[0] * m[5],
         m[3] * m[7] - m[4] * m[6], m[1] * m[6] - m[0] * m[7], m[0] * m[4] - m[1] * m[3]}};
    for (auto &value : result.values)
        value /= determinant(m);
    result.validate();
    return result;
}
Homography Homography::operator*(const Homography &right) const {
    Homography result{{0, 0, 0, 0, 0, 0, 0, 0, 0}};
    for (std::size_t row = 0; row < 3; ++row)
        for (std::size_t column = 0; column < 3; ++column)
            for (std::size_t i = 0; i < 3; ++i)
                result.values[row * 3 + column] +=
                    values[row * 3 + i] * right.values[i * 3 + column];
    result.validate();
    return result;
}
std::array<double, 3> Homography::homogeneous(double x, double y, double w) const {
    return {values[0] * x + values[1] * y + values[2] * w,
            values[3] * x + values[4] * y + values[5] * w,
            values[6] * x + values[7] * y + values[8] * w};
}
std::array<double, 2> Homography::point(double x, double y) const {
    const auto p = homogeneous(x, y);
    if (!std::isfinite(p[0]) || !std::isfinite(p[1]) || !std::isfinite(p[2]) ||
        std::abs(p[2]) < 1e-9)
        throw std::runtime_error("Projective lookup reaches a pole.");
    return {p[0] / p[2], p[1] / p[2]};
}
} // namespace luma
