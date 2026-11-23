#include "core/catalog/Identity.h"
#include "platform/macos/PhotoMetadataIO.h"
#include "platform/windows/MetadataPacket.h"
#include "platform/windows/UltraHdr.h"
#include <QCoreApplication>
#include <QFile>
#include <QImageReader>
#include <QtCore/qfloat16.h>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
#include <ultrahdr_api.h>

namespace {
void require(bool okay, const char *error) {
    if (!okay)
        throw std::runtime_error(error);
}
void requireSuccess(uhdr_error_info_t status, const char *operation) {
    if (status.error_code != UHDR_CODEC_OK)
        throw std::runtime_error(
            std::string(operation) + ": " +
            (status.has_detail ? status.detail : std::to_string(status.error_code)));
}
void verify(int width, int height, luma::MetadataPolicy policy) {
    luma::LinearOutput image(width, height);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const auto i = (static_cast<std::size_t>(y) * width + x) * 4;
            const float value = x < width / 2 ? 0.18F : 3.F;
            image.rgba[i] = image.rgba[i + 1] = image.rgba[i + 2] = value;
            image.rgba[i + 3] = 1;
        }
    const auto path = std::filesystem::current_path() / (luma::newID() + "-hdr-test.jpg");
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    } cleanup{path};
    luma::PhotoMetadata photo;
    photo.camera = "Fixture Camera";
    photo.lens = "Fixture 35mm Lens";
    photo.latitude = 43.11555;
    photo.longitude = 131.88549;
    luma::MetadataOverlay overlay;
    overlay.includeText = true;
    overlay.values.caption = QStringLiteral("Авторская подпись").toStdString();
    overlay.values.creators = {"Fixture Photographer"};
    overlay.values.copyright = "Fixture Copyright";
    overlay.values.keywords = {"Fixture Keyword"};
    overlay.values.location = "Fixture Location";
    const auto packet = luma::windows::exportXmp({}, photo, policy, overlay);
    luma::windows::writeUltraHdrJpeg(image, path, 95, {}, packet);
    QImageReader reader(QString::fromStdWString(path.native()), "jpeg");
    const auto base = reader.read();
    require(base.width() == width && base.height() == height, "SDR JPEG dimensions changed");
    require(base.pixelColor(width - 8, height / 2).red() > base.pixelColor(8, height / 2).red(),
            "SDR JPEG lost the highlight contrast");

    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Could not read the exported JPEG");
    auto bytes = file.readAll();
    file.close();
    require(bytes.contains("urn:iso:std:iso:ts:21496:-1") &&
                bytes.contains("http://ns.adobe.com/hdr-gain-map/1.0/"),
            "Missing ISO or XMP gain map metadata");
    const auto xmp = luma::windows::readEmbeddedXmp(path);
    const auto description = luma::macos::readDescriptiveMetadata(path, {});
    const bool full =
        policy == luma::MetadataPolicy::all || policy == luma::MetadataPolicy::removeLocation;
    const bool shareLocation = policy == luma::MetadataPolicy::all;
    const bool credit = policy != luma::MetadataPolicy::none;
    require(xmp.contains("GainMap"), "Primary XMP lost the gain map directory");
    require(xmp.contains("Fixture Camera") == full && xmp.contains("Fixture 35mm Lens") == full &&
                (description.caption == overlay.values.caption) == full &&
                (description.keywords == overlay.values.keywords) == full,
            "Source metadata or lens does not match the privacy policy");
    require(xmp.contains("GPSLatitude") == shareLocation &&
                xmp.contains("GPSLongitude") == shareLocation &&
                (description.location == overlay.values.location) == shareLocation,
            "Location does not match the privacy policy");
    require((description.copyright == overlay.values.copyright) == credit &&
                (description.creators == overlay.values.creators) == credit,
            "Author and copyright do not match the privacy policy");
    uhdr_compressed_image_t input{};
    input.data = bytes.data();
    input.data_sz = input.capacity = static_cast<std::size_t>(bytes.size());
    input.cg = UHDR_CG_UNSPECIFIED;
    input.ct = UHDR_CT_UNSPECIFIED;
    input.range = UHDR_CR_UNSPECIFIED;
    std::unique_ptr<uhdr_codec_private_t, decltype(&uhdr_release_decoder)> decoder(
        uhdr_create_decoder(), uhdr_release_decoder);
    require(decoder != nullptr, "Could not create reference decoder");
    requireSuccess(uhdr_dec_set_image(decoder.get(), &input), "Set decoder input");
    requireSuccess(uhdr_dec_set_out_img_format(decoder.get(), UHDR_IMG_FMT_64bppRGBAHalfFloat),
                   "Set decoder format");
    requireSuccess(uhdr_dec_set_out_color_transfer(decoder.get(), UHDR_CT_LINEAR),
                   "Set decoder transfer");
    requireSuccess(uhdr_dec_set_out_max_display_boost(decoder.get(), 8.F), "Set display boost");
    requireSuccess(uhdr_decode(decoder.get()), "HDR reconstruction failed");
    const auto result = uhdr_get_decoded_image(decoder.get());
    require(result && result->w == static_cast<unsigned>(width) &&
                result->h == static_cast<unsigned>(height),
            "HDR JPEG dimensions changed");
    const auto *pixels = static_cast<const unsigned char *>(result->planes[UHDR_PLANE_PACKED]);
    const auto offset = static_cast<std::size_t>(height / 2) * result->stride[0] * 4;
    const auto sample = [&](int x) {
        qfloat16 value;
        std::memcpy(&value, pixels + (offset + x * 4) * sizeof value, sizeof value);
        return float(value);
    };
    const auto midtone = sample(8);
    const auto highlight = sample(width - 8);
    if (std::abs(midtone - 0.18F) >= 0.07F || std::abs(highlight - 3.F) >= 0.3F)
        throw std::runtime_error("HDR JPEG recovered " + std::to_string(midtone) + " and " +
                                 std::to_string(highlight) + " instead of 0.18 and 3.0");
}
} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        verify(96, 64, luma::MetadataPolicy::none);
        verify(97, 63, luma::MetadataPolicy::all);
        verify(96, 64, luma::MetadataPolicy::copyrightOnly);
        verify(97, 63, luma::MetadataPolicy::removeLocation);
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
