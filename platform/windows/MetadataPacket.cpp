#include "platform/windows/MetadataPacket.h"
#include "core/image/Bayer.h"
#include <QFile>
#include <QtEndian>
#include <algorithm>
#include <limits>
#include <ostream>

namespace luma::windows {
namespace {
const QByteArray jpegXmp("http://ns.adobe.com/xap/1.0/\0", 29);
const QByteArray pngXmp("XML:com.adobe.xmp\0\0\0\0\0", 22);
constexpr qint64 maxPacket = 16 * 1024 * 1024;

QString filename(const std::filesystem::path &path) {
    return QString::fromStdWString(path.native());
}
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
QByteArray read(QFile &file, qint64 count) {
    const auto bytes = file.read(count);
    require(bytes.size() == count, "Image metadata container is truncated.");
    return bytes;
}
void write(QFile &file, QByteArrayView bytes) {
    require(file.write(bytes.data(), bytes.size()) == bytes.size(), "Cannot write image metadata.");
}

struct TiffDirectory {
    bool littleEndian = true;
    QByteArray entries;
    QByteArray next;
    quint16 u16(const char *p) const {
        return littleEndian ? qFromLittleEndian<quint16>(p) : qFromBigEndian<quint16>(p);
    }
    quint32 u32(const char *p) const {
        return littleEndian ? qFromLittleEndian<quint32>(p) : qFromBigEndian<quint32>(p);
    }
    template <class UInt> void put(UInt number, char *p) const {
        if (littleEndian)
            qToLittleEndian(number, p);
        else
            qToBigEndian(number, p);
    }
    qsizetype xmpEntry() const {
        for (qsizetype i = 0; i < entries.size(); i += 12)
            if (u16(entries.constData() + i) == 700)
                return i;
        return -1;
    }
};
TiffDirectory tiffDirectory(QFile &file, const QByteArray &head) {
    TiffDirectory result;
    result.littleEndian = head.startsWith("II");
    if (result.u16(head.constData() + 2) != 42)
        return result;
    const auto offset = result.u32(head.constData() + 4);
    require(offset >= 8 && file.seek(offset), "Invalid TIFF metadata directory.");
    const auto count = result.u16(read(file, 2).constData());
    require(count <= 4096, "TIFF metadata directory exceeds the supported size.");
    result.entries = read(file, count * 12);
    result.next = read(file, 4);
    return result;
}
QByteArray jpegPacket(QFile &file) {
    constexpr quint8 startOfScan = 0xda, end = 0xd9, xmp = 0xe1;
    require(file.seek(2), "Cannot read JPEG metadata.");
    for (int i = 0; i < 4096; ++i) {
        if (file.atEnd())
            return {};
        require(quint8(read(file, 1)[0]) == 0xff, "Invalid JPEG metadata marker.");
        quint8 marker;
        do {
            marker = quint8(read(file, 1)[0]);
        } while (marker == 0xff);
        if (marker == startOfScan || marker == end)
            return {};
        const auto size = qFromBigEndian<quint16>(read(file, 2).constData());
        require(size >= 2, "Invalid JPEG metadata segment.");
        if (marker == xmp) {
            const auto segment = read(file, size - 2);
            if (segment.startsWith(jpegXmp))
                return segment.sliced(jpegXmp.size());
        } else {
            require(file.pos() + size - 2 <= file.size() && file.seek(file.pos() + size - 2),
                    "Invalid JPEG metadata segment size.");
        }
    }
    throw std::runtime_error("Too many JPEG metadata segments.");
}
QByteArray jpegXmpSegment(const QByteArray &packet) {
    require(jpegXmp.size() + packet.size() + 2 <= std::numeric_limits<quint16>::max(),
            "Export metadata does not fit a JPEG XMP segment.");
    QByteArray segment("\xff\xe1\0\0", 4);
    qToBigEndian<quint16>(static_cast<quint16>(jpegXmp.size() + packet.size() + 2),
                          segment.data() + 2);
    return segment.append(jpegXmp).append(packet);
}

struct JpegSegment {
    qsizetype start = 0;
    qsizetype end = 0;
};

JpegSegment mpfPrimarySize(QByteArrayView jpeg, JpegSegment mpf) {
    // MP entries hold the image size and an offset relative to the TIFF header.
    constexpr quint16 mpEntries = 0xb002, undefined = 7;
    constexpr qsizetype entrySize = 16;
    const auto tiffOffset = mpf.start + 8;
    const auto tiff = jpeg.sliced(tiffOffset, mpf.end - tiffOffset);
    TiffDirectory endian;
    endian.littleEndian = tiff.startsWith("II");
    require(tiff.size() >= 8 && (endian.littleEndian || tiff.startsWith("MM")) &&
                endian.u16(tiff.data() + 2) == 42,
            "Invalid Ultra HDR image index.");
    const qsizetype directoryOffset = endian.u32(tiff.data() + 4);
    require(directoryOffset >= 8 && directoryOffset <= tiff.size() - 2,
            "Invalid Ultra HDR image directory.");
    const auto count = endian.u16(tiff.data() + directoryOffset);
    require(count * 12 <= tiff.size() - directoryOffset - 2,
            "Invalid Ultra HDR image directory size.");
    for (int index = 0; index < count; ++index) {
        const auto *entry = tiff.data() + directoryOffset + 2 + 12 * index;
        if (endian.u16(entry) != mpEntries)
            continue;
        const qsizetype offset = endian.u32(entry + 8);
        require(endian.u16(entry + 2) == undefined && endian.u32(entry + 4) == 2 * entrySize &&
                    offset <= tiff.size() - 2 * entrySize,
                "Invalid Ultra HDR image entries.");
        const auto *primary = tiff.data() + offset;
        const auto *secondary = primary + entrySize;
        const qsizetype gainMapOffset = tiffOffset + endian.u32(secondary + 8);
        require(endian.u32(primary + 8) == 0 && gainMapOffset == endian.u32(primary + 4) &&
                    gainMapOffset >= mpf.end && gainMapOffset <= jpeg.size() - 2 &&
                    endian.u32(secondary + 4) == jpeg.size() - gainMapOffset &&
                    jpeg.sliced(gainMapOffset, 2) == QByteArrayView("\xff\xd8", 2),
                "Invalid Ultra HDR gain map index.");
        const auto size = tiffOffset + offset + 4;
        return {size, size + 4};
    }
    throw std::runtime_error("Missing Ultra HDR image entries.");
}

QByteArray pngTextChunk(const QByteArray &packet) {
    const QByteArray data = "iTXt" + pngXmp + packet;
    quint32 crc = 0xffffffffU;
    for (const auto c : data) {
        crc ^= quint8(c);
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    QByteArray result(4, Qt::Uninitialized);
    qToBigEndian<quint32>(static_cast<quint32>(data.size() - 4), result.data());
    result.append(data);
    const auto size = result.size();
    result.resize(size + 4);
    qToBigEndian<quint32>(~crc, result.data() + size);
    return result;
}
void appendTiffPacket(const std::filesystem::path &path, const QByteArray &packet) {
    QFile file(filename(path));
    require(file.open(QIODevice::ReadWrite), "Cannot open TIFF staging file for metadata.");
    const auto head = read(file, 8);
    require(head.startsWith("II") || head.startsWith("MM"), "Invalid TIFF header.");
    auto directory = tiffDirectory(file, head);
    require(!directory.entries.isEmpty(), "Unsupported TIFF metadata directory.");
    auto entry = directory.xmpEntry();
    if (entry < 0) {
        entry = 0;
        while (entry < directory.entries.size() &&
               directory.u16(directory.entries.constData() + entry) < 700)
            entry += 12;
        directory.entries.insert(entry, QByteArray(12, '\0'));
    }
    const auto position = (file.size() + 1) & ~qint64{1};
    const auto newDirectory = (position + packet.size() + 1) & ~qint64{1};
    require(newDirectory + directory.entries.size() + 6 <= std::numeric_limits<quint32>::max(),
            "TIFF metadata would exceed classic TIFF bounds.");
    auto *p = directory.entries.data() + entry;
    directory.put<quint16>(700, p);
    directory.put<quint16>(1, p + 2);
    directory.put<quint32>(static_cast<quint32>(packet.size()), p + 4);
    directory.put<quint32>(static_cast<quint32>(position), p + 8);
    require(file.seek(position), "Cannot position TIFF metadata packet.");
    write(file, packet);
    require(file.seek(newDirectory), "Cannot position TIFF metadata directory.");
    QByteArray count(2, Qt::Uninitialized), offset(4, Qt::Uninitialized);
    directory.put<quint16>(static_cast<quint16>(directory.entries.size() / 12), count.data());
    directory.put<quint32>(static_cast<quint32>(newDirectory), offset.data());
    write(file, count);
    write(file, directory.entries);
    write(file, directory.next);
    require(file.seek(4), "Cannot update TIFF directory location.");
    write(file, offset);
    require(file.flush(), "Cannot flush TIFF metadata.");
}
} // namespace

QByteArray readEmbeddedXmp(const std::filesystem::path &path) {
    QFile file(filename(path));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const auto head = file.read(8);
    if (head.startsWith(QByteArrayView("\xff\xd8", 2)))
        return jpegPacket(file);
    if (head.size() == 8 && (head.startsWith("II") || head.startsWith("MM"))) {
        const auto directory = tiffDirectory(file, head);
        const auto index = directory.xmpEntry();
        if (index >= 0) {
            const auto *entry = directory.entries.constData() + index;
            const auto size = directory.u32(entry + 4);
            if (size > 4 && size <= maxPacket && file.seek(directory.u32(entry + 8)))
                return read(file, size);
        }
    }
    return {};
}

void embedXmp(const std::filesystem::path &encoded, const std::filesystem::path &staging,
              ExportFormat format, const QByteArray &packet, CancellationToken stop) {
    if (packet.isEmpty())
        return;
    require(packet.size() <= maxPacket, "Export metadata exceeds its size limit.");
    checkCancellation(stop);
    if (format == ExportFormat::tiff8 || format == ExportFormat::tiff16) {
        appendTiffPacket(staging, packet);
        return;
    }
    QByteArray data;
    qint64 prefix = 0;
    QFile input(filename(encoded));
    require(input.open(QIODevice::ReadOnly), "Cannot open image staging file for metadata.");
    if (format == ExportFormat::jpeg) {
        const auto head = read(input, 6);
        require(head.startsWith(QByteArrayView("\xff\xd8", 2)), "Invalid JPEG staging header.");
        prefix = head.sliced(2, 2) == QByteArrayView("\xff\xe0", 2)
                     ? 4 + qFromBigEndian<quint16>(head.constData() + 4)
                     : 2;
        data = jpegXmpSegment(packet);
    } else if (format == ExportFormat::png) {
        const auto head = read(input, 33);
        require(head.startsWith(QByteArrayView("\x89PNG\r\n\x1a\n", 8)) &&
                    head.sliced(12, 4) == "IHDR",
                "Invalid PNG staging header.");
        prefix = 33;
        data = pngTextChunk(packet);
    } else {
        throw std::runtime_error("This output format does not support an XMP packet.");
    }
    require(input.seek(0), "Cannot seek in image staging file.");
    QFile output(filename(staging));
    require(output.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "Cannot stage image metadata.");
    write(output, read(input, prefix));
    write(output, data);
    while (!input.atEnd()) {
        checkCancellation(stop);
        const auto block = input.read(1024 * 1024);
        require(!block.isEmpty(), "Cannot read image data while adding metadata.");
        write(output, block);
    }
    require(output.flush(), "Cannot flush image metadata.");
}

void writeUltraHdrWithXmp(std::ostream &output, QByteArrayView jpeg, const QByteArray &packet) {
    constexpr QByteArrayView mpfSignature("MPF\0", 4);
    require(jpeg.startsWith(QByteArrayView("\xff\xd8", 2)), "Invalid Ultra HDR JPEG header.");
    JpegSegment xmp, mpf;
    for (qsizetype pos = 2; pos + 4 <= jpeg.size();) {
        require(quint8(jpeg[pos]) == 0xff, "Invalid Ultra HDR JPEG marker.");
        const auto marker = quint8(jpeg[pos + 1]);
        if (marker == 0xda)
            break;
        const auto size = qFromBigEndian<quint16>(jpeg.data() + pos + 2);
        require(size >= 2 && pos + 2 + size <= jpeg.size(), "Invalid Ultra HDR JPEG segment.");
        const auto payload = jpeg.sliced(pos + 4, size - 2);
        if (marker == 0xe1 && payload.startsWith(jpegXmp)) {
            require(xmp.start == 0, "Duplicate Ultra HDR XMP packet.");
            xmp = {pos, pos + size + 2};
        } else if (marker == 0xe2 && payload.startsWith(mpfSignature)) {
            require(mpf.start == 0, "Duplicate Ultra HDR image index.");
            mpf = {pos, pos + size + 2};
        }
        pos += 2 + size;
    }
    require(xmp.start > 0 && xmp.end <= mpf.start,
            "Missing or misplaced Ultra HDR XMP or image index.");
    const auto offset = xmp.start + 4 + jpegXmp.size();
    const auto replacement = jpegXmpSegment(
        mergeUltraHdrXmp(jpeg.sliced(offset, xmp.end - offset).toByteArray(), packet));
    const auto sizeField = mpfPrimarySize(jpeg, mpf);
    const bool littleEndian = jpeg.sliced(mpf.start + 8, 2) == "II";
    TiffDirectory endian;
    endian.littleEndian = littleEndian;
    const auto newSize = qsizetype(endian.u32(jpeg.data() + sizeField.start)) + replacement.size() -
                         (xmp.end - xmp.start);
    require(newSize >= 0 && newSize <= std::numeric_limits<quint32>::max(),
            "Ultra HDR image exceeds its index size limit.");
    char size[4];
    endian.put(static_cast<quint32>(newSize), size);

    // Only the primary JPEG size changes. The MPF header and the gain map both
    // shift by the same amount, keeping their relative offset and size intact.
    const auto writeBytes = [&](QByteArrayView bytes) { output.write(bytes.data(), bytes.size()); };
    writeBytes(jpeg.first(xmp.start));
    writeBytes(replacement);
    writeBytes(jpeg.sliced(xmp.end, sizeField.start - xmp.end));
    writeBytes(QByteArrayView(size, sizeof size));
    writeBytes(jpeg.sliced(sizeField.end));
}
} // namespace luma::windows
