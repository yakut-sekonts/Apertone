#pragma once
#include "core/edits/TransformSettings.h"

namespace luma {
class TransformMapping {
  public:
    TransformMapping(int width, int height, int orientation, const TransformSettings &settings,
                     bool enabled);
    std::array<double, 2> sourceFromSensorCanvas(double x, double y) const;
    std::array<double, 2> sensorCanvasFromSource(double x, double y) const;
    bool active() const {
        return active_;
    }
    bool opaque() const {
        return !active_ || constrain_;
    }
    double extraZoom() const {
        return zoom_;
    }
    Homography forward() const {
        return forward_;
    }

  private:
    int width_, height_, uprightWidth_, uprightHeight_, orientation_;
    bool active_, constrain_;
    Homography forward_, inverse_;
    double zoom_ = 1;
    bool fits(double zoom) const;
    bool poleFree(double zoom) const;
};
} // namespace luma
