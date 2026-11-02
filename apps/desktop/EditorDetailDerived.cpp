#include "apps/desktop/EditorController.h"
#include "apps/shared/DetailService.h"
#include "core/render/NoiseProfile.h"
#include <cmath>

void EditorController::createRawDetails(double amount) {
    createDetailDerived(luma::DetailDerivedOperation::rawDetails, amount);
}

void EditorController::createDetail2x(double amount) {
    createDetailDerived(luma::DetailDerivedOperation::detail2x, amount);
}

void EditorController::createLensBlurRecovery(double amount) {
    createDetailDerived(luma::DetailDerivedOperation::lensBlurRecovery, amount);
}

void EditorController::createColorMoireCorrection(double amount) {
    createDetailDerived(luma::DetailDerivedOperation::colorMoireCorrection, amount);
}

void EditorController::createDetailDerived(luma::DetailDerivedOperation operation, double amount) {
    if (!editable() || !session_ || aiBusy_ || saving() || !std::isfinite(amount) || amount <= 0 ||
        amount > 100)
        return;
    const auto id = session_->asset.id;
    QString name;
    switch (operation) {
    case luma::DetailDerivedOperation::rawDetails:
        name = QStringLiteral("Raw Details");
        break;
    case luma::DetailDerivedOperation::detail2x:
        name = QStringLiteral("2× Detail");
        break;
    case luma::DetailDerivedOperation::lensBlurRecovery:
        name = QStringLiteral("Lens Blur Recovery");
        break;
    case luma::DetailDerivedOperation::colorMoireCorrection:
        name = QStringLiteral("Color Moire Correction");
        break;
    }
    runMerge(name, [catalog = catalog_, id, operation, amount](auto stop, const auto &progress) {
        return luma::DetailService(catalog).create(id, operation, amount / 100, std::move(stop),
                                                   progress);
    });
}

void EditorController::autoNoiseProfile() {
    if (!editable() || original_ || exporting_ || !session_ || !session_->preview)
        return;
    try {
        const auto recommendation =
            luma::estimateNoiseProfile(*session_->preview, session_->asset.metadata);
        auto next = history_.current();
        next.noiseLuminance = recommendation.luminance;
        next.noiseLuminanceDetail = recommendation.luminanceDetail;
        next.noiseLuminanceContrast = recommendation.luminanceContrast;
        next.noiseColor = recommendation.color;
        next.noiseColorDetail = recommendation.colorDetail;
        next.noiseColorSmoothness = recommendation.colorSmoothness;
        next.setEnabled(luma::Correction::luminanceNoise, true);
        next.setEnabled(luma::Correction::colorNoise, true);
        next.validate();
        endEdit();
        if (next != history_.current()) {
            history_.apply(std::move(next), "Auto noise profile");
            editsChanged();
        }
        error_.clear();
    } catch (const std::exception &failure) {
        error_ = QString::fromUtf8(failure.what());
        emit stateChanged();
    }
}
