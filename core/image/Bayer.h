#pragma once
#include "core/image/FloatImage.h"
#include <stop_token>

namespace luma {
void checkCancellation(std::stop_token stop);
FloatImage demosaicBilinear(const BayerFrame &frame, std::stop_token stop = {});
void cameraToLinearSRGB(FloatImage &image, const std::array<float, 9> &matrix,
                        std::stop_token stop = {});
} // namespace luma
