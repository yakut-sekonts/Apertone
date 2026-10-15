#include "core/edits/LocalMasks.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <numbers>
#include <set>
#include <sstream>
#include <stdexcept>

namespace luma {
namespace {
void require(bool valid, const char *message) {
    if (!valid)
        throw std::runtime_error(message);
}
bool validName(std::string_view name) {
    if (name.empty() || name.size() > 320 || name.front() == ' ' || name.back() == ' ')
        return false;
    std::size_t count = 0;
    for (std::size_t i = 0; i < name.size();) {
        const auto first = static_cast<unsigned char>(name[i++]);
        unsigned value = first;
        int remaining = 0;
        unsigned minimum = 0;
        if (first >= 0xc2 && first <= 0xdf) {
            value = first & 31U;
            remaining = 1;
            minimum = 0x80;
        } else if (first >= 0xe0 && first <= 0xef) {
            value = first & 15U;
            remaining = 2;
            minimum = 0x800;
        } else if (first >= 0xf0 && first <= 0xf4) {
            value = first & 7U;
            remaining = 3;
            minimum = 0x10000;
        } else if (first >= 0x80)
            return false;
        while (remaining-- > 0) {
            if (i == name.size())
                return false;
            const auto next = static_cast<unsigned char>(name[i++]);
            if ((next & 0xc0U) != 0x80U)
                return false;
            value = (value << 6U) | (next & 63U);
        }
        if (value < minimum || value < 0x20 || (value >= 0x7f && value <= 0x9f) ||
            (value >= 0xd800 && value <= 0xdfff) || value > 0x10ffff || ++count > 80)
            return false;
    }
    return true;
}
void range(double value, double lower, double upper) {
    require(std::isfinite(value) && value >= lower && value <= upper,
            "Local mask parameter is out of range.");
}
std::size_t parameterIndex(std::string_view name) {
    const auto it = std::find(localParameters.begin(), localParameters.end(), name);
    require(it != localParameters.end(), "Unknown local adjustment parameter.");
    return static_cast<std::size_t>(it - localParameters.begin());
}
double smooth(double value) {
    value = std::clamp(value, 0.0, 1.0);
    return value * value * (3 - 2 * value);
}
} // namespace
void LocalMask::validate() const {
    require(id.size() == 36, "Invalid local mask ID.");
    for (std::size_t i = 0; i < id.size(); ++i) {
        const bool separator = i == 8 || i == 13 || i == 18 || i == 23;
        const char c = id[i];
        require(separator ? c == '-' : ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')),
                "Invalid local mask ID.");
    }
    require(validName(name), "Mask name must contain 1–80 printable characters.");
    require(shape == MaskShape::linear || shape == MaskShape::radial,
            "Unsupported local mask shape.");
    for (double value : {x1, y1, x2, y2})
        range(value, -2, 3);
    if (shape == MaskShape::linear)
        require(std::hypot(x2 - x1, y2 - y1) >= 0.001, "Gradient endpoints are too close.");
    else {
        range(x2, 0.001, 3);
        range(y2, 0.001, 3);
    }
    range(angle, -180, 180);
    range(feather, 0, 100);
    range(opacity, 0, 100);
    for (std::size_t i = 0; i < adjustments.size(); ++i)
        range(adjustments[i], i == 0 ? -5 : -100, i == 0 ? 5 : 100);
}
bool LocalMask::neutral() const {
    return !enabled || opacity == 0 ||
           std::all_of(adjustments.begin(), adjustments.end(), [](double v) { return v == 0; });
}
double LocalMask::parameter(std::string_view name) const {
    if (name == "feather")
        return feather;
    if (name == "opacity")
        return opacity;
    if (name == "angle")
        return angle;
    return adjustments[parameterIndex(name)];
}
void LocalMask::setParameter(std::string_view name, double value) {
    auto next = *this;
    if (name == "feather")
        next.feather = value;
    else if (name == "opacity")
        next.opacity = value;
    else if (name == "angle")
        next.angle = value;
    else
        next.adjustments[parameterIndex(name)] = value;
    next.validate();
    *this = std::move(next);
}
double LocalMask::coverage(double x, double y) const {
    if (!enabled || !std::isfinite(x) || !std::isfinite(y))
        return 0;
    double amount = 0;
    const double softness = feather / 100;
    if (shape == MaskShape::linear) {
        const double dx = x2 - x1, dy = y2 - y1;
        const double t = ((x - x1) * dx + (y - y1) * dy) / (dx * dx + dy * dy);
        amount = softness == 0 ? (t >= 0.5 ? 1 : 0) : smooth((t - (1 - softness) / 2) / softness);
    } else {
        const double radians = angle * std::numbers::pi / 180, c = std::cos(radians),
                     s = std::sin(radians);
        const double dx = x - x1, dy = y - y1;
        const double radius = std::hypot((c * dx + s * dy) / x2, (-s * dx + c * dy) / y2);
        amount = softness == 0 ? (radius <= 1 ? 1 : 0)
                               : 1 - smooth((radius - (1 - softness)) / softness);
    }
    return (inverted ? 1 - amount : amount) * opacity / 100;
}
void validateLocalMasks(const std::vector<LocalMask> &masks) {
    require(masks.size() <= 128, "A photo supports at most 128 local masks.");
    std::set<std::string> ids;
    for (const auto &mask : masks) {
        mask.validate();
        require(ids.insert(mask.id).second, "Duplicate local mask ID.");
    }
}
std::string encodeLocalMasks(const std::vector<LocalMask> &masks) {
    validateLocalMasks(masks);
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10);
    out << "LUMA_LOCAL_MASKS 1 " << masks.size() << '\n';
    for (const auto &mask : masks) {
        out << std::quoted(mask.id) << ' ' << std::quoted(mask.name) << ' '
            << static_cast<int>(mask.shape) << ' ' << mask.enabled << ' ' << mask.inverted;
        for (double value :
             {mask.x1, mask.y1, mask.x2, mask.y2, mask.angle, mask.feather, mask.opacity})
            out << ' ' << value;
        for (double value : mask.adjustments)
            out << ' ' << value;
        out << '\n';
    }
    auto result = out.str();
    require(result.size() <= 1048576, "Local mask document exceeds 1 MiB.");
    return result;
}
std::vector<LocalMask> decodeLocalMasks(std::string_view text) {
    require(!text.empty() && text.size() <= 1048576, "Invalid local mask document size.");
    std::istringstream in{std::string(text)};
    in.imbue(std::locale::classic());
    std::string magic;
    int version = 0, count = 0;
    in >> magic >> version >> count;
    require(in && magic == "LUMA_LOCAL_MASKS" && version == 1 && count >= 0 && count <= 128,
            "Unsupported local mask document.");
    std::vector<LocalMask> result;
    for (int i = 0; i < count; ++i) {
        LocalMask mask;
        int shape = 0, enabled = 0, inverted = 0;
        in >> std::quoted(mask.id) >> std::quoted(mask.name) >> shape >> enabled >> inverted;
        require((shape == 0 || shape == 1) && (enabled == 0 || enabled == 1) &&
                    (inverted == 0 || inverted == 1),
                "Invalid local mask flags.");
        mask.shape = static_cast<MaskShape>(shape);
        mask.enabled = enabled != 0;
        mask.inverted = inverted != 0;
        in >> mask.x1 >> mask.y1 >> mask.x2 >> mask.y2 >> mask.angle >> mask.feather >>
            mask.opacity;
        for (auto &value : mask.adjustments)
            in >> value;
        require(static_cast<bool>(in), "Incomplete local mask document.");
        result.push_back(std::move(mask));
    }
    require(encodeLocalMasks(result) == text, "Noncanonical or trailing local mask data.");
    return result;
}
} // namespace luma
