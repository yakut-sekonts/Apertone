#pragma once
#include "core/ai/Providers.h"
#include "core/math/Homography.h"
namespace luma::macos {
// Input is upright sRGB, equal dimensions, maximum 1024 pixels per edge.
// Returns floating -> reference in normalized top-left coordinates.
Homography alignImages(ai::RgbaImage reference, ai::RgbaImage floating,
                       CancellationToken stop = {});
} // namespace luma::macos
