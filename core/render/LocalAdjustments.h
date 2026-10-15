#pragma once
#include "core/render/DevelopPipeline.h"
#include "core/render/MaskCoverage.h"
#include <optional>
namespace luma {
struct MaskOverlay {
    std::string id;
    std::array<double, 3> color{1, 0.15, 0.1};
    double opacity = 0.5;
};
class LocalAdjustmentPass {
  public:
    explicit LocalAdjustmentPass(const std::vector<LocalMask> &masks, double aspect = 1);
    std::array<float, 3> pixel(std::array<float, 3> color, double x, double y) const;
    double coverage(std::size_t mask, double x, double y, std::array<float, 3> reference) const;

  private:
    struct Layer {
        MaskCoverage coverage;
        std::optional<DevelopPipeline> pipeline;
    };
    std::vector<Layer> layers_;
};
} // namespace luma
