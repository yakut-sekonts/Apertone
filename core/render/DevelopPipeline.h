#pragma once
#include "core/color/RawColorCalibration.h"
#include "core/edits/EditDocument.h"
#include "core/render/AdvancedColor.h"
#include <array>

namespace luma {
class DevelopPipeline {
  public:
    explicit DevelopPipeline(EditDocument edits, RawColorCalibration calibration = {});
    std::array<float, 3> pixel(std::array<float, 3> camera) const;

  private:
    EditDocument edits_;
    RawColorCalibration calibration_;
    std::array<float, 3> gains_;
    float exposureGain_ = 1;
    std::array<ToneCurve, 4> curves_;
    bool curvesActive_ = false;
    bool hslActive_ = false;
};
} // namespace luma
