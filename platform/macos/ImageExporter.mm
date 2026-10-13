#include "platform/macos/ImageExporter.h"
#include "core/edits/EditDocument.h"
#include "core/export/JpegICC.h"
#include "core/image/Bayer.h"
#include "core/render/CpuRenderer.h"
#include "core/render/DevelopPipeline.h"
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <lcms2.h>
#include <unistd.h>

namespace luma::macos {
namespace {
template <typename T> class CFObject {
  public:
    explicit CFObject(T value) : value_(value) {
        if (!value)
            throw std::runtime_error("Cannot allocate export object.");
    }
    ~CFObject() {
        CFRelease(value_);
    }
    T get() const {
        return value_;
    }
    CFObject(const CFObject &) = delete;
    CFObject &operator=(const CFObject &) = delete;

  private:
    T value_;
};
class TemporaryFile {
  public:
    explicit TemporaryFile(const std::filesystem::path &destination) {
        auto pattern = destination.parent_path().string() + "/.luma-export-XXXXXX";
        std::vector<char> buffer(pattern.begin(), pattern.end());
        buffer.push_back(0);
        int fd = mkstemp(buffer.data());
        if (fd < 0)
            throw std::runtime_error("Cannot create export temporary file: " +
                                     std::string(std::strerror(errno)));
        close(fd);
        path = buffer.data();
    }
    ~TemporaryFile() {
        if (!path.empty()) {
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    }
    std::filesystem::path path;
};
std::vector<std::uint8_t> outputProfile() {
    auto profile = cmsCreate_sRGBProfile();
    if (!profile)
        throw std::runtime_error("Cannot create export ICC profile.");
    auto description = cmsMLUalloc(nullptr, 1);
    if (!description) {
        cmsCloseProfile(profile);
        throw std::runtime_error("Cannot allocate ICC description.");
    }
    const bool described = cmsMLUsetASCII(description, "en", "US", "Luma sRGB IEC 61966-2.1") &&
                           cmsWriteTag(profile, cmsSigProfileDescriptionTag, description);
    cmsMLUfree(description);
    cmsUInt32Number size = 0;
    if (!described || !cmsSaveProfileToMem(profile, nullptr, &size)) {
        cmsCloseProfile(profile);
        throw std::runtime_error("Cannot serialize export ICC.");
    }
    std::vector<std::uint8_t> bytes(size);
    const bool saved = cmsSaveProfileToMem(profile, bytes.data(), &size);
    cmsCloseProfile(profile);
    if (!saved || bytes.size() < 128)
        throw std::runtime_error("Cannot serialize export ICC.");
    // ICC header dateTimeNumber is fixed for reproducible exports across processes.
    const std::array<std::uint16_t, 6> date{2026, 1, 1, 0, 0, 0};
    for (std::size_t i = 0; i < date.size(); ++i) {
        bytes[24 + i * 2] = static_cast<std::uint8_t>(date[i] >> 8);
        bytes[25 + i * 2] = static_cast<std::uint8_t>(date[i] & 255);
    }
    return bytes;
}
std::uint16_t encode16(float value) {
    const double linear = std::clamp(static_cast<double>(value), 0.0, 1.0);
    const double encoded =
        linear <= 0.0031308 ? 12.92 * linear : 1.055 * std::pow(linear, 1 / 2.4) - 0.055;
    return static_cast<std::uint16_t>(std::lround(encoded * 65535));
}
} // namespace
void writeImage(const DecodedRaw &decoded, EditDocument edits,
                const std::filesystem::path &destination, ExportOptions options,
                CancellationToken stop, const std::function<void(double)> &progress) {
    @autoreleasepool {
        options.validate();
        edits.validate();
        checkCancellation(stop);
        const auto parent = std::filesystem::canonical(
            destination.parent_path().empty() ? "." : destination.parent_path());
        const auto output = parent / destination.filename();
        if (std::filesystem::exists(output) && !options.overwrite)
            throw std::runtime_error(
                "Destination exists. Choose another name or explicitly enable overwrite.");
        TemporaryFile temporary(output);
        const int orientation = decoded.metadata.orientation;
        const auto &input = decoded.image;
        if (orientation < 0 || orientation > 7 ||
            input.rgb.size() != pixelCount(input.width, input.height) * 3)
            throw std::runtime_error("Invalid full-resolution export input.");
        const int width = orientation & 4 ? input.height : input.width;
        const int height = orientation & 4 ? input.width : input.height;
        std::vector<std::uint8_t> rgba;
        std::vector<std::uint16_t> rgb16;
        int bits = 8, bytesPerPixel = 4;
        CGBitmapInfo bitmap = kCGImageAlphaLast;
        const void *bytes;
        std::size_t size;
        if (options.format == ExportFormat::tiff16) {
            bits = 16;
            bytesPerPixel = 6;
            bitmap = static_cast<CGBitmapInfo>(kCGImageAlphaNone) | kCGBitmapByteOrder16Little;
            rgb16.resize(pixelCount(width, height) * 3);
            const DevelopPipeline pipeline(edits, decoded.calibration);
            const SharpeningPass sharpening(input, pipeline, edits, stop, 1, DetailDiagnostic::none,
                                            [&](double value) {
                                                if (progress)
                                                    progress(0.5 + 0.15 * value);
                                            });
            for (int y = 0; y < height; ++y) {
                checkCancellation(stop);
                for (int x = 0; x < width; ++x) {
                    int sx = orientation & 4 ? y : x, sy = orientation & 4 ? x : y;
                    if (orientation & 1)
                        sx = input.width - 1 - sx;
                    if (orientation & 2)
                        sy = input.height - 1 - sy;
                    const auto offset = (static_cast<std::size_t>(sy) * input.width + sx) * 3;
                    const auto pixel =
                        sharpening.pixel(pipeline.pixel({input.rgb[offset], input.rgb[offset + 1],
                                                         input.rgb[offset + 2]}),
                                         sx, sy);
                    const auto target = (static_cast<std::size_t>(y) * width + x) * 3;
                    for (std::size_t c = 0; c < 3; ++c)
                        rgb16[target + c] = encode16(pixel[c]);
                }
                if (progress)
                    progress((sharpening.active() ? 0.65 : 0.5) +
                             (sharpening.active() ? 0.25 : 0.4) * static_cast<double>(y + 1) /
                                 height);
            }
            bytes = rgb16.data();
            size = rgb16.size() * sizeof(std::uint16_t);
        } else {
            auto rendered = renderCPU(input, edits, orientation, {}, stop, decoded.calibration);
            rgba = std::move(rendered.rgba);
            bytes = rgba.data();
            size = rgba.size();
            if (progress)
                progress(0.9);
        }
        const auto profile = outputProfile();
        CFObject profileData(
            CFDataCreate(nullptr, profile.data(), static_cast<CFIndex>(profile.size())));
        CFObject color(CGColorSpaceCreateWithICCData(profileData.get()));
        CFObject provider(CGDataProviderCreateWithData(nullptr, bytes, size, nullptr));
        CFObject image(CGImageCreate(width, height, bits, bytesPerPixel * 8,
                                     static_cast<std::size_t>(width) * bytesPerPixel, color.get(),
                                     bitmap, provider.get(), nullptr, false,
                                     kCGRenderingIntentRelativeColorimetric));
        CFObject url(CFURLCreateFromFileSystemRepresentation(
            nullptr, reinterpret_cast<const UInt8 *>(temporary.path.c_str()),
            static_cast<CFIndex>(temporary.path.string().size()), false));
        CFStringRef type = options.format == ExportFormat::jpeg  ? CFSTR("public.jpeg")
                           : options.format == ExportFormat::png ? CFSTR("public.png")
                                                                 : CFSTR("public.tiff");
        CFObject writer(CGImageDestinationCreateWithURL(url.get(), type, 1, nullptr));
        NSDictionary *properties = @{
            (__bridge NSString *)
            kCGImageDestinationLossyCompressionQuality : @(options.quality / 100.0),
            (__bridge NSString *)kCGImagePropertyOrientation : @1
        };
        CGImageDestinationAddImage(writer.get(), image.get(), (__bridge CFDictionaryRef)properties);
        checkCancellation(stop);
        if (!CGImageDestinationFinalize(writer.get()))
            throw std::runtime_error("ImageIO could not finish export.");
        checkCancellation(stop);
        if (options.format == ExportFormat::jpeg) {
            std::ifstream inputFile(temporary.path, std::ios::binary);
            const std::vector<std::uint8_t> original((std::istreambuf_iterator<char>(inputFile)),
                                                     {});
            if (!inputFile.eof() && inputFile.fail())
                throw std::runtime_error("Cannot read JPEG for ICC embedding.");
            inputFile.close();
            const auto tagged = embedJpegICC(original, profile);
            std::ofstream outputFile(temporary.path, std::ios::binary | std::ios::trunc);
            outputFile.write(reinterpret_cast<const char *>(tagged.data()),
                             static_cast<std::streamsize>(tagged.size()));
            outputFile.close();
            if (!outputFile)
                throw std::runtime_error("Cannot embed JPEG ICC profile.");
        }
        checkCancellation(stop);
        int fd = open(temporary.path.c_str(), O_RDONLY);
        if (fd < 0)
            throw std::runtime_error("Cannot reopen exported data for sync.");
        const int syncResult = fsync(fd);
        close(fd);
        if (syncResult != 0)
            throw std::runtime_error("Cannot sync exported data.");
        checkCancellation(stop);
        // link() refuses an existing destination atomically, including a concurrent writer.
        const int installed = options.overwrite ? rename(temporary.path.c_str(), output.c_str())
                                                : link(temporary.path.c_str(), output.c_str());
        if (installed != 0)
            throw std::runtime_error("Cannot install exported file: " +
                                     std::string(std::strerror(errno)));
        if (options.overwrite)
            temporary.path.clear();
        if (progress)
            progress(1);
    }
}
} // namespace luma::macos
