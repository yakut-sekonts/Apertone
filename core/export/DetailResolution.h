#pragma once
#include "core/export/LinearOutput.h"

namespace luma {
// Edge-aware microcontrast in scene-linear sRGB. Amount is normalized to [0, 1].
void enhanceRawDetails(LinearOutput &image, double amount, CancellationToken stop = {},
                       const std::function<void(double)> &progress = {});

// Deterministic 2x Lanczos-3 resampling in scene-linear sRGB. Straight alpha is
// premultiplied for filtering and restored afterward.
LinearOutput upscaleDetail2x(LinearOutput input, CancellationToken stop = {},
                             const std::function<void(double)> &progress = {});
} // namespace luma
