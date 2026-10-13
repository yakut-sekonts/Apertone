#include "apps/desktop/EditorController.h"
#include "core/lens/LensProfiles.h"
#include <algorithm>

QVariantMap EditorController::lensState() const {
    if (!session_)
        return {};
    const auto &settings = history_.current().lens;
    const auto resolved = luma::resolveLens(session_->asset.metadata, settings);
    return {{"profile", QString::fromStdString(settings.profile)},
            {"resolvedProfile",
             resolved.profile ? QString::fromUtf8(resolved.profile->name) : QStringLiteral("—")},
            {"camera",
             resolved.camera ? QString::fromUtf8(resolved.camera->name) : QStringLiteral("—")},
            {"status", QString::fromStdString(resolved.status)},
            {"distortionAvailable", resolved.distortion.has_value()},
            {"vignettingAvailable", resolved.vignetting.has_value()},
            {"caAvailable", resolved.tca.has_value()}};
}
QVariantList EditorController::lensOptions() const {
    QVariantList result{QVariantMap{{"id", ""}, {"name", "Auto · по метаданным"}}};
    if (!session_)
        return result;
    const auto resolved = luma::resolveLens(session_->asset.metadata, history_.current().lens);
    if (resolved.camera) {
        std::vector<const luma::LensProfile *> profiles;
        for (const auto &profile : luma::lensProfiles())
            if (luma::compatibleLens(*resolved.camera, profile))
                profiles.push_back(&profile);
        std::sort(profiles.begin(), profiles.end(), [](const auto *a, const auto *b) {
            return std::pair{a->name, a->id} < std::pair{b->name, b->id};
        });
        for (const auto *profile : profiles)
            result.append(
                QVariantMap{{"id", QString::fromUtf8(profile->id)},
                            {"name", QString::fromUtf8(profile->name) +
                                         QStringLiteral(" · crop %1").arg(profile->cropFactor)}});
    }
    const auto selected = QString::fromStdString(history_.current().lens.profile);
    if (!selected.isEmpty() && std::none_of(result.begin(), result.end(), [&](const auto &entry) {
            return entry.toMap().value("id").toString() == selected;
        }))
        result.append(QVariantMap{{"id", selected}, {"name", "Недоступный профиль · " + selected}});
    return result;
}
void EditorController::setLensProfile(const QString &id) {
    if (!editable())
        return;
    try {
        auto next = history_.current();
        next.lens.profile = id.toStdString();
        next.validate();
        // A saved preset may reference unavailable data, but manual selection
        // only accepts a profile actually listed for the current camera.
        if (!id.isEmpty()) {
            const auto resolved = luma::resolveLens(session_->asset.metadata, next.lens);
            if (!resolved.profile)
                throw std::runtime_error("Lens profile is unavailable for this camera.");
        }
        applyAdvanced(std::move(next));
    } catch (const std::exception &failure) {
        advancedFailure(failure);
    }
}
void EditorController::resetLens() {
    if (!editable())
        return;
    auto next = history_.current();
    next.lens = {};
    next.setEnabled(luma::Correction::lens, false);
    applyAdvanced(std::move(next));
}
