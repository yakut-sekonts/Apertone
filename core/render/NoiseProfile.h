#pragma once
#include "core/image/FloatImage.h"
#include "core/jobs/Cancellation.h"
#include "core/metadata/PhotoMetadata.h"
#include <cstddef>

namespace luma {
struct NoiseProfileRecommendation {
    double luminance = 0;
    double luminanceDetail = 50;
    double luminanceContrast = 0;
    double color = 0;
    double colorDetail = 50;
    double colorSmoothness = 50;
    double measuredLuminance = 0;
    double measuredColor = 0;
    std::size_t sampleCount = 0;
    bool measurementAvailable = false;
};

// Estimate conservative Develop noise settings from locally flat scene-linear
// samples. ISO is a bounded prior; camera identity is deliberately not treated
// as calibration data.
NoiseProfileRecommendation estimateNoiseProfile(const FloatImage &image,
                                                const PhotoMetadata &metadata,
                                                CancellationToken stop = {});
} // namespace luma
