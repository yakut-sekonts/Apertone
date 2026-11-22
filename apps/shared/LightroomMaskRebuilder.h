#pragma once

#include "apps/shared/LightroomDevelop.h"
#include "core/ai/Providers.h"
#include "core/render/CpuRenderer.h"
#include "core/render/CropGeometry.h"
#include <filesystem>
#include <optional>

namespace luma {
// A new local selection, used only when the user requests an approximation
// and the catalog and its history have no compatible saved mask.
class LightroomMaskRebuilder {
  public:
    LightroomMaskRebuilder(const std::filesystem::path &photo,
                           const std::shared_ptr<ai::ISegmentationProvider> &provider,
                           CancellationToken stop)
        : photo_(photo), provider_(provider), stop_(stop) {}
    BitmapMask rebuild(const LightroomProperties &component);

  private:
    const std::filesystem::path &photo_;
    std::shared_ptr<ai::ISegmentationProvider> provider_;
    CancellationToken stop_;
    std::optional<CropGeometry> geometry_;
    std::optional<DisplayImage> image_;
    int sourceWidth_ = 0, sourceHeight_ = 0;
    std::map<std::pair<ai::Selection, std::array<double, 2>>, BitmapMask> cached_;
};
} // namespace luma
