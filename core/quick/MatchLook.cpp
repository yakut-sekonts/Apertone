#include "core/quick/MatchLook.h"
#include "core/quick/QuickLooks.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <numeric>
#include <stdexcept>

namespace luma {
namespace {
constexpr std::array baseParameters{
    std::string_view{"exposure"},      std::string_view{"temperature"},
    std::string_view{"tint"},          std::string_view{"contrast"},
    std::string_view{"highlights"},    std::string_view{"shadows"},
    std::string_view{"whites"},        std::string_view{"blacks"},
    std::string_view{"vibrance"},      std::string_view{"saturation"},
    std::string_view{"curve.shadows"}, std::string_view{"curve.darks"},
    std::string_view{"curve.lights"},  std::string_view{"curve.highlights"},
};

bool circular(std::string_view parameter) {
    return parameter.starts_with("grading.") && parameter.ends_with(".hue");
}
double circularMean(std::span<const double> values) {
    double x = 0, y = 0;
    for (const double value : values) {
        const double radians = value * std::numbers::pi / 180.0;
        x += std::cos(radians);
        y += std::sin(radians);
    }
    if (std::abs(x) < 1e-12 && std::abs(y) < 1e-12)
        return values.front();
    double result = std::atan2(y, x) * 180.0 / std::numbers::pi;
    return result < 0 ? result + 360 : result;
}
double circularDelta(double from, double to) {
    double result = std::fmod(to - from + 540.0, 360.0) - 180.0;
    if (result == -180)
        result = 180;
    return result;
}
std::vector<std::string_view> parameters(const MatchLookOptions &options) {
    std::vector<std::string_view> result;
    result.reserve(64);
    for (const auto parameter : baseParameters) {
        if (parameter == "exposure" && !options.includeExposure)
            continue;
        if ((parameter == "temperature" || parameter == "tint") && !options.includeWhiteBalance)
            continue;
        result.push_back(parameter);
    }
    static const std::vector<std::string> hslParameters = [] {
        std::vector<std::string> names;
        names.reserve(hslBands.size() * 3);
        for (const auto band : hslBands)
            for (const auto component : {"hue", "saturation", "luminance"})
                names.push_back("hsl." + std::string(band) + "." + component);
        return names;
    }();
    for (const auto &parameter : hslParameters)
        result.push_back(parameter);
    for (const auto &parameter : creativeParameters) {
        // Point Color describes a sampled color range in one specific photo;
        // copying it to another photo would not represent a reusable style.
        if (!parameter.name.starts_with("point."))
            result.push_back(parameter.name);
    }
    return result;
}
} // namespace

bool isMatchLookParameter(std::string_view parameter, const MatchLookOptions &options) {
    const auto allowed = parameters(options);
    return std::find(allowed.begin(), allowed.end(), parameter) != allowed.end();
}

std::vector<QuickLookParameterDelta> matchLookDeltas(const EditDocument &target,
                                                     std::span<const EditDocument> references,
                                                     const MatchLookOptions &options) {
    if (references.empty() || references.size() > 16)
        throw std::runtime_error("Match Look requires between 1 and 16 reference photos.");
    target.validate();
    const auto resolvedTarget = resolveQuickLooks(target);
    std::vector<EditDocument> resolvedReferences;
    resolvedReferences.reserve(references.size());
    for (const auto &reference : references) {
        reference.validate();
        resolvedReferences.push_back(resolveQuickLooks(reference));
    }

    std::vector<QuickLookParameterDelta> result;
    std::vector<double> values;
    values.reserve(references.size());
    for (const auto parameter : parameters(options)) {
        values.clear();
        for (const auto &reference : resolvedReferences)
            values.push_back(reference.parameter(parameter));
        const double targetValue = resolvedTarget.parameter(parameter);
        const double matched = circular(parameter)
                                   ? circularMean(values)
                                   : std::accumulate(values.begin(), values.end(), 0.0) /
                                         static_cast<double>(values.size());
        const double delta =
            circular(parameter) ? circularDelta(targetValue, matched) : matched - targetValue;
        if (std::abs(delta) > 1e-9)
            result.push_back({std::string(parameter), delta});
    }
    if (result.empty())
        throw std::runtime_error("Reference photos do not contain a distinct editable look.");
    return result;
}
} // namespace luma
