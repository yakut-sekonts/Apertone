#include "apps/desktop/BundledModels.h"
#include "apps/desktop/EditorController.h"
#include "core/catalog/Identity.h"
#include "core/image/Bayer.h"
#include "core/quick/QuickLooks.h"
#include "core/render/CpuRenderer.h"
#include "platform/macos/CoreMLSky.h"
#include <QFileInfo>
#include <QPointer>
#include <QSettings>
#include <algorithm>
#include <cmath>

namespace {
luma::ai::Selection selectionFor(const QString &kind) {
    if (kind == QStringLiteral("subject"))
        return luma::ai::Selection::subject;
    if (kind == QStringLiteral("background"))
        return luma::ai::Selection::background;
    if (kind == QStringLiteral("object"))
        return luma::ai::Selection::object;
    if (kind == QStringLiteral("sky"))
        return luma::ai::Selection::sky;
    if (kind == QStringLiteral("face"))
        return luma::ai::Selection::face;
    if (kind == QStringLiteral("eyes"))
        return luma::ai::Selection::eyes;
    if (kind == QStringLiteral("lips"))
        return luma::ai::Selection::lips;
    throw std::runtime_error("Unknown AI selection category.");
}
const char *selectionName(luma::ai::Selection selection) {
    switch (selection) {
    case luma::ai::Selection::subject:
        return "Subject";
    case luma::ai::Selection::background:
        return "Background";
    case luma::ai::Selection::object:
        return "Object";
    case luma::ai::Selection::sky:
        return "Sky";
    case luma::ai::Selection::face:
        return "Face";
    case luma::ai::Selection::eyes:
        return "Eyes";
    case luma::ai::Selection::lips:
        return "Lips";
    }
    throw std::runtime_error("Unknown AI selection category.");
}
bool needsPoint(luma::ai::Selection selection) {
    return selection == luma::ai::Selection::object || selection == luma::ai::Selection::face ||
           selection == luma::ai::Selection::eyes || selection == luma::ai::Selection::lips;
}
} // namespace

QString EditorController::skyModelName() const {
    return QFileInfo(skyModelPath_).fileName();
}
bool EditorController::skyModelBundled() const {
    return !skyModelPath_.isEmpty() &&
           skyModelPath_ == luma::desktop::bundledModelPath(luma::desktop::BundledModel::sky);
}
void EditorController::setSkyModel(const QUrl &url) {
    if (!url.isEmpty() && !url.isLocalFile())
        return;
    cancelAI();
    const auto overridePath = url.isEmpty() ? QString() : url.toLocalFile();
    skyModelPath_ = overridePath.isEmpty()
                        ? luma::desktop::bundledModelPath(luma::desktop::BundledModel::sky)
                        : overridePath;
    QSettings settings(QString::fromStdString(catalog_.parent_path().string()) + "/preview.ini",
                       QSettings::IniFormat);
    settings.setValue("skyModel", overridePath);
    emit stateChanged();
}
void EditorController::cancelAI() {
    aiJob_.cancel();
    ++aiRevision_;
    pendingQuickLook_.clear();
    const bool changed = aiBusy_ || aiObjectPicking_ || !aiPickKind_.isEmpty();
    aiPickKind_.clear();
    if (!changed)
        return;
    aiBusy_ = false;
    aiObjectPicking_ = false;
    aiProgress_ = 0;
    emit stateChanged();
}
void EditorController::beginAIObjectSelection() {
    beginAIMaskSelection(QStringLiteral("object"));
}
void EditorController::beginAIMaskSelection(const QString &kind) {
    if (!editable() || original_)
        return;
    try {
        const auto selection = selectionFor(kind);
        if (!needsPoint(selection) || !segmentation_->supports(selection))
            throw std::runtime_error("The requested AI selection is not available locally.");
    } catch (const std::exception &failure) {
        advancedFailure(failure);
        return;
    }
    cancelAI();
    setRetouchMode(QStringLiteral("off"));
    endEdit();
    setLocalMode(false);
    setCropMode(false);
    closeTransformTool();
    targetedTool_ = QStringLiteral("none");
    setOneToOne(false);
    aiObjectPicking_ = true;
    aiPickKind_ = kind;
    emit stateChanged();
}
void EditorController::pickAIMask(double x, double y) {
    if (!aiObjectPicking_ || aiPickKind_.isEmpty())
        return;
    generateAIMask(aiPickKind_, x, y);
}
void EditorController::generateAIMask(const QString &kind, double x, double y) {
    if (!editable() || original_ || aiBusy_)
        return;
    try {
        const auto selection = selectionFor(kind);
        if ((selection == luma::ai::Selection::sky && !skyModelConfigured()) ||
            (selection != luma::ai::Selection::sky && !segmentation_->supports(selection)))
            throw std::runtime_error("The requested AI selection is not available locally.");
        if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || y < 0 || x > 1 || y > 1)
            return;
        const auto source = session_->preview;
        const auto metadata = session_->asset.metadata;
        const auto calibration = session_->calibration;
        const luma::CropGeometry displayed(source->width, source->height, metadata.orientation,
                                           history_.current());
        if (needsPoint(selection) &&
            !displayed.covered(x * displayed.width() - .5, y * displayed.height() - .5))
            return;
        const auto point =
            displayed.source(x * displayed.width() - .5, y * displayed.height() - .5);
        const std::array<double, 2> sourcePoint{
            std::clamp((point[0] + .5) / source->width, 0.0, 1.0),
            std::clamp((point[1] + .5) / source->height, 0.0, 1.0)};
        cancelAI();
        setRetouchMode(QStringLiteral("off"));
        endEdit();
        aiObjectPicking_ = false;
        aiBusy_ = true;
        aiOperation_ = QStringLiteral("AI selection");
        aiProgress_ = 0;
        error_.clear();
        const auto revision = ++aiRevision_;
        const auto provider = segmentation_;
        const auto skyModel = std::filesystem::path(skyModelPath_.toStdString());
        auto referenceEdits = history_.current();
        referenceEdits.localMasks.clear();
        referenceEdits.setEnabled(luma::Correction::crop, false);
        referenceEdits.setEnabled(luma::Correction::transform, false);
        const double previewScale = session_->previewScale;
        const QPointer<EditorController> self(this);
        emit stateChanged();
        aiJob_ = aiJobs_.submit(
            luma::JobPriority::background,
            [self, source, metadata, calibration, revision, provider, referenceEdits, selection,
             sourcePoint, previewScale, skyModel](const luma::JobContext &context) {
                try {
                    const auto stop = context.stopToken();
                    const auto input = luma::standardPreview(*source, 1024, stop);
                    const double scale =
                        previewScale * std::min(static_cast<double>(input.width) / source->width,
                                                static_cast<double>(input.height) / source->height);
                    const auto pixels =
                        luma::renderCPU(input, referenceEdits, metadata.orientation, {}, stop,
                                        calibration, luma::DetailDiagnostic::none, scale, metadata);
                    const luma::CropGeometry geometry(input.width, input.height,
                                                      metadata.orientation, referenceEdits);
                    const auto picked = geometry.output(sourcePoint[0] * input.width - .5,
                                                        sourcePoint[1] * input.height - .5);
                    const std::array<double, 2> uprightPoint{
                        std::clamp((picked[0] + .5) / geometry.width(), 0.0, 1.0),
                        std::clamp((picked[1] + .5) / geometry.height(), 0.0, 1.0)};
                    const auto activeProvider = selection == luma::ai::Selection::sky
                                                    ? luma::macos::makeCoreMLSky(skyModel)
                                                    : provider;
                    luma::checkCancellation(stop);
                    const auto bitmap = activeProvider->segment(
                        {pixels.width, pixels.height, pixels.rgba}, selection, uprightPoint, stop,
                        [self, revision](double value) {
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
                    std::vector<std::uint8_t> values(luma::pixelCount(input.width, input.height));
                    for (int row = 0; row < input.height; ++row) {
                        luma::checkCancellation(stop);
                        for (int column = 0; column < input.width; ++column) {
                            const auto point = geometry.output(column, row);
                            values[static_cast<std::size_t>(row) * input.width + column] =
                                static_cast<std::uint8_t>(std::lround(
                                    255 * bitmap.coverage((point[0] + .5) / geometry.width(),
                                                          (point[1] + .5) / geometry.height())));
                        }
                    }
                    luma::BitmapMask anchored(input.width, input.height, std::move(values));
                    luma::checkCancellation(stop);
                    if (self)
                        QMetaObject::invokeMethod(
                            self,
                            [self, revision, selection, anchored] {
                                if (!self || self->aiRevision_ != revision)
                                    return;
                                self->aiBusy_ = false;
                                const auto pendingRecipe = self->pendingQuickLook_;
                                self->pendingQuickLook_.clear();
                                if (!self->editable()) {
                                    emit self->stateChanged();
                                    return;
                                }
                                try {
                                    auto next = self->history_.current();
                                    luma::LocalMask mask;
                                    mask.id = luma::newID();
                                    mask.name = QCoreApplication::translate(
                                                    "Apertone", selectionName(selection))
                                                    .toStdString();
                                    mask.shape = luma::MaskShape::bitmap;
                                    mask.bitmap = anchored;
                                    if (!pendingRecipe.isEmpty()) {
                                        const auto &recipe =
                                            luma::quickLookRecipe(pendingRecipe.toStdString());
                                        mask.name = std::string(recipe.name);
                                        mask.adjustments =
                                            luma::quickLookLocalAdjustments(recipe, 100);
                                        mask.blur = luma::quickLookBlur(recipe, 100);
                                        next.creative.quickLooks.push_back({luma::newID(),
                                                                            std::string(recipe.id),
                                                                            100,
                                                                            true,
                                                                            mask.id,
                                                                            {},
                                                                            {}});
                                    }
                                    next.localMasks.push_back(mask);
                                    next.validate();
                                    // Check the serialized storage budget before changing history.
                                    luma::encodeLocalMasks(next.localMasks);
                                    if (pendingRecipe.isEmpty()) {
                                        self->localSelected_ = QString::fromStdString(mask.id);
                                        self->localComponentSelected_.clear();
                                        self->setLocalMode(true);
                                        self->localOverlay_ = true;
                                    }
                                    self->applyAdvanced(std::move(next));
                                    emit self->stateChanged();
                                } catch (const std::exception &failure) {
                                    self->advancedFailure(failure);
                                }
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
                                self->pendingQuickLook_.clear();
                                if (!cancelled)
                                    self->error_ = message;
                                emit self->stateChanged();
                            },
                            Qt::QueuedConnection);
                }
            });
    } catch (const std::exception &failure) {
        aiBusy_ = false;
        pendingQuickLook_.clear();
        advancedFailure(failure);
    }
}
