#pragma once
#include "core/color/RawColorCalibration.h"
#include "core/edits/EditDocument.h"
#include "core/image/FloatImage.h"
#include "core/jobs/Cancellation.h"
#include <cstdint>
#include <span>

namespace luma {
struct DisplayImage {
    int width;
    int height;
    std::vector<std::uint8_t> rgba;
};
FloatImage standardPreview(const FloatImage &input, int maximumDimension,
                           luma::CancellationToken stop = {});
DisplayImage renderCPU(const FloatImage &input, EditDocument edits, int orientation = 0,
                       std::span<const std::uint8_t> displayProfile = {},
                       luma::CancellationToken stop = {}, RawColorCalibration calibration = {});
float applyExposure(float linearValue, double ev);
} // namespace luma
