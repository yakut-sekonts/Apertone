#pragma once
#include "core/color/RawColorCalibration.h"
#include "core/edits/EditDocument.h"
#include "core/render/AdvancedColor.h"
#include <array>
#include <optional>

namespace luma {
class DevelopPipeline {
  public:
    explicit DevelopPipeline(EditDocument edits, RawColorCalibration calibration = {});
    std::array<float, 3> pixel(std::array<float, 3> camera) const;
    // Matrix[9], WB[3], exposure, contrast, shadows, highlights, blacks,
    // whites, saturation and vibrance. No active Curves/HSL in this GPU path.
    std::optional<std::array<float, 20>> basicGpuParameters() const;

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
