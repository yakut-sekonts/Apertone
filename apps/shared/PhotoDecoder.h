#pragma once
#include "core/image/DecodedImage.h"
#include "core/jobs/Cancellation.h"
#include <filesystem>
#include <functional>

namespace luma {
class PhotoDecoder {
  public:
    static bool supportedExtension(const std::filesystem::path &path);
    PhotoMetadata inspect(const std::filesystem::path &path, CancellationToken stop = {}) const;
    DecodedImage decode(const std::filesystem::path &path, CancellationToken stop = {},
                        const std::function<void(double)> &progress = {}) const;
};
} // namespace luma
