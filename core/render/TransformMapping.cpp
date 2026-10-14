#include "core/render/TransformMapping.h"
#include "core/image/FloatImage.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>

namespace luma {
TransformMapping::TransformMapping(int width, int height, int orientation,
                                   const TransformSettings &settings, bool enabled)
    : width_(width), height_(height), uprightWidth_(orientation & 4 ? height : width),
      uprightHeight_(orientation & 4 ? width : height), orientation_(orientation),
      active_(enabled && !settings.neutral()), constrain_(settings.constrainCrop) {
    pixelCount(width, height);
    settings.validate();
    if (orientation < 0 || orientation > 7)
        throw std::runtime_error("Invalid Transform orientation.");
    if (!active_ || width == 1 || height == 1) {
        active_ = false;
        return;
    }
    const double angle = settings.rotate * std::numbers::pi / 180;
    const double ratio = static_cast<double>(uprightWidth_ - 1) / (uprightHeight_ - 1);
    const Homography rotation{{std::cos(angle), -std::sin(angle) / ratio, 0,
                               std::sin(angle) * ratio, std::cos(angle), 0, 0, 0, 1}};
    const double aspect = std::exp2(settings.aspect / 100);
    const Homography stretch{{aspect, 0, 0, 0, 1 / aspect, 0, 0, 0, 1}};
    const double scale = settings.scale / 100;
    const Homography zoom{
        {scale, 0, settings.xOffset / 200, 0, scale, settings.yOffset / 200, 0, 0, 1}};
    const Homography perspective{
        {1, 0, 0, 0, 1, 0, settings.horizontal * 0.0035, settings.vertical * 0.0035, 1}};
    forward_ = zoom * stretch * rotation * perspective *
               (settings.guided ? settings.guideMatrix() : Homography{});
    inverse_ = forward_.inverse();
    if (constrain_) {
        double lower = 1, upper = 1;
        while (!fits(upper) && upper < 256)
            upper *= 2;
        if (!fits(upper))
            throw std::runtime_error("Transform cannot constrain an image around this center.");
        if (upper > 1) {
            for (int i = 0; i < 48; ++i) {
                const double middle = std::midpoint(lower, upper);
                if (fits(middle))
                    upper = middle;
                else
                    lower = middle;
            }
            zoom_ = upper * (1 + 1e-12);
        }
    } else if (!poleFree(1))
        throw std::runtime_error("Transform has a projective pole inside the output canvas.");
}
bool TransformMapping::poleFree(double zoom) const {
    const double center = inverse_.values[8];
    const double radius = (std::abs(inverse_.values[6]) + std::abs(inverse_.values[7])) / zoom;
    return std::abs(center) > radius + 1e-8;
}
bool TransformMapping::fits(double zoom) const {
    if (!poleFree(zoom))
        return false;
    for (double y : {-1.0, 1.0})
        for (double x : {-1.0, 1.0}) {
            const auto point = inverse_.point(x / zoom, y / zoom);
            if (std::abs(point[0]) > 1 || std::abs(point[1]) > 1)
                return false;
        }
    return true;
}
std::array<double, 2> TransformMapping::sourceFromSensorCanvas(double x, double y) const {
    if (!active_)
        return {x, y};
    // Crop has already inverse-mapped EXIF. Re-enter upright coordinates to
    // compose the projective inverse before returning to sensor coordinates.
    double fx = orientation_ & 1 ? width_ - 1 - x : x;
    double fy = orientation_ & 2 ? height_ - 1 - y : y;
    const double ux = orientation_ & 4 ? fy : fx, uy = orientation_ & 4 ? fx : fy;
    const auto normalized = inverse_.point((2 * ux / (uprightWidth_ - 1) - 1) / zoom_,
                                           (2 * uy / (uprightHeight_ - 1) - 1) / zoom_);
    const double sx = (normalized[0] + 1) * (uprightWidth_ - 1) / 2;
    const double sy = (normalized[1] + 1) * (uprightHeight_ - 1) / 2;
    fx = orientation_ & 4 ? sy : sx;
    fy = orientation_ & 4 ? sx : sy;
    return {orientation_ & 1 ? width_ - 1 - fx : fx, orientation_ & 2 ? height_ - 1 - fy : fy};
}
} // namespace luma
