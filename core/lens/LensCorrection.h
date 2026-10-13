#pragma once
#include "core/edits/EditDocument.h"
#include "core/image/FloatImage.h"
#include "core/jobs/Cancellation.h"
#include "core/lens/LensProfiles.h"
#include <optional>

namespace luma {
class LensMapping {
  public:
    LensMapping(int width, int height, const ResolvedLens &resolved, const LensSettings &settings);
    bool geometryActive() const;
    bool vignetteActive() const;
    std::array<double, 2> source(double x, double y, std::size_t channel) const;
    double gain(double x, double y) const;
    double zoom() const {
        return zoom_;
    }

  private:
    std::array<double, 6> distortion_{1, 0, 0, 0, 0, 0};
    std::array<double, 6> red_{1, 0, 0, 0, 0, 0}, blue_{1, 0, 0, 0, 0, 0};
    std::array<double, 6> vignette_{1, 0, 0, 0, 0, 0};
    double norm_ = 1, aspect_ = 1, centerX_ = 0, centerY_ = 0;
    double vignetteAmount_ = 0, zoom_ = 1;
};
class LensCorrectionPass {
  public:
    LensCorrectionPass(const FloatImage &input, const EditDocument &edits,
                       const PhotoMetadata &metadata, const CancellationToken &stop = {});
    const FloatImage &image() const {
        return corrected_ ? *corrected_ : input_;
    }
    bool active() const {
        return corrected_.has_value();
    }

  private:
    const FloatImage &input_;
    std::optional<FloatImage> corrected_;
};
} // namespace luma
