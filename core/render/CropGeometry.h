#pragma once
#include "core/edits/EditDocument.h"
#include "core/render/TransformMapping.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace luma {
class CropGeometry {
  public:
    CropGeometry(int sourceWidth, int sourceHeight, int orientation, const EditDocument &edits);
    int width() const {
        return width_;
    }
    int height() const {
        return height_;
    }
    int canvasWidth() const {
        return canvasWidth_;
    }
    int canvasHeight() const {
        return canvasHeight_;
    }
    bool opaque() const {
        return transform_.opaque();
    }
    double transformZoom() const {
        return transform_.extraZoom();
    }
    bool covered(double x, double y) const {
        if (opaque())
            return true;
        const auto point = source(x, y);
        return point[0] >= -1e-8 && point[1] >= -1e-8 && point[0] <= sourceWidth_ - 1 + 1e-8 &&
               point[1] <= sourceHeight_ - 1 + 1e-8;
    }
    std::array<double, 4> frame() const;
    std::array<double, 2> output(double x, double y) const;
    std::array<double, 2> source(double x, double y) const;
    template <class Pixel> std::array<float, 3> sample(int x, int y, const Pixel &pixel) const {
        const auto point = source(x, y);
        if (!opaque() &&
            (point[0] < -1e-8 || point[1] < -1e-8 || point[0] > sourceWidth_ - 1 + 1e-8 ||
             point[1] > sourceHeight_ - 1 + 1e-8))
            return {};
        const double sx = std::clamp(point[0], 0.0, static_cast<double>(sourceWidth_ - 1));
        const double sy = std::clamp(point[1], 0.0, static_cast<double>(sourceHeight_ - 1));
        const int x0 = static_cast<int>(std::floor(sx)), y0 = static_cast<int>(std::floor(sy));
        const double fx = sx - x0, fy = sy - y0;
        if (fx == 0 && fy == 0)
            return pixel(x0, y0);
        const int x1 = std::min(x0 + 1, sourceWidth_ - 1), y1 = std::min(y0 + 1, sourceHeight_ - 1);
        const auto a = pixel(x0, y0), b = pixel(x1, y0), c = pixel(x0, y1), d = pixel(x1, y1);
        std::array<float, 3> result{};
        for (std::size_t channel = 0; channel < 3; ++channel) {
            result[channel] =
                static_cast<float>((1 - fy) * ((1 - fx) * a[channel] + fx * b[channel]) +
                                   fy * ((1 - fx) * c[channel] + fx * d[channel]));
            if (!std::isfinite(result[channel]))
                throw std::runtime_error("Non-finite transformed pixel.");
        }
        return result;
    }

  private:
    int sourceWidth_, sourceHeight_, uprightWidth_, uprightHeight_, baseWidth_, baseHeight_;
    int canvasWidth_, canvasHeight_, width_, height_, left_, top_, orientation_;
    CropSettings crop_;
    TransformMapping transform_;
    double cosine_, sine_;
};
} // namespace luma
