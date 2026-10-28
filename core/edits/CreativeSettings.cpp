#include "core/edits/CreativeSettings.h"
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>

namespace luma {
namespace {
bool validID(std::string_view id) {
    if (id.size() != 36)
        return false;
    for (std::size_t i = 0; i < id.size(); ++i) {
        const bool separator = i == 8 || i == 13 || i == 18 || i == 23;
        const char c = id[i];
        if (separator ? c != '-' : !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return false;
    }
    return true;
}
bool validRecipe(std::string_view value) {
    if (value.empty() || value.size() > 80)
        return false;
    for (const char c : value)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
            return false;
    return true;
}
} // namespace
void QuickLookLayer::validate() const {
    if (!validID(id) || !validRecipe(recipe) || !std::isfinite(amount) || amount < 0 ||
        amount > 200 || (!mask.empty() && !validID(mask)))
        throw std::runtime_error("Invalid Quick Look layer.");
}
bool isCreativeParameter(std::string_view name) {
    return name.starts_with("presence.") || name.starts_with("point.") ||
           name.starts_with("grading.") || name.starts_with("vignette.") ||
           name.starts_with("grain.");
}
const CreativeParameter &creativeParameter(std::string_view name) {
    for (const auto &parameter : creativeParameters)
        if (parameter.name == name)
            return parameter;
    throw std::runtime_error("Unknown creative adjustment parameter.");
}
CreativeSettings::CreativeSettings() {
    for (std::size_t i = 0; i < values.size(); ++i)
        values[i] = creativeParameters[i].initial;
}
double CreativeSettings::parameter(std::string_view name) const {
    return values[static_cast<std::size_t>(&creativeParameter(name) - creativeParameters.data())];
}
void CreativeSettings::setParameter(std::string_view name, double value) {
    const auto &p = creativeParameter(name);
    if (!std::isfinite(value) || value < p.minimum || value > p.maximum)
        throw std::runtime_error("Creative adjustment is outside its supported range.");
    values[static_cast<std::size_t>(&p - creativeParameters.data())] = value;
}
void CreativeSettings::validate() const {
    if (enabled & ~creativeCorrectionMask)
        throw std::runtime_error("Invalid creative adjustment flags.");
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto &p = creativeParameters[i];
        if (!std::isfinite(values[i]) || values[i] < p.minimum || values[i] > p.maximum)
            throw std::runtime_error("Creative adjustment is outside its supported range.");
    }
    if (quickLooks.size() > 32)
        throw std::runtime_error("A photo supports at most 32 Quick Look layers.");
    std::set<std::string> ids;
    for (const auto &layer : quickLooks) {
        layer.validate();
        if (!ids.insert(layer.id).second)
            throw std::runtime_error("Duplicate Quick Look layer ID.");
    }
}
std::string encodeCreative(const CreativeSettings &settings) {
    settings.validate();
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "LUMA_CREATIVE 2 " << settings.enabled
        << std::setprecision(std::numeric_limits<double>::max_digits10);
    for (const auto value : settings.values)
        out << ' ' << value;
    out << ' ' << settings.quickLooks.size() << '\n';
    for (const auto &layer : settings.quickLooks)
        out << std::quoted(layer.id) << ' ' << std::quoted(layer.recipe) << ' ' << layer.amount
            << ' ' << layer.enabled << ' ' << std::quoted(layer.mask) << '\n';
    return out.str();
}
CreativeSettings decodeCreative(std::string_view text) {
    if (text.empty() || text.size() > 8192)
        throw std::runtime_error("Invalid creative adjustment document.");
    std::istringstream in{std::string(text)};
    in.imbue(std::locale::classic());
    std::string magic;
    int version = 0;
    CreativeSettings result;
    in >> magic >> version >> result.enabled;
    for (auto &value : result.values)
        in >> value;
    if (!in || magic != "LUMA_CREATIVE" || (version != 1 && version != 2))
        throw std::runtime_error("Invalid creative adjustment document.");
    if (version == 2) {
        std::int64_t count = -1;
        in >> count;
        if (!in || count < 0 || count > 32)
            throw std::runtime_error("Invalid creative adjustment document.");
        for (std::int64_t i = 0; i < count; ++i) {
            QuickLookLayer layer;
            int enabled = -1;
            in >> std::quoted(layer.id) >> std::quoted(layer.recipe) >> layer.amount >> enabled >>
                std::quoted(layer.mask);
            if (!in || (enabled != 0 && enabled != 1))
                throw std::runtime_error("Invalid creative adjustment document.");
            layer.enabled = enabled == 1;
            result.quickLooks.push_back(std::move(layer));
        }
    }
    result.validate();
    if (version == 1) {
        std::ostringstream legacy;
        legacy.imbue(std::locale::classic());
        legacy << "LUMA_CREATIVE 1 " << result.enabled
               << std::setprecision(std::numeric_limits<double>::max_digits10);
        for (const auto value : result.values)
            legacy << ' ' << value;
        legacy << '\n';
        if (legacy.str() != text)
            throw std::runtime_error("Invalid creative adjustment document.");
    } else if (encodeCreative(result) != text)
        throw std::runtime_error("Invalid creative adjustment document.");
    return result;
}
} // namespace luma
