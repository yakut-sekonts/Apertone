#include "apps/desktop/EditorController.h"
#include "core/presets/SettingsSubset.h"

void EditorController::applySettings(const luma::EditDocument &settings, std::uint32_t groups) {
    if (!editable())
        return;
    try {
        const auto next = luma::mergeSettings(history_.current(), settings, groups);
        endEdit();
        if (next == history_.current())
            return;
        history_.apply(next);
        editsChanged();
    } catch (const std::exception &failure) {
        advancedFailure(failure);
    }
}
void EditorController::previewSettings(const luma::EditDocument &settings, std::uint32_t groups) {
    if (!editable() || original_)
        return;
    try {
        const auto next = luma::mergeSettings(history_.current(), settings, groups);
        if (presetPreview_ == next)
            return;
        endEdit();
        presetPreview_ = next;
        render();
        emit stateChanged();
    } catch (const std::exception &failure) {
        advancedFailure(failure);
    }
}
void EditorController::clearPreviewSettings() {
    if (!presetPreview_)
        return;
    presetPreview_.reset();
    render();
    emit stateChanged();
}
