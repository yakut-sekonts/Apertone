#pragma once
#include "core/edits/EditDocument.h"
#include "core/render/AdvancedColor.h"
namespace luma {
bool creativeColorActive(const EditDocument &edits);
double pointColorWeight(const std::array<float, 3> &rgb, const CreativeSettings &settings);
std::array<float, 3> applyCreativeColor(std::array<float, 3> rgb, const EditDocument &edits);
std::array<float, 3> applyOutputEffects(std::array<float, 3> rgb, const EditDocument &edits, int x,
                                        int y, int width, int height, double scale = 1);
class DefringePass {
  public:
    DefringePass(const EditDocument &edits, int width, int height, double scale = 1);
    template <class Sample>
    std::array<float, 3> pixel(std::array<float, 3> rgb, int x, int y, const Sample &sample) const {
        if (amount_ == 0)
            return rgb;
        const auto weight = strength(rgb);
        if (weight <= 0)
            return rgb;
        const auto luma = [](const auto &color) {
            return 0.2126 * color[0] + 0.7152 * color[1] + 0.0722 * color[2];
        };
        const auto yCenter = luma(rgb);
        const auto across = std::abs(luma(sample(std::max(0, x - reach_), y)) -
                                     luma(sample(std::min(imageWidth_ - 1, x + reach_), y)));
        const auto down = std::abs(luma(sample(x, std::max(0, y - reach_))) -
                                   luma(sample(x, std::min(imageHeight_ - 1, y + reach_))));
        const auto edge = std::clamp(std::max(across, down) / std::max(.10, std::abs(yCenter) * .5), 0.0, 1.0);
        for (auto &channel : rgb)
            channel = static_cast<float>(std::lerp(static_cast<double>(channel), yCenter, weight * edge));
        return rgb;
    }
  private:
    double strength(const std::array<float, 3> &rgb) const;
    double amount_, hue_, width_;
    int imageWidth_, imageHeight_, reach_;
};
} // namespace luma
