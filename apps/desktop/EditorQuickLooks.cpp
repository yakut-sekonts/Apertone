#include "apps/desktop/EditorController.h"
#include "core/catalog/Catalog.h"
#include "core/catalog/Identity.h"
#include "core/quick/MatchLook.h"
#include "core/quick/QuickLooks.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace {
auto layerByID(luma::EditDocument &document, const QString &id) {
    return std::find_if(document.creative.quickLooks.begin(), document.creative.quickLooks.end(),
                        [&](const auto &layer) { return layer.id == id.toStdString(); });
}
void updateAdaptiveMask(luma::EditDocument &document, const luma::QuickLookLayer &layer,
                        const luma::QuickLookRecipe &recipe) {
    if (layer.mask.empty())
        return;
    const auto mask = std::find_if(document.localMasks.begin(), document.localMasks.end(),
                                   [&](const auto &item) { return item.id == layer.mask; });
    if (mask == document.localMasks.end())
        throw std::runtime_error("The adaptive Quick Look mask is missing.");
    mask->adjustments = luma::quickLookLocalAdjustments(recipe, layer.amount);
    mask->blur = luma::quickLookBlur(recipe, layer.amount);
    mask->enabled = layer.enabled;
}
} // namespace

void EditorController::applyQuickLook(const QString &recipeID) {
    if (!editable() || original_)
        return;
    try {
        const auto &recipe = luma::quickLookRecipe(recipeID.toStdString());
        if (recipe.target != luma::QuickLookTarget::global) {
            const auto kind =
                recipe.target == luma::QuickLookTarget::subject      ? QStringLiteral("subject")
                : recipe.target == luma::QuickLookTarget::background ? QStringLiteral("background")
                                                                     : QStringLiteral("sky");
            generateAIMask(kind);
            if (aiBusy_)
                pendingQuickLook_ = recipeID;
            return;
        }
        auto next = history_.current();
        if (next.creative.quickLooks.size() >= 32)
            throw std::runtime_error("A photo supports at most 32 Quick Look layers.");
        next.creative.quickLooks.push_back(
            {luma::newID(), std::string(recipe.id), 100, true, {}, {}, {}});
        applyAdvanced(std::move(next));
    } catch (const std::exception &failure) {
        advancedFailure(failure);
    }
}
void EditorController::setQuickLookAmount(const QString &id, double amount) {
    if (!editable() || !std::isfinite(amount))
        return;
    try {
        auto next = history_.current();
        const auto layer = layerByID(next, id);
        if (layer == next.creative.quickLooks.end())
            throw std::runtime_error("Quick Look layer no longer exists.");
        layer->amount = std::clamp(amount, 0.0, 200.0);
        updateAdaptiveMask(next, *layer, luma::quickLookRecipe(layer->recipe));
        applyAdvanced(std::move(next));
    } catch (const std::exception &failure) {
        advancedFailure(failure);
    }
}
void EditorController::setQuickLookEnabled(const QString &id, bool enabled) {
    if (!editable())
        return;
    try {
        auto next = history_.current();
        const auto layer = layerByID(next, id);
        if (layer == next.creative.quickLooks.end())
            throw std::runtime_error("Quick Look layer no longer exists.");
        layer->enabled = enabled;
        updateAdaptiveMask(next, *layer, luma::quickLookRecipe(layer->recipe));
        applyAdvanced(std::move(next));
    } catch (const std::exception &failure) {
        advancedFailure(failure);
    }
}
void EditorController::removeQuickLook(const QString &id) {
    if (!editable())
        return;
    try {
        auto next = history_.current();
        const auto layer = layerByID(next, id);
        if (layer == next.creative.quickLooks.end())
            return;
        const auto mask = layer->mask;
        next.creative.quickLooks.erase(layer);
        if (!mask.empty())
            std::erase_if(next.localMasks, [&](const auto &item) { return item.id == mask; });
        applyAdvanced(std::move(next));
    } catch (const std::exception &failure) {
        advancedFailure(failure);
    }
}
void EditorController::moveQuickLook(const QString &id, int direction) {
    if (!editable() || (direction != -1 && direction != 1))
        return;
    try {
        auto next = history_.current();
        const auto layer = layerByID(next, id);
        if (layer == next.creative.quickLooks.end())
            return;
        const auto index = static_cast<std::ptrdiff_t>(layer - next.creative.quickLooks.begin());
        const auto destination = index + direction;
        if (destination < 0 ||
            destination >= static_cast<std::ptrdiff_t>(next.creative.quickLooks.size()))
            return;
        std::iter_swap(next.creative.quickLooks.begin() + index,
                       next.creative.quickLooks.begin() + destination);
        applyAdvanced(std::move(next));
    } catch (const std::exception &failure) {
        advancedFailure(failure);
    }
}
void EditorController::clearQuickLooks() {
    if (!editable())
        return;
    try {
        auto next = history_.current();
        std::set<std::string> masks;
        for (const auto &layer : next.creative.quickLooks)
            if (!layer.mask.empty())
                masks.insert(layer.mask);
        next.creative.quickLooks.clear();
        std::erase_if(next.localMasks, [&](const auto &mask) { return masks.contains(mask.id); });
        applyAdvanced(std::move(next));
    } catch (const std::exception &failure) {
        advancedFailure(failure);
    }
}

void EditorController::applyMatchLook(const QStringList &referenceIDs, double amount,
                                      bool includeExposure, bool includeWhiteBalance) {
    if (!editable() || original_ || !std::isfinite(amount))
        return;
    try {
        auto next = history_.current();
        if (next.creative.quickLooks.size() >= 32)
            throw std::runtime_error("A photo supports at most 32 Quick Look layers.");
        const auto activeID = photoID();
        std::vector<luma::EditDocument> references;
        std::set<std::string> unique;
        luma::Catalog catalog(catalog_);
        for (const auto &id : referenceIDs) {
            const auto value = id.toStdString();
            if (value.empty() || id == activeID || !unique.insert(value).second)
                continue;
            if (references.size() == 16)
                throw std::runtime_error("Match Look supports at most 16 reference photos.");
            const auto asset = catalog.assetByID(value);
            if (!asset)
                throw std::runtime_error("A selected Match Look reference no longer exists.");
            references.push_back(asset->edits);
        }
        if (references.empty())
            throw std::runtime_error(
                "Select at least one reference photo other than the current photo.");
        auto deltas = luma::matchLookDeltas(
            next, references,
            {.includeExposure = includeExposure, .includeWhiteBalance = includeWhiteBalance});
        const auto count = references.size();
        const auto name = count == 1 ? std::string("Match Look · 1 reference")
                                     : "Match Look · " + std::to_string(count) + " references";
        next.creative.quickLooks.push_back(
            {luma::newID(), "", std::clamp(amount, 0.0, 200.0), true, {}, name, std::move(deltas)});
        applyAdvanced(std::move(next));
    } catch (const std::exception &failure) {
        advancedFailure(failure);
    }
}
