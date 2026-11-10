#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#include <filesystem>
#include <stdexcept>
#include <vector>

void writeExportExifFixture(const std::filesystem::path &path) {
    @autoreleasepool {
        NSString *pathString = [NSString stringWithUTF8String:path.c_str()];
        if (!pathString)
            throw std::runtime_error("Invalid export EXIF fixture path.");
        constexpr std::size_t width = 64, height = 48;
        const std::vector<unsigned char> pixels(width * height * 4, 180);
        NSData *data = [NSData dataWithBytes:pixels.data() length:pixels.size()];
        CGDataProviderRef provider = CGDataProviderCreateWithCFData((__bridge CFDataRef)data);
        CGColorSpaceRef color = CGColorSpaceCreateDeviceRGB();
        CGImageRef image =
            CGImageCreate(width, height, 8, 32, width * 4, color, kCGImageAlphaPremultipliedLast,
                          provider, nullptr, false, kCGRenderingIntentDefault);
        NSURL *url = [NSURL fileURLWithPath:pathString];
        CGImageDestinationRef target = CGImageDestinationCreateWithURL(
            (__bridge CFURLRef)url, CFSTR("public.png"), 1, nullptr);
        if (!image || !target)
            throw std::runtime_error("Cannot create export EXIF fixture.");
        NSDictionary *properties = @{
            (__bridge NSString *)kCGImagePropertyTIFFDictionary : @{
                (__bridge NSString *)kCGImagePropertyTIFFMake : @"Luma",
                (__bridge NSString *)kCGImagePropertyTIFFModel : @"Raster Test"
            },
            (__bridge NSString *)kCGImagePropertyExifDictionary : @{
                (__bridge NSString *)kCGImagePropertyExifISOSpeedRatings : @[ @200 ],
                (__bridge NSString *)kCGImagePropertyExifLensModel : @"Luma Test Lens",
                (__bridge NSString *)kCGImagePropertyExifDateTimeOriginal : @"2026:10:14 12:00:00",
                (__bridge NSString *)kCGImagePropertyExifOffsetTimeOriginal : @"+03:00"
            }
        };
        CGImageDestinationAddImage(target, image, (__bridge CFDictionaryRef)properties);
        const bool written = CGImageDestinationFinalize(target);
        CFRelease(target);
        CGImageRelease(image);
        CGColorSpaceRelease(color);
        CGDataProviderRelease(provider);
        if (!written)
            throw std::runtime_error("Cannot write export EXIF fixture.");
    }
}
