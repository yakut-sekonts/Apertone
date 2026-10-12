#include "core/raw/RawDecoder.h"
#include "core/image/Bayer.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <libraw/libraw.h>
#include <memory>
#include <sstream>

namespace luma {
namespace {
void requireSuccess(int code, const char *operation) {
    if (code != LIBRAW_SUCCESS) {
        throw std::runtime_error(std::string(operation) + ": " + libraw_strerror(code));
    }
}
template <std::size_t N> std::string text(const char (&value)[N]) {
    return {value, strnlen(value, N)};
}
std::optional<double> positive(double value) {
    return std::isfinite(value) && value > 0 ? std::optional<double>(value) : std::nullopt;
}
int cancelRaw(void *context, enum LibRaw_progress, int, int) {
    return static_cast<std::stop_token *>(context)->stop_requested() ? 1 : 0;
}
} // namespace

DecodedRaw RawDecoder::decode(const std::filesystem::path &path, std::stop_token stop,
                              const std::function<void(double)> &progress) const {
    checkCancellation(stop);
    if (!std::filesystem::is_regular_file(path) || std::filesystem::file_size(path) > 512'000'000) {
        throw std::runtime_error("RAW is missing or exceeds the 512 MB input budget.");
    }
    // LibRaw contains large metadata tables. macOS worker stacks are only 512 KB.
    const auto allocation = std::make_unique<LibRaw>();
    auto &decoder = *allocation;
    decoder.set_progress_handler(cancelRaw, &stop);
    decoder.imgdata.rawparams.max_raw_memory_mb = 512;
    requireSuccess(decoder.open_file(path.c_str()), "Open RAW");
    if (progress) {
        progress(0.1);
    }
    requireSuccess(decoder.unpack(), "Decode sensor data");
    checkCancellation(stop);
    const auto &data = decoder.imgdata;
    const auto &sizes = data.sizes;
    if (!data.rawdata.raw_image || data.idata.filters == 0 || data.idata.filters == 9 ||
        data.idata.colors != 3 || sizes.width < 2 || sizes.height < 2) {
        throw std::runtime_error(
            "M0 supports integer Bayer RAW only; X-Trans, monochrome and RGB RAW are planned.");
    }
    pixelCount(sizes.width, sizes.height);
    if (sizes.top_margin + sizes.height > sizes.raw_height ||
        sizes.left_margin + sizes.width > sizes.raw_width ||
        sizes.raw_pitch < static_cast<unsigned>(sizes.raw_width) * sizeof(unsigned short)) {
        throw std::runtime_error("Invalid RAW sensor bounds.");
    }
    PhotoMetadata metadata;
    metadata.camera = text(data.idata.make) + " " + text(data.idata.model);
    metadata.lens = text(data.lens.Lens);
    if (metadata.lens.empty()) {
        metadata.lens = text(data.lens.makernotes.Lens);
    }
    if (metadata.lens.empty() && data.lens.MinFocal > 0 && data.lens.MaxFocal > 0) {
        std::ostringstream description;
        description << data.lens.MinFocal;
        if (data.lens.MaxFocal != data.lens.MinFocal) {
            description << "–" << data.lens.MaxFocal;
        }
        description << " mm";
        if (data.lens.MaxAp4MinFocal > 0) {
            description << " f/" << data.lens.MaxAp4MinFocal;
            if (data.lens.MaxAp4MaxFocal > 0 &&
                data.lens.MaxAp4MaxFocal != data.lens.MaxAp4MinFocal) {
                description << "–" << data.lens.MaxAp4MaxFocal;
            }
        }
        metadata.lens = description.str();
    }
    metadata.iso = positive(data.other.iso_speed);
    metadata.shutterSeconds = positive(data.other.shutter);
    metadata.aperture = positive(data.other.aperture);
    metadata.focalLength = positive(data.other.focal_len);
    if (data.other.timestamp > 0) {
        metadata.captureTimestamp = data.other.timestamp;
    }
    metadata.width = sizes.width;
    metadata.height = sizes.height;
    metadata.orientation = sizes.flip;

    std::array<int, 4> cfa{};
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            const int channel = decoder.COLOR(y, x);
            cfa[y * 2 + x] = channel == 3 ? 1 : channel;
        }
    }
    BayerFrame frame(sizes.width, sizes.height, cfa);
    const auto &color = data.color;
    const auto green = color.cam_mul[1] > 0 ? color.cam_mul[1] : color.pre_mul[1];
    if (!std::isfinite(green) || green <= 0 || color.maximum == 0) {
        throw std::runtime_error("RAW has no usable white balance or white level.");
    }
    std::array<float, 4> gains{};
    for (int c = 0; c < 4; ++c) {
        const int channel = c == 3 ? 1 : c;
        const auto wb =
            color.cam_mul[channel] > 0 ? color.cam_mul[channel] : color.pre_mul[channel];
        gains[c] = wb / green;
        if (!std::isfinite(gains[c]) || gains[c] <= 0) {
            throw std::runtime_error("Invalid RAW white balance.");
        }
    }
    const auto stride = sizes.raw_pitch / sizeof(unsigned short);
    for (int y = 0; y < sizes.height; ++y) {
        checkCancellation(stop);
        for (int x = 0; x < sizes.width; ++x) {
            const int channel = decoder.COLOR(y, x);
            double black = static_cast<double>(color.black) + color.cblack[channel];
            if (color.cblack[4] > 0 && color.cblack[5] > 0) {
                const auto index =
                    6 + (y % color.cblack[4]) * color.cblack[5] + (x % color.cblack[5]);
                if (index >= LIBRAW_CBLACK_SIZE) {
                    throw std::runtime_error("Invalid RAW black-level grid.");
                }
                black += color.cblack[index];
            }
            const double white = color.maximum;
            if (black >= white) {
                throw std::runtime_error("Invalid RAW black/white levels.");
            }
            const double sample =
                data.rawdata.raw_image[(y + sizes.top_margin) * stride + x + sizes.left_margin];
            frame.samples[static_cast<std::size_t>(y) * sizes.width + x] =
                static_cast<float>(std::max(0.0, sample - black) / (white - black)) *
                gains[channel];
        }
    }
    std::array<float, 9> matrix{};
    for (int row = 0; row < 3; ++row) {
        matrix[row * 3] = color.rgb_cam[row][0];
        matrix[row * 3 + 1] = color.rgb_cam[row][1] + color.rgb_cam[row][3];
        matrix[row * 3 + 2] = color.rgb_cam[row][2];
    }
    decoder.recycle();
    if (progress) {
        progress(0.45);
    }
    auto image = demosaicBilinear(frame, stop);
    frame.samples.clear();
    frame.samples.shrink_to_fit();
    cameraToLinearSRGB(image, matrix, stop);
    if (progress) {
        progress(1);
    }
    return {std::move(image), std::move(metadata)};
}
} // namespace luma
