#include "apps/desktop/EditorController.h"
#include "core/lens/LensProfiles.h"
#include <QStringList>
#include <algorithm>

namespace {
QString lensDisplayName(const luma::LensProfile &profile) {
    auto name = QString::fromUtf8(profile.name);
    if (!profile.russianName.empty() && profile.russianName != profile.name)
        name += QStringLiteral(" / ") + QString::fromUtf8(profile.russianName);
    return name;
}
} // namespace

QVariantMap EditorController::lensState() const {
    if (!session_)
        return {};
    const auto &settings = history_.current().lens;
    const auto resolved = luma::resolveLens(session_->asset.metadata, settings);
    const auto translate = [](const char *value) {
        return QCoreApplication::translate("Apertone", value);
    };
    auto status = QCoreApplication::translate("Apertone", resolved.status.c_str());
    if (resolved.status.starts_with("Lensfun · ")) {
        status = QStringLiteral("Lensfun · ") + translate("Distortion") +
                 (resolved.distortion ? QStringLiteral(" ✓")
                                      : QStringLiteral(": ") + translate("No calibration")) +
                 QStringLiteral(" · CA") +
                 (resolved.tca ? QStringLiteral(" ✓")
                               : QStringLiteral(": ") + translate("No calibration")) +
                 QStringLiteral(" · ") + translate("Vignetting") +
                 (resolved.vignetting ? QStringLiteral(" ✓ · ") + translate("Distant focus")
                                      : QStringLiteral(": ") + translate("No calibration"));
    }
    return {{"profile", QString::fromStdString(settings.profile)},
            {"resolvedProfile",
             resolved.profile ? lensDisplayName(*resolved.profile) : QStringLiteral("—")},
            {"camera",
             resolved.camera ? QString::fromUtf8(resolved.camera->name) : QStringLiteral("—")},
            {"status", status},
            {"distortionAvailable", resolved.distortion.has_value()},
            {"vignettingAvailable", resolved.vignetting.has_value()},
            {"caAvailable", resolved.tca.has_value()}};
}
QVariantList EditorController::lensOptions() const {
    if (!session_)
        return {QVariantMap{{"id", ""}, {"name", "Auto · from metadata"}}};
    const auto &metadata = session_->asset.metadata;
    const auto &settings = history_.current().lens;
    if (lensOptionsCache_ && lensOptionsCache_->camera == metadata.camera &&
        lensOptionsCache_->profile == settings.profile)
        return lensOptionsCache_->options;

    // QML sequence indexing can read the whole property for each element.
    QVariantList result{QVariantMap{{"id", ""}, {"name", "Auto · from metadata"}}};
    const auto resolved = luma::resolveLens(metadata, settings);
    if (resolved.camera) {
        std::vector<const luma::LensProfile *> profiles;
        for (const auto &profile : luma::lensProfiles())
            if (luma::compatibleLens(*resolved.camera, profile))
                profiles.push_back(&profile);
        std::sort(profiles.begin(), profiles.end(), [](const auto *a, const auto *b) {
            return std::pair{a->name, a->id} < std::pair{b->name, b->id};
        });
        for (const auto *profile : profiles) {
            const auto name =
                lensDisplayName(*profile) + QStringLiteral(" · crop %1").arg(profile->cropFactor);
            QStringList labels{name};
            for (const auto alias : profile->aliases)
                labels.append(QString::fromUtf8(alias));
            result.append(QVariantMap{{"id", QString::fromUtf8(profile->id)},
                                      {"name", name},
                                      {"searchText", labels.join(QLatin1Char('\n')).toLower()}});
        }
    }
    const auto selected = QString::fromStdString(settings.profile);
    if (!selected.isEmpty() && std::none_of(result.begin(), result.end(), [&](const auto &entry) {
            return entry.toMap().value("id").toString() == selected;
        }))
        result.append(QVariantMap{{"id", selected}, {"name", "Unavailable profile · " + selected}});
    lensOptionsCache_ = LensOptionsCache{metadata.camera, settings.profile, result};
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
