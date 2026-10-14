#include "core/render/CropGeometry.h"
#include "core/image/FloatImage.h"
#include <numbers>

namespace luma {
CropGeometry::CropGeometry(int sourceWidth, int sourceHeight, int orientation,
                           const EditDocument &edits)
    : sourceWidth_(sourceWidth), sourceHeight_(sourceHeight), orientation_(orientation),
      crop_(edits.enabled(Correction::crop) ? edits.crop : CropSettings{}),
      transform_(sourceWidth, sourceHeight, orientation, edits.transform,
                 edits.enabled(Correction::transform)) {
    pixelCount(sourceWidth, sourceHeight);
    edits.validate();
    if (orientation < 0 || orientation > 7)
        throw std::runtime_error("Invalid geometry orientation.");
    uprightWidth_ = orientation & 4 ? sourceHeight : sourceWidth;
    uprightHeight_ = orientation & 4 ? sourceWidth : sourceHeight;
    baseWidth_ = crop_.quarterTurns % 2 ? uprightHeight_ : uprightWidth_;
    baseHeight_ = crop_.quarterTurns % 2 ? uprightWidth_ : uprightHeight_;
    const double radians = crop_.angle * std::numbers::pi / 180;
    cosine_ = std::cos(radians);
    sine_ = std::sin(radians);
    const double zoom = std::max(std::abs(cosine_) + std::abs(sine_) * baseHeight_ / baseWidth_,
                                 std::abs(cosine_) + std::abs(sine_) * baseWidth_ / baseHeight_);
    canvasWidth_ = std::max(1, static_cast<int>(std::floor(baseWidth_ / zoom)));
    canvasHeight_ = std::max(1, static_cast<int>(std::floor(baseHeight_ / zoom)));
    double w = crop_.width * canvasWidth_, h = crop_.height * canvasHeight_;
    const double ratio =
        crop_.aspect == -1 ? static_cast<double>(baseWidth_) / baseHeight_ : crop_.aspect;
    if (ratio > 0) {
        w = std::min(w, h * ratio);
        h = w / ratio;
    }
    const double x = crop_.x * canvasWidth_ + (crop_.width * canvasWidth_ - w) / 2;
    const double y = crop_.y * canvasHeight_ + (crop_.height * canvasHeight_ - h) / 2;
    if (ratio > 0) {
        width_ = std::clamp(static_cast<int>(std::lround(w)), 1, canvasWidth_);
        height_ = std::clamp(static_cast<int>(std::lround(h)), 1, canvasHeight_);
        left_ = std::clamp(static_cast<int>(std::lround(x)), 0, canvasWidth_ - width_);
        top_ = std::clamp(static_cast<int>(std::lround(y)), 0, canvasHeight_ - height_);
    } else {
        left_ = std::clamp(static_cast<int>(std::lround(x)), 0, canvasWidth_ - 1);
        top_ = std::clamp(static_cast<int>(std::lround(y)), 0, canvasHeight_ - 1);
        const int right = std::clamp(static_cast<int>(std::lround(x + w)), left_ + 1, canvasWidth_);
        const int bottom =
            std::clamp(static_cast<int>(std::lround(y + h)), top_ + 1, canvasHeight_);
        width_ = right - left_;
        height_ = bottom - top_;
    }
}
std::array<double, 4> CropGeometry::frame() const {
    return {static_cast<double>(left_) / canvasWidth_, static_cast<double>(top_) / canvasHeight_,
            static_cast<double>(width_) / canvasWidth_,
            static_cast<double>(height_) / canvasHeight_};
}
std::array<double, 2> CropGeometry::source(double x, double y) const {
    if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || y < 0 || x > width_ - 1 ||
        y > height_ - 1)
        throw std::runtime_error("Geometry target outside output.");
    const double dx = left_ + x + 0.5 - canvasWidth_ / 2.0;
    const double dy = top_ + y + 0.5 - canvasHeight_ / 2.0;
    double bx = cosine_ * dx + sine_ * dy + baseWidth_ / 2.0 - 0.5;
    double by = -sine_ * dx + cosine_ * dy + baseHeight_ / 2.0 - 0.5;
    if (crop_.flipHorizontal)
        bx = baseWidth_ - 1 - bx;
    if (crop_.flipVertical)
        by = baseHeight_ - 1 - by;
    double ux = bx, uy = by;
    switch (crop_.quarterTurns) {
    case 1:
        ux = by;
        uy = uprightHeight_ - 1 - bx;
        break;
    case 2:
        ux = uprightWidth_ - 1 - bx;
        uy = uprightHeight_ - 1 - by;
        break;
    case 3:
        ux = uprightWidth_ - 1 - by;
        uy = bx;
        break;
    default:
        break;
    }
    double sx = orientation_ & 4 ? uy : ux, sy = orientation_ & 4 ? ux : uy;
    if (orientation_ & 1)
        sx = sourceWidth_ - 1 - sx;
    if (orientation_ & 2)
        sy = sourceHeight_ - 1 - sy;
    return transform_.sourceFromSensorCanvas(sx, sy);
}
} // namespace luma
