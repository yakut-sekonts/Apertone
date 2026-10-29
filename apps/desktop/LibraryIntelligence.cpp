#include "apps/desktop/LibraryModel.h"
#include "apps/shared/PhotoDecoder.h"
#include "core/image/Bayer.h"
#include "core/render/CpuRenderer.h"
#include "platform/macos/FileHasher.h"
#include "platform/macos/PhotoCulling.h"
#include <QPointer>
#include <QSettings>
#include <algorithm>

void LibraryModel::cancelCulling() {
    cullingJob_.cancel();
    ++cullingRevision_;
    cullingBusy_ = false;
    emit stateChanged();
}

void LibraryModel::analyzeLibrary() {
    const auto selected = selectionIDs();
    startCulling(selected, !selected.empty());
}

void LibraryModel::analyzePendingLibrary() {
    startCulling({}, false);
}

void LibraryModel::startCulling(const std::vector<std::string> &selected, bool selectionScope) {
    if (working())
        return;
    cancelCulling();
    cullingBusy_ = true;
    cullingProgress_ = 0;
    cullingError_.clear();
    cullingStatus_ = selectionScope ? QStringLiteral("Analyzing selection locally…")
                                    : QStringLiteral("Analyzing library locally…");
    const auto revision = ++cullingRevision_;
    const auto path = catalog_;
    const QPointer<LibraryModel> self(this);
    emit stateChanged();
    try {
        cullingJob_ = cullingJobs_.submit(
            luma::JobPriority::background,
            [self, path, selected, revision](const luma::JobContext &context) {
                QString error;
                std::size_t analyzed = 0, failed = 0, total = 0;
                try {
                    const auto stop = context.stopToken();
                    const auto provider = luma::macos::makePhotoCulling();
                    if (provider->location() != luma::ai::ExecutionLocation::local)
                        throw std::runtime_error("Intelligent Library must run locally.");
                    luma::Catalog catalog(path);
                    if (const auto existing = catalog.cullingProvider();
                        existing && *existing != provider->identifier())
                        throw std::runtime_error(
                            "Local analysis model changed. Clear analysis before rebuilding it.");
                    const auto ids = catalog.pendingCullingImages(provider->identifier(), selected);
                    total = ids.size();
                    for (const auto &id : ids) {
                        luma::checkCancellation(stop);
                        try {
                            const auto asset = catalog.assetByID(id);
                            if (!asset)
                                continue;
                            if (luma::macos::sha256File(asset->path, stop) != asset->checksum)
                                throw std::runtime_error(
                                    "Original changed; local analysis skipped.");
                            const auto completed = analyzed + failed;
                            auto decoded = luma::PhotoDecoder().decode(asset->path, stop);
                            const auto preview = luma::standardPreview(decoded.image, 1024, stop);
                            luma::EditDocument neutral;
                            neutral.enabledCorrections = 0;
                            const auto display =
                                luma::renderCPU(preview, neutral, asset->metadata.orientation, {},
                                                stop, decoded.calibration);
                            auto observation = provider->analyze(
                                {display.width, display.height, display.rgba}, stop,
                                [self, revision, completed, total](double value) {
                                    if (!self || !total)
                                        return;
                                    QMetaObject::invokeMethod(
                                        self,
                                        [self, revision, completed, total, value] {
                                            if (!self || self->cullingRevision_ != revision)
                                                return;
                                            self->cullingProgress_ =
                                                std::clamp((static_cast<double>(completed) +
                                                            std::clamp(value, 0., 1.)) /
                                                               static_cast<double>(total),
                                                           0., 1.);
                                            emit self->stateChanged();
                                        },
                                        Qt::QueuedConnection);
                                });
                            if (luma::macos::sha256File(asset->path, stop) != asset->checksum)
                                throw std::runtime_error("Original changed during local analysis.");
                            luma::checkCancellation(stop);
                            catalog.saveCullingAnalysis(id, asset->checksum, provider->identifier(),
                                                        observation);
                            ++analyzed;
                        } catch (const std::exception &failure) {
                            luma::checkCancellation(stop);
                            ++failed;
                            error = QString::fromUtf8(failure.what());
                        }
                        if (self)
                            QMetaObject::invokeMethod(
                                self,
                                [self, revision, analyzed, failed, total] {
                                    if (!self || self->cullingRevision_ != revision)
                                        return;
                                    self->cullingProgress_ =
                                        total ? static_cast<double>(analyzed + failed) /
                                                    static_cast<double>(total)
                                              : 1.;
                                    self->cullingStatus_ =
                                        LocalizedText(QStringLiteral("Analyzed: %1 · errors: %2"))
                                            .arg(static_cast<qulonglong>(analyzed))
                                            .arg(static_cast<qulonglong>(failed));
                                    emit self->stateChanged();
                                },
                                Qt::QueuedConnection);
                    }
                    luma::checkCancellation(stop);
                    catalog.rebuildIntelligentGroups(provider->identifier(), stop);
                } catch (const std::exception &failure) {
                    if (!context.stopToken().isCancelled())
                        error = QString::fromUtf8(failure.what());
                }
                if (self)
                    QMetaObject::invokeMethod(
                        self,
                        [self, revision, analyzed, failed, total, error] {
                            if (!self || self->cullingRevision_ != revision)
                                return;
                            self->cullingBusy_ = false;
                            self->cullingProgress_ = 1;
                            self->cullingError_ = error;
                            if (!total && error.isEmpty())
                                self->cullingStatus_ =
                                    QStringLiteral("Local analysis is up to date");
                            else
                                self->cullingStatus_ =
                                    LocalizedText(QStringLiteral("Analyzed: %1 · errors: %2"))
                                        .arg(static_cast<qulonglong>(analyzed))
                                        .arg(static_cast<qulonglong>(failed));
                            self->reload(false);
                            emit self->stateChanged();
                            self->checkClose();
                        },
                        Qt::QueuedConnection);
            });
    } catch (const std::exception &failure) {
        cullingBusy_ = false;
        cullingError_ = QString::fromUtf8(failure.what());
        emit stateChanged();
    }
}

void LibraryModel::setCullingAutoEnabled(bool enabled) {
    if (cullingAutoEnabled_ == enabled)
        return;
    cullingAutoEnabled_ = enabled;
    QSettings settings(QString::fromStdString(catalog_.parent_path().string()) + "/preview.ini",
                       QSettings::IniFormat);
    settings.setValue("cullingAutoEnabled", enabled);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        cullingAutoEnabled_ = !enabled;
        cullingError_ = QStringLiteral("Cannot save automatic local-analysis preference.");
    }
    emit stateChanged();
    if (cullingAutoEnabled_ && !working())
        QTimer::singleShot(0, this, [this] { analyzePendingLibrary(); });
}

void LibraryModel::applyCullingSuggestions() {
    const auto selected = selectionIDs();
    const auto filter = filter_;
    const QPointer<LibraryModel> self(this);
    mutate([selected, filter, self](luma::Catalog &catalog) {
        const auto ids = selected.empty() ? catalog.libraryIDs(filter) : selected;
        const auto changed = catalog.applyCullingSuggestions(ids);
        if (self)
            QMetaObject::invokeMethod(
                self,
                [self, changed] {
                    if (self)
                        self->cullingStatus_ =
                            LocalizedText(QStringLiteral("Applied suggestions: %1"))
                                .arg(static_cast<qlonglong>(changed));
                },
                Qt::QueuedConnection);
    });
}

void LibraryModel::clearCullingIndex() {
    filter_.cullingRecommendation = -2;
    filter_.faceGroupID.clear();
    filter_.bestOfBurstOnly = false;
    mutate([](luma::Catalog &catalog) { catalog.clearCullingIndex(); });
}

void LibraryModel::renameFaceGroup(const QString &id, const QString &name) {
    const auto trimmed = name.trimmed();
    mutate([id, trimmed](luma::Catalog &catalog) {
        catalog.renameFaceGroup(id.toStdString(), trimmed.toStdString());
    });
}

void LibraryModel::setIntelligentFilter(int recommendation, const QString &faceGroupID,
                                        bool bestOfBurstOnly) {
    auto next = filter_;
    next.cullingRecommendation = recommendation;
    next.faceGroupID = faceGroupID.toStdString();
    next.bestOfBurstOnly = bestOfBurstOnly;
    try {
        next.validate();
    } catch (const std::exception &failure) {
        error_ = QString::fromUtf8(failure.what());
        emit stateChanged();
        return;
    }
    filter_ = std::move(next);
    filterTimer_.start();
}
