#include "core/catalog/Identity.h"
#include "core/edits/CreativeSettings.h"
#include "core/edits/LocalMasks.h"
#include "core/image/Bayer.h"
#include "core/quick/QuickLooks.h"
#include "core/render/CpuRenderer.h"
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void catalogChecks() {
    const auto recipes = luma::quickLookRecipes();
    require(recipes.size() >= 70, "Quick Look catalog is incomplete.");
    std::set<std::string_view> ids, categories;
    bool subject = false, background = false, sky = false, blur = false;
    for (const auto &recipe : recipes) {
        require(ids.insert(recipe.id).second, "Duplicate Quick Look recipe ID.");
        categories.insert(recipe.category);
        subject |= recipe.target == luma::QuickLookTarget::subject;
        background |= recipe.target == luma::QuickLookTarget::background;
        sky |= recipe.target == luma::QuickLookTarget::sky;
        blur |= recipe.blur > 0;
    }
    require(categories.size() >= 8 && subject && background && sky && blur,
            "Quick Look category or adaptive coverage is incomplete.");
}
void stackChecks() {
    luma::EditDocument source;
    source.creative.quickLooks = {
        {luma::newID(), "wide-cinema", 75, true, {}},
        {luma::newID(), "scene-travel", 125, true, {}},
        {luma::newID(), "pure-mono", 0, true, {}},
    };
    source.validate();
    const auto resolved = luma::resolveQuickLooks(source);
    require(resolved.creative.quickLooks.empty(), "Resolved document retained Quick Look layers.");
    require(resolved.contrast != source.contrast && resolved.vibrance != source.vibrance,
            "Stacked Quick Looks did not affect the document.");
    const auto encoded = luma::encodeCreative(source.creative);
    require(luma::decodeCreative(encoded) == source.creative,
            "Quick Look stack did not survive the creative codec.");
    const auto legacy = luma::encodeCreative(luma::CreativeSettings{});
    require(legacy.starts_with("LUMA_CREATIVE 2 "), "Creative codec was not upgraded.");
}
void localBlurChecks() {
    luma::FloatImage image(48, 24);
    for (int y = 0; y < image.height; ++y)
        for (int x = 0; x < image.width; ++x) {
            const float value = ((x / 2) % 2) ? 1.F : 0.F;
            const auto offset = (static_cast<std::size_t>(y) * image.width + x) * 3;
            image.rgb[offset] = image.rgb[offset + 1] = image.rgb[offset + 2] = value;
        }
    luma::EditDocument edit;
    luma::LocalMask mask;
    mask.id = luma::newID();
    mask.name = "Background blur";
    mask.shape = luma::MaskShape::bitmap;
    mask.bitmap = luma::BitmapMask(2, 1, {0, 255});
    mask.blur = 100;
    edit.localMasks.push_back(mask);
    edit.validate();
    const auto decoded = luma::decodeLocalMasks(luma::encodeLocalMasks(edit.localMasks));
    require(decoded == edit.localMasks && decoded.front().blur == 100,
            "Local blur did not survive the mask codec.");
    const auto plain = luma::renderCPU(image, {}), blurred = luma::renderCPU(image, edit);
    require(plain.rgba != blurred.rgba, "Local blur did not affect rendered pixels.");
    require(std::equal(plain.rgba.begin(), plain.rgba.begin() + 4 * 4, blurred.rgba.begin()),
            "Local blur changed pixels outside the selection.");
}
} // namespace
int main() {
    try {
        catalogChecks();
        stackChecks();
        localBlurChecks();
        std::cout << "Quick Look catalog, stack codec and adaptive blur passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
