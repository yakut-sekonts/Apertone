#pragma once

#include "core/ai/Providers.h"
#include "core/math/Homography.h"

namespace luma {
// Looks for a translation, with an optional native registration as a candidate.
// Returns floating to reference in normalized top-left coordinates.
Homography verifiedMergeAlignment(ai::RgbaImage reference, ai::RgbaImage floating,
                                  const Homography &initial, CancellationToken stop);
} // namespace luma
