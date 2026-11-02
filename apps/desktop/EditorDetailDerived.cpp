#include "apps/desktop/EditorController.h"
#include "apps/shared/DetailService.h"
#include <cmath>

void EditorController::createRawDetails(double amount) {
    createDetailDerived(false, amount);
}

void EditorController::createDetail2x(double amount) {
    createDetailDerived(true, amount);
}

void EditorController::createDetailDerived(bool twoX, double amount) {
    if (!editable() || aiBusy_ || saving() || !std::isfinite(amount) || amount <= 0 || amount > 100)
        return;
    const auto id = session_->asset.id;
    const auto operation =
        twoX ? luma::DetailDerivedOperation::detail2x : luma::DetailDerivedOperation::rawDetails;
    runMerge(twoX ? QStringLiteral("2× Detail") : QStringLiteral("Raw Details"),
             [catalog = catalog_, id, operation, amount](auto stop, const auto &progress) {
                 return luma::DetailService(catalog).create(id, operation, amount / 100, stop,
                                                            progress);
             });
}
