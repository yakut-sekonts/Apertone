#include "platform/macos/ImageExporter.h"
#include "core/export/DngWriter.h"
#include "core/export/LinearOutput.h"
#include "core/image/Bayer.h"
#include "core/io/FileSystem.h"
#include "core/io/StagingFile.h"
#include "platform/windows/MetadataPacket.h"
#include "platform/windows/TiffWriter.h"
#include "platform/windows/UltraHdr.h"
#include <QColorSpace>
#include <QColorTransform>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QImageReader>
#include <QImageWriter>
#include <QPainter>
#include <QRgbaFloat>
#include <algorithm>
#include <cmath>

namespace luma::macos {
namespace {
QColorSpace colorSpace(ExportColorSpace space) {
    switch (space) {
    case ExportColorSpace::srgb:
        return QColorSpace::SRgb;
    case ExportColorSpace::displayP3:
        return QColorSpace::DisplayP3;
    case ExportColorSpace::adobeRGB:
        return QColorSpace::AdobeRgb;
    case ExportColorSpace::proPhoto:
        return QColorSpace::ProPhotoRgb;
    }
    throw std::runtime_error("Unsupported export color space.");
}

QImage watermarkImage(const WatermarkOptions &options, int box) {
    if (options.kind == WatermarkKind::image) {
        QImageReader reader(QString::fromStdWString(options.image.native()));
        reader.setAutoTransform(true);
        auto image = reader.read();
        if (image.isNull())
            throw std::runtime_error("Cannot read watermark image.");
        return image.scaled(box, box, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if (!qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
        throw std::runtime_error("Text watermark requires a Qt GUI application.");
    const auto text = QString::fromStdString(options.text);
    QFont font;
    font.setPixelSize(std::clamp(box, 1, 128));
    const auto bounds = QFontMetrics(font).boundingRect(text);
    QImage image(std::max(1, bounds.width() + 2), std::max(1, bounds.height() + 2),
                 QImage::Format_ARGB32_Premultiplied);
    if (image.isNull())
        throw std::runtime_error("Cannot allocate text watermark.");
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setPen(Qt::white);
        painter.setFont(font);
        painter.drawText(1 - bounds.x(), 1 - bounds.y(), text);
    }
    return image.scaled(box, box, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void applyWatermark(LinearOutput &output, const WatermarkOptions &options, CancellationToken stop) {
    if (options.kind == WatermarkKind::none || options.opacity == 0)
        return;
    const int margin = std::lround(std::min(output.width, output.height) * options.margin / 100);
    const int box = std::max(1, static_cast<int>(std::lround(std::max(output.width, output.height) *
                                                             options.size / 100)));
    auto mark = watermarkImage(options, std::min({box, std::max(1, output.width - margin * 2),
                                                  std::max(1, output.height - margin * 2)}));
    if (!mark.colorSpace().isValid())
        mark.setColorSpace(QColorSpace::SRgb);
    mark.convertToColorSpace(QColorSpace::SRgbLinear, QImage::Format_RGBA32FPx4);
    if (mark.format() != QImage::Format_RGBA32FPx4)
        throw std::runtime_error("Cannot color-manage the watermark.");
    const auto offset = [margin](int space, int anchor) {
        return anchor == 1 ? space / 2 : anchor == 0 ? margin : space - margin;
    };
    const int x0 = std::max(0, offset(output.width - mark.width(), options.anchor % 3));
    const int y0 = std::max(0, offset(output.height - mark.height(), options.anchor / 3));
    for (int y = 0; y < mark.height(); ++y) {
        checkCancellation(stop);
        const auto *row = reinterpret_cast<const QRgbaFloat32 *>(mark.constScanLine(y));
        for (int x = 0; x < mark.width(); ++x) {
            auto *pixel =
                output.rgba.data() + (static_cast<std::size_t>(y + y0) * output.width + x + x0) * 4;
            const auto ink = row[x];
            const float alpha = ink.alpha() * static_cast<float>(options.opacity / 100);
            const float remaining = pixel[3] * (1 - alpha), coverage = alpha + remaining;
            if (coverage > 0) {
                pixel[0] = (ink.red() * alpha + pixel[0] * remaining) / coverage;
                pixel[1] = (ink.green() * alpha + pixel[1] * remaining) / coverage;
                pixel[2] = (ink.blue() * alpha + pixel[2] * remaining) / coverage;
                pixel[3] = coverage;
            }
        }
    }
}

void encodeRaster(const LinearOutput &image, const QColorSpace &space, const ExportOptions &opts,
                  const QString &path, CancellationToken stop) {
    QImage input(reinterpret_cast<const uchar *>(image.rgba.data()), image.width, image.height,
                 static_cast<qsizetype>(image.width) * 16, QImage::Format_RGBA32FPx4);
    input.setColorSpace(QColorSpace::SRgbLinear);
    // JPEG composites transparency over black before conversion to encoded RGB.
    if (opts.format == ExportFormat::jpeg) {
        input = input.convertToFormat(QImage::Format_RGBA32FPx4_Premultiplied);
        if (input.isNull())
            throw std::runtime_error("Cannot allocate JPEG image.");
        for (int y = 0; y < image.height; ++y) {
            checkCancellation(stop);
            auto *row = reinterpret_cast<QRgbaFloat32 *>(input.scanLine(y));
            for (int x = 0; x < image.width; ++x)
                row[x].a = 1;
        }
    }
    const auto format = opts.format == ExportFormat::tiff16        ? QImage::Format_RGBA64
                        : opts.format == ExportFormat::jpeg        ? QImage::Format_RGB888
                        : opts.compression == TiffCompression::zip ? QImage::Format_ARGB32
                                                                   : QImage::Format_RGBA8888;
    const auto output = input.convertedToColorSpace(space, format);
    if (output.isNull())
        throw std::runtime_error("Cannot convert export image to the selected color space.");
    if (opts.compression == TiffCompression::zip) {
        windows::writeZipTiff(output, std::filesystem::path(path.toStdWString()), stop);
        return;
    }
    const QByteArray codec = opts.format == ExportFormat::jpeg  ? "jpeg"
                             : opts.format == ExportFormat::png ? "png"
                                                                : "tiff";
    QImageWriter writer(path, codec);
    if (opts.format == ExportFormat::jpeg)
        writer.setQuality(opts.quality);
    if (codec == "tiff") {
        writer.setCompression(opts.compression == TiffCompression::lzw ? 1 : 0);
    }
    checkCancellation(stop);
    if (!writer.write(output))
        throw std::runtime_error("Cannot encode export image: " +
                                 writer.errorString().toStdString());
}
} // namespace

void writeImage(const DecodedRaw &decoded, EditDocument edits,
                const std::filesystem::path &destination, ExportOptions options,
                CancellationToken stop, const std::function<void(double)> &progress,
                const std::filesystem::path &source, const ExportInstallGuard &guard,
                const std::optional<MetadataOverlay> &overlay) {
    options.validate();
    edits.validate();
    checkCancellation(stop);
    if (options.format == ExportFormat::heicHDR)
        throw std::runtime_error("Adaptive HDR HEIC is unavailable on Windows.");
    if (options.format == ExportFormat::jpegHDR && options.metadata != MetadataPolicy::none)
        throw std::runtime_error("Source metadata is unavailable for Ultra HDR JPEG export.");
    const auto target = std::filesystem::absolute(destination);
    if (!options.overwrite && std::filesystem::exists(target))
        throw std::runtime_error("Destination already exists.");
    StagingFile temporary(target.parent_path(), ".luma-export-");
    const auto &staging = temporary.path();
    const auto packet = windows::exportXmp(source, decoded.metadata, options.metadata, overlay);
    const auto space = colorSpace(options.colorSpace);
    auto image = renderLinearOutput(decoded, edits, stop,
                                    progress ? [&](double value) { progress(0.75 * value); }
                                             : std::function<void(double)>{});
    image = resizeOutput(std::move(image), options, stop);
    sharpenOutput(image, options, stop);
    applyWatermark(image, options.watermark, stop);
    if (options.format == ExportFormat::dng) {
        const auto linear = space.withTransferFunction(QColorSpace::TransferFunction::Linear);
        const auto profile = linear.iccProfile();
        const auto transform =
            QColorSpace(QColorSpace::SRgbLinear).transformationToColorSpace(linear);
        const auto bytes = std::span(reinterpret_cast<const std::uint8_t *>(profile.constData()),
                                     static_cast<std::size_t>(profile.size()));
        writeLinearDNG(
            image, rgbToXYZD50(bytes),
            [&](std::span<const float> source, std::span<float> dest) {
                for (std::size_t i = 0; i < source.size(); i += 3) {
                    const auto value =
                        transform.map(QRgbaFloat32{source[i], source[i + 1], source[i + 2], 1.F});
                    dest[i] = value.red();
                    dest[i + 1] = value.green();
                    dest[i + 2] = value.blue();
                }
            },
            staging, space.description().toStdString(),
            {reinterpret_cast<const std::uint8_t *>(packet.constData()),
             static_cast<std::size_t>(packet.size())},
            stop);
    } else if (options.format == ExportFormat::jpegHDR) {
        windows::writeUltraHdrJpeg(image, staging, options.quality, stop);
    } else {
        std::optional<StagingFile> raster;
        if (!packet.isEmpty() &&
            (options.format == ExportFormat::jpeg || options.format == ExportFormat::png))
            raster.emplace(staging.parent_path(), ".luma-export-");
        const auto &encoded = raster ? raster->path() : staging;
        encodeRaster(image, space, options, QString::fromStdWString(encoded.native()), stop);
        windows::embedXmp(encoded, staging, options.format, packet, stop);
    }
    checkCancellation(stop);
    if (files::syncFile(staging) != 0)
        throw std::runtime_error("Cannot sync export staging file.");
    const auto install = [&] {
        checkCancellation(stop);
        if (files::move(staging, target, options.overwrite) != 0)
            throw std::runtime_error("Cannot install export at the selected path.");
    };
    if (guard)
        guard(install);
    else
        install();
    if (progress)
        progress(1);
}
} // namespace luma::macos
