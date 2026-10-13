#include "apps/desktop/EditorController.h"
#include <QGuiApplication>
#include <QKeyEvent>

QString EditorController::detailDiagnostic() const {
    switch (diagnostic_) {
    case luma::DetailDiagnostic::amount:
        return QStringLiteral("Amount · яркость");
    case luma::DetailDiagnostic::radius:
        return QStringLiteral("Radius · детали");
    case luma::DetailDiagnostic::detail:
        return QStringLiteral("Detail · детали после порога");
    case luma::DetailDiagnostic::masking:
        return QStringLiteral("Masking · белое обрабатывается");
    case luma::DetailDiagnostic::noiseLuminance:
        return QStringLiteral("Luminance · яркость без цвета");
    case luma::DetailDiagnostic::noiseColor:
        return QStringLiteral("Color · цвет на нейтральной яркости");
    case luma::DetailDiagnostic::none:
        return {};
    }
    return {};
}
void EditorController::setDetailControl(const QString &parameter) {
    if (!parameter.isEmpty()) {
        try {
            const auto correction = luma::EditDocument::correction(parameter.toStdString());
            if (correction != luma::Correction::sharpening &&
                correction != luma::Correction::luminanceNoise &&
                correction != luma::Correction::colorNoise)
                return;
            history_.current().parameter(parameter.toStdString());
        } catch (const std::exception &) {
            return;
        }
    }
    detailControl_ = editable() && !original_ ? parameter : QString();
    if (!detailControl_.isEmpty())
        optionHeld_ =
            optionHeld_ || QGuiApplication::queryKeyboardModifiers().testFlag(Qt::AltModifier);
    refreshDetailDiagnostic();
}
void EditorController::refreshDetailDiagnostic() {
    auto next = luma::DetailDiagnostic::none;
    if (optionHeld_ && editable() && !original_ && !detailControl_.isEmpty() &&
        history_.current().enabled(luma::EditDocument::correction(detailControl_.toStdString()))) {
        if (detailControl_ == QStringLiteral("sharpen.amount"))
            next = luma::DetailDiagnostic::amount;
        else if (detailControl_ == QStringLiteral("sharpen.radius"))
            next = luma::DetailDiagnostic::radius;
        else if (detailControl_ == QStringLiteral("sharpen.detail"))
            next = luma::DetailDiagnostic::detail;
        else if (detailControl_ == QStringLiteral("sharpen.masking"))
            next = luma::DetailDiagnostic::masking;
        else if (detailControl_.startsWith(QStringLiteral("noise.luminance")))
            next = luma::DetailDiagnostic::noiseLuminance;
        else if (detailControl_.startsWith(QStringLiteral("noise.color")))
            next = luma::DetailDiagnostic::noiseColor;
    }
    if (next == diagnostic_)
        return;
    diagnostic_ = next;
    render();
    emit stateChanged();
}
void EditorController::clearDetailDiagnostic() {
    detailControl_.clear();
    refreshDetailDiagnostic();
}
bool EditorController::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        const auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Alt && !key->isAutoRepeat()) {
            optionHeld_ = event->type() == QEvent::KeyPress;
            refreshDetailDiagnostic();
        }
    } else if (event->type() == QEvent::ApplicationDeactivate) {
        optionHeld_ = false;
        clearDetailDiagnostic();
    }
    return QObject::eventFilter(watched, event);
}
