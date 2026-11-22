#include "core/io/Path.h"
#include "apps/desktop/EditorController.h"
#include "apps/shared/OfflinePreview.h"
#include "core/image/Bayer.h"
#include "core/render/CpuRenderer.h"
#include <QPointer>
#include <algorithm>

namespace {
QString comparisonURL(qsizetype index, std::uint64_t revision) {
    return QStringLiteral("image://preview/compare-%1?%2").arg(index).arg(revision);
}

QImage renderComparison(const luma::SessionService &service, const luma::AssetRecord &asset,
                        const std::filesystem::path &catalog, int maxEdge,
                        std::span<const std::uint8_t> profile, bool hdrToneMapping,
                        luma::CancellationToken stop) {
    std::shared_ptr<const luma::DecodedImage> decoded;
    std::optional<luma::OfflinePreview> offline;
    std::error_code error;
    if (maxEdge && !std::filesystem::is_regular_file(asset.path, error)) {
        offline = luma::loadOfflinePreview(catalog, asset.checksum, stop);
        if (!offline)
            throw std::runtime_error("Original unavailable. Reconnect it to generate an offline preview.");
    } else {
        decoded = service.fullResolution(asset, stop);
    }
    const auto &original = offline ? offline->image : decoded->image;
    const auto &calibration = offline ? offline->calibration : decoded->calibration;
    auto preview = maxEdge ? std::optional{luma::standardPreview(original, maxEdge, stop)}
                           : std::nullopt;
    const auto &input = preview ? *preview : original;
    const int width = offline ? asset.metadata.width : original.width;
    const double scale = static_cast<double>(input.width) / std::max(1, width);
    const auto rendered = luma::renderCPU(
        input, asset.edits, asset.metadata.orientation, profile, stop, calibration,
        luma::DetailDiagnostic::none, scale, asset.metadata, {}, nullptr, {}, 4,
        hdrToneMapping ? luma::DisplayMapping::adaptiveHDR : luma::DisplayMapping::clip);
    auto image = QImage(rendered.rgba.data(), rendered.width, rendered.height,
                        static_cast<qsizetype>(rendered.width) * 4, QImage::Format_RGBA8888).copy();
    if (image.isNull())
        throw std::runtime_error("Cannot allocate the comparison preview.");
    return image;
}
} // namespace

QVariantMap EditorController::referencePhoto() const {
    return {{"busy", referenceBusy_}, {"name", referenceName_}, {"error", referenceError_},
            {"source", referenceUrl_}};
}
void EditorController::clearReference() {
    referenceJob_.cancel();
    ++referenceRevision_;
    referenceBusy_ = false;
    referenceName_.clear();
    referenceError_.clear();
    referenceUrl_.clear();
    provider_->updateReference({});
    emit referenceChanged();
}
void EditorController::pinReference(const QString &id) {
    if (closing_ || id.isEmpty())
        return;
    clearReference();
    referenceBusy_ = true;
    emit referenceChanged();
    const auto revision = referenceRevision_;
    const auto catalogPath = catalog_;
    const auto service = service_;
    auto active = session_ && photoID() == id ? std::optional{history_.current()} : std::nullopt;
    const QPointer<EditorController> self(this);
    try {
        referenceJob_ = compareJobs_.submit(luma::JobPriority::interactive,
            [self, revision, catalogPath, service, id, active = std::move(active)](const auto &job) {
                QImage image;
                QString name, error;
                try {
                    auto asset = luma::Catalog(catalogPath, luma::CatalogOpenMode::existingOnly)
                                     .assetByID(id.toStdString());
                    if (!asset)
                        throw std::runtime_error("The reference photo is no longer in the catalog.");
                    if (active)
                        asset->edits = *active;
                    image = renderComparison(service, *asset, catalogPath, 2048, {}, true, job.stopToken());
                    name = QString::fromStdString(luma::pathUtf8(asset->path.filename()));
                } catch (const std::exception &failure) {
                    error = QString::fromUtf8(failure.what());
                }
                if (!self || job.stopToken().isCancelled())
                    return;
                QMetaObject::invokeMethod(self, [self, revision, image = std::move(image), name, error]() mutable {
                    if (!self || self->referenceRevision_ != revision)
                        return;
                    self->referenceBusy_ = false;
                    self->referenceError_ = error;
                    self->referenceName_ = name;
                    if (!image.isNull()) {
                        self->provider_->updateReference(std::move(image));
                        self->referenceUrl_ = QStringLiteral("image://preview/reference?%1").arg(revision);
                    }
                    emit self->referenceChanged();
                }, Qt::QueuedConnection);
            });
    } catch (const std::exception &failure) {
        referenceBusy_ = false;
        referenceError_ = QString::fromUtf8(failure.what());
        emit referenceChanged();
    }
}

QVariantMap EditorController::comparison() const {
    QStringList sources;
    for (qsizetype i = 0; i < compareIDs_.size(); ++i)
        sources.append(comparisonURL(i, compareImageRevision_));
    return {
        {"busy", compareBusy_},
        {"ready", compareReady_},
        {"error", compareError_},
        {"fullResolution", compareFull_},
        {"beforeAfter", compareBeforeAfter_},
        {"ids", compareIDs_},
        {"names", compareNames_},
        {"sources", sources},
        {"errors", compareErrors_},
        {"left", comparisonURL(0, compareImageRevision_)},
        {"right", comparisonURL(1, compareImageRevision_)}};
}
void EditorController::closeCompare() {
    compareJob_.cancel();
    ++compareRevision_;
    compareBusy_ = false;
    compareReady_ = false;
    compareError_.clear();
    compareBeforeAfter_ = false;
    compareIDs_.clear();
    compareNames_.clear();
    compareErrors_.clear();
    provider_->updateCompare({});
    emit stateChanged();
}
void EditorController::prepareCompare(const QStringList &ids, bool fullResolution) {
    prepareCompareImpl(ids, fullResolution, false);
}
void EditorController::prepareSurvey(const QStringList &ids) {
    if (ids.size() >= 2 && ids.size() <= 12)
        prepareCompareImpl(ids, false, false, true);
}
void EditorController::prepareBeforeAfter(bool fullResolution) {
    if (!session_)
        return;
    const auto id = QString::fromStdString(session_->asset.id);
    prepareCompareImpl({id, id}, fullResolution, true);
}
void EditorController::prepareCompareImpl(const QStringList &ids, bool fullResolution,
                                          bool beforeAfter, bool survey) {
    auto unique = ids;
    unique.removeDuplicates();
    if (closing_ || (!survey && ids.size() != 2) ||
        (!beforeAfter && ids.size() != unique.size()))
        return;
    finishHistoryTools();
    compareJob_.cancel();
    const auto revision = ++compareRevision_;
    compareBusy_ = true;
    compareReady_ = false;
    compareError_.clear();
    compareFull_ = fullResolution;
    compareBeforeAfter_ = beforeAfter;
    compareIDs_ = ids;
    compareNames_.clear();
    compareErrors_.clear();
    provider_->updateCompare({});
    const auto active = session_ ? session_->asset.id : std::string{};
    const auto activeEdits = history_.current();
    const auto catalogPath = catalog_;
    const auto service = service_;
    const auto profile = displayProfile_;
    const auto hdrToneMapping = hdrToneMapping_;
    const QPointer<EditorController> self(this);
    try {
        compareJob_ = compareJobs_.submit(
            luma::JobPriority::interactive,
            [self, revision, ids, fullResolution, beforeAfter, survey, active, activeEdits,
             catalogPath, service, profile, hdrToneMapping](const luma::JobContext &context) {
                std::vector<QImage> images(static_cast<std::size_t>(ids.size()));
                QStringList names, errors;
                QString error;
                try {
                    luma::Catalog catalog(catalogPath);
                    // Freeze edits before decoding. Never change the active Library asset.
                    std::vector<luma::AssetRecord> assets;
                    assets.reserve(images.size());
                    for (qsizetype i = 0; i < ids.size(); ++i) {
                        auto snapshot = catalog.assetSnapshot(ids[i].toStdString());
                        if (!snapshot)
                            throw std::runtime_error(
                                "Comparison photo is no longer in the catalog.");
                        auto &asset = assets.emplace_back(std::move(snapshot->asset));
                        if (beforeAfter)
                            asset.edits = i == 0 ? luma::EditDocument{} : activeEdits;
                        else if (asset.id == active)
                            asset.edits = activeEdits;
                        names.append(QString::fromStdString(luma::pathUtf8(asset.path.filename())));
                    }
                    const int maxEdge = fullResolution ? 0 : survey ? 1200 : 2560;
                    for (std::size_t i = 0; i < assets.size(); ++i) {
                        luma::checkCancellation(context.stopToken());
                        try {
                            images[i] = renderComparison(service, assets[i], catalogPath, maxEdge,
                                                         profile, hdrToneMapping, context.stopToken());
                            errors.append(QString());
                        } catch (const std::exception &failure) {
                            if (!survey || context.stopToken().isCancelled())
                                throw;
                            errors.append(QString::fromUtf8(failure.what()));
                        }
                    }
                } catch (const std::exception &failure) {
                    error = QString::fromUtf8(failure.what());
                }
                if (!self || context.stopToken().isCancelled())
                    return;
                QMetaObject::invokeMethod(
                    self,
                    [self, revision, images = std::move(images), names, errors, error]() mutable {
                        if (!self || self->compareRevision_ != revision)
                            return;
                        self->compareBusy_ = false;
                        self->compareError_ = error;
                        self->compareReady_ = error.isEmpty();
                        if (error.isEmpty()) {
                            self->provider_->updateCompare(std::move(images));
                            self->compareNames_ = names;
                            self->compareErrors_ = errors;
                            ++self->compareImageRevision_;
                        }
                        emit self->stateChanged();
                    },
                    Qt::QueuedConnection);
            });
    } catch (const std::exception &failure) {
        compareBusy_ = false;
        compareError_ = QString::fromUtf8(failure.what());
    }
    emit stateChanged();
}
