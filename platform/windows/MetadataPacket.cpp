#include "platform/windows/MetadataPacket.h"
#include "core/image/Bayer.h"
#include <QFile>
#include <QtEndian>
#include <algorithm>
#include <limits>

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
        require(jpegXmp.size() + packet.size() + 2 <= std::numeric_limits<quint16>::max(),
                "Export metadata does not fit a JPEG XMP segment.");
        data = QByteArray("\xff\xe1\0\0", 4);
        qToBigEndian<quint16>(static_cast<quint16>(jpegXmp.size() + packet.size() + 2),
                              data.data() + 2);
        data.append(jpegXmp).append(packet);
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
} // namespace luma::windows
