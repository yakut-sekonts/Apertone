#pragma once
#include "core/edits/TransformSettings.h"
#include "core/render/CpuRenderer.h"

namespace luma {
enum class GuideAxis { vertical, horizontal };
struct PerspectiveGuide {
    GuideAxis axis = GuideAxis::vertical;
    std::array<double, 2> first{}, second{}; // Upright source coordinates, 0..1.
    void validate() const;
};
TransformSettings solvePerspectiveGuides(const std::vector<PerspectiveGuide> &guides,
                                         double imageAspect);
std::vector<PerspectiveGuide> detectPerspectiveGuides(const DisplayImage &image,
                                                      const CancellationToken &stop = {});
} // namespace luma
