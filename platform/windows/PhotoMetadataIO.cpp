#include "platform/macos/PhotoMetadataIO.h"
#include "platform/windows/MetadataPacket.h"
#include <QDateTime>
#include <QDomDocument>
#include <QImageReader>
#include <QTimeZone>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <shobjidl.h>
#include <propvarutil.h>

namespace luma::macos {
namespace {
const QString rdf = QStringLiteral("http://www.w3.org/1999/02/22-rdf-syntax-ns#");
const QString dc = QStringLiteral("http://purl.org/dc/elements/1.1/");
const QString xmp = QStringLiteral("http://ns.adobe.com/xap/1.0/");
const QString iptc = QStringLiteral("http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/");
const QString exif = QStringLiteral("http://ns.adobe.com/exif/1.0/");
const QString exifEX = QStringLiteral("http://cipa.jp/exif/1.0/");
const QString tiff = QStringLiteral("http://ns.adobe.com/tiff/1.0/");
const QString xml = QStringLiteral("http://www.w3.org/XML/1998/namespace");
const QByteArray emptyPacket =
    "<x:xmpmeta xmlns:x='adobe:ns:meta/'>"
    "<rdf:RDF xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#'>"
    "<rdf:Description rdf:about=''/>"
    "</rdf:RDF></x:xmpmeta>";

QDomDocument parse(const QByteArray &bytes) {
    QDomDocument result;
    // Inspect Unicode text as well as the parsed tree so XML entity declarations
    // are refused before Qt expands them, including packets saved as UTF-16.
    QByteArray ascii = bytes;
    ascii.replace(QByteArray(1, '\0'), QByteArray());
    if (ascii.contains("<!DOCTYPE") || ascii.contains("<!ENTITY") ||
        !result.setContent(bytes, QDomDocument::ParseOption::UseNamespaceProcessing) ||
        !result.doctype().name().isEmpty())
        throw std::runtime_error("Invalid or unsupported XMP XML.");
    return result;
}

QList<QDomElement> descriptions(QDomDocument document) {
    const auto groups = document.elementsByTagNameNS(rdf, "RDF");
    if (groups.size() != 1)
        throw std::runtime_error("XMP must contain a single RDF element.");
    QList<QDomElement> entries;
    for (auto e = groups.at(0).firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
        if (e.namespaceURI() != rdf || e.localName() != "Description" ||
            e.hasAttributeNS(rdf, "ID") || e.hasAttributeNS(rdf, "nodeID") ||
            (!entries.empty() && entries.front().attributeNS(rdf, "about") !=
                                     e.attributeNS(rdf, "about")))
            throw std::runtime_error("XMP has multiple or unsupported RDF subjects.");
        entries.append(e);
    }
    if (entries.empty())
        throw std::runtime_error("XMP has no RDF description.");
    return entries;
}

QStringList values(const QList<QDomElement> &entries, const QString &space, const QString &key) {
    for (const auto &e : entries) {
        if (e.hasAttributeNS(space, key))
            return {e.attributeNS(space, key)};
        for (auto child = e.firstChildElement(); !child.isNull();
             child = child.nextSiblingElement()) {
            if (child.namespaceURI() != space || child.localName() != key)
                continue;
            auto items = child.elementsByTagNameNS(rdf, "li");
            if (items.isEmpty())
                return {child.text()};
            QStringList result;
            for (qsizetype i = 0; i < items.size(); ++i) {
                const auto item = items.at(i).toElement();
                if (item.attributeNS(xml, "lang") == "x-default")
                    result.prepend(item.text());
                else
                    result.append(item.text());
            }
            return result;
        }
    }
    return {};
}

void overlay(DescriptiveMetadata &metadata, const QByteArray &packet) {
    const auto entries = descriptions(parse(packet));
    const auto scalar = [&](std::string &value, const QString &space, const QString &key) {
        const auto result = values(entries, space, key);
        if (!result.empty())
            value = result.front().toStdString();
    };
    const auto list = [&](std::vector<std::string> &value, const QString &key) {
        const auto result = values(entries, dc, key);
        if (!result.empty()) {
            value.clear();
            for (const auto &text : result)
                if (!text.isEmpty())
                    value.push_back(text.toStdString());
        }
    };
    scalar(metadata.title, dc, "title");
    scalar(metadata.caption, dc, "description");
    scalar(metadata.copyright, dc, "rights");
    scalar(metadata.location, iptc, "Location");
    scalar(metadata.label, xmp, "Label");
    list(metadata.creators, "creator");
    list(metadata.keywords, "subject");
    const auto raw = values(entries, xmp, "Rating");
    if (!raw.empty()) {
        bool valid = false;
        const double rating = raw.front().toDouble(&valid);
        if (!valid || !(rating >= -1 && rating <= 5) || rating != std::trunc(rating))
            throw std::runtime_error("Invalid XMP rating.");
        metadata.rating = rating < 0 ? 0 : static_cast<int>(rating);
        metadata.flag = rating < 0 ? -1 : 0;
    }
}

void replace(QDomDocument &doc, QList<QDomElement> &entries, const QString &space,
             const QString &qualified, const QStringList &text, const QString &container = {}) {
    const auto key = qualified.section(':', 1);
    auto parent = entries.front();
    for (auto &e : entries) {
        if (e.hasAttributeNS(space, key)) {
            e.removeAttributeNS(space, key);
            parent = e;
        }
        for (auto child = e.firstChildElement(); !child.isNull();) {
            const auto next = child.nextSiblingElement();
            if (child.namespaceURI() == space && child.localName() == key) {
                e.removeChild(child);
                parent = e;
            }
            child = next;
        }
    }
    auto property = doc.createElementNS(space, qualified);
    if (container.isEmpty()) {
        property.appendChild(doc.createTextNode(text.value(0)));
    } else {
        auto list = doc.createElementNS(rdf, "rdf:" + container);
        for (const auto &value : text) {
            auto element = doc.createElementNS(rdf, "rdf:li");
            if (container == "Alt")
                element.setAttributeNS(xml, "xml:lang", "x-default");
            element.appendChild(doc.createTextNode(value));
            list.appendChild(element);
        }
        property.appendChild(list);
    }
    parent.appendChild(property);
}
} // namespace

DescriptiveMetadata readDescriptiveMetadata(const std::filesystem::path &source,
                                            std::span<const std::uint8_t> sidecar) {
    DescriptiveMetadata metadata;
    PhotoMetadata photo;
    windows::readRasterProperties(source, photo, &metadata);
    auto packet = windows::readEmbeddedXmp(source);
    QImageReader reader(QString::fromStdWString(source.native()));
    if (packet.isEmpty() && reader.canRead())
        packet = reader.text("XML:com.adobe.xmp").toUtf8();
    if (!packet.isEmpty())
        overlay(metadata, packet);
    if (!sidecar.empty())
        overlay(metadata, QByteArrayView(reinterpret_cast<const char *>(sidecar.data()),
                                          static_cast<qsizetype>(sidecar.size())).toByteArray());
    metadata.validate();
    return metadata;
}

std::vector<std::uint8_t> writeDescriptiveXMP(const DescriptiveMetadata &metadata,
                                              std::span<const std::uint8_t> existing) {
    metadata.validate();
    auto document = parse(existing.empty() ? emptyPacket
                                           : QByteArray(reinterpret_cast<const char *>(existing.data()),
                                                        static_cast<qsizetype>(existing.size())));
    auto entries = descriptions(document);
    const auto text = [](const std::string &value) { return QString::fromStdString(value); };
    const auto list = [&](const std::vector<std::string> &items) {
        QStringList result;
        for (const auto &item : items)
            result.append(text(item));
        return result;
    };
    replace(document, entries, dc, "dc:title", {text(metadata.title)}, "Alt");
    replace(document, entries, dc, "dc:description", {text(metadata.caption)}, "Alt");
    replace(document, entries, dc, "dc:rights", {text(metadata.copyright)}, "Alt");
    replace(document, entries, dc, "dc:creator", list(metadata.creators), "Seq");
    replace(document, entries, dc, "dc:subject", list(metadata.keywords), "Bag");
    replace(document, entries, iptc, "Iptc4xmpCore:Location", {text(metadata.location)});
    replace(document, entries, xmp, "xmp:Rating", {QString::number(metadata.xmpRating())});
    replace(document, entries, xmp, "xmp:Label", {text(metadata.label)});
    const auto packet = document.toByteArray(0);
    (void)parse(packet);
    return {packet.begin(), packet.end()};
}
} // namespace luma::macos

namespace luma::windows {
namespace {
class ShellProperties {
  public:
    explicit ShellProperties(const std::filesystem::path &source)
        : initialized_(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {
        if (SUCCEEDED(initialized_) || initialized_ == RPC_E_CHANGED_MODE)
            (void)SHGetPropertyStoreFromParsingName(source.c_str(), nullptr, GPS_DEFAULT,
                                                      IID_IPropertyStore,
                                                      reinterpret_cast<void **>(&store_));
    }
    ~ShellProperties() {
        if (store_)
            store_->Release();
        if (SUCCEEDED(initialized_))
            CoUninitialize();
    }
    ShellProperties(const ShellProperties &) = delete;
    ShellProperties &operator=(const ShellProperties &) = delete;

    template <typename Read> void get(const wchar_t *name, Read read) const {
        PROPERTYKEY key{};
        if (!store_ || FAILED(PSGetPropertyKeyFromName(name, &key)))
            return;
        PROPVARIANT value{};
        const bool available = SUCCEEDED(store_->GetValue(key, &value)) && value.vt != VT_EMPTY;
        if (available) {
            try {
                read(value);
            } catch (...) {
                PropVariantClear(&value);
                throw;
            }
        }
        PropVariantClear(&value);
    }
    std::string text(const wchar_t *name) const {
        std::string result;
        get(name, [&](const auto &value) {
            if (value.vt == VT_LPWSTR && value.pwszVal)
                result = QString::fromWCharArray(value.pwszVal).trimmed().toStdString();
        });
        return result;
    }
    std::optional<double> number(const wchar_t *name, double minimum, double maximum) const {
        std::optional<double> result;
        get(name, [&](const auto &value) {
            double n = 0;
            if (SUCCEEDED(PropVariantToDouble(value, &n)) && std::isfinite(n) &&
                n >= minimum && n <= maximum)
                result = n;
        });
        return result;
    }

  private:
    HRESULT initialized_;
    IPropertyStore *store_ = nullptr;
};
} // namespace

void readRasterProperties(const std::filesystem::path &path, PhotoMetadata &photo,
                          DescriptiveMetadata *description) {
    const ShellProperties properties(path);
    if (photo.camera.empty()) {
        auto make = properties.text(L"System.Photo.CameraManufacturer");
        const auto model = properties.text(L"System.Photo.CameraModel");
        photo.camera = model.starts_with(make) || make.empty() ? model : model.empty() ? make
                                                                                       : make + " " + model;
    }
    if (photo.lens.empty())
        photo.lens = properties.text(L"System.Photo.LensModel");
    if (!photo.iso)
        photo.iso = properties.number(L"System.Photo.ISOSpeed", 1, 10'000'000);
    if (!photo.aperture)
        photo.aperture = properties.number(L"System.Photo.FNumber", .01, 1000);
    if (!photo.focalLength)
        photo.focalLength = properties.number(L"System.Photo.FocalLength", .01, 100'000);
    if (!photo.shutterSeconds)
        photo.shutterSeconds = properties.number(L"System.Photo.ExposureTime", 1e-8, 86'400);
    if (!photo.latitude || !photo.longitude) {
        photo.latitude = properties.number(L"System.GPS.LatitudeDecimal", -90, 90);
        photo.longitude = properties.number(L"System.GPS.LongitudeDecimal", -180, 180);
    }
    if (!photo.captureTimestamp)
        properties.get(L"System.Photo.DateTaken", [&](const auto &value) {
            if (value.vt != VT_FILETIME)
                return;
            constexpr std::uint64_t unixEpoch = 116444736000000000ULL;
            const auto ticks = (std::uint64_t(value.filetime.dwHighDateTime) << 32) +
                               value.filetime.dwLowDateTime;
            if (ticks >= unixEpoch)
                photo.captureTimestamp = static_cast<std::int64_t>((ticks - unixEpoch) / 10'000'000);
        });
    if (!description)
        return;
    description->title = properties.text(L"System.Title");
    description->caption = properties.text(L"System.Comment");
    description->copyright = properties.text(L"System.Copyright");
    const auto list = [&](const wchar_t *key, std::vector<std::string> &items) {
        properties.get(key, [&](const auto &value) {
            if (value.vt != (VT_VECTOR | VT_LPWSTR) || !value.calpwstr.pElems)
                return;
            for (ULONG i = 0; i < std::min(value.calpwstr.cElems, 100UL); ++i) {
                const auto word = QString::fromWCharArray(value.calpwstr.pElems[i]).trimmed().toStdString();
                if (!word.empty() && std::find(items.begin(), items.end(), word) == items.end())
                    items.push_back(word);
            }
        });
    };
    list(L"System.Author", description->creators);
    list(L"System.Keywords", description->keywords);
}

QByteArray exportXmp(const std::filesystem::path &source, const PhotoMetadata &photo,
                     MetadataPolicy policy, const std::optional<MetadataOverlay> &overlaid) {
    if (policy == MetadataPolicy::none)
        return {};
    auto metadata = macos::readDescriptiveMetadata(source, {});
    if (overlaid) {
        if (overlaid->includeText)
            metadata = overlaid->values;
        else {
            metadata.rating = overlaid->values.rating;
            metadata.flag = overlaid->values.flag;
            metadata.label = overlaid->values.label;
        }
    }
    if (policy == MetadataPolicy::removeLocation)
        metadata.location.clear();
    const auto text = [](const std::string &s) { return QString::fromStdString(s); };
    if (policy == MetadataPolicy::copyrightOnly) {
        auto doc = macos::parse(macos::emptyPacket);
        auto entries = macos::descriptions(doc);
        if (!metadata.copyright.empty())
            macos::replace(doc, entries, macos::dc, "dc:rights", {text(metadata.copyright)}, "Alt");
        QStringList authors;
        for (const auto &name : metadata.creators)
            authors.push_back(text(name));
        if (!authors.isEmpty())
            macos::replace(doc, entries, macos::dc, "dc:creator", authors, "Seq");
        return metadata.copyright.empty() && authors.isEmpty() ? QByteArray{} : doc.toByteArray(0);
    }
    const auto bytes = macos::writeDescriptiveXMP(metadata, {});
    auto doc = macos::parse(QByteArray(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
    auto entries = macos::descriptions(doc);
    const auto set = [&](const QString &ns, const char *key, const QString &value) {
        if (!value.isEmpty())
            macos::replace(doc, entries, ns, QString::fromLatin1(key), {value});
    };
    const auto ratio = [](const std::optional<double> &value) {
        return value ? QString::number(qRound64(*value * 1'000'000)) + "/1000000" : QString{};
    };
    set(macos::tiff, "tiff:Model", text(photo.camera));
    set(macos::exifEX, "exifEX:LensModel", text(photo.lens));
    set(macos::exif, "exif:FNumber", ratio(photo.aperture));
    set(macos::exif, "exif:FocalLength", ratio(photo.focalLength));
    set(macos::exif, "exif:ExposureTime", ratio(photo.shutterSeconds));
    if (photo.iso)
        macos::replace(doc, entries, macos::exif, "exif:ISOSpeedRatings",
                       {QString::number(qRound64(*photo.iso))}, "Seq");
    if (photo.captureTimestamp)
        set(macos::exif, "exif:DateTimeOriginal",
            QDateTime::fromSecsSinceEpoch(*photo.captureTimestamp, QTimeZone::UTC).toString(Qt::ISODate));
    if (policy == MetadataPolicy::all && photo.latitude && photo.longitude) {
        const auto format = [](double value, char positive, char negative) {
            const auto magnitude = std::abs(value);
            const auto degrees = static_cast<int>(magnitude);
            return QString::number(degrees) + ',' + QString::number((magnitude - degrees) * 60, 'f', 9) +
                   QLatin1Char(value < 0 ? negative : positive);
        };
        set(macos::exif, "exif:GPSLatitude", format(*photo.latitude, 'N', 'S'));
        set(macos::exif, "exif:GPSLongitude", format(*photo.longitude, 'E', 'W'));
    }
    return doc.toByteArray(0);
}
} // namespace luma::windows
