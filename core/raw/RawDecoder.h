#pragma once
#include "core/color/RawColorCalibration.h"
#include "core/image/FloatImage.h"
#include "core/jobs/Cancellation.h"
#include "core/metadata/PhotoMetadata.h"
#include <filesystem>
#include <functional>

namespace luma {
struct DecodedRaw {
    FloatImage image;
    PhotoMetadata metadata;
    RawColorCalibration calibration;
};

class RawDecoder {
  public:
    PhotoMetadata inspect(const std::filesystem::path &path, CancellationToken stop = {}) const;
    std::vector<std::uint8_t> embeddedJPEG(const std::filesystem::path &path,
                                           CancellationToken stop = {}) const;
    DecodedRaw decode(const std::filesystem::path &path, luma::CancellationToken stop = {},
                      const std::function<void(double)> &progress = {}) const;
};
} // namespace luma
