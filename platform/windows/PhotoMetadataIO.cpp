#include "platform/macos/PhotoMetadataIO.h"
#include <QDomDocument>
#include <QImageReader>
#include <cmath>
#include <stdexcept>

namespace luma::macos {
namespace {
const QString rdf = QStringLiteral("http://www.w3.org/1999/02/22-rdf-syntax-ns#");
const QString dc = QStringLiteral("http://purl.org/dc/elements/1.1/");
const QString xmp = QStringLiteral("http://ns.adobe.com/xap/1.0/");
const QString iptc = QStringLiteral("http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/");
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
    QImageReader reader(QString::fromStdWString(source.native()));
    if (reader.canRead()) {
        const auto packet = reader.text("XML:com.adobe.xmp").toUtf8();
        if (!packet.isEmpty())
            overlay(metadata, packet);
    }
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
