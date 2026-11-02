#pragma once
// This product includes DNG technology under license by Adobe.
#include "core/color/RawColorCalibration.h"
#include <optional>

namespace luma {
struct ResolvedRawColorCalibration {
    std::array<float, 9> cameraToSRGB{1, 0, 0, 0, 1, 0, 0, 0, 1};
    std::array<float, 3> gains{1, 1, 1};
};

// DNG CalibrationIlluminant values with a defined correlated color temperature.
std::optional<double> dngIlluminantTemperature(unsigned illuminant);

// Resolve a DNG camera profile for the requested WB. Non-DNG calibrations retain
// their fixed camera matrix and use the existing Temperature/Tint gain model.
ResolvedRawColorCalibration resolveRawColorCalibration(const RawColorCalibration &calibration,
                                                       double temperature, double tint);

// Validate the embedded profile, infer its As Shot interpolation point and fill
// the fixed As Shot matrix used when Custom White Balance is disabled.
void finalizeDngColorCalibration(RawColorCalibration &calibration);
} // namespace luma
