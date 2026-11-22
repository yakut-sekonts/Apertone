#include "apps/shared/LightroomDevelop.h"
#include <QXmlStreamWriter>
#include <charconv>
#include <cmath>
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

  private:
    std::string_view bytes_;

    [[noreturn]] static void invalid() {
        throw std::runtime_error("Unsupported Lightroom Develop table.");
    }
    static QString fromString(std::string_view text) {
        return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
    }
    static bool isCurve(std::string_view key) {
        return key == "ToneCurve" || key == "ToneCurveRed" || key == "ToneCurveGreen" ||
               key == "ToneCurveBlue" || key == "ToneCurvePV2012" ||
               key == "ToneCurvePV2012Red" || key == "ToneCurvePV2012Green" ||
               key == "ToneCurvePV2012Blue";
    }
    void skipSpace() {
        while (!bytes_.empty() && std::string_view(" \t\r\n").find(bytes_.front()) !=
                                      std::string_view::npos)
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
        if (token.empty() || token.size() > 100)
            invalid();
        double number = 0;
        const auto result = std::from_chars(token.data(), token.data() + token.size(), number);
        if (result.ec != std::errc{} || result.ptr != token.data() + token.size() ||
            !std::isfinite(number))
            invalid();
        return fromString(token);
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
} // namespace luma
