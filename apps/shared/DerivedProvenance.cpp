#include "apps/shared/DerivedProvenance.h"
#include "core/image/Bayer.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace luma {
namespace {
bool validToken(const std::string &value, std::size_t maximum = 128) {
    return !value.empty() && value.size() <= maximum &&
           std::all_of(value.begin(), value.end(), [](unsigned char c) {
               return std::isalnum(c) || c == ' ' || c == '-' || c == '_' || c == '.' || c == ':' ||
                      c == '/' || c == '+';
           });
}

std::string xml(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        switch (character) {
        case '&':
            result += "&amp;";
            break;
        case '<':
            result += "&lt;";
            break;
        case '>':
            result += "&gt;";
            break;
        case '\"':
            result += "&quot;";
            break;
        case '\'':
            result += "&apos;";
            break;
        default:
            if (static_cast<unsigned char>(character) < 0x20 && character != '\t' &&
                character != '\n' && character != '\r')
                throw std::runtime_error("Derived provenance contains invalid XML text.");
            result += character;
        }
    }
    return result;
}

void attribute(std::ostringstream &output, std::string_view name, std::string_view value) {
    output << ' ' << name << "=\"" << xml(value) << '\"';
}
} // namespace

std::string linearFloatContract(int width, int height, bool alpha) {
    pixelCount(width, height);
    return "scene-linear-sRGB/float32/" + std::string(alpha ? "straight-alpha/" : "opaque/") +
           std::to_string(width) + "x" + std::to_string(height);
}

std::string encodeDerivedProvenance(const DerivedProvenance &value) {
    if (!validToken(value.operation) || !validToken(value.provider) ||
        !validToken(value.providerVersion, 64) || !validToken(value.algorithm) ||
        !validToken(value.algorithmVersion, 64) || value.inputContract.empty() ||
        value.inputContract.size() > 256 || value.outputContract.empty() ||
        value.outputContract.size() > 256 || value.sources.empty() || value.sources.size() > 24 ||
        (!value.model.empty() && !validToken(value.model)) ||
        (!value.modelVersion.empty() && !validToken(value.modelVersion, 128)) ||
        value.parameters.size() > 32)
        throw std::runtime_error("Invalid derived provenance contract.");
    for (const auto &source : value.sources)
        if (source.id.empty() || source.id.size() > 128 || source.checksum.size() != 64 ||
            !std::all_of(source.checksum.begin(), source.checksum.end(),
                         [](unsigned char c) { return std::isxdigit(c); }) ||
            source.revision < 0)
            throw std::runtime_error("Invalid derived provenance source.");
    for (const auto &[name, parameter] : value.parameters)
        if (!validToken(name, 64) || parameter.size() > 512)
            throw std::runtime_error("Invalid derived provenance parameter.");

    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\"><rdf:RDF "
              "xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"><rdf:Description "
              "rdf:about=\"\" xmlns:luma=\"https://luma.photo/ns/1.0/\"";
    attribute(output, "luma:Operation", value.operation);
    attribute(output, "luma:Provider", value.provider);
    attribute(output, "luma:ProviderVersion", value.providerVersion);
    attribute(output, "luma:Algorithm", value.algorithm);
    attribute(output, "luma:AlgorithmVersion", value.algorithmVersion);
    attribute(output, "luma:InputContract", value.inputContract);
    attribute(output, "luma:OutputContract", value.outputContract);
    attribute(output, "luma:Deterministic", value.deterministic ? "true" : "false");
    attribute(output, "luma:Execution", value.local ? "local" : "remote");
    if (!value.model.empty())
        attribute(output, "luma:Model", value.model);
    if (!value.modelVersion.empty())
        attribute(output, "luma:ModelVersion", value.modelVersion);
    output << "><luma:Sources><rdf:Seq>";
    for (const auto &source : value.sources)
        output << "<rdf:li>"
               << xml(source.id + ":" + source.checksum + ":" + std::to_string(source.revision))
               << "</rdf:li>";
    output << "</rdf:Seq></luma:Sources>";
    if (!value.parameters.empty()) {
        output << "<luma:Parameters><rdf:Seq>";
        for (const auto &[name, parameter] : value.parameters)
            output << "<rdf:li>" << xml(name + "=" + parameter) << "</rdf:li>";
        output << "</rdf:Seq></luma:Parameters>";
    }
    output << "</rdf:Description></rdf:RDF></x:xmpmeta>";
    const auto result = output.str();
    if (result.size() > 1024 * 1024)
        throw std::runtime_error("Derived provenance exceeds its storage budget.");
    return result;
}
} // namespace luma
