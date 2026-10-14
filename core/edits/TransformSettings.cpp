#include "core/edits/TransformSettings.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace luma {
namespace {
constexpr std::array<double TransformSettings::*, 7> members{
    &TransformSettings::rotate, &TransformSettings::horizontal, &TransformSettings::vertical,
    &TransformSettings::aspect, &TransformSettings::scale,      &TransformSettings::xOffset,
    &TransformSettings::yOffset};
std::size_t index(std::string_view name) {
    const auto found = std::find(transformParameters.begin(), transformParameters.end(), name);
    if (found == transformParameters.end())
        throw std::runtime_error("Unknown Transform parameter.");
    return static_cast<std::size_t>(found - transformParameters.begin());
}
} // namespace
Homography TransformSettings::guideMatrix() const {
    Homography matrix;
    std::copy(guide.begin(), guide.end(), matrix.values.begin());
    return matrix;
}
void TransformSettings::validate() const {
    for (std::size_t i = 0; i < members.size(); ++i) {
        const double value = this->*members[i];
        const double minimum = i == 0 ? -45 : i == 4 ? 50 : -100;
        const double maximum = i == 0 ? 45 : i == 4 ? 200 : 100;
        if (!std::isfinite(value) || value < minimum || value > maximum)
            throw std::runtime_error("Transform parameter outside its supported range.");
    }
    for (double value : guide)
        if (!std::isfinite(value) || std::abs(value) > 32)
            throw std::runtime_error("Invalid guided Transform matrix.");
    guideMatrix().validate();
}
double TransformSettings::parameter(std::string_view name) const {
    const auto i = index(name);
    if (i < members.size())
        return this->*members[i];
    if (i == 7)
        return constrainCrop;
    if (i == 8)
        return guided;
    return guide[i - 9];
}
void TransformSettings::setParameters(
    const std::vector<std::pair<std::string, double>> &parameters) {
    auto next = *this;
    for (const auto &[name, value] : parameters) {
        const auto i = index(name);
        if (i < members.size())
            next.*members[i] = value;
        else if (i == 7 || i == 8) {
            if (value != 0 && value != 1)
                throw std::runtime_error("Transform boolean must be 0 or 1.");
            (i == 7 ? next.constrainCrop : next.guided) = value != 0;
        } else
            next.guide[i - 9] = value;
    }
    next.validate();
    *this = next;
}
bool TransformSettings::neutral() const {
    return rotate == 0 && horizontal == 0 && vertical == 0 && aspect == 0 && scale == 100 &&
           xOffset == 0 && yOffset == 0 && (!guided || guideMatrix().values == Homography{}.values);
}
} // namespace luma
