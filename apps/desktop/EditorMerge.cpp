#include "apps/desktop/EditorController.h"
#include "apps/shared/MergeService.h"
#include <QPointer>
#include <cmath>

void EditorController::mergeHDRPhotos(const QStringList &ids, double deghost) {
    if (!std::isfinite(deghost) || deghost < 0 || deghost > 100)
        return;
    std::vector<std::string> sources;
    for (const auto &id : ids)
        sources.push_back(id.toStdString());
    runMerge(QStringLiteral("HDR merge"),
             [catalog = catalog_, sources, deghost](auto stop, const auto &progress) {
                 return luma::MergeService(catalog).hdr(sources, {deghost / 100}, stop, progress);
             });
}

void EditorController::mergePanoramaPhotos(const QStringList &ids, int projection,
                                           double fieldOfView, bool boundaryWarp) {
    if (projection < 0 || projection > 2 || !std::isfinite(fieldOfView) || fieldOfView < 10 ||
        fieldOfView > 140)
        return;
    std::vector<std::string> sources;
    for (const auto &id : ids)
        sources.push_back(id.toStdString());
    const luma::PanoramaOptions options{static_cast<luma::PanoramaProjection>(projection),
                                        fieldOfView, boundaryWarp};
    runMerge(QStringLiteral("Panorama merge"),
             [catalog = catalog_, sources, options](auto stop, const auto &progress) {
                 return luma::MergeService(catalog).panorama(sources, options, stop, progress);
             });
}

void EditorController::runMerge(const QString &operation,
                                std::function<std::filesystem::path(
                                    luma::CancellationToken, const std::function<void(double)> &)>
                                    work) {
    if (busy_ || closing_ || editingLocked_ || aiBusy_)
        return;
    cancelAI();
    setRetouchMode(QStringLiteral("off"));
    endEdit();
    const auto revision = ++aiRevision_;
    const QPointer<EditorController> self(this);
    aiBusy_ = true;
    aiProgress_ = 0;
    aiOperation_ = operation;
    error_.clear();
    emit stateChanged();
    try {
        const auto task = std::make_shared<decltype(work)>(std::move(work));
        aiJob_ = aiJobs_.submit(
            luma::JobPriority::background, [self, revision, task](const luma::JobContext &context) {
                try {
                    const auto created =
                        (*task)(context.stopToken(), [self, revision](double value) {
                            if (self)
                                QMetaObject::invokeMethod(
                                    self,
                                    [self, revision, value] {
                                        if (self && self->aiRevision_ == revision) {
                                            self->aiProgress_ = value;
                                            emit self->stateChanged();
                                        }
                                    },
                                    Qt::QueuedConnection);
                        });
                    if (self)
                        QMetaObject::invokeMethod(
                            self,
                            [self, revision, created] {
                                if (!self || self->aiRevision_ != revision)
                                    return;
                                self->aiBusy_ = false;
                                self->aiProgress_ = 1;
                                emit self->stateChanged();
                                self->openFile(
                                    QUrl::fromLocalFile(QString::fromStdString(created.string())));
                            },
                            Qt::QueuedConnection);
                } catch (const std::exception &failure) {
                    const auto message = QString::fromUtf8(failure.what());
                    const bool cancelled = context.stopToken().isCancelled();
                    if (self)
                        QMetaObject::invokeMethod(
                            self,
                            [self, revision, message, cancelled] {
                                if (!self || self->aiRevision_ != revision)
                                    return;
                                self->aiBusy_ = false;
                                if (!cancelled)
                                    self->error_ = message;
                                emit self->stateChanged();
                            },
                            Qt::QueuedConnection);
                }
            });
    } catch (const std::exception &failure) {
        cancelAI();
        advancedFailure(failure);
    }
}
