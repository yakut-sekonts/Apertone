#pragma once
#include "core/render/DevelopPipeline.h"
namespace luma {
struct MaskOverlay {
    std::string id;
    std::array<double, 3> color{1, 0.15, 0.1};
    double opacity = 0.5;
};
class LocalAdjustmentPass {
  public:
    explicit LocalAdjustmentPass(const std::vector<LocalMask> &masks);
    std::array<float, 3> pixel(std::array<float, 3> color, double x, double y) const;

  private:
    struct Layer {
        LocalMask mask;
        DevelopPipeline pipeline;
    };
    std::vector<Layer> layers_;
};
} // namespace luma
