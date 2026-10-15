#include "core/render/LocalAdjustments.h"
namespace luma {
LocalAdjustmentPass::LocalAdjustmentPass(const std::vector<LocalMask> &masks) {
    validateLocalMasks(masks);
    for (const auto &mask : masks) {
        if (mask.neutral())
            continue;
        EditDocument settings;
        for (std::size_t i = 0; i < localParameters.size(); ++i)
            settings.setParameter(localParameters[i], mask.adjustments[i]);
        layers_.push_back({mask, DevelopPipeline(settings)});
    }
}
std::array<float, 3> LocalAdjustmentPass::pixel(std::array<float, 3> color, double x,
                                                double y) const {
    for (const auto &layer : layers_) {
        const auto coverage = layer.mask.coverage(x, y);
        if (coverage == 0)
            continue;
        const auto adjusted = layer.pipeline.pixel(color);
        for (std::size_t c = 0; c < 3; ++c)
            color[c] = static_cast<float>(color[c] + (adjusted[c] - color[c]) * coverage);
    }
    return color;
}
} // namespace luma
