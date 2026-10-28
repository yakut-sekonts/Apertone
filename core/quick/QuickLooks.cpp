#include "core/quick/QuickLooks.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace luma {
namespace {
using T = QuickLookTarget;
QuickLookRecipe make(std::string_view id, std::string_view name, std::string_view category,
                     std::string_view description, std::initializer_list<QuickLookDelta> deltas) {
    return {id, name, category, description, T::global, deltas};
}
QuickLookRecipe adaptive(std::string_view id, std::string_view name, std::string_view category,
                         std::string_view description, T target, std::array<double, 8> local,
                         double blur = 0) {
    return {id, name, category, description, target, {}, local, blur};
}
double bounded(const EditDocument &document, std::string_view name, double delta) {
    const double value = document.parameter(name) + delta;
    if (name == "exposure")
        return std::clamp(value, -5.0, 5.0);
    if (name.starts_with("presence.") || name.starts_with("grading.") ||
        name.starts_with("vignette.") || name.starts_with("grain.")) {
        const auto &parameter = creativeParameter(name);
        return std::clamp(value, parameter.minimum, parameter.maximum);
    }
    return std::clamp(value, -100.0, 100.0);
}
const std::vector<QuickLookRecipe> &catalog() {
    static const std::vector<QuickLookRecipe> value{
        // Recommended / one-click foundations.
        make("clean-base", "Clean Base", "Recommended", "Balanced color and gentle contrast.",
             {{"contrast", 5}, {"highlights", -8}, {"shadows", 8}, {"vibrance", 5}}),
        make("quiet-tone", "Quiet Tone", "Recommended", "Subtle low-contrast finish.",
             {{"contrast", -8}, {"highlights", -8}, {"shadows", 10}, {"saturation", -4}}),
        make("bold-color", "Bold Color", "Recommended", "Stronger shape and color separation.",
             {{"contrast", 16}, {"whites", 8}, {"blacks", -10}, {"vibrance", 16}}),
        make("pure-mono", "Pure Mono", "Recommended", "Neutral monochrome foundation.",
             {{"saturation", -100}, {"contrast", 12}, {"blacks", -6}}),
        make("cool-air", "Cool Air", "Recommended", "Clean cool color grade.",
             {{"grading.global.hue", 218}, {"grading.global.saturation", 9}, {"vibrance", 5}}),
        make("warm-light", "Warm Light", "Recommended", "Warm highlights and open shadows.",
             {{"grading.highlights.hue", 42},
              {"grading.highlights.saturation", 16},
              {"shadows", 6}}),
        make("low-key", "Low Key", "Recommended", "Dense shadows and restrained color.",
             {{"exposure", -0.35}, {"contrast", 18}, {"blacks", -18}, {"saturation", -6}}),
        make("high-key", "High Key", "Recommended", "Bright, soft editorial finish.",
             {{"exposure", 0.35}, {"highlights", -12}, {"shadows", 15}, {"contrast", -5}}),
        make("wide-cinema", "Wide Cinema", "Recommended", "Cool shadows and warm highlights.",
             {{"contrast", 15},
              {"grading.shadows.hue", 205},
              {"grading.shadows.saturation", 18},
              {"grading.highlights.hue", 38},
              {"grading.highlights.saturation", 13},
              {"vignette.amount", -12}}),
        make("open-range", "Open Range", "Recommended", "HDR-like tonal recovery without halos.",
             {{"highlights", -35},
              {"shadows", 35},
              {"whites", 8},
              {"blacks", -8},
              {"presence.dehaze", 8}}),

        // Adaptive selections use Apertone's subject/sky segmentation.
        adaptive("subject-pop", "Subject Pop", "Adaptive", "Lift and separate the main subject.",
                 T::subject, {0.22, 12, -8, 10, 6, -5, 14, 4}),
        adaptive("subject-warm", "Subject Warm", "Adaptive", "Warm, gentle subject emphasis.",
                 T::subject, {0.12, -4, -10, 12, 4, 0, 10, 5}),
        adaptive("sky-clear", "Clear Sky", "Adaptive", "Restore sky color and highlight detail.",
                 T::sky, {-0.08, 10, -25, 5, 2, -5, 18, 10}),
        adaptive("sky-drama", "Dramatic Sky", "Adaptive", "Deep, graphic sky with stronger color.",
                 T::sky, {-0.28, 24, -38, 2, 4, -16, 28, 18}),
        adaptive("sky-sunset", "Sunset Sky", "Adaptive", "Open shadows and enrich sunset color.",
                 T::sky, {0.05, 8, -18, 14, 5, -4, 24, 14}),
        adaptive("background-soft", "Soft Background", "Adaptive",
                 "Gentle optical background blur.", T::background, {0, -4, 0, 0, 0, 0, -4, -2}, 38),
        adaptive("background-strong", "Strong Background", "Adaptive",
                 "Pronounced optical background blur.", T::background, {0, -8, 0, 0, 0, 0, -8, -4},
                 78),
        adaptive("background-orb", "Orb Background", "Adaptive",
                 "Creamy background separation for portraits.", T::background,
                 {0.06, -10, 0, 5, 0, 0, -10, -5}, 100),

        // Portrait families cover broad skin/lighting situations without pretending to isolate
        // teeth or eyes.
        adaptive("portrait-deep", "Deep Tone Portrait", "Portrait",
                 "Balanced portrait for deep skin tones.", T::subject,
                 {0.16, -5, -16, 18, 5, -2, 10, 3}),
        adaptive("portrait-medium", "Medium Tone Portrait", "Portrait",
                 "Natural mid-tone portrait finish.", T::subject, {0.13, -7, -14, 16, 4, -1, 8, 2}),
        adaptive("portrait-light", "Light Tone Portrait", "Portrait",
                 "Protect highlights in light skin tones.", T::subject,
                 {0.08, -9, -22, 13, 2, 0, 6, 1}),
        make("portrait-mono", "Portrait Mono", "Portrait", "Soft monochrome portrait rendering.",
             {{"saturation", -100},
              {"contrast", 6},
              {"highlights", -18},
              {"shadows", 16},
              {"presence.texture", -12},
              {"grain.amount", 12}}),
        adaptive("portrait-edge", "Edge Portrait", "Portrait", "Crisp editorial portrait emphasis.",
                 T::subject, {0.05, 20, -12, 8, 8, -12, 8, -3}),
        adaptive("portrait-group", "Group Balance", "Portrait", "Conservative lift for groups.",
                 T::subject, {0.1, -4, -18, 16, 4, 0, 6, 1}),

        make("spring-air", "Spring Air", "Seasons", "Fresh greens and a light tonal curve.",
             {{"exposure", 0.12},
              {"vibrance", 12},
              {"hsl.green.hue", -6},
              {"hsl.green.saturation", 14},
              {"hsl.yellow.luminance", 8}}),
        make("summer-sun", "Summer Sun", "Seasons", "Warm bright travel color.",
             {{"exposure", 0.16},
              {"contrast", 8},
              {"vibrance", 16},
              {"grading.highlights.hue", 48},
              {"grading.highlights.saturation", 12}}),
        make("autumn-ember", "Autumn Ember", "Seasons", "Amber foliage and muted greens.",
             {{"contrast", 10},
              {"hsl.orange.saturation", 18},
              {"hsl.yellow.hue", -18},
              {"hsl.green.saturation", -28},
              {"grading.highlights.hue", 36},
              {"grading.highlights.saturation", 10}}),
        make("winter-glass", "Winter Glass", "Seasons", "Cool, clean winter atmosphere.",
             {{"exposure", 0.08},
              {"highlights", -18},
              {"saturation", -8},
              {"grading.global.hue", 214},
              {"grading.global.saturation", 8}}),

        // Style and film-inspired families.
        make("mono-soft", "Soft B&W", "Style", "Open-shadow monochrome.",
             {{"saturation", -100}, {"contrast", -5}, {"shadows", 18}, {"grain.amount", 8}}),
        make("mono-hard", "Hard B&W", "Style", "Punchy monochrome with firm blacks.",
             {{"saturation", -100}, {"contrast", 28}, {"blacks", -20}, {"grain.amount", 18}}),
        make("mono-matte", "Matte B&W", "Style", "Muted monochrome film finish.",
             {{"saturation", -100}, {"contrast", 5}, {"blacks", 14}, {"grain.amount", 28}}),
        make("cinema-amber", "Amber Cinema", "Style", "Amber highlights and cool shadows.",
             {{"contrast", 18},
              {"grading.highlights.hue", 36},
              {"grading.highlights.saturation", 20},
              {"grading.shadows.hue", 210},
              {"grading.shadows.saturation", 16},
              {"vignette.amount", -15}}),
        make("cinema-steel", "Steel Cinema", "Style", "Low-saturation steel-blue cinema grade.",
             {{"contrast", 17},
              {"saturation", -14},
              {"grading.shadows.hue", 215},
              {"grading.shadows.saturation", 22},
              {"vignette.amount", -12}}),
        make("cinema-neon", "Neon Cinema", "Style", "Colorful night cinema treatment.",
             {{"contrast", 20},
              {"blacks", -12},
              {"vibrance", 22},
              {"grading.shadows.hue", 250},
              {"grading.shadows.saturation", 20}}),
        make("cinema-fade", "Faded Cinema", "Style", "Lifted blacks and restrained cinema color.",
             {{"contrast", 7}, {"blacks", 16}, {"saturation", -10}, {"grain.amount", 15}}),
        make("film-warm-gold", "Warm Gold", "Film", "Warm gold film response.",
             {{"contrast", 7},
              {"grading.highlights.hue", 44},
              {"grading.highlights.saturation", 17},
              {"grain.amount", 20}}),
        make("film-rich-gold", "Rich Gold", "Film", "Dense gold color with deep blacks.",
             {{"contrast", 18},
              {"blacks", -12},
              {"grading.highlights.hue", 40},
              {"grading.highlights.saturation", 22},
              {"grain.amount", 25}}),
        make("film-warm-mono", "Warm B&W", "Film", "Warm silver monochrome.",
             {{"saturation", -100},
              {"grading.global.hue", 42},
              {"grading.global.saturation", 8},
              {"grain.amount", 24}}),
        make("film-soft-ember", "Soft Ember", "Film", "Soft red-gold film palette.",
             {{"contrast", -3},
              {"blacks", 8},
              {"grading.highlights.hue", 28},
              {"grading.highlights.saturation", 14},
              {"grain.amount", 22}}),
        make("film-strong-ember", "Strong Ember", "Film", "Strong red-gold film palette.",
             {{"contrast", 20},
              {"blacks", -10},
              {"grading.highlights.hue", 24},
              {"grading.highlights.saturation", 24},
              {"grain.amount", 30}}),
        make("film-muted-mono", "Muted B&W", "Film", "Low-contrast archival monochrome.",
             {{"saturation", -100}, {"contrast", -8}, {"blacks", 12}, {"grain.amount", 30}}),
        make("film-faded-blue", "Faded Blue", "Film", "Lifted blue shadows and faded color.",
             {{"contrast", -2},
              {"blacks", 14},
              {"saturation", -10},
              {"grading.shadows.hue", 220},
              {"grading.shadows.saturation", 16},
              {"grain.amount", 20}}),
        make("film-deep-blue", "Deep Blue", "Film", "Dense blue-shadow film stock.",
             {{"contrast", 18},
              {"blacks", -16},
              {"grading.shadows.hue", 218},
              {"grading.shadows.saturation", 25},
              {"grain.amount", 26}}),
        make("film-cool-mono", "Cool B&W", "Film", "Cool selenium monochrome.",
             {{"saturation", -100},
              {"grading.global.hue", 215},
              {"grading.global.saturation", 8},
              {"grain.amount", 25}}),
        make("film-light-sage", "Light Sage", "Film", "Pale green editorial film palette.",
             {{"exposure", 0.1},
              {"contrast", -4},
              {"saturation", -10},
              {"grading.global.hue", 105},
              {"grading.global.saturation", 7},
              {"grain.amount", 18}}),
        make("film-deep-sage", "Deep Sage", "Film", "Dense muted green film palette.",
             {{"contrast", 14},
              {"blacks", -12},
              {"saturation", -14},
              {"grading.global.hue", 112},
              {"grading.global.saturation", 10},
              {"grain.amount", 25}}),
        make("film-bold-mono", "Bold B&W", "Film", "Bold grainy monochrome.",
             {{"saturation", -100},
              {"contrast", 25},
              {"blacks", -18},
              {"grain.amount", 38},
              {"grain.size", 12}}),
        make("future-cyan", "Future Cyan", "Style", "Clean cyan futuristic grade.",
             {{"contrast", 16},
              {"vibrance", 14},
              {"grading.shadows.hue", 190},
              {"grading.shadows.saturation", 24}}),
        make("future-violet", "Future Violet", "Style", "Violet neon futuristic grade.",
             {{"contrast", 18},
              {"vibrance", 18},
              {"grading.shadows.hue", 270},
              {"grading.shadows.saturation", 22}}),
        make("vintage-paper", "Paper Vintage", "Style", "Warm faded print.",
             {{"contrast", -8},
              {"blacks", 16},
              {"saturation", -18},
              {"grading.global.hue", 42},
              {"grading.global.saturation", 8},
              {"grain.amount", 28}}),
        make("vintage-slide", "Slide Vintage", "Style", "Saturated slide-film color.",
             {{"contrast", 16}, {"vibrance", 18}, {"blacks", -8}, {"grain.amount", 18}}),
        make("vintage-dust", "Dust Vintage", "Style", "Low-contrast aged color.",
             {{"contrast", -10},
              {"blacks", 18},
              {"saturation", -24},
              {"vignette.amount", -18},
              {"grain.amount", 40}}),
        make("vintage-night", "Night Vintage", "Style", "Dense faded night color.",
             {{"exposure", -0.2},
              {"contrast", 10},
              {"blacks", 10},
              {"saturation", -15},
              {"grading.shadows.hue", 215},
              {"grading.shadows.saturation", 12},
              {"grain.amount", 32}}),

        // Subject and scene families.
        make("scene-concert", "Concert", "Scenes", "Deep blacks and vivid stage light.",
             {{"contrast", 24},
              {"blacks", -22},
              {"highlights", -18},
              {"vibrance", 24},
              {"presence.dehaze", 12}}),
        make("scene-food", "Food", "Scenes", "Warm appetizing color and texture.",
             {{"exposure", 0.12},
              {"contrast", 8},
              {"vibrance", 14},
              {"presence.texture", 12},
              {"grading.highlights.hue", 42},
              {"grading.highlights.saturation", 8}}),
        make("scene-landscape", "Landscape Depth", "Scenes",
             "Depth, recovered sky and foliage color.",
             {{"highlights", -30},
              {"shadows", 22},
              {"vibrance", 18},
              {"presence.dehaze", 16},
              {"presence.clarity", 10}}),
        make("scene-lifestyle", "Lifestyle", "Scenes", "Open, warm everyday color.",
             {{"exposure", 0.1},
              {"contrast", -2},
              {"shadows", 12},
              {"vibrance", 10},
              {"grading.highlights.hue", 44},
              {"grading.highlights.saturation", 7}}),
        make("scene-travel", "Travel Color", "Travel", "Vivid travel documentary color.",
             {{"contrast", 12},
              {"highlights", -18},
              {"shadows", 15},
              {"vibrance", 22},
              {"presence.dehaze", 8}}),
        make("scene-travel-coast", "Coastal Travel", "Travel", "Airy blue and sand travel palette.",
             {{"exposure", 0.12},
              {"highlights", -16},
              {"vibrance", 14},
              {"hsl.blue.luminance", 12},
              {"hsl.orange.luminance", 8}}),
        make("scene-travel-night", "Night Travel", "Travel", "Clear city-night travel color.",
             {{"contrast", 18},
              {"highlights", -28},
              {"blacks", -14},
              {"vibrance", 18},
              {"presence.dehaze", 10}}),
        make("scene-architecture", "Urban Architecture", "Scenes",
             "Crisp surfaces and firm geometry cues.",
             {{"contrast", 18},
              {"highlights", -22},
              {"shadows", 14},
              {"presence.texture", 18},
              {"presence.clarity", 14},
              {"saturation", -5}}),
        adaptive("scene-people", "People in Scene", "Scenes",
                 "Lift people while keeping the environment intact.", T::subject,
                 {0.14, -3, -15, 15, 4, -2, 8, 2}),
        adaptive("scene-background", "Background Depth", "Scenes",
                 "Add depth and color to the environment.", T::background,
                 {-0.04, 8, -12, 10, 4, -6, 12, 6}),
        make("scene-mountains", "Mountain Range", "Scenes",
             "Recover distant ridges and atmospheric depth.",
             {{"highlights", -24},
              {"shadows", 18},
              {"presence.dehaze", 20},
              {"presence.clarity", 12},
              {"vibrance", 12}}),
        make("scene-water", "Open Water", "Scenes", "Clear blue water with controlled glare.",
             {{"highlights", -28},
              {"vibrance", 16},
              {"hsl.aqua.saturation", 18},
              {"hsl.blue.saturation", 14},
              {"hsl.blue.luminance", 8}}),
        make("scene-snow", "Snow Detail", "Scenes", "Protect snow texture and keep clean whites.",
             {{"exposure", 0.1},
              {"highlights", -35},
              {"whites", 12},
              {"shadows", 12},
              {"grading.global.hue", 215},
              {"grading.global.saturation", 4}}),

        // Auto+ alternatives: compact foundations designed to accept more layers.
        make("auto-classic", "Auto+ Classic", "Auto+", "Classic balanced starting point.",
             {{"contrast", 8}, {"highlights", -15}, {"shadows", 15}, {"vibrance", 8}}),
        make("auto-retro-warm", "Auto+ Warm Retro", "Auto+", "Warm retro starting point.",
             {{"contrast", 4},
              {"blacks", 10},
              {"saturation", -7},
              {"grading.highlights.hue", 40},
              {"grading.highlights.saturation", 10}}),
        make("auto-retro-cool", "Auto+ Cool Retro", "Auto+", "Cool retro starting point.",
             {{"contrast", 7},
              {"blacks", 8},
              {"saturation", -10},
              {"grading.shadows.hue", 215},
              {"grading.shadows.saturation", 12}}),
        make("auto-retro-mono", "Auto+ Retro B&W", "Auto+", "Grainy monochrome starting point.",
             {{"saturation", -100}, {"contrast", 12}, {"blacks", 5}, {"grain.amount", 24}}),
    };
    return value;
}
} // namespace

std::span<const QuickLookRecipe> quickLookRecipes() {
    return catalog();
}
const QuickLookRecipe &quickLookRecipe(std::string_view id) {
    const auto &recipes = catalog();
    const auto found = std::find_if(recipes.begin(), recipes.end(),
                                    [&](const auto &recipe) { return recipe.id == id; });
    if (found == recipes.end())
        throw std::runtime_error("Unknown Quick Look recipe.");
    return *found;
}
EditDocument resolveQuickLooks(const EditDocument &source) {
    auto result = source;
    const auto layers = result.creative.quickLooks;
    result.creative.quickLooks.clear();
    for (const auto &layer : layers) {
        if (!layer.enabled || !layer.mask.empty() || layer.amount == 0)
            continue;
        const auto &recipe = quickLookRecipe(layer.recipe);
        const double scale = layer.amount / 100;
        for (const auto &[parameter, value] : recipe.deltas)
            result.setParameter(parameter, bounded(result, parameter, value * scale));
    }
    result.validate();
    return result;
}
std::array<double, 8> quickLookLocalAdjustments(const QuickLookRecipe &recipe, double amount) {
    auto result = recipe.local;
    for (auto &value : result)
        value *= amount / 100;
    return result;
}
double quickLookBlur(const QuickLookRecipe &recipe, double amount) {
    return std::clamp(recipe.blur * amount / 100, 0.0, 100.0);
}
} // namespace luma
