#include "platform/macos/PhotoCulling.h"
#include "core/ai/Embedding.h"
#include "core/image/Bayer.h"
#include "platform/macos/CFHandle.h"
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <Vision/Vision.h>
#include <algorithm>
#include <cmath>

namespace luma::macos {
namespace {
CFHandle<CGImageRef> cgImage(ai::RgbaImage input) {
    input.validate();
    CFHandle color(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    CFHandle data(
        CGDataProviderCreateWithData(nullptr, input.pixels.data(), input.pixels.size(), nullptr));
    CFHandle image(
        color && data
            ? CGImageCreate(input.width, input.height, 8, 32, input.width * 4, color.get(),
                            static_cast<CGBitmapInfo>(kCGImageAlphaLast) | kCGBitmapByteOrder32Big,
                            data.get(), nullptr, false, kCGRenderingIntentDefault)
            : nullptr);
    if (!image)
        throw std::runtime_error("Cannot prepare the local culling preview.");
    return image;
}

std::vector<float> descriptor(CGImageRef image) {
    constexpr std::size_t side = 16;
    std::array<std::uint8_t, side * side * 4> pixels{};
    CFHandle color(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    CFHandle context(
        color ? CGBitmapContextCreate(pixels.data(), side, side, 8, side * 4, color.get(),
                                      static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) |
                                          kCGBitmapByteOrder32Big)
              : nullptr);
    if (!context)
        throw std::runtime_error("Cannot allocate the perceptual descriptor.");
    CGContextSetInterpolationQuality(context.get(), kCGInterpolationHigh);
    CGContextSetBlendMode(context.get(), kCGBlendModeCopy);
    CGContextDrawImage(context.get(), CGRectMake(0, 0, side, side), image);
    std::array<double, side * side> luminance{};
    double mean = 0, red = 0, green = 0, blue = 0;
    for (std::size_t i = 0; i < side * side; ++i) {
        red += pixels[i * 4] / 255.;
        green += pixels[i * 4 + 1] / 255.;
        blue += pixels[i * 4 + 2] / 255.;
        luminance[i] =
            (.2126 * pixels[i * 4] + .7152 * pixels[i * 4 + 1] + .0722 * pixels[i * 4 + 2]) / 255.;
        mean += luminance[i];
    }
    mean /= luminance.size();
    double variance = 0;
    for (const auto value : luminance)
        variance += (value - mean) * (value - mean);
    const double deviation = std::sqrt(variance / luminance.size());
    const double scale = std::max(.02, deviation);
    std::vector<float> result;
    result.reserve(luminance.size() + 5);
    for (const auto value : luminance)
        result.push_back(static_cast<float>((value - mean) / scale));
    result.push_back(static_cast<float>(red / luminance.size()));
    result.push_back(static_cast<float>(green / luminance.size()));
    result.push_back(static_cast<float>(blue / luminance.size()));
    result.push_back(static_cast<float>(deviation));
    result.push_back(.25F);
    return ai::normalizedEmbedding(result);
}

double overlap(CGRect a, CGRect b) {
    const auto intersection = CGRectIntersection(a, b);
    if (CGRectIsNull(intersection) || CGRectIsEmpty(intersection))
        return 0;
    const double area = intersection.size.width * intersection.size.height;
    const double total = a.size.width * a.size.height + b.size.width * b.size.height - area;
    return total > 0 ? area / total : 0;
}

double openness(VNFaceLandmarkRegion2D *eye) {
    if (!eye || eye.pointCount < 4)
        return .5;
    const auto *points = eye.normalizedPoints;
    CGFloat minX = 1, maxX = 0, minY = 1, maxY = 0;
    for (NSUInteger i = 0; i < eye.pointCount; ++i) {
        minX = std::min(minX, points[i].x);
        maxX = std::max(maxX, points[i].x);
        minY = std::min(minY, points[i].y);
        maxY = std::max(maxY, points[i].y);
    }
    const double width = maxX - minX;
    if (width < 1e-5)
        return .5;
    const double ratio = (maxY - minY) / width;
    return std::clamp((ratio - .07) / .18, 0., 1.);
}

std::vector<ai::FaceObservation> faces(CGImageRef image, CancellationToken stop) {
    checkCancellation(stop);
    VNDetectFaceLandmarksRequest *landmarks = [[VNDetectFaceLandmarksRequest alloc] init];
    landmarks.revision = VNDetectFaceLandmarksRequestRevision3;
    VNImageRequestHandler *handler = [[VNImageRequestHandler alloc] initWithCGImage:image
                                                                            options:@{}];
    NSError *error = nil;
    if (![handler performRequests:@[ landmarks ] error:&error])
        throw std::runtime_error("Vision face detection failed.");
    NSArray<VNFaceObservation *> *observations = landmarks.results ? landmarks.results : @[];
    if (observations.count > 64)
        throw std::runtime_error("The photo contains too many detected faces.");

    VNDetectFaceCaptureQualityRequest *quality = [[VNDetectFaceCaptureQualityRequest alloc] init];
    quality.revision = VNDetectFaceCaptureQualityRequestRevision3;
    quality.inputFaceObservations = observations;
    if (observations.count && ![handler performRequests:@[ quality ] error:&error])
        throw std::runtime_error("Vision face-quality analysis failed.");
    NSArray<VNFaceObservation *> *qualityFaces = quality.results ? quality.results : @[];

    const auto width = CGImageGetWidth(image), height = CGImageGetHeight(image);
    std::vector<ai::FaceObservation> result;
    result.reserve(observations.count);
    for (VNFaceObservation *face in observations) {
        checkCancellation(stop);
        const CGRect box = face.boundingBox;
        if (box.size.width <= 0 || box.size.height <= 0)
            continue;
        double faceQuality = .5;
        double bestOverlap = 0;
        for (VNFaceObservation *candidate in qualityFaces) {
            const double value = overlap(box, candidate.boundingBox);
            if (value > bestOverlap && candidate.faceCaptureQuality) {
                bestOverlap = value;
                faceQuality = std::clamp(candidate.faceCaptureQuality.doubleValue, 0., 1.);
            }
        }
        const double left = std::clamp(box.origin.x, 0., 1.);
        const double top = std::clamp(1. - box.origin.y - box.size.height, 0., 1.);
        const double normalizedWidth = std::min(box.size.width, 1. - left);
        const double normalizedHeight = std::min(box.size.height, 1. - top);
        if (normalizedWidth <= 0 || normalizedHeight <= 0)
            continue;
        const CGRect crop = CGRectMake(std::floor(left * width), std::floor(top * height),
                                       std::max(1., std::ceil(normalizedWidth * width)),
                                       std::max(1., std::ceil(normalizedHeight * height)));
        CFHandle faceImage(CGImageCreateWithImageInRect(
            image, CGRectIntersection(crop, CGRectMake(0, 0, width, height))));
        if (!faceImage)
            continue;
        const double leftEye = openness(face.landmarks.leftEye);
        const double rightEye = openness(face.landmarks.rightEye);
        result.push_back({{left, top, normalizedWidth, normalizedHeight},
                          faceQuality,
                          (leftEye + rightEye) / 2.,
                          descriptor(faceImage.get())});
    }
    return result;
}

class PhotoCulling final : public ai::ICullingProvider {
  public:
    std::string identifier() const override {
        return "apertone.culling-v1.grid16.face-quality3.landmarks3";
    }
    ai::ExecutionLocation location() const override {
        return ai::ExecutionLocation::local;
    }
    ai::CullingObservation analyze(ai::RgbaImage input, CancellationToken stop,
                                   const ai::Progress &progress) override {
        @autoreleasepool {
            input.validate();
            checkCancellation(stop);
            auto image = cgImage(input);
            if (progress)
                progress(.10);
            auto imageEmbedding = descriptor(image.get());
            if (progress)
                progress(.35);
            auto faceObservations = faces(image.get(), stop);
            if (progress)
                progress(.75);
            auto result = ai::scoreCullingPreview(input, std::move(imageEmbedding),
                                                  std::move(faceObservations), stop);
            if (progress)
                progress(1);
            return result;
        }
    }
};
} // namespace

std::shared_ptr<ai::ICullingProvider> makePhotoCulling() {
    return std::make_shared<PhotoCulling>();
}
} // namespace luma::macos
