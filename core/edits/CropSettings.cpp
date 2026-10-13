#include "core/edits/CropSettings.h"
#include <array>
#include <cmath>
#include <stdexcept>

namespace luma {
namespace {
constexpr std::array fields{
    std::pair{"crop.x", &CropSettings::x},         std::pair{"crop.y", &CropSettings::y},
    std::pair{"crop.width", &CropSettings::width}, std::pair{"crop.height", &CropSettings::height},
    std::pair{"crop.angle", &CropSettings::angle}, std::pair{"crop.aspect", &CropSettings::aspect}};
}
void CropSettings::validate() const {
    for (const auto &[name, member] : fields) {
        if (!std::isfinite(this->*member))
            throw std::runtime_error(std::string("Non-finite ") + name);
    }
    if (x < 0 || y < 0 || width < 1.0 / 20000 || height < 1.0 / 20000 || x + width > 1 + 1e-12 ||
        y + height > 1 + 1e-12 || angle < -45 || angle > 45 || quarterTurns < 0 ||
        quarterTurns > 3 || !(aspect == -1 || aspect == 0 || (aspect >= 0.01 && aspect <= 100)))
        throw std::runtime_error("Invalid crop bounds, angle, rotation or aspect.");
}
double CropSettings::parameter(std::string_view name) const {
    for (const auto &[key, member] : fields)
        if (name == key)
            return this->*member;
    if (name == "crop.quarterTurns")
        return quarterTurns;
    if (name == "crop.flipHorizontal")
        return flipHorizontal ? 1 : 0;
    if (name == "crop.flipVertical")
        return flipVertical ? 1 : 0;
    throw std::runtime_error("Unknown crop parameter.");
}
void CropSettings::setParameters(const std::vector<std::pair<std::string, double>> &values) {
    auto next = *this;
    for (const auto &[name, value] : values) {
        if (!std::isfinite(value))
            throw std::runtime_error("Non-finite crop parameter.");
        bool assigned = false;
        for (const auto &[key, member] : fields)
            if (name == key) {
                next.*member = value;
                assigned = true;
                break;
            }
        if (assigned)
            continue;
        if (name == "crop.quarterTurns") {
            if (value < 0 || value > 3 || std::floor(value) != value)
                throw std::runtime_error("Crop quarter turns must be an integer in 0–3.");
            next.quarterTurns = static_cast<int>(value);
        } else if (name == "crop.flipHorizontal" || name == "crop.flipVertical") {
            if (value != 0 && value != 1)
                throw std::runtime_error("Crop flip must be 0 or 1.");
            if (name == "crop.flipHorizontal")
                next.flipHorizontal = value == 1;
            else
                next.flipVertical = value == 1;
        } else
            throw std::runtime_error("Unknown crop parameter.");
    }
    next.validate();
    *this = next;
}
} // namespace luma
