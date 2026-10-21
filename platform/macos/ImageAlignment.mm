#include "platform/macos/ImageAlignment.h"
#include "core/image/Bayer.h"
#include "platform/macos/CFHandle.h"
#import <Vision/Vision.h>
#include <algorithm>

namespace luma::macos {
namespace {
CFHandle<CGImageRef> image(ai::RgbaImage input) {
    input.validate();
    if (input.width > 1024 || input.height > 1024)
        throw std::runtime_error("Alignment reference exceeds its budget.");
    CFHandle color(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    CFHandle data(
        CGDataProviderCreateWithData(nullptr, input.pixels.data(), input.pixels.size(), nullptr));
    CFHandle result(
        CGImageCreate(input.width, input.height, 8, 32, input.width * 4, color.get(),
                      static_cast<CGBitmapInfo>(kCGImageAlphaLast) | kCGBitmapByteOrder32Big,
                      data.get(), nullptr, false, kCGRenderingIntentDefault));
    if (!result)
        throw std::runtime_error("Cannot prepare alignment image.");
    return result;
}
} // namespace
Homography alignImages(ai::RgbaImage reference, ai::RgbaImage floating, CancellationToken stop) {
    checkCancellation(stop);
    if (reference.width != floating.width || reference.height != floating.height)
        throw std::runtime_error("Alignment requires matching upright image dimensions.");
    @autoreleasepool {
        const auto target = image(reference), source = image(floating);
        auto *request =
            [[VNHomographicImageRegistrationRequest alloc] initWithTargetedCGImage:source.get()
                                                                           options:@{}];
        request.revision = VNHomographicImageRegistrationRequestRevision1;
        auto *handler = [[VNImageRequestHandler alloc] initWithCGImage:target.get() options:@{}];
        NSError *error = nil;
        if (![handler performRequests:@[ request ] error:&error] || request.results.count != 1)
            throw std::runtime_error(
                "Could not align these frames. Use images with sufficient overlap.");
        checkCancellation(stop);
        const auto matrix = request.results.firstObject.warpTransform;
        Homography native;
        for (std::size_t row = 0; row < 3; ++row)
            for (std::size_t column = 0; column < 3; ++column)
                native.values[row * 3 + column] = matrix.columns[column][row];
        // Vision/Core Image pixel coordinates are bottom-left. Convert to the
        // normalized top-left canvas used by the CPU merge code.
        const double w = reference.width, h = reference.height;
        const Homography fromNormalized{{w, 0, 0, 0, -h, h, 0, 0, 1}};
        const Homography toNormalized{{1 / w, 0, 0, 0, -1 / h, 1, 0, 0, 1}};
        const auto result = toNormalized * native * fromNormalized;
        const auto center = result.point(.5, .5);
        if (center[0] < -1 || center[0] > 2 || center[1] < -1 || center[1] > 2)
            throw std::runtime_error("Image alignment lies outside the supported overlap.");
        // Refuse a horizon/pole crossing inside the image.
        const auto origin = result.homogeneous(0, 0);
        for (const auto &p : {std::array{0., 1.}, std::array{1., 0.}, std::array{1., 1.}})
            if (origin[2] * result.homogeneous(p[0], p[1])[2] <= 0)
                throw std::runtime_error("Unstable image alignment.");
        return result;
    }
}
} // namespace luma::macos
