#include "core/library/SmartCollection.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
namespace luma {
namespace {
void require(bool value) {
    if (!value)
        throw std::runtime_error("Invalid smart collection rule.");
}
std::string column(const std::string &field) {
    if (field == "rating")
        return "a.rating";
    if (field == "flag")
        return "a.flag";
    if (field == "iso")
        return "a.iso";
    if (field == "width")
        return "a.width";
    if (field == "height")
        return "a.height";
    if (field == "camera")
        return "a.camera";
    if (field == "lens")
        return "a.lens";
    if (field == "filename")
        return "a.filename";
    if (field == "captureDate")
        return "strftime('%Y-%m-%d',a.capture_date,'unixepoch')";
    if (field == "editDate")
        return "(SELECT date(updated_at) FROM edit_documents ed WHERE ed.asset_id=a.id)";
    if (field == "copyName")
        return "a.copy_name";
    if (field == "fileType")
        return "(CASE lower(a.extension) WHEN '.jpg' THEN 'JPEG' WHEN '.jpeg' THEN 'JPEG' "
               "WHEN '.tif' THEN 'TIFF' WHEN '.tiff' THEN 'TIFF' WHEN '.dng' THEN 'DNG' "
               "WHEN '.png' THEN 'PNG' WHEN '.webp' THEN 'WEBP' WHEN '.heic' THEN 'HEIC' "
               "WHEN '.avif' THEN 'AVIF' WHEN '.bmp' THEN 'BMP' WHEN '.psd' THEN 'PSD' "
               "WHEN '.mp4' THEN 'VIDEO' WHEN '.mov' THEN 'VIDEO' ELSE 'RAW' END)";
    if (field == "label")
        return "COALESCE(a.color_label,'')";
    if (field == "keyword")
        return "k.keyword";
    if (field == "labelText")
        return "(SELECT m.xmp_label FROM asset_metadata m WHERE m.asset_id=a.id)";
    throw std::runtime_error("Unsupported smart collection field.");
}
bool numeric(const std::string &field) {
    return field == "rating" || field == "flag" || field == "iso" || field == "width" ||
           field == "height";
}
bool relativeDate(const std::string &duration) {
    const auto separator = duration.find(' ');
    if (separator == duration.npos)
        return false;
    unsigned amount = 0;
    const auto result = std::from_chars(duration.data(), duration.data() + separator, amount);
    const auto unit = std::string_view(duration).substr(separator + 1);
    return result.ec == std::errc{} && result.ptr == duration.data() + separator && amount >= 1 &&
           amount <= 36500 && (unit == "days" || unit == "months" || unit == "years");
}
void validateRule(const SmartRule &r, int depth, int &count) {
    require(++count <= 64 && depth <= 8);
    if (r.kind == "all" || r.kind == "any" || r.kind == "not") {
        require(!r.children.empty() && r.children.size() <= 16 &&
                (r.kind != "not" || r.children.size() == 1));
        for (const auto &c : r.children)
            validateRule(c, depth + 1, count);
    } else {
        require(r.kind == "condition" && r.children.empty());
        (void)column(r.field);
        require(r.operation == "eq" || r.operation == "ne" || r.operation == "gt" ||
                r.operation == "ge" || r.operation == "lt" || r.operation == "le" ||
                r.operation == "contains" || r.operation == "notContains" ||
                r.operation == "startsWith" || r.operation == "endsWith" ||
                r.operation == "empty" || r.operation == "notEmpty" || r.operation == "inLast");
        if (numeric(r.field)) {
            require(std::holds_alternative<double>(r.value) &&
                    (r.operation == "eq" || r.operation == "ne" || r.operation == "gt" ||
                     r.operation == "ge" || r.operation == "lt" || r.operation == "le"));
            const double v = std::get<double>(r.value);
            require(std::isfinite(v));
            require(r.field == "flag"     ? v >= -1 && v <= 1
                    : r.field == "rating" ? v >= 0 && v <= 5
                                          : v >= 0 && v <= 1e9);
        } else {
            require(std::holds_alternative<std::string>(r.value));
            const auto &v = std::get<std::string>(r.value);
            require((!v.empty() || r.operation == "eq" || r.operation == "ne" ||
                     r.operation == "empty" || r.operation == "notEmpty") &&
                    v.size() <= 1024 && v.find('\0') == std::string::npos);
            if (r.operation == "inLast")
                require((r.field == "captureDate" || r.field == "editDate") && relativeDate(v));
        }
    }
}
void write(std::ostream &out, const SmartRule &r) {
    out << std::quoted(r.kind) << ' ';
    if (r.kind == "condition") {
        out << std::quoted(r.field) << ' ' << std::quoted(r.operation) << ' ';
        if (numeric(r.field))
            out << std::get<double>(r.value);
        else
            out << std::quoted(std::get<std::string>(r.value));
    } else {
        out << r.children.size();
        for (const auto &c : r.children) {
            out << ' ';
            write(out, c);
        }
    }
}
SmartRule read(std::istream &in, int depth, int &count) {
    require(++count <= 64 && depth <= 8);
    SmartRule r;
    in >> std::quoted(r.kind);
    if (r.kind == "condition") {
        in >> std::quoted(r.field) >> std::quoted(r.operation);
        if (numeric(r.field)) {
            double v = 0;
            in >> v;
            r.value = v;
        } else {
            std::string v;
            in >> std::quoted(v);
            r.value = v;
        }
    } else {
        require(r.kind == "all" || r.kind == "any" || r.kind == "not");
        unsigned n = 0;
        in >> n;
        require(n > 0 && n <= 16);
        for (unsigned i = 0; i < n; ++i)
            r.children.push_back(read(in, depth + 1, count));
    }
    require(static_cast<bool>(in));
    return r;
}
std::string compile(const SmartRule &r, SmartSQL &q, int first) {
    if (r.kind != "condition") {
        std::string result = r.kind == "not" ? "(NOT " : "(";
        for (std::size_t i = 0; i < r.children.size(); ++i) {
            if (i)
                result += r.kind == "all" ? " AND " : " OR ";
            result += compile(r.children[i], q, first);
        }
        return result + ")";
    }
    const auto name = column(r.field);
    if (r.operation == "empty" || r.operation == "notEmpty") {
        const bool empty = r.operation == "empty";
        if (r.field == "keyword")
            return std::string(empty ? "NOT " : "") +
                   "EXISTS(SELECT 1 FROM asset_keywords k WHERE k.asset_id=a.id)";
        return "(COALESCE(" + name + ",'')" + (empty ? "=" : "<>") + "'')";
    }
    auto value = r.value;
    const bool pattern = r.operation == "contains" || r.operation == "notContains" ||
                         r.operation == "startsWith" || r.operation == "endsWith";
    const bool negate = r.operation == "notContains";
    const std::string op = (r.operation == "eq" || (r.field == "keyword" && r.operation == "ne"))
                               ? "="
                           : r.operation == "ne" ? "!="
                           : r.operation == "gt" ? ">"
                           : r.operation == "ge" ? ">="
                           : r.operation == "lt" ? "<"
                           : r.operation == "le" ? "<="
                                                 : " LIKE ";
    if (pattern) {
        std::string escaped = r.operation == "startsWith" ? "" : "%";
        for (char c : std::get<std::string>(value)) {
            if (c == '%' || c == '_' || c == '\\')
                escaped += '\\';
            escaped += c;
        }
        value = r.operation == "endsWith" ? escaped : escaped + "%";
    }
    const auto index = first + static_cast<int>(q.values.size());
    q.values.push_back(value);
    const auto parameter = "?" + std::to_string(index);
    if (r.operation == "inLast")
        return "COALESCE((" + name + " BETWEEN date('now','-'||" + parameter +
               ",'floor') AND date('now')),0)";
    auto expr = name + (negate && r.field != "keyword" ? " NOT LIKE " : op) + parameter +
                (pattern ? " ESCAPE '\\'" : "");
    if (r.field == "keyword")
        expr = (r.operation == "ne" || negate
                    ? "NOT EXISTS(SELECT 1 FROM asset_keywords k WHERE k.asset_id=a.id AND "
                    : "EXISTS(SELECT 1 FROM asset_keywords k WHERE k.asset_id=a.id AND ") +
               expr + ")";
    return "COALESCE((" + expr + "),0)";
}
} // namespace
void SmartRule::validate() const {
    int count = 0;
    validateRule(*this, 0, count);
}
std::string encodeSmartRule(const SmartRule &rule) {
    rule.validate();
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "LUMA_SMART 1 " << std::setprecision(17);
    write(out, rule);
    out << '\n';
    return out.str();
}
SmartRule decodeSmartRule(const std::string &text) {
    require(!text.empty() && text.size() <= 65536);
    std::istringstream in(text);
    in.imbue(std::locale::classic());
    std::string magic;
    int version = 0;
    in >> magic >> version;
    require(magic == "LUMA_SMART" && version == 1);
    int count = 0;
    auto result = read(in, 0, count);
    result.validate();
    require(encodeSmartRule(result) == text);
    return result;
}
SmartSQL compileSmartRule(const SmartRule &rule, int first) {
    rule.validate();
    require(first >= 1 && first <= 100);
    SmartSQL result;
    result.expression = compile(rule, result, first);
    return result;
}
} // namespace luma
