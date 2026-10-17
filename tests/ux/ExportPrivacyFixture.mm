#include "tests/ux/ExportPrivacyFixture.h"
#include "platform/macos/CFHandle.h"
#include "tests/ux/UxDriver.h"
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#include <QFile>
#include <QFileInfo>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ux {
namespace {
using luma::macos::CFHandle;
CFHandle<CFURLRef> url(const QString &path) {
    const auto bytes = QFile::encodeName(path);
    return CFHandle<CFURLRef>(CFURLCreateFromFileSystemRepresentation(
        nullptr, reinterpret_cast<const UInt8 *>(bytes.constData()),
        static_cast<CFIndex>(bytes.size()), false));
}
struct KnownTag {
    const char *key;
    CFStringRef dictionary;
    CFStringRef property;
};
QJsonObject metadataTags(CGImageMetadataRef metadata, const std::vector<KnownTag> &known) {
    QJsonObject result;
    if (!metadata)
        return result;
    for (const auto &entry : known) {
        CFHandle tag(CGImageMetadataCopyTagMatchingImageProperty(metadata, entry.dictionary,
                                                                 entry.property));
        if (!tag)
            continue;
        CFHandle value(CGImageMetadataTagCopyValue(tag.get()));
        require(static_cast<bool>(value),
                "A known ImageIO metadata tag has no readable primary value");
        result[QLatin1String(entry.key)] =
            QString::fromUtf8([[(__bridge id)value.get() description] UTF8String]);
    }
    return result;
}
QJsonObject propertyTags(NSDictionary *properties, const std::vector<KnownTag> &known) {
    QJsonObject result;
    for (const auto &entry : known) {
        id dictionary = properties[(__bridge NSString *)entry.dictionary];
        require(!dictionary || [dictionary isKindOfClass:[NSDictionary class]],
                "Known primary ImageIO property dictionary has an unexpected type");
        id value = [(NSDictionary *)dictionary objectForKey:(__bridge NSString *)entry.property];
        if (value)
            result[QLatin1String(entry.key)] = QString::fromUtf8([[value description] UTF8String]);
    }
    return result;
}
} // namespace
void createPrivacySource(const QString &path) {
    @autoreleasepool {
        require(!QFileInfo::exists(path), "Privacy input must be a new owned fixture");
        std::vector<std::uint8_t> rgba(std::size_t{128} * 96 * 4, 255);
        for (int y = 0; y < 96; ++y)
            for (int x = 0; x < 128; ++x) {
                const auto offset = (static_cast<std::size_t>(y) * 128 + x) * 4;
                rgba[offset] = static_cast<std::uint8_t>(x * 255 / 127);
                rgba[offset + 1] = static_cast<std::uint8_t>(y * 255 / 95);
                rgba[offset + 2] = 80;
            }
        CFHandle color(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
        CFHandle provider(CGDataProviderCreateWithData(nullptr, rgba.data(), rgba.size(), nullptr));
        CFHandle image(CGImageCreate(128, 96, 8, 32, std::size_t{128} * 4, color.get(),
                                     kCGImageAlphaLast, provider.get(), nullptr, false,
                                     kCGRenderingIntentRelativeColorimetric));
        auto output = url(path);
        CFHandle writer(
            CGImageDestinationCreateWithURL(output.get(), CFSTR("public.jpeg"), 1, nullptr));
        require(image && writer, "Cannot create own primary ImageIO metadata input");
        NSDictionary *properties = @{
            (__bridge NSString *)kCGImagePropertyOrientation : @1,
            (__bridge NSString *)kCGImageDestinationLossyCompressionQuality : @1.0,
            (__bridge NSString *)kCGImagePropertyTIFFDictionary : @{
                (__bridge NSString *)kCGImagePropertyTIFFArtist : @"Luma Privacy Artist",
                (__bridge NSString *)kCGImagePropertyTIFFCopyright : @"Luma Privacy Copyright"
            },
            (__bridge NSString *)kCGImagePropertyExifDictionary : @{
                (__bridge NSString *)kCGImagePropertyExifDateTimeOriginal : @"2026:10:17 10:20:30",
                (__bridge NSString *)kCGImagePropertyExifLensModel : @"Luma Privacy Lens"
            },
            (__bridge NSString *)kCGImagePropertyGPSDictionary : @{
                (__bridge NSString *)kCGImagePropertyGPSLatitude : @48.5,
                (__bridge NSString *)kCGImagePropertyGPSLatitudeRef : @"N",
                (__bridge NSString *)kCGImagePropertyGPSLongitude : @2.3,
                (__bridge NSString *)kCGImagePropertyGPSLongitudeRef : @"E"
            },
            (__bridge NSString *)kCGImagePropertyIPTCDictionary : @{
                (__bridge NSString *)kCGImagePropertyIPTCCity : @"PrivateFixtureCity",
                (__bridge NSString *)kCGImagePropertyIPTCProvinceState : @"PrivateFixtureState",
                (__bridge NSString *)
                kCGImagePropertyIPTCCountryPrimaryLocationName : @"PrivateFixtureCountry",
                (__bridge NSString *)kCGImagePropertyIPTCSubLocation : @"PrivateFixtureStreet",
                (__bridge NSString *)kCGImagePropertyIPTCCopyrightNotice : @"Luma Privacy Copyright"
            }
        };
        CGImageDestinationAddImage(writer.get(), image.get(), (__bridge CFDictionaryRef)properties);
        require(CGImageDestinationFinalize(writer.get()),
                "Cannot encode known private metadata fixture");
    }
}
QJsonObject encodedMetadata(const QString &path) {
    @autoreleasepool {
        auto input = url(path);
        CFHandle source(CGImageSourceCreateWithURL(input.get(), nullptr));
        require(source && CGImageSourceGetCount(source.get()) == 1,
                "Metadata oracle requires one actually decodable encoded image");
        CFHandle raw(CGImageSourceCopyPropertiesAtIndex(source.get(), 0, nullptr));
        CFHandle metadata(CGImageSourceCopyMetadataAtIndex(source.get(), 0, nullptr));
        require(static_cast<bool>(raw), "Primary ImageIO reader cannot inspect encoded properties");
        auto *properties = (__bridge NSDictionary *)raw.get();
        const std::vector<KnownTag> known{
            {"artist", kCGImagePropertyTIFFDictionary, kCGImagePropertyTIFFArtist},
            {"copyright", kCGImagePropertyTIFFDictionary, kCGImagePropertyTIFFCopyright},
            {"iptcCopyright", kCGImagePropertyIPTCDictionary, kCGImagePropertyIPTCCopyrightNotice},
            {"capture", kCGImagePropertyExifDictionary, kCGImagePropertyExifDateTimeOriginal},
            {"lens", kCGImagePropertyExifDictionary, kCGImagePropertyExifLensModel},
            {"latitude", kCGImagePropertyGPSDictionary, kCGImagePropertyGPSLatitude},
            {"latitudeRef", kCGImagePropertyGPSDictionary, kCGImagePropertyGPSLatitudeRef},
            {"longitude", kCGImagePropertyGPSDictionary, kCGImagePropertyGPSLongitude},
            {"longitudeRef", kCGImagePropertyGPSDictionary, kCGImagePropertyGPSLongitudeRef},
            {"city", kCGImagePropertyIPTCDictionary, kCGImagePropertyIPTCCity},
            {"state", kCGImagePropertyIPTCDictionary, kCGImagePropertyIPTCProvinceState},
            {"country", kCGImagePropertyIPTCDictionary,
             kCGImagePropertyIPTCCountryPrimaryLocationName},
            {"street", kCGImagePropertyIPTCDictionary, kCGImagePropertyIPTCSubLocation}};
        const auto propertyValues = propertyTags(properties, known);
        const auto metadataValues = metadataTags(metadata.get(), known);
        QJsonObject result{
            {"width", [properties[(__bridge NSString *)kCGImagePropertyPixelWidth] intValue]},
            {"height", [properties[(__bridge NSString *)kCGImagePropertyPixelHeight] intValue]},
            // Keep both primary channels: a metadata value must never mask a raw
            // properties value left behind by an incomplete privacy filter.
            {"properties", propertyValues},
            {"metadata", metadataValues}};
        for (const auto &entry : known) {
            const auto key = QLatin1String(entry.key);
            result[key] = metadataValues.contains(key) ? metadataValues[key] : propertyValues[key];
        }
        QString xmp;
        if (metadata) {
            CFHandle bytes(CGImageMetadataCreateXMPData(metadata.get(), nullptr));
            if (bytes)
                xmp =
                    QString::fromUtf8(reinterpret_cast<const char *>(CFDataGetBytePtr(bytes.get())),
                                      CFDataGetLength(bytes.get()));
        }
        result["xmp"] = xmp;
        return result;
    }
}
} // namespace ux
