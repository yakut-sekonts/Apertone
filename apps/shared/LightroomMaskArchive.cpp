#include "apps/shared/LightroomMaskArchive.h"
#include "apps/shared/LightroomDevelop.h"
#include "core/image/Bayer.h"
#include "core/io/Path.h"
#include "core/io/TiffDirectory.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QUuid>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <jxl/decode.h>
#include <memory>

namespace luma {
namespace {
constexpr qsizetype maximumMaskBytes = 12 * 1024 * 1024;
constexpr qsizetype maximumArchiveIndex = 32'768;
constexpr auto envelope = QByteArrayView("\x02\0\0\0\x01\0\0\0\0\0\0\0\0\0\0\0", 16);
constexpr std::uint32_t blobMagic = 0x00248f37;

template <typename U> U little(QByteArrayView bytes, qsizetype offset) {
    return qFromLittleEndian<U>(bytes.data() + offset);
}
std::span<const std::uint8_t> view(QByteArrayView bytes) {
    return {reinterpret_cast<const std::uint8_t *>(bytes.data()),
            static_cast<std::size_t>(bytes.size())};
}
void require(bool valid) {
    if (!valid)
        throw std::runtime_error("Unsupported or damaged Lightroom mask data.");
}
bool digestMatches(QByteArrayView data, QByteArrayView digest) {
    return QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex() == digest;
}

struct MaskTileLayout {
    int width, height, tileWidth, tileHeight, compression, color;
    std::vector<std::uint32_t> offsets, sizes;
    std::size_t extent;
};

MaskTileLayout layout(const TiffDirectory &tiff) {
    const auto number = [&](std::uint16_t tag, std::uint32_t initial = 0) {
        const auto values = tiff.integers(tag, 1);
        return values.empty() ? initial : values.front();
    };
    const auto width = number(256), height = number(257), compression = number(259, 1);
    const auto color = number(262);
    const bool tiled = tiff.has(324);
    const auto tileWidth = tiled ? number(322) : width;
    const auto tileHeight = tiled ? number(323) : std::min(height, number(278, height));
    require(width > 0 && height > 0 && width <= 8192 && height <= 8192 &&
            static_cast<std::uint64_t>(width) * height <= 24'000'000 && tileWidth > 0 &&
            tileHeight > 0 && tileWidth <= 8192 && tileHeight <= 8192 &&
            static_cast<std::uint64_t>(tileWidth) * tileHeight <= 24'000'000 &&
            (compression == 52546 || compression == 1) && color <= 1 && number(258) == 8 &&
            number(277, 1) == 1 && number(339, 1) == 1 && number(284, 1) == 1);
    auto offsets = tiff.integers(tiled ? 324 : 273, 1024);
    auto sizes = tiff.integers(tiled ? 325 : 279, 1024);
    const std::size_t count =
        ((width + tileWidth - 1) / tileWidth) * ((height + tileHeight - 1) / tileHeight);
    require(offsets.size() == count && sizes.size() == count);
    auto extent = tiff.extent();
    for (std::size_t i = 0; i < count; ++i) {
        require(sizes[i] && offsets[i] >= tiff.extent());
        (void)tiff.slice(offsets[i], sizes[i]);
        extent = std::max(extent, static_cast<std::size_t>(offsets[i]) + sizes[i]);
    }
    return {static_cast<int>(width),
            static_cast<int>(height),
            static_cast<int>(tileWidth),
            static_cast<int>(tileHeight),
            static_cast<int>(compression),
            static_cast<int>(color),
            std::move(offsets),
            std::move(sizes),
            extent};
}

struct DecodedTile {
    std::vector<std::uint8_t> gray;
    int stride = 0;
};

DecodedTile decodeJxl(std::span<const std::uint8_t> data, int width, int height, int minWidth,
                      int minHeight, CancellationToken stop) {
    const std::unique_ptr<JxlDecoder, decltype(&JxlDecoderDestroy)> decoder(
        JxlDecoderCreate(nullptr), JxlDecoderDestroy);
    require(decoder &&
            JxlDecoderSubscribeEvents(decoder.get(), JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE) ==
                JXL_DEC_SUCCESS &&
            JxlDecoderSetInput(decoder.get(), data.data(), data.size()) == JXL_DEC_SUCCESS);
    JxlDecoderCloseInput(decoder.get());
    const JxlPixelFormat grayscale{1, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
    DecodedTile tile;
    auto size = std::size_t{0};
    bool full = false;
    for (int event = 0; event < 12; ++event) {
        checkCancellation(stop);
        const auto status = JxlDecoderProcessInput(decoder.get());
        if (status == JXL_DEC_BASIC_INFO) {
            JxlBasicInfo info{};
            require(JxlDecoderGetBasicInfo(decoder.get(), &info) == JXL_DEC_SUCCESS &&
                    info.xsize >= static_cast<unsigned>(minWidth) &&
                    info.xsize <= static_cast<unsigned>(width) &&
                    info.ysize >= static_cast<unsigned>(minHeight) &&
                    info.ysize <= static_cast<unsigned>(height) && info.num_color_channels == 1 &&
                    info.num_extra_channels == 0 && info.bits_per_sample == 8 &&
                    !info.have_animation);
            tile.stride = static_cast<int>(info.xsize);
            size = static_cast<std::size_t>(info.xsize) * info.ysize;
        } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            require(size && tile.gray.empty());
            tile.gray.resize(size);
            require(JxlDecoderSetImageOutBuffer(decoder.get(), &grayscale, tile.gray.data(),
                                                size) == JXL_DEC_SUCCESS);
        } else if (status == JXL_DEC_FULL_IMAGE) {
            full = true;
        } else if (status == JXL_DEC_SUCCESS && full && !tile.gray.empty()) {
            return tile;
        } else {
            break;
        }
    }
    throw std::runtime_error("Cannot decode the Lightroom JPEG XL mask tile.");
}

std::vector<std::uint8_t> pixels(const TiffDirectory &tiff, const MaskTileLayout &info,
                                 CancellationToken stop) {
    std::vector<std::uint8_t> result(static_cast<std::size_t>(info.width) * info.height);
    const auto columns = (info.width + info.tileWidth - 1) / info.tileWidth;
    for (std::size_t i = 0; i < info.offsets.size(); ++i) {
        checkCancellation(stop);
        const auto x = static_cast<int>(i % columns) * info.tileWidth;
        const auto y = static_cast<int>(i / columns) * info.tileHeight;
        const int copyWidth = std::min(info.width - x, info.tileWidth);
        const int copyHeight = std::min(info.height - y, info.tileHeight);
        const auto encoded = tiff.slice(info.offsets[i], info.sizes[i]);
        DecodedTile decoded;
        if (info.compression == 52546)
            decoded =
                decodeJxl(encoded, info.tileWidth, info.tileHeight, copyWidth, copyHeight, stop);
        else {
            require(encoded.size() >= static_cast<std::size_t>(info.tileWidth) * copyHeight);
            decoded = {{encoded.begin(), encoded.end()}, info.tileWidth};
        }
        for (int row = 0; row < copyHeight; ++row) {
            const auto *start =
                decoded.gray.data() + static_cast<std::size_t>(row) * decoded.stride;
            auto *target = result.data() + static_cast<std::size_t>(y + row) * info.width + x;
            std::transform(start, start + copyWidth, target, [&](std::uint8_t density) {
                return info.color ? density : 255 - density;
            });
        }
    }
    return result;
}

double coordinate(const QString &text) {
    const auto parts = text.split('/');
    bool parsed = false, denominatorParsed = true;
    require(parts.size() == 1 || parts.size() == 2);
    const double value = parts[0].toDouble(&parsed);
    const double denominator = parts.size() == 2 ? parts[1].toDouble(&denominatorParsed) : 1.0;
    require(parsed && denominatorParsed && denominator != 0 && std::isfinite(value / denominator) &&
            std::abs(value / denominator) <= 16'384);
    return value / denominator;
}

BitmapMask placeMask(const TiffDirectory &directory, const MaskTileLayout &info,
                     const QString &origin, const QString &area, CancellationToken stop) {
    const auto parts = area.split(','), offset = origin.split(',');
    require(parts.size() == 4 && offset.size() == 2);
    const double left = coordinate(parts[1]), top = coordinate(parts[0]);
    const double width = coordinate(parts[3]) - left, height = coordinate(parts[2]) - top;
    const double x0 = coordinate(offset[0]) - left, y0 = coordinate(offset[1]) - top;
    require(width > 0 && height > 0 && x0 < width && y0 < height && x0 + info.width > 0 &&
            y0 + info.height > 0);
    const auto raw = pixels(directory, info, stop);
    // Store in normalized source coordinates. Preserve the placement of cropped
    // tiles, including the zero-coverage area outside the stored rectangle.
    const double scale = std::min(1.0, 768.0 / std::max(width, height));
    const int columns = std::max(1, static_cast<int>(std::round(width * scale)));
    const int rows = std::max(1, static_cast<int>(std::round(height * scale)));
    std::vector<std::uint8_t> result(static_cast<std::size_t>(columns) * rows);
    for (int y = 0; y < rows; ++y) {
        checkCancellation(stop);
        const auto sy = static_cast<int>(std::floor((y + 0.5) * height / rows - y0));
        if (sy < 0 || sy >= info.height)
            continue;
        for (int x = 0; x < columns; ++x) {
            const auto sx = static_cast<int>(std::floor((x + 0.5) * width / columns - x0));
            if (sx >= 0 && sx < info.width)
                result[static_cast<std::size_t>(y) * columns + x] =
                    raw[static_cast<std::size_t>(sy) * info.width + sx];
        }
    }
    return BitmapMask(columns, rows, std::move(result));
}
} // namespace

LightroomMaskArchive::LightroomMaskArchive(const std::filesystem::path &catalog,
                                           CancellationToken stop) {
    auto companion = catalog;
    companion.replace_extension(".lrcat-data");
    const QDir root(QString::fromStdString(pathUtf8(companion)));
    const auto files = root.entryInfoList({"*.blob", "*.sst", "*.log"},
                                          QDir::Files | QDir::NoSymLinks, QDir::Name);
    for (const auto &file : files.sliced(0, std::min<qsizetype>(files.size(), 256))) {
        checkCancellation(stop);
        if (file.suffix() == "blob") {
            indexBlob(file.absoluteFilePath(), stop);
        } else if (file.size() <= 64 * 1024 * 1024) {
            if (file.suffix() == "log") {
                indexLog(file.absoluteFilePath(), stop);
            } else {
                QFile in(file.absoluteFilePath());
                if (in.open(QIODevice::ReadOnly))
                    indexInline(in.read(64 * 1024 * 1024), file.absoluteFilePath(), stop);
            }
        }
    }
}

void LightroomMaskArchive::indexBlob(const QString &path, CancellationToken stop) {
    QFile input(path);
    if (!input.open(QIODevice::ReadOnly))
        return;
    const auto header = input.read(30);
    if (header.size() != 30 || little<std::uint32_t>(header, 0) != blobMagic ||
        little<std::uint32_t>(header, 4) != 1 || header[12] != 0 || header[13] != 0)
        return;
    while (input.size() - input.pos() >= 32 && masks_.size() < maximumArchiveIndex) {
        checkCancellation(stop);
        const auto frame = input.read(32);
        if (frame.size() != 32)
            break;
        const auto keyBytes = little<std::uint64_t>(frame, 0),
                   bytes = little<std::uint64_t>(frame, 8);
        const auto available = static_cast<std::uint64_t>(input.size() - input.pos());
        if (keyBytes == 0 || keyBytes > 1024 || keyBytes > available ||
            bytes > available - keyBytes)
            break;
        const auto key = input.read(static_cast<qint64>(keyBytes)).toLower();
        const auto position = input.pos();
        if (key.size() == 32 && bytes >= 24 && bytes <= maximumMaskBytes &&
            QByteArray::fromHex(key).toHex() == key)
            masks_.try_emplace(key, Payload{path, position, static_cast<qint64>(bytes), {}});
        if (!input.seek(position + static_cast<qint64>(bytes)))
            break;
    }
}

void LightroomMaskArchive::indexInline(const QByteArray &data, const QString &file,
                                       CancellationToken stop) {
    // Inline values in the observed uncompressed SST blocks and WAL batches
    // retain the same self-contained TIFF envelope as the blob values.
    const QByteArrayView bytes(data);
    qsizetype at = 0;
    while (masks_.size() < maximumArchiveIndex && (at = bytes.indexOf(envelope, at)) >= 0) {
        checkCancellation(stop);
        const auto start = at++;
        try {
            const auto tail = bytes.sliced(start + envelope.size());
            const TiffDirectory directory(view(tail));
            const auto info = layout(directory);
            if (info.extent > 65536 - envelope.size())
                continue;
            const auto length = envelope.size() + static_cast<qsizetype>(info.extent);
            const auto insert = [&](QByteArrayView span) {
                auto digest = QCryptographicHash::hash(span, QCryptographicHash::Md5).toHex();
                if (!masks_.contains(digest) &&
                    (!file.isEmpty() || inlineBytes_ + span.size() <= 64 * 1024 * 1024)) {
                    auto inlineValue = file.isEmpty() ? span.toByteArray() : QByteArray{};
                    inlineBytes_ += static_cast<std::size_t>(inlineValue.size());
                    masks_.emplace(std::move(digest),
                                   Payload{file, start, span.size(), std::move(inlineValue)});
                }
            };
            insert(bytes.sliced(start, length));
            if (length % 2 && start + length < bytes.size() && bytes[start + length] == 0)
                insert(bytes.sliced(start, length + 1));
            at = start + length;
        } catch (const std::runtime_error &) {
            // The same marker can occur in unrelated data; a candidate must parse as a mask.
        }
    }
}

void LightroomMaskArchive::indexLog(const QString &path, CancellationToken stop) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;
    QByteArray batch;
    bool fragmented = false;
    while (!file.atEnd()) {
        checkCancellation(stop);
        const auto block = file.read(32768);
        qsizetype at = 0;
        while (at + 7 <= block.size()) {
            const auto type = static_cast<unsigned char>(block[at + 6]);
            const auto length = little<std::uint16_t>(block, at + 4);
            const auto header = type >= 5 && type <= 8 ? 11 : 7;
            if (length == 0 || type == 0 || at + header + length > block.size())
                break;
            const auto fragment = QByteArrayView(block).sliced(at + header, length);
            at += header + length;
            const auto kind = type <= 4 ? type : type - 4;
            if (kind == 1) {
                batch.clear();
                fragmented = false;
                indexInline(fragment.toByteArray(), {}, stop);
            } else if (kind == 2) {
                batch = fragment.toByteArray();
                fragmented = true;
            } else if ((kind == 3 || kind == 4) && fragmented &&
                       batch.size() + length <= 16 * 1024 * 1024) {
                batch.append(fragment);
                if (kind == 4) {
                    indexInline(batch, {}, stop);
                    batch.clear();
                    fragmented = false;
                }
            } else {
                batch.clear();
                fragmented = false;
            }
        }
    }
}

BitmapMask LightroomMaskArchive::bitmap(const QString &digest, const QString &origin,
                                        const QString &area, CancellationToken stop) const {
    const auto key = digest.toLatin1().toLower();
    const auto found = masks_.find(key);
    if (found == masks_.end())
        return {};
    checkCancellation(stop);
    const auto &payload = found->second;
    auto encoded = payload.inlineValue;
    if (!payload.path.isEmpty()) {
        QFile file(payload.path);
        if (file.open(QIODevice::ReadOnly) && file.seek(payload.offset))
            encoded = file.read(payload.bytes);
    }
    if (encoded.size() != payload.bytes || !QByteArrayView(encoded).startsWith(envelope) ||
        !digestMatches(encoded, key))
        return {};
    const TiffDirectory directory(view(QByteArrayView(encoded).sliced(envelope.size())));
    return placeMask(directory, layout(directory), origin, area, stop);
}

LocalMask convertLightroomMask(const LightroomMaskGroup &group, const LightroomMaskArchive &archive,
                               std::size_t index, CancellationToken stop) {
    const auto get = [](const LightroomProperties &fields, std::string_view key) {
        const auto found = fields.find(key);
        return found == fields.end() ? QString{} : found->second;
    };
    const auto scalar = [&](const LightroomProperties &fields, std::string_view key,
                            double fallback) {
        const auto value = get(fields, key);
        if (value.isEmpty())
            return fallback;
        bool ok = false;
        const auto number = value.toDouble(&ok);
        require(ok && std::isfinite(number));
        return number;
    };
    const auto truth = [&](const LightroomProperties &fields, std::string_view key, bool fallback) {
        const auto value = get(fields, key).toLower();
        if (value.isEmpty())
            return fallback;
        require(value == "true" || value == "false");
        return value == "true";
    };
    const auto uuid = [](const QByteArray &value) {
        static const QUuid scope(QStringLiteral("{7eb62fd5-c969-4c12-8dba-f372eff0ef27}"));
        return QUuid::createUuidV5(scope, value).toString(QUuid::WithoutBraces).toStdString();
    };
    const auto seed = get(group.fields, "CorrectionID").toUtf8() + ':' + QByteArray::number(index);
    require(!group.components.empty());
    LocalMask result;
    result.id = uuid(seed);
    auto name = get(group.fields, "CorrectionName").simplified().left(76);
    for (auto &letter : name)
        if (letter.category() == QChar::Other_Control)
            letter = ' ';
    result.name = name.trimmed().toStdString();
    if (result.name.empty())
        result.name = "Lightroom mask " + std::to_string(index + 1);
    result.enabled = truth(group.fields, "CorrectionActive", true);
    const double amount = scalar(group.fields, "CorrectionAmount", 1);
    require(amount >= 0 && amount <= 2);
    result.setParameter(
        "exposure",
        std::clamp(5 * amount * scalar(group.fields, "LocalExposure2012", 0), -5.0, 5.0));
    for (const auto &[destination, source] : std::array{
             std::pair{"contrast", "LocalContrast2012"},
             std::pair{"highlights", "LocalHighlights2012"},
             std::pair{"shadows", "LocalShadows2012"}, std::pair{"whites", "LocalWhites2012"},
             std::pair{"blacks", "LocalBlacks2012"}, std::pair{"saturation", "LocalSaturation"}})
        result.setParameter(
            destination, std::clamp(100 * amount * scalar(group.fields, source, 0), -100.0, 100.0));

    // A zero-valued base makes the first operation explicit, so an inverted
    // first component still composes correctly with later operands.
    result.shape = MaskShape::bitmap;
    result.bitmap = BitmapMask(1, 1, {0});
    for (std::size_t i = 0; i < group.components.size(); ++i) {
        const auto &input = group.components[i];
        MaskComponent component;
        component.id = uuid(seed + '/' + QByteArray::number(i));
        component.enabled = truth(input, "MaskActive", true);
        component.inverted = truth(input, "MaskInverted", false);
        const double blend = scalar(input, "MaskBlendMode", 0);
        require(blend == 0 || blend == 1 || blend == 2);
        component.operation = blend == 1   ? MaskOperation::subtract
                              : blend == 2 ? MaskOperation::intersect
                                           : MaskOperation::add;
        require(scalar(input, "MaskValue", 1) == 1);
        const auto type = get(input, "What");
        if (type == "Mask/Image") {
            component.geometry.shape = MaskShape::bitmap;
            component.geometry.bitmap = archive.bitmap(
                get(input, "MaskDigest"), get(input, "Origin"), get(input, "WholeImageArea"), stop);
        } else if (type == "Mask/Gradient") {
            for (const auto name : {"ZeroX", "ZeroY", "FullX", "FullY"})
                require(!get(input, name).isEmpty());
            component.geometry.x1 = scalar(input, "FullX", 0);
            component.geometry.y1 = scalar(input, "FullY", 0);
            component.geometry.x2 = scalar(input, "ZeroX", 0);
            component.geometry.y2 = scalar(input, "ZeroY", 0);
        } else {
            throw std::runtime_error("This Lightroom mask type has no compatible geometry.");
        }
        component.geometry.validate();
        result.components.push_back(std::move(component));
    }
    if (result.components.size() == 1 &&
        result.components.front().operation == MaskOperation::add) {
        const auto &component = result.components.front();
        static_cast<MaskGeometry &>(result) = component.geometry;
        result.enabled = result.enabled && component.enabled;
        result.inverted = component.inverted;
        result.components.clear();
    }
    result.validate();
    return result;
}
} // namespace luma
