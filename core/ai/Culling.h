#pragma once

#include "core/ai/Providers.h"
#include <array>
#include <memory>
#include <vector>

namespace luma::ai {

enum class CullingRecommendation : int { reject = -1, review = 0, pick = 1 };

enum CullingReason : std::uint32_t {
    noCullingReason = 0,
    softFocus = 1U << 0,
    exposureIssue = 1U << 1,
    lowFaceQuality = 1U << 2,
    closedEyes = 1U << 3,
};

struct FaceObservation {
    // Normalized top-left coordinates in the oriented preview.
    std::array<double, 4> bounds{};
    double quality = .5;
    double eyesOpen = .5;
    std::vector<float> embedding;
};

struct CullingObservation {
    double sharpness = 0;
    double exposure = 0;
    double faceQuality = .5;
    double eyesOpen = .5;
    double score = 0;
    CullingRecommendation recommendation = CullingRecommendation::review;
    std::uint32_t reasons = noCullingReason;
    std::vector<float> imageEmbedding;
    std::vector<FaceObservation> faces;
    void validate() const;
};

class ICullingProvider {
  public:
    virtual ~ICullingProvider() = default;
    virtual std::string identifier() const = 0;
    virtual ExecutionLocation location() const = 0;
    virtual CullingObservation analyze(RgbaImage image, CancellationToken stop,
                                       const Progress &progress = {}) = 0;
};

// Deterministic quality metrics over an oriented sRGB preview. Vision supplies
// faces and feature prints; these metrics remain testable without macOS APIs.
CullingObservation scoreCullingPreview(RgbaImage image, std::vector<float> imageEmbedding,
                                       std::vector<FaceObservation> faces,
                                       CancellationToken stop = {});

} // namespace luma::ai
