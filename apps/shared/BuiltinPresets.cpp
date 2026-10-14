#include "apps/shared/BuiltinPresets.h"

namespace luma {
namespace {
struct Look {
    const char *id;
    const char *name;
    std::array<PointCurve, 4> curves;
    std::array<HSLBand, 8> hsl;
};
PointCurve curve(std::initializer_list<CurvePoint> points) {
    return {points};
}
} // namespace
bool builtinPresetID(const QString &id) {
    return id.startsWith(QStringLiteral("builtin:"));
}
const std::vector<StoredPreset> &builtinPresets() {
    static const std::vector<StoredPreset> presets = [] {
        // RGB / Red / Green / Blue point curves, then Red..Magenta HSL bands.
        // Values are visible/editable after application; no hidden look processor.
        const std::array<Look, 8> looks{{
            {"natural", "Natural", {}, {}},
            {"soft-portrait",
             "Soft Portrait",
             {curve({{0, 0.015}, {0.2, 0.22}, {0.5, 0.53}, {0.8, 0.81}, {1, 0.98}}), {}, {}, {}},
             {{{0, -8, 2}, {0, -6, 4}, {0, -10, 0}, {0, -12, 0}, {0, -8, 0}, {0, -8, 0}, {}, {}}}},
            {"landscape",
             "Landscape",
             {curve({{0, 0}, {0.2, 0.17}, {0.5, 0.52}, {0.8, 0.84}, {1, 1}}), {}, {}, {}},
             {{{0, 4, 0}, {0, 4, 0}, {-3, 8, 2}, {-4, 12, 0}, {0, 8, -2}, {-2, 12, -4}, {}, {}}}},
            {"warm-film",
             "Warm Film",
             {curve({{0, 0.025}, {0.2, 0.19}, {0.5, 0.51}, {0.8, 0.82}, {1, 0.98}}),
              curve({{0, 0}, {0.5, 0.52}, {1, 1}}),
              {},
              curve({{0, 0.015}, {0.5, 0.48}, {1, 0.96}})},
             {{{0, -4, 0},
               {0, -5, 2},
               {-4, -8, 0},
               {-6, -16, 0},
               {0, -10, 0},
               {0, -12, 2},
               {},
               {}}}},
            {"cool-cinema",
             "Cool Cinema",
             {curve({{0, 0.012}, {0.2, 0.17}, {0.5, 0.49}, {0.8, 0.83}, {1, 0.99}}),
              curve({{0, 0}, {0.25, 0.235}, {0.75, 0.76}, {1, 1}}),
              {},
              curve({{0, 0.025}, {0.3, 0.325}, {0.8, 0.79}, {1, 0.97}})},
             {{{0, -8, 0}, {0, -4, 2}, {0, -15, 0}, {6, -18, 0}, {0, -8, 0}, {4, -6, -2}, {}, {}}}},
            {"matte",
             "Matte",
             {curve({{0, 0.055}, {0.2, 0.23}, {0.5, 0.51}, {0.8, 0.79}, {1, 0.955}}), {}, {}, {}},
             {{{0, -10, 0},
               {0, -8, 0},
               {0, -12, 0},
               {0, -12, 0},
               {0, -10, 0},
               {0, -10, 0},
               {0, -10, 0},
               {0, -10, 0}}}},
            {"vintage",
             "Vintage",
             {curve({{0, 0.035}, {0.2, 0.23}, {0.5, 0.54}, {0.8, 0.8}, {1, 0.96}}),
              curve({{0, 0.01}, {0.5, 0.52}, {1, 1}}),
              {},
              curve({{0, 0.025}, {0.5, 0.47}, {1, 0.93}})},
             {{{0, -10, 0},
               {0, -8, 0},
               {-6, -10, 0},
               {-8, -22, 2},
               {0, -18, 0},
               {0, -18, 2},
               {0, -10, 0},
               {0, -10, 0}}}},
            {"crisp",
             "Crisp",
             {curve({{0, 0}, {0.2, 0.16}, {0.5, 0.5}, {0.8, 0.85}, {1, 1}}), {}, {}, {}},
             {{{0, 4, 0},
               {0, 2, 0},
               {0, 4, 0},
               {0, 5, 0},
               {0, 5, 0},
               {0, 5, 0},
               {0, 4, 0},
               {0, 4, 0}}}},
        }};
        std::vector<StoredPreset> result;
        for (const auto &look : looks) {
            DevelopPreset preset;
            preset.name = QString::fromUtf8(look.name);
            preset.folder = QStringLiteral("Luma");
            preset.groups = 4 | 8;
            preset.settings.curves = look.curves;
            preset.settings.hsl = look.hsl;
            preset.validate();
            result.push_back(
                {QStringLiteral("builtin:") + QString::fromLatin1(look.id), std::move(preset)});
        }
        return result;
    }();
    return presets;
}
} // namespace luma
