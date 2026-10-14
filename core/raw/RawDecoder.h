#pragma once
#include "core/color/RawColorCalibration.h"
#include "core/image/DecodedImage.h"
#include "core/jobs/Cancellation.h"
#include "core/metadata/PhotoMetadata.h"
#include <filesystem>
#include <functional>

namespace luma {
// Compatibility name for existing RAW callers; the payload also represents raster photos.
using DecodedRaw = DecodedImage;

class RawDecoder {
  public:
    bool recognizes(const std::filesystem::path &path, CancellationToken stop = {}) const;
    PhotoMetadata inspect(const std::filesystem::path &path, CancellationToken stop = {}) const;
    std::vector<std::uint8_t> embeddedJPEG(const std::filesystem::path &path,
                                           CancellationToken stop = {}) const;
    DecodedRaw decode(const std::filesystem::path &path, luma::CancellationToken stop = {},
                      const std::function<void(double)> &progress = {}) const;
};
} // namespace luma
