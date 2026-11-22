#include "platform/macos/RasterDecoder.h"
#include "core/image/Bayer.h"
#include "platform/windows/MetadataPacket.h"
#include <QColorSpace>
#include <QImageReader>
#include <QRgbaFloat>
#include <algorithm>

namespace luma::macos {
namespace {
QString localName(const std::filesystem::path &path) {
    return QString::fromStdWString(path.native());
}
PhotoMetadata metadata(QImageReader &reader) {
    if (!reader.canRead())
        throw std::runtime_error("Cannot open raster image: " + reader.errorString().toStdString());
    const auto size = reader.size();
    pixelCount(size.width(), size.height());
    const auto transform = reader.transformation();
    const bool swapped = transform.testFlag(QImageIOHandler::TransformationRotate90);
    PhotoMetadata result;
    result.width = swapped ? size.height() : size.width();
    result.height = swapped ? size.width() : size.height();
    return result;
}
} // namespace

bool RasterDecoder::available(const std::string &extension) const {
    if (extension.size() <= 1 || extension.front() != '.')
        return false;
    const auto format = QByteArray::fromStdString(extension.substr(1));
    return QImageReader::supportedImageFormats().contains(format);
}

bool RasterDecoder::cameraRaw(const std::filesystem::path &) const {
    // PhotoDecoder detects known RAW extensions and TIFF-encapsulated RAW with LibRaw.
    return false;
}

PhotoMetadata RasterDecoder::inspect(const std::filesystem::path &path, CancellationToken stop) const {
    checkCancellation(stop);
    QImageReader reader(localName(path));
    auto result = metadata(reader);
    windows::readRasterProperties(path, result);
    return result;
}

DecodedImage RasterDecoder::decode(const std::filesystem::path &path, CancellationToken stop,
                                   const std::function<void(double)> &progress) const {
    checkCancellation(stop);
    QImageReader reader(localName(path));
    reader.setAutoTransform(true);
    auto info = metadata(reader);
    windows::readRasterProperties(path, info);
    auto image = reader.read();
    checkCancellation(stop);
    if (image.isNull() || image.width() != info.width || image.height() != info.height)
        throw std::runtime_error("Cannot decode raster image: " + reader.errorString().toStdString());
    if (!image.colorSpace().isValid())
        image.setColorSpace(QColorSpace::SRgb);
    image.convertToColorSpace(QColorSpace::SRgbLinear, QImage::Format_RGBA32FPx4);
    if (image.format() != QImage::Format_RGBA32FPx4)
        throw std::runtime_error("Cannot convert raster image to linear sRGB.");
    FloatImage output(info.width, info.height);
    output.alpha.resize(pixelCount(info.width, info.height));
    bool opaque = true;
    for (int y = 0; y < info.height; ++y) {
        checkCancellation(stop);
        const auto *row = reinterpret_cast<const QRgbaFloat32 *>(image.constScanLine(y));
        for (int x = 0; x < info.width; ++x) {
            const auto i = static_cast<std::size_t>(y) * info.width + x;
            const auto &pixel = row[x];
            const float alpha = pixel.alpha();
            if (!std::isfinite(alpha) || alpha < 0 || alpha > 1)
                throw std::runtime_error("Invalid raster alpha channel.");
            output.alpha[i] = alpha;
            opaque = opaque && alpha == 1;
            const float values[] = {pixel.red(), pixel.green(), pixel.blue()};
            for (int c = 0; c < 3; ++c) {
                if (!std::isfinite(values[c]))
                    throw std::runtime_error("Invalid raster color sample.");
                output.rgb[i * 3 + c] = alpha > 0 ? values[c] : 0;
            }
        }
        if (progress)
            progress((y + 1.) / info.height);
    }
    if (opaque)
        std::vector<float>().swap(output.alpha);
    DecodeDiagnostics diagnostics;
    diagnostics.decoder = "Qt Image IO";
    diagnostics.decoderVersion = QT_VERSION_STR;
    diagnostics.source = DecodeSource::raster;
    diagnostics.color = DecodeColorPath::embeddedProfileToLinearSRGB;
    diagnostics.highlights = DecodeHighlightPath::sourceRange;
    diagnostics.sourceWidth = diagnostics.activeWidth = info.width;
    diagnostics.sourceHeight = diagnostics.activeHeight = info.height;
    return {std::move(output), std::move(info), {}, std::move(diagnostics)};
}
} // namespace luma::macos
