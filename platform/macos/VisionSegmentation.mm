#include "platform/macos/VisionSegmentation.h"
#include "core/image/Bayer.h"
#include "platform/macos/CFHandle.h"
#import <Vision/Vision.h>
#include <algorithm>
#include <cmath>
#include <dispatch/dispatch.h>
#include <limits>
#include <vector>

namespace luma::macos {
namespace {
class PixelLock {
  public:
    explicit PixelLock(CVPixelBufferRef buffer) : buffer_(buffer) {
        if (!buffer_ ||
            CVPixelBufferLockBaseAddress(buffer_, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess)
            throw std::runtime_error("Cannot read Vision pixel buffer.");
    }
    ~PixelLock() {
        CVPixelBufferUnlockBaseAddress(buffer_, kCVPixelBufferLock_ReadOnly);
    }

  private:
    CVPixelBufferRef buffer_;
};
class RequestCancellation {
  public:
    RequestCancellation(VNRequest *request, CancellationToken stop) {
        timer_ = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0,
                                        dispatch_get_global_queue(QOS_CLASS_UTILITY, 0));
        if (!timer_)
            throw std::runtime_error("Cannot schedule AI cancellation.");
        dispatch_source_set_timer(timer_, DISPATCH_TIME_NOW, 50 * NSEC_PER_MSEC, 5 * NSEC_PER_MSEC);
        dispatch_source_set_event_handler(timer_, ^{
          if (stop.isCancelled())
              [request cancel];
        });
        dispatch_resume(timer_);
    }
    ~RequestCancellation() {
        dispatch_source_cancel(timer_);
    }

  private:
    dispatch_source_t timer_;
};
struct Point {
    double x = 0, y = 0;
};
using Polygon = std::vector<Point>;

bool faceSelection(ai::Selection selection) {
    return selection == ai::Selection::face || selection == ai::Selection::eyes ||
           selection == ai::Selection::lips;
}
bool inside(const Polygon &polygon, double x, double y) {
    if (polygon.size() < 3)
        return false;
    bool result = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const auto &a = polygon[i], &b = polygon[j];
        if (((a.y > y) != (b.y > y)) && x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x)
            result = !result;
    }
    return result;
}
Polygon polygon(VNFaceLandmarkRegion2D *region, int width, int height) {
    if (!region || region.pointCount < 3)
        return {};
    const CGPoint *points = [region pointsInImageOfSize:CGSizeMake(width, height)];
    if (!points)
        return {};
    Polygon result;
    result.reserve(region.pointCount);
    for (NSUInteger i = 0; i < region.pointCount; ++i)
        result.push_back({points[i].x, height - points[i].y});
    return result;
}
std::array<double, 4> faceBox(VNFaceObservation *face, int width, int height) {
    const auto box = face.boundingBox;
    return {box.origin.x * width, (1 - box.origin.y - box.size.height) * height,
            (box.origin.x + box.size.width) * width, (1 - box.origin.y) * height};
}
double boxDistanceSquared(const std::array<double, 4> &box, double x, double y) {
    const double dx = x < box[0] ? box[0] - x : x > box[2] ? x - box[2] : 0;
    const double dy = y < box[1] ? box[1] - y : y > box[3] ? y - box[3] : 0;
    return dx * dx + dy * dy;
}
BitmapMask faceMask(VNImageRequestHandler *handler, ai::RgbaImage image, ai::Selection selection,
                    std::array<double, 2> point, CancellationToken stop,
                    const ai::Progress &progress) {
    VNDetectFaceLandmarksRequest *request = [[VNDetectFaceLandmarksRequest alloc] init];
    request.revision = VNDetectFaceLandmarksRequestRevision3;
    request.constellation = VNRequestFaceLandmarksConstellation76Points;
    RequestCancellation cancellation(request, stop);
    if (progress)
        progress(.1);
    NSError *error = nil;
    const BOOL success = [handler performRequests:@[ request ] error:&error];
    checkCancellation(stop);
    if (!success || !request.results.count)
        throw std::runtime_error("No face was detected at the selected location.");

    VNFaceObservation *chosen = nil;
    double best = std::numeric_limits<double>::infinity();
    const double px = point[0] * image.width, py = point[1] * image.height;
    for (VNFaceObservation *face in request.results) {
        const double distance =
            boxDistanceSquared(faceBox(face, image.width, image.height), px, py);
        if (distance < best) {
            best = distance;
            chosen = face;
        }
    }
    if (best > 0)
        throw std::runtime_error("No face was detected at the selected location.");
    if (!chosen || !chosen.landmarks || chosen.confidence < .35 ||
        chosen.landmarks.confidence < .35)
        throw std::runtime_error("Face landmarks are not reliable enough for this photo.");

    const auto box = faceBox(chosen, image.width, image.height);
    const auto leftEye = polygon(chosen.landmarks.leftEye, image.width, image.height);
    const auto rightEye = polygon(chosen.landmarks.rightEye, image.width, image.height);
    const auto outerLips = polygon(chosen.landmarks.outerLips, image.width, image.height);
    const auto innerLips = polygon(chosen.landmarks.innerLips, image.width, image.height);
    if (selection == ai::Selection::eyes && (leftEye.size() < 3 || rightEye.size() < 3))
        throw std::runtime_error("Vision did not return complete eye landmarks.");
    if (selection == ai::Selection::lips && outerLips.size() < 3)
        throw std::runtime_error("Vision did not return complete lip landmarks.");

    const auto selected = [&](double x, double y) {
        if (selection == ai::Selection::face) {
            const double rx = (box[2] - box[0]) / 2, ry = (box[3] - box[1]) / 2;
            if (rx <= 0 || ry <= 0)
                return false;
            const double nx = (x - (box[0] + box[2]) / 2) / rx;
            const double ny = (y - (box[1] + box[3]) / 2) / ry;
            return nx * nx + ny * ny <= 1;
        }
        if (selection == ai::Selection::eyes)
            return inside(leftEye, x, y) || inside(rightEye, x, y);
        return inside(outerLips, x, y) && !inside(innerLips, x, y);
    };

    std::vector<std::uint8_t> values(pixelCount(image.width, image.height));
    constexpr std::array offsets{.25, .75};
    for (int y = 0; y < image.height; ++y) {
        checkCancellation(stop);
        for (int x = 0; x < image.width; ++x) {
            int samples = 0;
            for (const double oy : offsets)
                for (const double ox : offsets)
                    samples += selected(x + ox, y + oy) ? 1 : 0;
            values[static_cast<std::size_t>(y) * image.width + x] =
                static_cast<std::uint8_t>(samples * 255 / 4);
        }
        if (progress && y % 64 == 0 && y + 1 < image.height)
            progress(.75 + .25 * (y + 1) / image.height);
    }
    if (progress)
        progress(1);
    return BitmapMask(image.width, image.height, std::move(values));
}
class VisionSegmentation final : public ai::ISegmentationProvider {
  public:
    std::string identifier() const override {
        return "apple.vision.selections.v2";
    }
    ai::ExecutionLocation location() const override {
        return ai::ExecutionLocation::local;
    }
    bool supports(ai::Selection selection) const override {
        return selection == ai::Selection::subject || selection == ai::Selection::background ||
               selection == ai::Selection::object || faceSelection(selection);
    }
    BitmapMask segment(ai::RgbaImage image, ai::Selection selection, std::array<double, 2> point,
                       CancellationToken stop, const ai::Progress &progress) override {
        image.validate();
        if (!supports(selection))
            throw std::runtime_error(
                "This local provider does not support the requested selection.");
        for (const auto coordinate : point)
            if (!std::isfinite(coordinate) || coordinate < 0 || coordinate > 1)
                throw std::runtime_error("Object selection point is outside the photo.");
        checkCancellation(stop);
        @autoreleasepool {
            CFHandle color(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
            CFHandle data(CGDataProviderCreateWithData(nullptr, image.pixels.data(),
                                                       image.pixels.size(), nullptr));
            CFHandle cgImage(CGImageCreate(
                image.width, image.height, 8, 32, image.width * 4, color.get(),
                static_cast<CGBitmapInfo>(kCGImageAlphaLast) | kCGBitmapByteOrder32Big, data.get(),
                nullptr, false, kCGRenderingIntentDefault));
            if (!cgImage)
                throw std::runtime_error("Cannot prepare photo for local segmentation.");
            VNImageRequestHandler *handler =
                [[VNImageRequestHandler alloc] initWithCGImage:cgImage.get() options:@{}];
            if (faceSelection(selection))
                return faceMask(handler, image, selection, point, stop, progress);
            VNGenerateForegroundInstanceMaskRequest *request =
                [[VNGenerateForegroundInstanceMaskRequest alloc] init];
            request.revision = VNGenerateForegroundInstanceMaskRequestRevision1;
            RequestCancellation cancellation(request, stop);
            if (progress)
                progress(.1);
            NSError *error = nil;
            const BOOL success = [handler performRequests:@[ request ] error:&error];
            checkCancellation(stop);
            if (!success || request.results.count != 1)
                throw std::runtime_error("Local subject detection failed.");
            VNInstanceMaskObservation *observation = request.results.firstObject;
            if (!observation.allInstances.count)
                throw std::runtime_error("No foreground subject was detected.");
            NSIndexSet *instances = observation.allInstances;
            if (selection == ai::Selection::object) {
                CVPixelBufferRef labels = observation.instanceMask;
                if (CVPixelBufferGetPixelFormatType(labels) != kCVPixelFormatType_OneComponent8)
                    throw std::runtime_error("Unsupported Vision instance label format.");
                PixelLock lock(labels);
                const auto width = CVPixelBufferGetWidth(labels),
                           height = CVPixelBufferGetHeight(labels);
                if (!width || !height)
                    throw std::runtime_error("Empty Vision instance mask.");
                const auto x = std::min(static_cast<std::size_t>(point[0] * width), width - 1);
                const auto y = std::min(static_cast<std::size_t>(point[1] * height), height - 1);
                const auto *bytes =
                    static_cast<const std::uint8_t *>(CVPixelBufferGetBaseAddress(labels));
                if (!bytes)
                    throw std::runtime_error("Vision instance labels have no readable storage.");
                const auto label = bytes[y * CVPixelBufferGetBytesPerRow(labels) + x];
                if (!label || ![instances containsIndex:label])
                    throw std::runtime_error("No foreground object was detected at this point.");
                instances = [NSIndexSet indexSetWithIndex:label];
            }
            if (progress)
                progress(.8);
            CFHandle mask([observation generateScaledMaskForImageForInstances:instances
                                                           fromRequestHandler:handler
                                                                        error:&error]);
            checkCancellation(stop);
            if (!mask || CVPixelBufferGetPixelFormatType(mask.get()) !=
                             kCVPixelFormatType_OneComponent32Float)
                throw std::runtime_error("Vision did not return a supported coverage mask.");
            PixelLock lock(mask.get());
            const auto width = CVPixelBufferGetWidth(mask.get()),
                       height = CVPixelBufferGetHeight(mask.get());
            if (width < 1 || height < 1 || width > 1024 || height > 1024)
                throw std::runtime_error("Vision mask exceeds the supported dimensions.");
            std::vector<std::uint8_t> values(width * height);
            const auto *bytes =
                static_cast<const std::uint8_t *>(CVPixelBufferGetBaseAddress(mask.get()));
            if (!bytes)
                throw std::runtime_error("Vision coverage mask has no readable storage.");
            for (std::size_t y = 0; y < height; ++y) {
                checkCancellation(stop);
                const auto *row = reinterpret_cast<const float *>(
                    bytes + y * CVPixelBufferGetBytesPerRow(mask.get()));
                for (std::size_t x = 0; x < width; ++x) {
                    if (!std::isfinite(row[x]))
                        throw std::runtime_error("Vision returned non-finite coverage.");
                    const double value =
                        selection == ai::Selection::background ? 1.0 - row[x] : row[x];
                    values[y * width + x] =
                        static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0, 1.0) * 255));
                }
            }
            if (progress)
                progress(1);
            return BitmapMask(static_cast<int>(width), static_cast<int>(height), std::move(values));
        }
    }
};
} // namespace
std::shared_ptr<ai::ISegmentationProvider> makeVisionSegmentation() {
    return std::make_shared<VisionSegmentation>();
}
} // namespace luma::macos
