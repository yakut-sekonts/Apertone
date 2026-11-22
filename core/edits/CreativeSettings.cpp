#include "core/edits/CreativeSettings.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>

namespace luma {
namespace {
constexpr std::size_t legacyParameterCount = 34;
constexpr std::uint32_t legacyCorrectionMask = 4063232;
constexpr int creativeVersion = 7;
std::size_t parameterCount(int version) {
    return version < 5 ? legacyParameterCount : version == 5 ? 43 : creativeParameters.size();
}
std::uint32_t corrections(int version) {
    return version < 5    ? legacyCorrectionMask
           : version == 5 ? legacyCorrectionMask | 4194304U
                          : creativeCorrectionMask;
}
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
    if (value.size() > 80)
        return false;
    for (const char c : value)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
            return false;
    return true;
}
bool validText(std::string_view value) {
    if (value.empty() || value.size() > 128)
        return false;
    for (const unsigned char c : value)
        if (c < 0x20 || c == 0x7f)
            return false;
    return true;
}
bool validParameter(std::string_view value) {
    if (value.empty() || value.size() > 80)
        return false;
    for (const char c : value)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '.'))
            return false;
    return true;
}
bool validProvider(std::string_view value) {
    if (value.empty() || value.size() > 80)
        return false;
    for (const char c : value)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.'))
            return false;
    return true;
}
bool validDigest(std::string_view value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](const char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}
} // namespace
void QuickLookLayer::validate() const {
    const bool custom = recipe.empty();
    const bool generated = !provider.empty();
    if (!validID(id) || !validRecipe(recipe) || !std::isfinite(amount) || amount < 0 ||
        amount > 200 || (!mask.empty() && !validID(mask)) ||
        (custom && (!mask.empty() || !validText(name) || deltas.empty() ||
                    deltas.size() > maxQuickLookParameterDeltas)) ||
        (!custom && (!name.empty() || !deltas.empty() || generated)) ||
        (!generated && (!providerVersion.empty() || !model.empty() || !modelDigest.empty() ||
                        deterministicFallback)) ||
        (generated && (!custom || !validProvider(provider) || !validText(providerVersion) ||
                       (model.empty() != modelDigest.empty()) ||
                       (!model.empty() && (!validText(model) || !validDigest(modelDigest))) ||
                       (model.empty() != deterministicFallback))))
        throw std::runtime_error("Invalid Quick Look layer.");
    for (const auto &delta : deltas)
        if (!validParameter(delta.parameter) || !std::isfinite(delta.value) ||
            std::abs(delta.value) > 50000)
            throw std::runtime_error("Invalid Quick Look parameter delta.");
}
bool isCreativeParameter(std::string_view name) {
    return name.starts_with("presence.") || name.starts_with("point.") ||
           name.starts_with("grading.") || name.starts_with("vignette.") ||
           name.starts_with("grain.") || name.starts_with("bw.") || name.starts_with("defringe.") ||
           name.starts_with("calibration.");
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
    if ((!cameraProfile.digest.empty() && !validDigest(cameraProfile.digest)) ||
        (cameraProfile.profile && cameraProfile.digest.empty()))
        throw std::runtime_error("Invalid camera profile reference.");
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
namespace {
std::string encodeVersion(const CreativeSettings &settings, int version) {
    settings.validate();
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "LUMA_CREATIVE " << version << ' ' << (settings.enabled & corrections(version))
        << std::setprecision(std::numeric_limits<double>::max_digits10);
    for (std::size_t i = 0; i < parameterCount(version); ++i)
        out << ' ' << settings.values[i];
    if (version == 1) {
        out << '\n';
        return out.str();
    }
    out << ' ' << settings.quickLooks.size() << '\n';
    for (const auto &layer : settings.quickLooks) {
        out << std::quoted(layer.id) << ' ' << std::quoted(layer.recipe) << ' ' << layer.amount
            << ' ' << layer.enabled << ' ' << std::quoted(layer.mask);
        if (version >= 3) {
            out << ' ' << std::quoted(layer.name) << ' ' << layer.deltas.size();
            for (const auto &delta : layer.deltas)
                out << ' ' << std::quoted(delta.parameter) << ' ' << delta.value;
        }
        if (version >= 4)
            out << ' ' << std::quoted(layer.provider) << ' ' << std::quoted(layer.providerVersion)
                << ' ' << std::quoted(layer.model) << ' ' << std::quoted(layer.modelDigest) << ' '
                << layer.deterministicFallback;
        out << '\n';
    }
    if (version >= 7)
        out << std::quoted(settings.cameraProfile.digest) << '\n';
    return out.str();
}
} // namespace
std::string encodeCreative(const CreativeSettings &settings) {
    return encodeVersion(settings, settings.cameraProfile.digest.empty() ? 6 : creativeVersion);
}
CreativeSettings decodeCreative(std::string_view text) {
    if (text.empty() || text.size() > 262144)
        throw std::runtime_error("Invalid creative adjustment document.");
    std::istringstream in{std::string(text)};
    in.imbue(std::locale::classic());
    std::string magic;
    int version = 0;
    CreativeSettings result;
    in >> magic >> version >> result.enabled;
    if (!in || magic != "LUMA_CREATIVE" || (version < 1 || version > creativeVersion))
        throw std::runtime_error("Invalid creative adjustment document.");
    const auto accepted = corrections(version);
    if (result.enabled & ~accepted)
        throw std::runtime_error("Invalid creative adjustment flags.");
    result.enabled |= creativeCorrectionMask & ~accepted;
    for (std::size_t i = 0; i < parameterCount(version); ++i)
        in >> result.values[i];
    if (!in)
        throw std::runtime_error("Invalid creative adjustment document.");
    if (version >= 2) {
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
            if (version >= 3) {
                std::int64_t deltaCount = -1;
                in >> std::quoted(layer.name) >> deltaCount;
                if (!in || deltaCount < 0 ||
                    deltaCount > static_cast<std::int64_t>(maxQuickLookParameterDeltas))
                    throw std::runtime_error("Invalid creative adjustment document.");
                for (std::int64_t j = 0; j < deltaCount; ++j) {
                    QuickLookParameterDelta delta;
                    in >> std::quoted(delta.parameter) >> delta.value;
                    if (!in)
                        throw std::runtime_error("Invalid creative adjustment document.");
                    layer.deltas.push_back(std::move(delta));
                }
            }
            if (version >= 4) {
                int fallback = -1;
                in >> std::quoted(layer.provider) >> std::quoted(layer.providerVersion) >>
                    std::quoted(layer.model) >> std::quoted(layer.modelDigest) >> fallback;
                if (!in || (fallback != 0 && fallback != 1))
                    throw std::runtime_error("Invalid creative adjustment document.");
                layer.deterministicFallback = fallback == 1;
            }
            result.quickLooks.push_back(std::move(layer));
        }
    }
    if (version >= 7 && !(in >> std::quoted(result.cameraProfile.digest)))
        throw std::runtime_error("Invalid stored camera profile reference.");
    if (encodeVersion(result, version) != text)
        throw std::runtime_error("Invalid creative adjustment document.");
    return result;
}
} // namespace luma
