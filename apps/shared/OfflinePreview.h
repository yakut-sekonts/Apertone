#pragma once

#include "core/color/RawColorCalibration.h"
#include "core/image/FloatImage.h"
#include "core/jobs/Cancellation.h"
#include <filesystem>
#include <optional>
#include <string>

namespace luma {
struct OfflinePreview {
    FloatImage image;
    RawColorCalibration calibration;
};

std::optional<OfflinePreview> loadOfflinePreview(const std::filesystem::path &catalog,
                                                 const std::string &checksum,
                                                 CancellationToken stop);
void saveOfflinePreview(const std::filesystem::path &catalog, const std::string &checksum,
                        const FloatImage &preview, const RawColorCalibration &calibration,
                        CancellationToken stop, bool repair = false);
bool hasOfflinePreview(const std::filesystem::path &catalog, const std::string &checksum);
} // namespace luma
