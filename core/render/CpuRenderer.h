#pragma once
#include "core/edits/EditDocument.h"
#include "core/image/FloatImage.h"
#include <cstdint>
#include <span>
#include <stop_token>

namespace luma {
struct DisplayImage {
    int width;
    int height;
    std::vector<std::uint8_t> rgba;
};
FloatImage standardPreview(const FloatImage &input, int maximumDimension,
                           std::stop_token stop = {});
DisplayImage renderCPU(const FloatImage &input, EditDocument edits, int orientation = 0,
                       std::span<const std::uint8_t> displayProfile = {},
                       std::stop_token stop = {});
float applyExposure(float linearValue, double ev);
} // namespace luma
