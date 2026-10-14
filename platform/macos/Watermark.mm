#include "platform/macos/Watermark.h"
#include "core/image/Bayer.h"
#include "platform/macos/CFHandle.h"
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <ImageIO/ImageIO.h>
#include <algorithm>
#include <cmath>

namespace luma::macos {
namespace {
LinearOutput raster(CGImageRef image, int width, int height) {
    LinearOutput result(width, height);
    CFHandle color(CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB));
    const CGBitmapInfo bitmap = static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) |
                                kCGBitmapFloatComponents | kCGBitmapByteOrder32Little;
    CFHandle context(color ? CGBitmapContextCreate(result.rgba.data(), width, height, 32,
                                                   static_cast<std::size_t>(width) * 16,
                                                   color.get(), bitmap)
                           : nullptr);
    if (!context)
        throw std::runtime_error("Cannot allocate watermark raster.");
    CGContextSetInterpolationQuality(context.get(), kCGInterpolationHigh);
    CGContextDrawImage(context.get(), CGRectMake(0, 0, width, height), image);
    for (std::size_t i = 0; i < result.rgba.size(); i += 4) {
        const float alpha = result.rgba[i + 3];
        for (std::size_t c = 0; c < 3; ++c)
            result.rgba[i + c] = alpha > 1e-8F ? result.rgba[i + c] / alpha : 0;
    }
    return result;
}
LinearOutput text(const WatermarkOptions &options, int bound) {
    CFHandle string(CFStringCreateWithBytes(
        nullptr, reinterpret_cast<const UInt8 *>(options.text.data()),
        static_cast<CFIndex>(options.text.size()), kCFStringEncodingUTF8, false));
    if (!string)
        throw std::runtime_error("Watermark text is not valid UTF-8.");
    CFHandle font(CTFontCreateWithName(CFSTR("HelveticaNeue"), 96, nullptr));
    const void *keys[]{kCTFontAttributeName, kCTForegroundColorFromContextAttributeName};
    const void *values[]{font.get(), kCFBooleanTrue};
    CFHandle attributes(CFDictionaryCreate(nullptr, keys, values, 2, &kCFTypeDictionaryKeyCallBacks,
                                           &kCFTypeDictionaryValueCallBacks));
    CFHandle attributed(
        attributes ? CFAttributedStringCreate(nullptr, string.get(), attributes.get()) : nullptr);
    CFHandle line(attributed ? CTLineCreateWithAttributedString(attributed.get()) : nullptr);
    if (!font || !line)
        throw std::runtime_error("Cannot lay out watermark text.");
    CGFloat ascent = 0, descent = 0, leading = 0;
    const double lineWidth = CTLineGetTypographicBounds(line.get(), &ascent, &descent, &leading);
    const double lineHeight = ascent + descent + leading;
    if (!std::isfinite(lineWidth) || lineWidth <= 0 || lineHeight <= 0)
        throw std::runtime_error("Watermark text has no visible bounds.");
    const double scale = std::min(static_cast<double>(bound) / (lineWidth + 8),
                                  static_cast<double>(bound) / (lineHeight + 8));
    const int width = std::max(1, static_cast<int>(std::ceil((lineWidth + 8) * scale))),
              height = std::max(1, static_cast<int>(std::ceil((lineHeight + 8) * scale)));
    LinearOutput result(width, height);
    CFHandle color(CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB));
    const CGBitmapInfo bitmap = static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) |
                                kCGBitmapFloatComponents | kCGBitmapByteOrder32Little;
    CFHandle context(color ? CGBitmapContextCreate(result.rgba.data(), width, height, 32,
                                                   static_cast<std::size_t>(width) * 16,
                                                   color.get(), bitmap)
                           : nullptr);
    if (!context)
        throw std::runtime_error("Cannot allocate text watermark.");
    CGContextScaleCTM(context.get(), scale, scale);
    CGContextSetRGBFillColor(context.get(), 1, 1, 1, 1);
    CGContextSetTextPosition(context.get(), 4, descent + 4);
    CTLineDraw(line.get(), context.get());
    // CoreText uses a bottom-left baseline; output rows use a top-left origin.
    for (int y = 0; y < height / 2; ++y)
        for (int x = 0; x < width; ++x)
            for (std::size_t c = 0; c < 4; ++c)
                std::swap(
                    result.rgba[(static_cast<std::size_t>(y) * width + x) * 4 + c],
                    result.rgba[(static_cast<std::size_t>(height - 1 - y) * width + x) * 4 + c]);
    for (std::size_t i = 0; i < result.rgba.size(); i += 4)
        for (std::size_t c = 0; c < 3; ++c)
            result.rgba[i + c] =
                result.rgba[i + 3] > 1e-8F ? result.rgba[i + c] / result.rgba[i + 3] : 0;
    return result;
}
} // namespace
void applyWatermark(LinearOutput &output, const WatermarkOptions &options, CancellationToken stop) {
    options.validate();
    output.validate();
    checkCancellation(stop);
    if (options.kind == WatermarkKind::none || options.opacity == 0)
        return;
    const int targetBound = std::max(
        1,
        static_cast<int>(std::lround(std::max(output.width, output.height) * options.size / 100)));
    const int rasterBound = std::min(4096, targetBound);
    LinearOutput mark(1, 1);
    if (options.kind == WatermarkKind::text)
        mark = text(options, rasterBound);
    else {
        if (!std::filesystem::is_regular_file(options.image) ||
            std::filesystem::file_size(options.image) > 64'000'000)
            throw std::runtime_error("Watermark image is missing or exceeds 64 MB.");
        CFHandle url(CFURLCreateFromFileSystemRepresentation(
            nullptr, reinterpret_cast<const UInt8 *>(options.image.c_str()),
            static_cast<CFIndex>(options.image.string().size()), false));
        CFHandle source(url ? CGImageSourceCreateWithURL(url.get(), nullptr) : nullptr);
        if (!source)
            throw std::runtime_error("Cannot read watermark image.");
        const void *keys[]{kCGImageSourceCreateThumbnailFromImageAlways,
                           kCGImageSourceCreateThumbnailWithTransform,
                           kCGImageSourceThumbnailMaxPixelSize};
        CFHandle size(CFNumberCreate(nullptr, kCFNumberIntType, &rasterBound));
        const void *values[]{kCFBooleanTrue, kCFBooleanTrue, size.get()};
        CFHandle settings(CFDictionaryCreate(nullptr, keys, values, 3,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks));
        CFHandle image(settings
                           ? CGImageSourceCreateThumbnailAtIndex(source.get(), 0, settings.get())
                           : nullptr);
        if (!image)
            throw std::runtime_error("Watermark image cannot be decoded.");
        const double scale = static_cast<double>(rasterBound) /
                             std::max(CGImageGetWidth(image.get()), CGImageGetHeight(image.get()));
        mark = raster(
            image.get(),
            std::max(1, static_cast<int>(std::lround(CGImageGetWidth(image.get()) * scale))),
            std::max(1, static_cast<int>(std::lround(CGImageGetHeight(image.get()) * scale))));
    }
    checkCancellation(stop);
    ExportOptions resize;
    resize.resize = ResizeMode::longEdge;
    resize.resizeValue = targetBound;
    mark = resizeOutput(std::move(mark), resize, stop);
    const int margin =
        static_cast<int>(std::lround(std::min(output.width, output.height) * options.margin / 100));
    const int availableWidth = std::max(1, output.width - 2 * margin),
              availableHeight = std::max(1, output.height - 2 * margin);
    if (mark.width > availableWidth || mark.height > availableHeight) {
        const double scale = std::min(static_cast<double>(availableWidth) / mark.width,
                                      static_cast<double>(availableHeight) / mark.height);
        resize.resize = ResizeMode::percentage;
        resize.resizeValue = scale * 100;
        mark = resizeOutput(std::move(mark), resize, stop);
    }
    const int column = options.anchor % 3, row = options.anchor / 3;
    const int left = column == 0   ? margin
                     : column == 1 ? (output.width - mark.width) / 2
                                   : output.width - margin - mark.width;
    const int top = row == 0   ? margin
                    : row == 1 ? (output.height - mark.height) / 2
                               : output.height - margin - mark.height;
    for (int y = 0; y < mark.height; ++y) {
        checkCancellation(stop);
        for (int x = 0; x < mark.width; ++x) {
            const int dx = left + x, dy = top + y;
            if (dx < 0 || dy < 0 || dx >= output.width || dy >= output.height)
                continue;
            const auto source = (static_cast<std::size_t>(y) * mark.width + x) * 4,
                       target = (static_cast<std::size_t>(dy) * output.width + dx) * 4;
            const double alpha = mark.rgba[source + 3] * options.opacity / 100,
                         original = output.rgba[target + 3],
                         combined = alpha + original * (1 - alpha);
            for (std::size_t c = 0; c < 3; ++c)
                output.rgba[target + c] =
                    combined > 1e-8
                        ? static_cast<float>((mark.rgba[source + c] * alpha +
                                              output.rgba[target + c] * original * (1 - alpha)) /
                                             combined)
                        : 0;
            output.rgba[target + 3] = static_cast<float>(combined);
        }
    }
}
} // namespace luma::macos
