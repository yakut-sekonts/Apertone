#pragma once
#include "core/image/Bayer.h"
#include "core/jobs/Cancellation.h"
#include <array>
#include <functional>
#include <vector>

namespace luma {
class LocalBlurPass {
  public:
    LocalBlurPass(int width, int height, bool active,
                  const std::function<std::array<float, 3>(int, int)> &source,
                  CancellationToken stop);
    bool active() const {
        return !pixels_.empty();
    }
    std::array<float, 3> pixel(int x, int y) const;

  private:
    int width_ = 0, height_ = 0, sourceWidth_ = 0, sourceHeight_ = 0;
    std::vector<float> pixels_;
};
} // namespace luma
