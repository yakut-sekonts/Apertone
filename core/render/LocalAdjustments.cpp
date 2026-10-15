#include "core/render/LocalAdjustments.h"
namespace luma {
LocalAdjustmentPass::LocalAdjustmentPass(const std::vector<LocalMask> &masks, double aspect) {
    validateLocalMasks(masks);
    for (const auto &mask : masks) {
        std::optional<DevelopPipeline> pipeline;
        if (!mask.neutral()) {
            EditDocument settings;
            for (std::size_t i = 0; i < localParameters.size(); ++i)
                settings.setParameter(localParameters[i], mask.adjustments[i]);
            pipeline.emplace(settings);
        }
        layers_.push_back({MaskCoverage(mask, aspect), std::move(pipeline)});
    }
}
std::array<float, 3> LocalAdjustmentPass::pixel(std::array<float, 3> color, double x,
                                                double y) const {
    const auto reference = color;
    for (const auto &layer : layers_) {
        if (!layer.pipeline)
            continue;
        const double amount = layer.coverage.coverage(x, y, reference);
        if (amount == 0)
            continue;
        const auto adjusted = layer.pipeline->pixel(color);
        for (std::size_t c = 0; c < 3; ++c)
            color[c] = static_cast<float>(color[c] + (adjusted[c] - color[c]) * amount);
    }
    return color;
}
double LocalAdjustmentPass::coverage(std::size_t mask, double x, double y,
                                     std::array<float, 3> reference) const {
    return layers_.at(mask).coverage.coverage(x, y, reference);
}
} // namespace luma
