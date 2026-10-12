#pragma once
#include "core/image/FloatImage.h"
#include "core/metadata/PhotoMetadata.h"
#include <filesystem>
#include <functional>
#include <stop_token>

namespace luma {
struct DecodedRaw {
    FloatImage image;
    PhotoMetadata metadata;
};

class RawDecoder {
  public:
    DecodedRaw decode(const std::filesystem::path &path, std::stop_token stop = {},
                      const std::function<void(double)> &progress = {}) const;
};
} // namespace luma
