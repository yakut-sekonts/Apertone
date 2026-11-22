#include "apps/shared/LightroomDevelop.h"
#include <QDateTime>
#include <QXmlStreamWriter>
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <stdexcept>

namespace luma {
namespace {
bool letter(char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}
bool digit(char ch) {
    return ch >= '0' && ch <= '9';
}

class LiteralTable {
  public:
    explicit LiteralTable(std::string_view bytes) : bytes_(bytes) {}
    QByteArray toXmp() {
        if (bytes_.empty() || bytes_.size() > 1'048'576 || identifier() != "s")
            invalid();
        expect('=');
        expect('{');
        QByteArray output;
        QXmlStreamWriter xml(&output);
        xml.writeStartElement(QStringLiteral("rdf:RDF"));
        xml.writeNamespace(QStringLiteral("http://www.w3.org/1999/02/22-rdf-syntax-ns#"),
                           QStringLiteral("rdf"));
        xml.writeNamespace(QStringLiteral("http://ns.adobe.com/camera-raw-settings/1.0/"),
                           QStringLiteral("crs"));
        xml.writeStartElement(QStringLiteral("rdf:Description"));
        for (int fields = 0; !take('}'); ++fields) {
            if (fields >= 1024)
                invalid();
            const auto key = identifier();
            expect('=');
            xml.writeStartElement(QStringLiteral("crs:") + fromString(key));
            if (peek() == '{') {
                if (isCurve(key))
                    writeCurve(xml);
                else
                    skipTable();
            } else {
                xml.writeCharacters(scalar());
            }
            xml.writeEndElement();
            if (!take(',') && peek() != '}')
                invalid();
        }
        skipSpace();
        if (!bytes_.empty())
            invalid();
        xml.writeEndElement();
        xml.writeEndElement();
        return output;
    }

    SmartRule toSmartRule() {
        if (bytes_.empty() || bytes_.size() > 65536 || identifier() != "s")
            invalidRule();
        expect('=');
        auto rule = readRule(0);
        skipSpace();
        if (!bytes_.empty())
            invalidRule();
        rule.validate();
        return rule;
    }

  private:
    std::string_view bytes_;
    int ruleCount_ = 0;

    [[noreturn]] static void invalid() {
        throw std::runtime_error("Unsupported Lightroom Develop table.");
    }
    [[noreturn]] static void invalidRule() {
        throw std::runtime_error("Unsupported Lightroom Smart Collection rule.");
    }

    SmartRule readRule(int depth) {
        if (++ruleCount_ > 64 || depth > 8)
            invalidRule();
        expect('{');
        std::map<std::string_view, QString> properties;
        SmartRule rule;
        while (!take('}')) {
            if (peek() == '{') {
                if (rule.children.size() == 16)
                    invalidRule();
                rule.children.push_back(readRule(depth + 1));
            } else {
                const auto key = identifier();
                expect('=');
                if (properties.size() > 16 || !properties.emplace(key, scalar()).second)
                    invalidRule();
            }
            if (!take(',') && peek() != '}')
                invalidRule();
        }
        const auto get = [&](std::string_view key) {
            const auto it = properties.find(key);
            return it == properties.end() ? QString{} : it->second;
        };
        const auto combine = get("combine");
        if (!rule.children.empty()) {
            if (properties.size() != 1)
                invalidRule();
            if (combine == "union")
                rule.kind = "any";
            else if (combine == "exclude" || combine == "none") {
                rule.kind = "any";
                SmartRule exclusion;
                exclusion.kind = "not";
                exclusion.children.push_back(std::move(rule));
                return exclusion;
            } else if (combine != "intersect")
                invalidRule();
            return rule;
        }
        if (std::ranges::any_of(properties, [](const auto &entry) {
                return entry.first != "criteria" && entry.first != "operation" &&
                       entry.first != "value" && entry.first != "value_units";
            }))
            invalidRule();

        static const std::map<QString, std::string> fields{
            {"rating", "rating"},      {"pick", "flag"},
            {"pickFlag", "flag"},      {"camera", "camera"},
            {"lens", "lens"},          {"filename", "filename"},
            {"copyName", "copyName"},  {"fileFormat", "fileType"},
            {"isoSpeedRating", "iso"}, {"captureTime", "captureDate"},
            {"touchTime", "editDate"}, {"keywords", "keyword"},
            {"labelColor", "label"},   {"labelText", "labelText"},
            {"width", "width"},        {"height", "height"}};
        static const std::map<QString, std::string> operations{{"==", "eq"},
                                                               {"!=", "ne"},
                                                               {"~=", "ne"},
                                                               {">=", "ge"},
                                                               {">", "gt"},
                                                               {"<=", "le"},
                                                               {"<", "lt"},
                                                               {"contains", "contains"},
                                                               {"doesNotContain", "notContains"},
                                                               {"startsWith", "startsWith"},
                                                               {"endsWith", "endsWith"},
                                                               {"empty", "empty"},
                                                               {"notEmpty", "notEmpty"},
                                                               {"inLast", "inLast"}};
        const auto criterion = get("criteria");
        const auto field = fields.find(criterion), op = operations.find(get("operation"));
        if (field == fields.end() || op == operations.end())
            invalidRule();
        rule.kind = "condition";
        rule.field = field->second;
        rule.operation = op->second;
        QString value = get("value");
        const auto units = get("value_units");
        if (!units.isEmpty() && rule.operation != "inLast")
            invalidRule();
        if (rule.operation == "inLast") {
            bool ok = false;
            int amount = value.toInt(&ok);
            auto unit = units;
            if (!ok || amount < 1 || amount > 36500)
                invalidRule();
            if (unit == "weeks") {
                amount *= 7;
                unit = "days";
            }
            value = QString::number(amount) + ' ' + unit;
        } else if (criterion == "labelColor" && rule.operation != "empty" &&
                   rule.operation != "notEmpty") {
            static const std::array colors{"", "red", "yellow", "green", "blue", "purple"};
            bool ok = false;
            const int index = value.toInt(&ok);
            if (!ok || index < 0 || index >= static_cast<int>(colors.size()) ||
                (rule.operation != "eq" && rule.operation != "ne"))
                invalidRule();
            value = QString::fromLatin1(colors[static_cast<std::size_t>(index)]);
        } else if (rule.field == "fileType") {
            value = value.toUpper();
            if (value == "JPG")
                value = "JPEG";
        } else if ((rule.field == "captureDate" || rule.field == "editDate") &&
                   rule.operation != "empty" && rule.operation != "notEmpty") {
            const auto date = QDate::fromString(value.left(10), Qt::ISODate);
            if (!date.isValid())
                invalidRule();
            value = date.toString(Qt::ISODate);
        }
        if (rule.field == "rating" || rule.field == "flag" || rule.field == "iso" ||
            rule.field == "width" || rule.field == "height") {
            bool ok = false;
            rule.value = value.toDouble(&ok);
            if (!ok)
                invalidRule();
        } else {
            rule.value = value.toStdString();
        }
        return rule;
    }
    static QString fromString(std::string_view text) {
        return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
    }
    static bool isCurve(std::string_view key) {
        return key == "ToneCurve" || key == "ToneCurveRed" || key == "ToneCurveGreen" ||
               key == "ToneCurveBlue" || key == "ToneCurvePV2012" || key == "ToneCurvePV2012Red" ||
               key == "ToneCurvePV2012Green" || key == "ToneCurvePV2012Blue";
    }
    void skipSpace() {
        while (!bytes_.empty() &&
               std::string_view(" \t\r\n").find(bytes_.front()) != std::string_view::npos)
            bytes_.remove_prefix(1);
    }
    char peek() {
        skipSpace();
        return bytes_.empty() ? '\0' : bytes_.front();
    }
    bool take(char token) {
        if (peek() != token || bytes_.empty())
            return false;
        bytes_.remove_prefix(1);
        return true;
    }
    void expect(char token) {
        if (!take(token))
            invalid();
    }
    std::string_view identifier() {
        if (!letter(peek()))
            invalid();
        std::size_t length = 1;
        while (length < bytes_.size() && (letter(bytes_[length]) || digit(bytes_[length])))
            ++length;
        if (length > 128)
            invalid();
        const auto value = bytes_.substr(0, length);
        bytes_.remove_prefix(length);
        return value;
    }
    std::string quoted() {
        const auto delimiter = peek();
        expect(delimiter);
        std::string value;
        while (!bytes_.empty()) {
            auto ch = bytes_.front();
            bytes_.remove_prefix(1);
            if (ch == delimiter)
                return value;
            if (ch == '\\') {
                if (bytes_.empty())
                    invalid();
                ch = bytes_.front();
                bytes_.remove_prefix(1);
                if (digit(ch)) {
                    unsigned escaped = static_cast<unsigned>(ch - '0');
                    for (int i = 1; i < 3 && !bytes_.empty() && digit(bytes_.front()); ++i) {
                        escaped = 10 * escaped + static_cast<unsigned>(bytes_.front() - '0');
                        bytes_.remove_prefix(1);
                    }
                    if (escaped > 255)
                        invalid();
                    ch = static_cast<char>(escaped);
                } else if (ch == 'n')
                    ch = '\n';
                else if (ch == 'r')
                    ch = '\r';
                else if (ch == 't')
                    ch = '\t';
            }
            value.push_back(ch);
        }
        invalid();
    }
    QString scalar() {
        const auto first = peek();
        if (first == '"' || first == '\'')
            return fromString(quoted());
        const auto end = bytes_.find_first_of(" \t\r\n,}");
        const auto token = bytes_.substr(0, end);
        bytes_.remove_prefix(token.size());
        if (token == "true")
            return QStringLiteral("True");
        if (token == "false")
            return QStringLiteral("False");
        if (token == "nil")
            return {};
        if (token.empty() || token.size() > 100 || token.front() == '+' ||
            token.find_first_not_of("0123456789.+-eE") != std::string_view::npos)
            invalid();
        const QString value = fromString(token);
        bool validNumber = false;
        const double number = value.toDouble(&validNumber);
        if (!validNumber || !std::isfinite(number))
            invalid();
        return value;
    }
    void skipTable() {
        expect('{');
        int depth = 1;
        while (depth > 0 && !bytes_.empty()) {
            const auto ch = peek();
            if (ch == '"' || ch == '\'') {
                (void)quoted();
                continue;
            }
            if (bytes_.empty())
                break;
            bytes_.remove_prefix(1);
            if (ch == '{' && ++depth > 128)
                invalid();
            if (ch == '}')
                --depth;
        }
        if (depth != 0)
            invalid();
    }
    void writeCurve(QXmlStreamWriter &xml) {
        expect('{');
        xml.writeStartElement(QStringLiteral("rdf:Seq"));
        for (int count = 0; !take('}'); ++count) {
            if (count >= 256)
                invalid();
            const auto x = scalar();
            expect(',');
            const auto y = scalar();
            xml.writeTextElement(QStringLiteral("rdf:li"), x + QStringLiteral(", ") + y);
            if (!take(',') && peek() != '}')
                invalid();
        }
        xml.writeEndElement();
    }
};
} // namespace

PresetImport decodeLightroomDevelop(std::string_view document) {
    return decodeXmpPreset(LiteralTable(document).toXmp(), QStringLiteral("Lightroom Develop"));
}
SmartRule decodeLightroomSmartRule(std::string_view document) {
    return LiteralTable(document).toSmartRule();
}
} // namespace luma
