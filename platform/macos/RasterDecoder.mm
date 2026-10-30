#include "platform/macos/RasterDecoder.h"
#include "core/image/Bayer.h"
#include "platform/macos/CFHandle.h"
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <algorithm>
#include <cmath>
#include <set>

namespace luma::macos {
namespace {
constexpr std::array orientation{0, 0, 1, 3, 2, 4, 6, 7, 5};
bool allowed(CFStringRef type) {
    if (!type)
        return false;
    static const std::set<std::string> types{
        "public.jpeg",          "public.png",        "public.tiff",       "public.heic",
        "public.heif",          "public.heics",      "public.avif",       "public.avis",
        "org.webmproject.webp", "com.microsoft.bmp", "com.compuserve.gif"};
    NSString *value = (__bridge NSString *)type;
    const char *bytes = value.UTF8String;
    return bytes && types.contains(bytes);
}
CFHandle<CGImageSourceRef> source(const std::filesystem::path &path) {
    if (!std::filesystem::is_regular_file(path) || std::filesystem::file_size(path) > 512000000)
        throw std::runtime_error("Image is missing or exceeds the 512 MB input budget.");
    CFHandle url(CFURLCreateFromFileSystemRepresentation(
        nullptr, reinterpret_cast<const UInt8 *>(path.c_str()),
        static_cast<CFIndex>(path.native().size()), false));
    NSDictionary *options = @{(__bridge NSString *)kCGImageSourceShouldCache : @NO};
    CFHandle result(url ? CGImageSourceCreateWithURL(url.get(), (__bridge CFDictionaryRef)options)
                        : nullptr);
    if (!result)
        throw std::runtime_error("ImageIO cannot read the image header.");
    return result;
}
std::size_t primary(CGImageSourceRef source) {
    const auto count = CGImageSourceGetCount(source);
    const auto index = CGImageSourceGetPrimaryImageIndex(source);
    if (!count || index >= count)
        throw std::runtime_error("Image has no valid primary frame.");
    return index;
}
double number(NSDictionary *dictionary, CFStringRef key, double fallback = 0) {
    id value = dictionary[(__bridge NSString *)key];
    if (![value isKindOfClass:NSNumber.class])
        return fallback;
    const double result = [value doubleValue];
    return std::isfinite(result) ? result : fallback;
}
std::optional<double> positive(NSDictionary *dictionary, CFStringRef key) {
    const double value = number(dictionary, key);
    return value > 0 ? std::optional<double>(value) : std::nullopt;
}
std::string text(NSDictionary *dictionary, CFStringRef key) {
    id value = dictionary[(__bridge NSString *)key];
    if (![value isKindOfClass:NSString.class])
        return {};
    const char *bytes = [value UTF8String];
    return bytes ? std::string(bytes) : std::string();
}
NSDictionary *section(NSDictionary *dictionary, CFStringRef key) {
    id value = dictionary[(__bridge NSString *)key];
    return [value isKindOfClass:NSDictionary.class] ? value : @{};
}
PhotoMetadata metadata(CGImageSourceRef source, std::size_t index) {
    CFHandle properties(CGImageSourceCopyPropertiesAtIndex(source, index, nullptr));
    if (!properties)
        throw std::runtime_error("ImageIO cannot read image properties.");
    auto *root = (__bridge NSDictionary *)properties.get();
    const double width = number(root, kCGImagePropertyPixelWidth),
                 height = number(root, kCGImagePropertyPixelHeight);
    if (width < 1 || height < 1 || width > 20000 || height > 20000 || width != std::floor(width) ||
        height != std::floor(height))
        throw std::runtime_error("Invalid or oversized image dimensions.");
    pixelCount(static_cast<int>(width), static_cast<int>(height));
    const double exifOrientation = number(root, kCGImagePropertyOrientation, 1);
    if (exifOrientation < 1 || exifOrientation > 8 ||
        exifOrientation != std::floor(exifOrientation))
        throw std::runtime_error("Invalid image orientation.");
    PhotoMetadata result;
    result.width = static_cast<int>(width);
    result.height = static_cast<int>(height);
    result.orientation = orientation[static_cast<std::size_t>(exifOrientation)];
    auto *tiff = section(root, kCGImagePropertyTIFFDictionary);
    auto *exif = section(root, kCGImagePropertyExifDictionary);
    auto *aux = section(root, kCGImagePropertyExifAuxDictionary);
    const auto make = text(tiff, kCGImagePropertyTIFFMake),
               model = text(tiff, kCGImagePropertyTIFFModel);
    if (make.empty() || model.starts_with(make))
        result.camera = model;
    else if (model.empty())
        result.camera = make;
    else
        result.camera = make + " " + model;
    result.lens = text(exif, kCGImagePropertyExifLensModel);
    if (result.lens.empty())
        result.lens = text(aux, kCGImagePropertyExifAuxLensModel);
    result.aperture = positive(exif, kCGImagePropertyExifFNumber);
    result.shutterSeconds = positive(exif, kCGImagePropertyExifExposureTime);
    result.focalLength = positive(exif, kCGImagePropertyExifFocalLength);
    id iso = exif[(__bridge NSString *)kCGImagePropertyExifISOSpeedRatings];
    if ([iso isKindOfClass:NSArray.class] && [iso count] > 0 &&
        [[iso firstObject] isKindOfClass:NSNumber.class]) {
        const double value = [[iso firstObject] doubleValue];
        if (std::isfinite(value) && value > 0)
            result.iso = value;
    }
    const auto date = text(exif, kCGImagePropertyExifDateTimeOriginal);
    if (!date.empty()) {
        NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
        formatter.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
        formatter.dateFormat = @"yyyy:MM:dd HH:mm:ss";
        formatter.lenient = NO;
        formatter.timeZone = [NSTimeZone timeZoneForSecondsFromGMT:0];
        const auto offset = text(exif, kCGImagePropertyExifOffsetTimeOriginal);
        if (offset.size() == 6 && (offset[0] == '+' || offset[0] == '-') && offset[3] == ':' &&
            std::all_of(offset.begin() + 1, offset.begin() + 3,
                        [](unsigned char c) { return c >= '0' && c <= '9'; }) &&
            std::all_of(offset.begin() + 4, offset.end(),
                        [](unsigned char c) { return c >= '0' && c <= '9'; })) {
            const int hours = (offset[1] - '0') * 10 + offset[2] - '0',
                      minutes = (offset[4] - '0') * 10 + offset[5] - '0';
            if (hours <= 23 && minutes <= 59)
                formatter.timeZone = [NSTimeZone
                    timeZoneForSecondsFromGMT:static_cast<NSInteger>(offset[0] == '-' ? -1 : 1) *
                                              (hours * 3600 + minutes * 60)];
        }
        NSString *dateText = [NSString stringWithUTF8String:date.c_str()];
        NSDate *parsed = dateText ? [formatter dateFromString:dateText] : nil;
        if (parsed)
            result.captureTimestamp = static_cast<std::int64_t>(parsed.timeIntervalSince1970);
        [formatter release];
    }
    return result;
}
void validateType(CGImageSourceRef source) {
    if (!allowed(CGImageSourceGetType(source)))
        throw std::runtime_error("Unsupported raster image format.");
    if (CGImageSourceGetStatus(source) != kCGImageStatusComplete)
        throw std::runtime_error("Image data is incomplete or corrupt.");
}
} // namespace
bool RasterDecoder::available(const std::string &extension) const {
    const char *identifier = extension == ".jpg" || extension == ".jpeg"   ? "public.jpeg"
                             : extension == ".png"                         ? "public.png"
                             : extension == ".tif" || extension == ".tiff" ? "public.tiff"
                             : extension == ".heic"                        ? "public.heic"
                             : extension == ".heif"                        ? "public.heif"
                             : extension == ".webp"                        ? "org.webmproject.webp"
                             : extension == ".bmp"                         ? "com.microsoft.bmp"
                             : extension == ".gif"                         ? "com.compuserve.gif"
                             : extension == ".avif"                        ? "public.avif"
                                                                           : nullptr;
    if (!identifier)
        return false;
    CFHandle list(CGImageSourceCopyTypeIdentifiers());
    CFHandle value(CFStringCreateWithCString(nullptr, identifier, kCFStringEncodingUTF8));
    return list && value &&
           CFArrayContainsValue(list.get(), CFRangeMake(0, CFArrayGetCount(list.get())),
                                value.get());
}
bool RasterDecoder::cameraRaw(const std::filesystem::path &path) const {
    @autoreleasepool {
        auto input = source(path);
        auto type = CGImageSourceGetType(input.get());
        UTType *kind = type ? [UTType typeWithIdentifier:(__bridge NSString *)type] : nil;
        return kind && [kind conformsToType:UTTypeRAWImage];
    }
}
PhotoMetadata RasterDecoder::inspect(const std::filesystem::path &path,
                                     CancellationToken stop) const {
    @autoreleasepool {
        checkCancellation(stop);
        auto input = source(path);
        validateType(input.get());
        const auto result = metadata(input.get(), primary(input.get()));
        checkCancellation(stop);
        return result;
    }
}
DecodedImage RasterDecoder::decode(const std::filesystem::path &path, CancellationToken stop,
                                   const std::function<void(double)> &progress) const {
    @autoreleasepool {
        checkCancellation(stop);
        auto input = source(path);
        validateType(input.get());
        const auto index = primary(input.get());
        const auto info = metadata(input.get(), index);
        if (progress)
            progress(.05);
        checkCancellation(stop);
        NSMutableDictionary *options = [@{
            (__bridge NSString *)kCGImageSourceShouldAllowFloat : @YES,
            (__bridge NSString *)kCGImageSourceShouldCache : @NO,
            (__bridge NSString *)
            kCGImageSourceDecodeRequest : (__bridge NSString *)kCGImageSourceDecodeToHDR
        } mutableCopy];
        if (@available(macOS 16.0, *))
            options[(__bridge NSString *)kCGImageSourceDecodeRequestOptions] =
                @{(__bridge NSString *)kCGComputeHDRStats : @YES};
        CFHandle image(
            CGImageSourceCreateImageAtIndex(input.get(), index, (__bridge CFDictionaryRef)options));
        [options release];
        checkCancellation(stop);
        if (!image || CGImageGetWidth(image.get()) != static_cast<std::size_t>(info.width) ||
            CGImageGetHeight(image.get()) != static_cast<std::size_t>(info.height) ||
            CGImageSourceGetStatusAtIndex(input.get(), index) != kCGImageStatusComplete)
            throw std::runtime_error("ImageIO could not decode the complete image.");
        const auto count = pixelCount(info.width, info.height);
        std::vector<float> pixels(count * 4);
        CFHandle color(CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB));
        const auto bitmap = static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) |
                            kCGBitmapFloatComponents | kCGBitmapByteOrder32Little;
        CFHandle context(color ? CGBitmapContextCreate(pixels.data(), info.width, info.height, 32,
                                                       static_cast<std::size_t>(info.width) * 16,
                                                       color.get(), bitmap)
                               : nullptr);
        if (!context)
            throw std::runtime_error("Cannot allocate color-managed image raster.");
        CGContextSetBlendMode(context.get(), kCGBlendModeCopy);
        CGContextSetRenderingIntent(context.get(), kCGRenderingIntentRelativeColorimetric);
        CGContextDrawImage(context.get(), CGRectMake(0, 0, info.width, info.height), image.get());
        checkCancellation(stop);
        if (CGImageSourceGetStatusAtIndex(input.get(), index) != kCGImageStatusComplete)
            throw std::runtime_error("Image data became incomplete during decoding.");
        FloatImage result(info.width, info.height);
        result.alpha.resize(count);
        bool opaque = true;
        for (std::size_t i = 0; i < count; ++i) {
            if (i % static_cast<std::size_t>(info.width) == 0) {
                checkCancellation(stop);
                if (progress)
                    progress(.3 + .7 * static_cast<double>(i) / count);
            }
            const float alpha = pixels[i * 4 + 3];
            if (!std::isfinite(alpha) || alpha < 0 || alpha > 1)
                throw std::runtime_error("Invalid decoded image alpha.");
            result.alpha[i] = alpha;
            opaque = opaque && alpha == 1;
            for (std::size_t c = 0; c < 3; ++c) {
                const float value = alpha > 1e-8F ? pixels[i * 4 + c] / alpha : 0;
                if (!std::isfinite(value))
                    throw std::runtime_error("Non-finite decoded image color.");
                result.rgb[i * 3 + c] = value;
            }
        }
        if (opaque)
            std::vector<float>().swap(result.alpha);
        if (progress)
            progress(1);
        return {std::move(result), info, {}};
    }
}
} // namespace luma::macos
