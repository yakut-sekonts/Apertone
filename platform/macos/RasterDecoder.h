#pragma once
#include "core/image/DecodedImage.h"
#include "core/jobs/Cancellation.h"
#include <filesystem>
#include <functional>
#include <string>

namespace luma::macos {
class RasterDecoder {
  public:
    bool available(const std::string &extension) const;
    bool cameraRaw(const std::filesystem::path &path) const;
    PhotoMetadata inspect(const std::filesystem::path &path, CancellationToken stop = {}) const;
    DecodedImage decode(const std::filesystem::path &path, CancellationToken stop = {},
                        const std::function<void(double)> &progress = {}) const;
};
} // namespace luma::macos
