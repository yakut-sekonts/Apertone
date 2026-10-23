#include "platform/macos/CFHandle.h"
#include "platform/macos/FileHasher.h"
#include "platform/macos/PhotoMetadataIO.h"
#include <ImageIO/ImageIO.h>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
std::string packet(const std::string &properties) {
    return "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\"><rdf:RDF "
           "xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">"
           "<rdf:Description rdf:about=\"\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
           "xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\">" +
           properties + "</rdf:Description></rdf:RDF></x:xmpmeta>";
}
luma::DescriptiveMetadata read(const std::filesystem::path &source, const std::string &xml) {
    return luma::macos::readDescriptiveMetadata(
        source, std::span(reinterpret_cast<const std::uint8_t *>(xml.data()), xml.size()));
}
void rejects(const std::filesystem::path &source, const std::string &xml) {
    bool rejected = false;
    try {
        (void)read(source, xml);
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected, "Unsupported or oversized XMP list was accepted.");
}
void preservesUneditedProperties(const std::filesystem::path &source,
                                 const luma::DescriptiveMetadata &values) {
    const auto original = packet(
        "<dc:title><rdf:Alt><rdf:li xml:lang=\"x-default\">Before</rdf:li>"
        "<rdf:li xml:lang=\"de-DE\">Deutscher Titel</rdf:li></rdf:Alt></dc:title>"
        "<qa:Note xmlns:qa=\"https://example.invalid/luma-qa/\">Keep &amp; preserve</qa:Note>"
        "<crs:Exposure2012 xmlns:crs=\"http://ns.adobe.com/camera-raw-settings/1.0/\">"
        "1.25</crs:Exposure2012>");
    const auto output = luma::macos::writeDescriptiveXMP(
        values,
        std::span(reinterpret_cast<const std::uint8_t *>(original.data()), original.size()));
    require(luma::macos::readDescriptiveMetadata(source, output) == values,
            "Updating an existing XMP packet did not replace the supported metadata exactly.");
    luma::macos::CFHandle data(
        CFDataCreate(nullptr, output.data(), static_cast<CFIndex>(output.size())));
    luma::macos::CFHandle metadata(CGImageMetadataCreateFromXMPData(data.get()));
    require(static_cast<bool>(metadata), "Updated XMP cannot be parsed by ImageIO.");
    for (const auto &[path, expected] :
         {std::pair{CFSTR("qa:Note"), CFSTR("Keep & preserve")},
          std::pair{CFSTR("crs:Exposure2012"), CFSTR("1.25")},
          std::pair{CFSTR("dc:title[de-DE]"), CFSTR("Deutscher Titel")}}) {
        luma::macos::CFHandle actual(
            CGImageMetadataCopyStringValueWithPath(metadata.get(), nullptr, path));
        require(actual && CFEqual(actual.get(), expected),
                "Writing supported metadata removed an external property or another language.");
    }
}
} // namespace
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    try {
        const std::filesystem::path source(argv[1]);
        const auto hash = luma::macos::sha256File(source);
        const auto xml = packet(
            "<dc:title><rdf:Alt><rdf:li xml:lang=\"x-default\">Север &amp; 夜景</rdf:li></rdf:Alt>"
            "</dc:title><dc:creator><rdf:Seq><rdf:li>Անի Սարգսյան</rdf:li>"
            "<rdf:li>山田 太郎</rdf:li><rdf:li>Անի Սարգսյան</rdf:li></rdf:Seq></dc:creator>"
            "<dc:subject><rdf:Bag><rdf:li>ночь</rdf:li><rdf:li>빛</rdf:li>"
            "<rdf:li>ночь</rdf:li><rdf:li>café</rdf:li></rdf:Bag></dc:subject>");
        const auto values = read(source, xml);
        require(
            values.title == "Север & 夜景" &&
                values.creators ==
                    std::vector<std::string>{"Անի Սարգսյան", "山田 太郎", "Անի Սարգսյան"} &&
                values.keywords == std::vector<std::string>{"ночь", "빛", "café"},
            "Native ImageIO arrays lost Unicode, creator sequence/repeats or keyword uniqueness.");
        const auto written = luma::macos::writeDescriptiveXMP(values, {});
        require(luma::macos::readDescriptiveMetadata(source, written) == values,
                "Written XMP lists cannot be read back exactly.");
        preservesUneditedProperties(source, values);
        for (const auto &property : {"creator", "subject"}) {
            const std::string prefix = std::string("<dc:") + property + "><rdf:Seq>";
            const std::string suffix = std::string("</rdf:Seq></dc:") + property + ">";
            rejects(source,
                    packet(prefix +
                           "<rdf:li rdf:parseType=\"Resource\"><xmp:Label>nested</xmp:Label>"
                           "</rdf:li>" +
                           suffix));
            rejects(source, packet(prefix +
                                   "<rdf:li><rdf:Seq><rdf:li>nested</rdf:li>"
                                   "</rdf:Seq></rdf:li>" +
                                   suffix));
            // ImageIO normalizes a scalar spelling of these known list properties.
            const auto scalar = read(
                source, packet(std::string("<dc:") + property + ">scalar</dc:" + property + ">"));
            const auto &normalized =
                std::string_view(property) == "creator" ? scalar.creators : scalar.keywords;
            require(normalized == std::vector<std::string>{"scalar"},
                    "ImageIO-normalized single-element XMP list lost its text.");
            std::string items;
            for (int index = 0; index < 257; ++index)
                items += "<rdf:li>entry-" + std::to_string(index) + "</rdf:li>";
            rejects(source, packet(prefix + items + suffix));
        }
        require(luma::macos::sha256File(source) == hash, "Metadata reads rewrote the original.");
        std::cout << "Native XMP lists: Unicode/order/duplicates, roundtrip and strict "
                     "structure/size rejection passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
