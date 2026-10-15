#include "apps/desktop/EditorController.h"
#include "core/catalog/Identity.h"
#include <QColor>
#include <algorithm>
#include <cmath>
#include <numbers>

int EditorController::localIndex() const {
    const auto &masks = history_.current().localMasks;
    const auto found = std::find_if(masks.begin(), masks.end(), [&](const auto &mask) {
        return mask.id == localSelected_.toStdString();
    });
    return found == masks.end() ? -1 : static_cast<int>(found - masks.begin());
}
QVariantList EditorController::localMasks() const {
    QVariantList result;
    for (const auto &mask : history_.current().localMasks)
        result.append(
            QVariantMap{{"id", QString::fromStdString(mask.id)},
                        {"name", QString::fromStdString(mask.name)},
                        {"enabled", mask.enabled},
                        {"shape", mask.shape == luma::MaskShape::linear ? "linear" : "radial"}});
    return result;
}
QVariantMap EditorController::localState() const {
    const int index = localIndex();
    if (index < 0)
        return {};
    const auto &mask = history_.current().localMasks[static_cast<std::size_t>(index)];
    QVariantMap state{{"id", QString::fromStdString(mask.id)},
                      {"name", QString::fromStdString(mask.name)},
                      {"shape", mask.shape == luma::MaskShape::linear ? "linear" : "radial"},
                      {"enabled", mask.enabled},
                      {"inverted", mask.inverted},
                      {"feather", mask.feather},
                      {"opacity", mask.opacity},
                      {"angle", mask.angle},
                      {"index", index}};
    for (std::size_t i = 0; i < luma::localParameters.size(); ++i)
        state[QString::fromUtf8(luma::localParameters[i])] = mask.adjustments[i];
    return state;
}
luma::MaskOverlay EditorController::localRenderOverlay() const {
    luma::MaskOverlay result;
    if (!localMode_ || !localOverlay_ || original_ || presetPreview_ ||
        diagnostic_ != luma::DetailDiagnostic::none)
        return result;
    const QColor color(localOverlayColor_);
    result.id = localSelected_.toStdString();
    result.color = {color.redF(), color.greenF(), color.blueF()};
    result.opacity = localOverlayOpacity_;
    return result;
}
void EditorController::setLocalMode(bool value) {
    if (value == localMode_ || (value && (!editable() || original_)))
        return;
    endEdit();
    if (value) {
        setCropMode(false);
        closeTransformTool();
        targetedTool_ = QStringLiteral("none");
        clearPreviewSettings();
    }
    localMode_ = value;
    localCanvasReady_ = false;
    render();
    emit stateChanged();
}
void EditorController::setLocalOverlay(bool value) {
    if (localOverlay_ == value)
        return;
    localOverlay_ = value;
    render();
    emit stateChanged();
}
void EditorController::setLocalOverlayColor(const QString &value) {
    const QColor color(value);
    if (!color.isValid())
        return;
    localOverlayColor_ = color.name();
    render();
    emit stateChanged();
}
void EditorController::setLocalOverlayOpacity(double value) {
    if (!std::isfinite(value) || value < 0 || value > 1)
        return;
    localOverlayOpacity_ = value;
    render();
    emit stateChanged();
}
QString EditorController::addLocalMask(const QString &shape) {
    if (!editable() || original_)
        return {};
    try {
        if (shape != "linear" && shape != "radial")
            throw std::runtime_error("Unsupported mask shape.");
        endEdit();
        auto next = history_.current();
        luma::LocalMask mask;
        mask.id = luma::newID();
        mask.name = QCoreApplication::translate("Luma", shape == "linear" ? "Linear gradient"
                                                                          : "Radial gradient")
                        .toStdString();
        if (shape == "radial") {
            mask.shape = luma::MaskShape::radial;
            mask.x1 = mask.y1 = 0.5;
            mask.x2 = 0.3;
            mask.y2 = 0.25;
        }
        next.localMasks.push_back(mask);
        next.validate();
        localSelected_ = QString::fromStdString(mask.id);
        setLocalMode(true);
        applyAdvanced(std::move(next));
        return localSelected_;
    } catch (const std::exception &failure) {
        advancedFailure(failure);
        return {};
    }
}
void EditorController::selectLocalMask(const QString &id) {
    if (!editable())
        return;
    const auto &masks = history_.current().localMasks;
    if (std::none_of(masks.begin(), masks.end(),
                     [&](const auto &mask) { return mask.id == id.toStdString(); }))
        return;
    endEdit();
    localSelected_ = id;
    render();
    emit stateChanged();
}
void EditorController::removeLocalMask() {
    if (!editable() || localIndex() < 0)
        return;
    endEdit();
    auto next = history_.current();
    next.localMasks.erase(next.localMasks.begin() + localIndex());
    localSelected_ =
        next.localMasks.empty() ? QString() : QString::fromStdString(next.localMasks.back().id);
    applyAdvanced(std::move(next));
}
void EditorController::moveLocalMask(int direction) {
    if (!editable() || localIndex() < 0 || (direction != -1 && direction != 1))
        return;
    const int index = localIndex(), target = index + direction;
    if (target < 0 || static_cast<std::size_t>(target) >= history_.current().localMasks.size())
        return;
    endEdit();
    auto next = history_.current();
    std::swap(next.localMasks[static_cast<std::size_t>(index)],
              next.localMasks[static_cast<std::size_t>(target)]);
    applyAdvanced(std::move(next));
}
void EditorController::renameLocalMask(const QString &name) {
    if (!editable() || localIndex() < 0)
        return;
    try {
        endEdit();
        auto next = history_.current();
        next.localMasks[static_cast<std::size_t>(localIndex())].name = name.trimmed().toStdString();
        applyAdvanced(std::move(next));
    } catch (const std::exception &failure) {
        advancedFailure(failure);
    }
}
void EditorController::setLocalEnabled(bool enabled) {
    if (!editable() || localIndex() < 0)
        return;
    endEdit();
    auto next = history_.current();
    next.localMasks[static_cast<std::size_t>(localIndex())].enabled = enabled;
    applyAdvanced(std::move(next));
}
void EditorController::setLocalInverted(bool inverted) {
    if (!editable() || localIndex() < 0)
        return;
    endEdit();
    auto next = history_.current();
    next.localMasks[static_cast<std::size_t>(localIndex())].inverted = inverted;
    applyAdvanced(std::move(next));
}
void EditorController::setLocalParameter(const QString &name, double value) {
    if (!editable() || localIndex() < 0)
        return;
    try {
        auto next = history_.current();
        next.localMasks[static_cast<std::size_t>(localIndex())].setParameter(name.toStdString(),
                                                                             value);
        applyAdvanced(std::move(next));
    } catch (const std::exception &failure) {
        advancedFailure(failure);
    }
}
namespace {
std::array<double, 2> sourcePoint(const luma::CropGeometry &geometry, int width, int height,
                                  double x, double y) {
    if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || x > 1 || y < 0 || y > 1)
        throw std::runtime_error("Mask gesture is outside the photo.");
    const auto px = std::clamp(x * geometry.width() - 0.5, 0.0, geometry.width() - 1.0),
               py = std::clamp(y * geometry.height() - 0.5, 0.0, geometry.height() - 1.0);
    if (!geometry.covered(px, py))
        throw std::runtime_error("Mask gesture is outside image coverage.");
    const auto point = geometry.source(px, py);
    return {(point[0] + 0.5) / width, (point[1] + 0.5) / height};
}
} // namespace
bool EditorController::beginLocalGesture(int handle, double x, double y) {
    if (!editable() || !localCanvasReady() || localIndex() < 0 || handle < -1 || handle > 2)
        return false;
    try {
        endEdit();
        const auto &image = *session_->preview;
        luma::CropGeometry geometry(image.width, image.height, session_->asset.metadata.orientation,
                                    history_.current());
        const auto point = sourcePoint(geometry, image.width, image.height, x, y);
        localGesture_ =
            LocalGesture{history_.current().localMasks[static_cast<std::size_t>(localIndex())],
                         geometry, handle, point};
        history_.begin();
        return true;
    } catch (const std::exception &failure) {
        advancedFailure(failure);
        return false;
    }
}
void EditorController::adjustLocalGesture(double x, double y) {
    if (!editable() || !localGesture_ || localIndex() < 0)
        return;
    try {
        const auto &gesture = *localGesture_;
        const auto &image = *session_->preview;
        const auto point = sourcePoint(gesture.geometry, image.width, image.height, x, y);
        auto mask = gesture.base;
        const double dx = point[0] - gesture.anchor[0], dy = point[1] - gesture.anchor[1];
        if (gesture.handle == -1) {
            mask.x1 = gesture.anchor[0];
            mask.y1 = gesture.anchor[1];
            if (mask.shape == luma::MaskShape::linear) {
                mask.x2 = point[0];
                mask.y2 = point[1];
            } else {
                mask.x2 = std::abs(dx);
                mask.y2 = std::abs(dy);
                mask.angle = 0;
            }
        } else if (mask.shape == luma::MaskShape::linear) {
            if (gesture.handle == 0 || gesture.handle == 2) {
                mask.x1 += dx;
                mask.y1 += dy;
            }
            if (gesture.handle == 1 || gesture.handle == 2) {
                mask.x2 += dx;
                mask.y2 += dy;
            }
        } else if (gesture.handle == 0) {
            mask.x1 += dx;
            mask.y1 += dy;
        } else {
            const double radians = mask.angle * std::numbers::pi / 180;
            const double px = point[0] - mask.x1, py = point[1] - mask.y1;
            if (gesture.handle == 1)
                mask.x2 = std::abs(std::cos(radians) * px + std::sin(radians) * py);
            else
                mask.y2 = std::abs(-std::sin(radians) * px + std::cos(radians) * py);
        }
        // Near-zero drags keep the previous valid geometry, without partial writes.
        if ((mask.shape == luma::MaskShape::linear &&
             std::hypot(mask.x2 - mask.x1, mask.y2 - mask.y1) < 0.001) ||
            (mask.shape == luma::MaskShape::radial && (mask.x2 < 0.001 || mask.y2 < 0.001)))
            return;
        mask.validate();
        auto next = history_.current();
        next.localMasks[static_cast<std::size_t>(localIndex())] = mask;
        applyAdvanced(std::move(next));
    } catch (const std::exception &failure) {
        advancedFailure(failure);
    }
}
QVariantList EditorController::localHandles() const {
    QVariantList result;
    if (!session_ || !localMode_ || localIndex() < 0)
        return result;
    try {
        const auto &mask = history_.current().localMasks[static_cast<std::size_t>(localIndex())];
        const auto &image = *session_->preview;
        const luma::CropGeometry geometry(image.width, image.height,
                                          session_->asset.metadata.orientation, history_.current());
        std::array<std::array<double, 2>, 3> points;
        if (mask.shape == luma::MaskShape::linear)
            points = {{{mask.x1, mask.y1},
                       {mask.x2, mask.y2},
                       {(mask.x1 + mask.x2) / 2, (mask.y1 + mask.y2) / 2}}};
        else {
            const double r = mask.angle * std::numbers::pi / 180;
            points = {{{mask.x1, mask.y1},
                       {mask.x1 + std::cos(r) * mask.x2, mask.y1 + std::sin(r) * mask.x2},
                       {mask.x1 - std::sin(r) * mask.y2, mask.y1 + std::cos(r) * mask.y2}}};
        }
        for (std::size_t i = 0; i < points.size(); ++i) {
            const auto point = geometry.output(points[i][0] * image.width - 0.5,
                                               points[i][1] * image.height - 0.5);
            result.append(QVariantMap{{"x", (point[0] + 0.5) / geometry.width()},
                                      {"y", (point[1] + 0.5) / geometry.height()},
                                      {"handle", static_cast<int>(i)}});
        }
    } catch (const std::exception &) {
        return {};
    }
    return result;
}
