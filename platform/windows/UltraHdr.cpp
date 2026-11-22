#include "platform/windows/UltraHdr.h"
#include "core/image/Bayer.h"
#include <QtCore/qfloat16.h>
#include <algorithm>
#include <fstream>
#include <memory>
#include <ultrahdr_api.h>

namespace luma::windows {
namespace {
void requireSuccess(uhdr_error_info_t status) {
    if (status.error_code != UHDR_CODEC_OK) {
        const auto end = std::find(std::begin(status.detail), std::end(status.detail), '\0');
        throw std::runtime_error(status.has_detail ? "Ultra HDR encoding failed: " +
                                                         std::string(status.detail, end)
                                                   : "Ultra HDR encoding failed.");
    }
}
} // namespace

void writeUltraHdrJpeg(const LinearOutput &image, const std::filesystem::path &path, int quality,
                       CancellationToken stop) {
    image.validate();
    checkCancellation(stop);
    static_assert(sizeof(qfloat16) == 2);
    std::vector<qfloat16> packed(image.rgba.size());
    // The linear input uses SDR white at 1.0 and is composited over black.
    // 203 cd/m² is the SDR white reference used by the codec.
    constexpr float sdrWhite = 203.F;
    constexpr float maximum = 10'000.F / sdrWhite;
    float peak = 1'000.F / sdrWhite;
    for (int y = 0; y < image.height; ++y) {
        checkCancellation(stop);
        const auto start = static_cast<std::size_t>(y) * image.width * 4;
        for (std::size_t i = start; i < start + static_cast<std::size_t>(image.width) * 4; i += 4) {
            for (int channel = 0; channel < 3; ++channel) {
                const auto value =
                    std::clamp(image.rgba[i + channel] * image.rgba[i + 3], 0.F, maximum);
                peak = std::max(peak, value);
                packed[i + channel] = qfloat16(value);
            }
            packed[i + 3] = qfloat16(1.F);
        }
    }
    std::unique_ptr<uhdr_codec_private_t, decltype(&uhdr_release_encoder)> encoder(
        uhdr_create_encoder(), uhdr_release_encoder);
    if (!encoder)
        throw std::runtime_error("Cannot create the Ultra HDR encoder.");
    uhdr_raw_image_t raw{};
    raw.fmt = UHDR_IMG_FMT_64bppRGBAHalfFloat;
    raw.cg = UHDR_CG_BT_709;
    raw.ct = UHDR_CT_LINEAR;
    raw.range = UHDR_CR_FULL_RANGE;
    raw.w = static_cast<unsigned>(image.width);
    raw.h = static_cast<unsigned>(image.height);
    raw.planes[UHDR_PLANE_PACKED] = packed.data();
    raw.stride[UHDR_PLANE_PACKED] = raw.w;
    requireSuccess(uhdr_enc_set_raw_image(encoder.get(), &raw, UHDR_HDR_IMG));
    requireSuccess(uhdr_enc_set_output_format(encoder.get(), UHDR_CODEC_JPG));
    requireSuccess(uhdr_enc_set_quality(encoder.get(), quality, UHDR_BASE_IMG));
    requireSuccess(uhdr_enc_set_quality(encoder.get(), 95, UHDR_GAIN_MAP_IMG));
    requireSuccess(uhdr_enc_set_target_display_peak_brightness(
        encoder.get(), std::min(peak * sdrWhite, 10'000.F)));
    requireSuccess(uhdr_enc_set_using_multi_channel_gainmap(encoder.get(), 1));
    // Downsampling odd dimensions changes the aspect ratio of small gain maps.
    const int gainMapScale = raw.w % 2 == 0 && raw.h % 2 == 0 ? 2 : 1;
    requireSuccess(uhdr_enc_set_gainmap_scale_factor(encoder.get(), gainMapScale));
    checkCancellation(stop);
    requireSuccess(uhdr_encode(encoder.get()));
    checkCancellation(stop);
    const auto *output = uhdr_get_encoded_stream(encoder.get());
    if (!output || !output->data || output->data_sz == 0)
        throw std::runtime_error("Ultra HDR encoder returned an empty image.");
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(static_cast<const char *>(output->data),
              static_cast<std::streamsize>(output->data_sz));
    out.close();
    if (!out)
        throw std::runtime_error("Cannot write the Ultra HDR JPEG.");
}
} // namespace luma::windows
