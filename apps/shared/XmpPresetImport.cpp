#include "apps/shared/PresetStore.h"
#include <QMap>
#include <QSet>
#include <QXmlStreamReader>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <tuple>

namespace luma {
namespace {
const QString crs = QStringLiteral("http://ns.adobe.com/camera-raw-settings/1.0/");
const QString rdf = QStringLiteral("http://www.w3.org/1999/02/22-rdf-syntax-ns#");
const QString xmlNamespace = QStringLiteral("http://www.w3.org/XML/1998/namespace");
void require(bool valid, const char *message) {
    if (!valid)
        throw std::runtime_error(message);
}
struct Attribute {
    QString space, name, value;
};
struct Node {
    QString space, name, text;
    std::vector<Attribute> attributes;
    std::vector<Node> children;
};
void safeToken(const QXmlStreamReader &reader) {
    require(reader.tokenType() != QXmlStreamReader::DTD &&
                reader.tokenType() != QXmlStreamReader::EntityReference,
            "XMP DTD and entity declarations are not supported.");
}
Node readNode(QXmlStreamReader &reader, int depth, int &count) {
    require(depth <= 32 && ++count <= 4096, "XMP structure exceeds the import limits.");
    require(reader.name().size() <= 128 && reader.namespaceUri().size() <= 512 &&
                reader.attributes().size() <= 512,
            "XMP property exceeds the import limits.");
    Node node{reader.namespaceUri().toString(), reader.name().toString(), {}, {}, {}};
    for (const auto &attribute : reader.attributes()) {
        require(attribute.name().size() <= 128, "XMP property name is too long.");
        node.attributes.push_back({attribute.namespaceUri().toString(), attribute.name().toString(),
                                   attribute.value().toString()});
    }
    while (!reader.atEnd()) {
        reader.readNext();
        safeToken(reader);
        if (reader.isStartElement())
            node.children.push_back(readNode(reader, depth + 1, count));
        else if (reader.isCharacters())
            node.text += reader.text();
        else if (reader.isEndElement())
            return node;
    }
    throw std::runtime_error("Invalid or incomplete XMP XML.");
}
QString scalar(const Node &node) {
    require(node.children.empty(), "Expected a scalar XMP property.");
    return node.text.trimmed();
}
QString localized(const Node &node) {
    if (node.children.empty())
        return scalar(node);
    require(node.children.size() == 1 && node.children[0].space == rdf &&
                node.children[0].name == QStringLiteral("Alt"),
            "Invalid localized XMP text.");
    QString first, english, preferred;
    QSet<QString> languages;
    for (const auto &entry : node.children[0].children) {
        require(entry.space == rdf && entry.name == QStringLiteral("li"),
                "Invalid localized XMP entry.");
        QString language;
        for (const auto &attribute : entry.attributes)
            if (attribute.space == xmlNamespace && attribute.name == QStringLiteral("lang"))
                language = attribute.value.toLower();
        require(!language.isEmpty() && !languages.contains(language),
                "Missing or duplicate XMP text language.");
        languages.insert(language);
        const auto value = scalar(entry);
        if (first.isEmpty())
            first = value;
        if (language == QStringLiteral("en"))
            english = value;
        if (language == QStringLiteral("x-default"))
            preferred = value;
    }
    return !preferred.isEmpty() ? preferred : !english.isEmpty() ? english : first;
}
double number(const QString &text) {
    bool valid = false;
    const auto value = text.toDouble(&valid);
    require(valid && std::isfinite(value), "Invalid numeric XMP setting.");
    return value;
}
PointCurve pointCurve(const Node &node) {
    require(node.children.size() == 1 && node.children[0].space == rdf &&
                node.children[0].name == QStringLiteral("Seq"),
            "XMP tone curve must be an RDF sequence.");
    PointCurve curve;
    curve.points.clear();
    for (const auto &entry : node.children[0].children) {
        require(entry.space == rdf && entry.name == QStringLiteral("li"),
                "Invalid XMP curve entry.");
        const auto pair = scalar(entry).split(',');
        require(pair.size() == 2, "XMP curve point must contain two coordinates.");
        const auto x = number(pair[0].trimmed()), y = number(pair[1].trimmed());
        require(x >= 0 && x <= 255 && y >= 0 && y <= 255 && std::floor(x) == x &&
                    std::floor(y) == y,
                "XMP curve coordinates must be integers within 0..255.");
        curve.points.push_back({x / 255, y / 255});
    }
    curve.validate();
    return curve;
}
} // namespace
PresetImport decodeXmpPreset(const QByteArray &bytes, const QString &fallbackName) {
    require(!bytes.isEmpty() && bytes.size() <= 1'048'576,
            "XMP preset is empty or exceeds the 1 MiB limit.");
    QXmlStreamReader reader(bytes);
    reader.setEntityExpansionLimit(1024);
    std::optional<Node> root;
    int count = 0;
    while (!reader.atEnd()) {
        reader.readNext();
        safeToken(reader);
        if (reader.isStartElement()) {
            require(!root, "XMP must have one document root.");
            root = readNode(reader, 1, count);
        }
    }
    require(!reader.hasError() && root.has_value(), "Invalid XMP XML.");
    const Node *rdfNode = &*root;
    if (root->space == QStringLiteral("adobe:ns:meta/") &&
        root->name == QStringLiteral("xmpmeta")) {
        require(root->children.size() == 1, "XMP must contain one RDF document.");
        rdfNode = &root->children[0];
    }
    require(rdfNode->space == rdf && rdfNode->name == QStringLiteral("RDF"),
            "XMP RDF document is missing.");
    QMap<QString, Node> fields;
    int descriptions = 0;
    for (const auto &description : rdfNode->children) {
        require(description.space == rdf && description.name == QStringLiteral("Description"),
                "Unsupported XMP RDF structure.");
        QMap<QString, Node> current;
        const auto insert = [&](const QString &name, Node node) {
            require(!current.contains(name), "Duplicate Camera Raw XMP property.");
            current.insert(name, std::move(node));
        };
        for (const auto &attribute : description.attributes)
            if (attribute.space == crs)
                insert(attribute.name, {crs, attribute.name, attribute.value, {}, {}});
        for (const auto &child : description.children)
            if (child.space == crs)
                insert(child.name, child);
        if (!current.isEmpty()) {
            require(++descriptions == 1, "Multiple Camera Raw descriptions are ambiguous.");
            fields = std::move(current);
        }
    }
    require(descriptions == 1, "No Camera Raw settings found in XMP.");
    PresetImport result{{fallbackName.trimmed().left(80), {}, {}, 0}, {}, true};
    QSet<QString> consumed;
    auto &preset = result.preset;
    const auto text = [&](const QString &key) {
        consumed.insert(key);
        return scalar(fields[key]);
    };
    for (const auto &key : {QStringLiteral("Name"), QStringLiteral("Group")}) {
        if (!fields.contains(key))
            continue;
        consumed.insert(key);
        const auto value = localized(fields[key]);
        if (key == QStringLiteral("Group"))
            preset.folder = value;
        else if (!value.isEmpty())
            preset.name = value;
    }
    const auto set = [&](const QString &key, const std::string &parameter) {
        if (!fields.contains(key))
            return;
        preset.settings.setParameter(parameter, number(text(key)));
        preset.parameters.push_back(parameter);
    };
    for (const auto &[modern, legacy, parameter] :
         {std::tuple{"Exposure2012", "Exposure", "exposure"},
          std::tuple{"Contrast2012", "Contrast", "contrast"}}) {
        const auto key = QString::fromLatin1(modern);
        const auto old = QString::fromLatin1(legacy);
        set(fields.contains(key) ? key : old, parameter);
        if (fields.contains(key))
            consumed.insert(old);
    }
    for (const auto &[key, parameter] :
         {std::pair{"Highlights2012", "highlights"},
          {"Shadows2012", "shadows"},
          {"Whites2012", "whites"},
          {"Blacks2012", "blacks"},
          {"Vibrance", "vibrance"},
          {"Saturation", "saturation"},
          {"ParametricShadows", "curve.shadows"},
          {"ParametricDarks", "curve.darks"},
          {"ParametricLights", "curve.lights"},
          {"ParametricHighlights", "curve.highlights"},
          {"Sharpness", "sharpen.amount"},
          {"SharpenRadius", "sharpen.radius"},
          {"SharpenDetail", "sharpen.detail"},
          {"SharpenEdgeMasking", "sharpen.masking"},
          {"LuminanceSmoothing", "noise.luminance"},
          {"LuminanceNoiseReductionDetail", "noise.luminanceDetail"},
          {"LuminanceNoiseReductionContrast", "noise.luminanceContrast"},
          {"ColorNoiseReduction", "noise.color"},
          {"ColorNoiseReductionDetail", "noise.colorDetail"},
          {"ColorNoiseReductionSmoothness", "noise.colorSmoothness"}})
        set(QString::fromLatin1(key), parameter);
    QString whiteBalance;
    if (fields.contains(QStringLiteral("WhiteBalance")))
        whiteBalance = scalar(fields[QStringLiteral("WhiteBalance")]);
    const bool asShot = whiteBalance.compare(QStringLiteral("As Shot"), Qt::CaseInsensitive) == 0;
    const bool custom = whiteBalance.compare(QStringLiteral("Custom"), Qt::CaseInsensitive) == 0;
    if (whiteBalance.isEmpty() || asShot || custom) {
        set(QStringLiteral("Temperature"), "temperature");
        set(QStringLiteral("Tint"), "tint");
        if (asShot || custom) {
            consumed.insert(QStringLiteral("WhiteBalance"));
            preset.parameters.emplace_back("whiteBalance");
            preset.settings.customWhiteBalance = custom;
        }
    }
    for (std::size_t i = 0; i < hslBands.size(); ++i) {
        const auto band = QString::fromUtf8(hslBands[i]);
        auto suffix = band;
        suffix[0] = suffix[0].toUpper();
        for (const auto &[prefix, parameter] : {std::pair{"HueAdjustment", "hue"},
                                                {"SaturationAdjustment", "saturation"},
                                                {"LuminanceAdjustment", "luminance"}})
            set(QString::fromLatin1(prefix) + suffix,
                "hsl." + std::string(hslBands[i]) + "." + parameter);
    }
    for (std::size_t i = 0; i < curveChannels.size(); ++i) {
        const auto suffix = i == 0 ? QString()
                                   : QString::fromLatin1(i == 1   ? "Red"
                                                         : i == 2 ? "Green"
                                                                  : "Blue");
        const auto modern = QStringLiteral("ToneCurvePV2012") + suffix;
        const auto legacy = QStringLiteral("ToneCurve") + suffix;
        const auto key = fields.contains(modern) ? modern : legacy;
        if (!fields.contains(key))
            continue;
        preset.settings.curves[i] = pointCurve(fields[key]);
        preset.parameters.push_back("points." + std::string(curveChannels[i]));
        consumed.insert(key);
        if (fields.contains(modern))
            consumed.insert(legacy);
    }
    const auto curveName = fields.contains(QStringLiteral("ToneCurveName2012"))
                               ? QStringLiteral("ToneCurveName2012")
                               : QStringLiteral("ToneCurveName");
    if (fields.contains(curveName)) {
        const auto value = scalar(fields[curveName]);
        if (consumed.contains(QStringLiteral("ToneCurvePV2012")) ||
            consumed.contains(QStringLiteral("ToneCurve")) || value.isEmpty())
            consumed.insert(curveName);
        else if (value == QStringLiteral("Linear")) {
            preset.parameters.emplace_back("points.rgb");
            consumed.insert(curveName);
        }
    }
    static const QSet<QString> metadata{"Version",
                                        "ProcessVersion",
                                        "UUID",
                                        "PresetType",
                                        "Cluster",
                                        "Copyright",
                                        "ContactInfo",
                                        "HasSettings",
                                        "AlreadyApplied",
                                        "Description",
                                        "ShortName",
                                        "SortName",
                                        "SupportsAmount",
                                        "SupportsColor",
                                        "SupportsMonochrome",
                                        "SupportsHighDynamicRange",
                                        "SupportsNormalDynamicRange",
                                        "SupportsSceneReferred",
                                        "SupportsOutputReferred"};
    for (auto it = fields.cbegin(); it != fields.cend(); ++it) {
        if (consumed.contains(it.key()) || metadata.contains(it.key()))
            continue;
        // These properties are inert when empty/false; no renderer data is discarded.
        if ((it.key() == QStringLiteral("CameraModelRestriction") &&
             scalar(it.value()).isEmpty()) ||
            (it.key() == QStringLiteral("ConvertToGrayscale") &&
             scalar(it.value()).compare(QStringLiteral("False"), Qt::CaseInsensitive) == 0))
            continue;
        result.unsupported.append(it.key());
    }
    require(!preset.parameters.empty(), "XMP preset has no settings supported by Apertone.");
    preset.groups = parameterSelectionGroups(preset.parameters);
    preset.validate();
    return result;
}
} // namespace luma
