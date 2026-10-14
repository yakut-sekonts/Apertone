#pragma once
#include "core/export/LinearOutput.h"

namespace luma::macos {
void applyWatermark(LinearOutput &output, const WatermarkOptions &options,
                    CancellationToken stop = {});
} // namespace luma::macos
