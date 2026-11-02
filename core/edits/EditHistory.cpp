#include "core/edits/EditHistory.h"
#include <limits>
#include <set>
#include <stdexcept>

namespace luma {
EditHistory::EditHistory(PersistedEditHistory persisted) {
    if (persisted.entries.empty() || persisted.entries.size() > 201 ||
        persisted.currentIndex >= persisted.entries.size())
        throw std::runtime_error("Invalid stored processing history.");

    std::set<std::uint64_t> identities;
    for (const auto &entry : persisted.entries) {
        entry.document.validate();
        if (entry.action.size() > 256 || entry.action.find('\0') != std::string::npos ||
            !identities.insert(entry.id).second)
            throw std::runtime_error("Invalid stored processing history.");
        if (entry.id == std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("Stored processing history identity is exhausted.");
        nextID_ = std::max(nextID_, entry.id + 1);
    }

    const auto state = [&](std::size_t index) {
        const auto &entry = persisted.entries[index];
        std::vector<EditHistoryChange> changes;
        if (index)
            changes = describeEditChange(persisted.entries[index - 1].document, entry.document);
        return State{entry.document, entry.id, entry.action, std::move(changes)};
    };
    undo_.reserve(persisted.currentIndex);
    for (std::size_t index = 0; index < persisted.currentIndex; ++index)
        undo_.push_back(state(index));
    current_ = state(persisted.currentIndex);
    redo_.reserve(persisted.entries.size() - persisted.currentIndex - 1);
    for (std::size_t index = persisted.entries.size(); index > persisted.currentIndex + 1; --index)
        redo_.push_back(state(index - 1));
}

PersistedEditHistory EditHistory::persisted() const {
    PersistedEditHistory result;
    result.entries.reserve(undo_.size() + redo_.size() + 2);
    const auto append = [&](const State &state) {
        result.entries.push_back({state.id, state.action, state.document});
    };
    for (const auto &state : undo_)
        append(state);

    if (gesture_ && gesture_->document != current_.document) {
        append(*gesture_);
        result.entries.push_back({nextID_, {}, current_.document});
        if (result.entries.size() > 201)
            result.entries.erase(result.entries.begin());
        result.currentIndex = result.entries.size() - 1;
        return result;
    }

    append(current_);
    result.currentIndex = result.entries.size() - 1;
    for (auto it = redo_.rbegin(); it != redo_.rend(); ++it)
        append(*it);
    return result;
}

std::vector<EditHistoryChange> describeEditChange(const EditDocument &before,
                                                  const EditDocument &after) {
    static_assert(EditDocument::schemaVersion == 8,
                  "Update history descriptions for new edit fields.");
    std::vector<EditHistoryChange> changes;
    const auto group = [&](bool changed, const char *label, const std::string &subject = {}) {
        if (changed)
            changes.push_back({label, std::nullopt, subject});
    };
    const auto number = [&](double oldValue, double value, const char *label,
                            const std::string &subject = {}) {
        if (oldValue != value)
            changes.push_back({label, value, subject});
    };
    number(before.exposure, after.exposure, "Exposure");
    number(before.temperature, after.temperature, "Temperature");
    number(before.tint, after.tint, "Tint");
    group(before.customWhiteBalance != after.customWhiteBalance,
          after.customWhiteBalance ? "Custom white balance" : "As Shot");
    number(before.contrast, after.contrast, "Contrast");
    number(before.highlights, after.highlights, "Highlights");
    number(before.shadows, after.shadows, "Shadows");
    number(before.whites, after.whites, "Whites");
    number(before.blacks, after.blacks, "Blacks");
    number(before.vibrance, after.vibrance, "Vibrance");
    number(before.saturation, after.saturation, "Saturation");
    group(before.enabledCorrections != after.enabledCorrections, "Correction switches");
    number(before.sharpenAmount, after.sharpenAmount, "Sharpening amount");
    number(before.sharpenRadius, after.sharpenRadius, "Sharpening radius");
    number(before.sharpenDetail, after.sharpenDetail, "Sharpening detail");
    number(before.sharpenMasking, after.sharpenMasking, "Sharpening masking");
    number(before.noiseLuminance, after.noiseLuminance, "Luminance noise reduction");
    number(before.noiseLuminanceDetail, after.noiseLuminanceDetail, "Luminance noise detail");
    number(before.noiseLuminanceContrast, after.noiseLuminanceContrast, "Luminance noise contrast");
    number(before.noiseColor, after.noiseColor, "Color noise reduction");
    number(before.noiseColorDetail, after.noiseColorDetail, "Color noise detail");
    number(before.noiseColorSmoothness, after.noiseColorSmoothness, "Color noise smoothness");
    group(before.crop != after.crop, "Crop");
    group(before.lens != after.lens, "Lens corrections");
    group(before.transform != after.transform, "Transform");
    group(before.curves != after.curves || before.curveShadows != after.curveShadows ||
              before.curveDarks != after.curveDarks || before.curveLights != after.curveLights ||
              before.curveHighlights != after.curveHighlights,
          "Tone curves");
    group(before.hsl != after.hsl, "HSL / COLOR MIXER");
    group(before.retouch != after.retouch, "Retouch");
    group(before.creative.enabled != after.creative.enabled, "Correction switches");
    for (std::size_t i = 0; i < creativeParameters.size(); ++i)
        number(before.creative.values[i], after.creative.values[i],
               creativeParameters[i].label.data());
    for (const auto &mask : after.localMasks) {
        const auto previous = std::find_if(before.localMasks.begin(), before.localMasks.end(),
                                           [&](const auto &item) { return item.id == mask.id; });
        if (previous == before.localMasks.end()) {
            group(true, "Mask added", mask.name);
            continue;
        }
        if (*previous == mask)
            continue;
        constexpr std::array<const char *, 8> labels{"Exposure", "Contrast",  "Highlights",
                                                     "Shadows",  "Whites",    "Blacks",
                                                     "Vibrance", "Saturation"};
        for (std::size_t i = 0; i < labels.size(); ++i)
            number(previous->adjustments[i], mask.adjustments[i], labels[i], mask.name);
        number(previous->opacity, mask.opacity, "Opacity, %", mask.name);
        group(previous->name != mask.name || previous->enabled != mask.enabled ||
                  previous->inverted != mask.inverted || previous->components != mask.components ||
                  static_cast<const MaskGeometry &>(*previous) !=
                      static_cast<const MaskGeometry &>(mask),
              "Mask edited", mask.name);
    }
    for (const auto &mask : before.localMasks) {
        const bool removed = std::none_of(after.localMasks.begin(), after.localMasks.end(),
                                          [&](const auto &item) { return item.id == mask.id; });
        group(removed, "Mask removed", mask.name);
    }
    // Reordering masks is also a document change even if their contents match.
    if (changes.empty())
        group(true, "Processing settings");
    return changes;
}
} // namespace luma
