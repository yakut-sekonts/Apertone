#include "core/ai/Culling.h"
#include "core/ai/Embedding.h"
#include "core/image/Bayer.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace luma::ai {
namespace {
bool unit(double value) {
    return std::isfinite(value) && value >= 0 && value <= 1;
}
double luminance(const std::uint8_t *pixel) {
    return (.2126 * pixel[0] + .7152 * pixel[1] + .0722 * pixel[2]) / 255.;
}
} // namespace

void CullingObservation::validate() const {
    if (!unit(sharpness) || !unit(exposure) || !unit(faceQuality) || !unit(eyesOpen) ||
        !unit(score) ||
        (recommendation != CullingRecommendation::reject &&
         recommendation != CullingRecommendation::review &&
         recommendation != CullingRecommendation::pick) ||
        (reasons & ~(softFocus | exposureIssue | lowFaceQuality | closedEyes)) != 0 ||
        imageEmbedding.empty() || imageEmbedding.size() > 2048 || faces.size() > 64)
        throw std::runtime_error("Invalid culling observation.");
    (void)normalizedEmbedding(imageEmbedding);
    for (const auto &face : faces) {
        if (!unit(face.quality) || !unit(face.eyesOpen) || face.embedding.empty() ||
            face.embedding.size() > 2048)
            throw std::runtime_error("Invalid culling face observation.");
        for (const auto value : face.bounds)
            if (!unit(value))
                throw std::runtime_error("Invalid culling face bounds.");
        if (face.bounds[2] <= 0 || face.bounds[3] <= 0 ||
            face.bounds[0] + face.bounds[2] > 1.000001 ||
            face.bounds[1] + face.bounds[3] > 1.000001)
            throw std::runtime_error("Invalid culling face rectangle.");
        (void)normalizedEmbedding(face.embedding);
    }
}

CullingObservation scoreCullingPreview(RgbaImage image, std::vector<float> imageEmbedding,
                                       std::vector<FaceObservation> faces,
                                       const CancellationToken &stop) {
    image.validate();
    if (image.width < 8 || image.height < 8)
        throw std::runtime_error("Culling preview is too small.");
    auto embedding = normalizedEmbedding(imageEmbedding);
    for (auto &face : faces)
        face.embedding = normalizedEmbedding(face.embedding);

    double laplacianMean = 0, laplacianSquare = 0, luminanceSum = 0;
    std::size_t laplacianCount = 0, dark = 0, bright = 0, samples = 0;
    constexpr int stride = 2;
    for (int y = 1; y + 1 < image.height; y += stride) {
        checkCancellation(stop);
        for (int x = 1; x + 1 < image.width; x += stride) {
            const auto at = [&](int sx, int sy) {
                return luminance(image.pixels.data() +
                                 (static_cast<std::size_t>(sy) * image.width + sx) * 4);
            };
            const double center = at(x, y);
            const double laplacian =
                at(x - 1, y) + at(x + 1, y) + at(x, y - 1) + at(x, y + 1) - 4 * center;
            laplacianMean += laplacian;
            laplacianSquare += laplacian * laplacian;
            ++laplacianCount;
            luminanceSum += center;
            dark += center < .015;
            bright += center > .985;
            ++samples;
        }
    }
    const auto laplacianSamples = static_cast<double>(laplacianCount);
    const double variance = std::max(0., laplacianSquare / laplacianSamples -
                                             std::pow(laplacianMean / laplacianSamples, 2));
    // The saturating curve is resolution-independent enough for a 512–1024 px
    // preview and avoids declaring already-sharp images progressively "better".
    const double sharpness = std::clamp(1. - std::exp(-variance * 180.), 0., 1.);
    const auto luminanceSamples = static_cast<double>(samples);
    const double mean = luminanceSum / luminanceSamples;
    const double darkFraction = static_cast<double>(dark) / luminanceSamples;
    const double brightFraction = static_cast<double>(bright) / luminanceSamples;
    const double meanPenalty = mean < .08   ? (.08 - mean) * 3.5
                               : mean > .92 ? (mean - .92) * 3.5
                                            : 0.;
    const double clipPenalty =
        std::max(0., darkFraction - .45) * .8 + std::max(0., brightFraction - .20) * 1.4;
    const double exposure = std::clamp(1. - meanPenalty - clipPenalty, 0., 1.);

    double faceQuality = .5, eyesOpen = .5;
    if (!faces.empty()) {
        faceQuality =
            std::min_element(faces.begin(), faces.end(), [](const auto &first, const auto &second) {
                return first.quality < second.quality;
            })->quality;
        eyesOpen =
            std::min_element(faces.begin(), faces.end(), [](const auto &first, const auto &second) {
                return first.eyesOpen < second.eyesOpen;
            })->eyesOpen;
    }
    const double score =
        faces.empty() ? .68 * sharpness + .32 * exposure
                      : .40 * sharpness + .22 * exposure + .23 * faceQuality + .15 * eyesOpen;
    std::uint32_t reasons = noCullingReason;
    if (sharpness < .32)
        reasons |= softFocus;
    if (exposure < .48)
        reasons |= exposureIssue;
    if (!faces.empty() && faceQuality < .35)
        reasons |= lowFaceQuality;
    if (!faces.empty() && eyesOpen < .30)
        reasons |= closedEyes;
    const auto recommendation = score < .36 || (reasons & closedEyes)
                                    ? CullingRecommendation::reject
                                : score >= .72 ? CullingRecommendation::pick
                                               : CullingRecommendation::review;
    CullingObservation result{sharpness,       exposure,       faceQuality, eyesOpen,
                              score,           recommendation, reasons,     std::move(embedding),
                              std::move(faces)};
    result.validate();
    return result;
}
} // namespace luma::ai
