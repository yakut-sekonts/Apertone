#include "platform/macos/PhotoMetadataIO.h"
#include "platform/macos/CFHandle.h"
#import <Foundation/Foundation.h>
#include <ImageIO/ImageIO.h>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace luma::macos {
namespace {
CFHandle<CGImageMetadataRef> parse(std::span<const std::uint8_t> bytes) {
    if (bytes.empty() || bytes.size() > 8 * 1024 * 1024)
        throw std::runtime_error("XMP must fit the 8 MiB limit.");
    NSString *xml = [[NSString alloc] initWithBytes:bytes.data()
                                             length:bytes.size()
                                           encoding:NSUTF8StringEncoding];
    if (!xml ||
        [xml rangeOfString:@"<!DOCTYPE" options:NSCaseInsensitiveSearch].location != NSNotFound ||
        [xml rangeOfString:@"<!ENTITY" options:NSCaseInsensitiveSearch].location != NSNotFound)
        throw std::runtime_error("XMP must be UTF-8 without DTD or entity declarations.");
    CFHandle data(CFDataCreate(nullptr, bytes.data(), static_cast<CFIndex>(bytes.size())));
    CFHandle result(CGImageMetadataCreateFromXMPData(data.get()));
    if (!result)
        throw std::runtime_error("Invalid or unsupported XMP packet.");
    return result;
}
std::string string(CFStringRef value) {
    const char *utf8 = value ? [(__bridge NSString *)value UTF8String] : nullptr;
    return utf8 ? utf8 : "";
}
std::string scalar(CGImageMetadataRef data, CFStringRef path) {
    CFHandle value(CGImageMetadataCopyStringValueWithPath(data, nullptr, path));
    return string(value.get());
}
bool exists(CGImageMetadataRef data, CFStringRef path) {
    CFHandle tag(CGImageMetadataCopyTagWithPath(data, nullptr, path));
    return static_cast<bool>(tag);
}
std::vector<std::string> array(CGImageMetadataRef data, CFStringRef path, bool unique = true) {
    CFHandle tag(CGImageMetadataCopyTagWithPath(data, nullptr, path));
    if (!tag)
        return {};
    CFHandle value(CGImageMetadataTagCopyValue(tag.get()));
    if (!value || CFGetTypeID(value.get()) != CFArrayGetTypeID())
        throw std::runtime_error("XMP list property must be an array.");
    auto list = static_cast<CFArrayRef>(value.get());
    if (CFArrayGetCount(list) > 256)
        throw std::runtime_error("XMP list exceeds its item limit.");
    std::vector<std::string> result;
    for (CFIndex i = 0; i < CFArrayGetCount(list); ++i) {
        const auto item = CFArrayGetValueAtIndex(list, i);
        if (CFGetTypeID(item) != CFStringGetTypeID())
            throw std::runtime_error("XMP list must contain text.");
        auto text = string(static_cast<CFStringRef>(item));
        if (!text.empty() &&
            (!unique || std::find(result.begin(), result.end(), text) == result.end()))
            result.push_back(std::move(text));
    }
    return result;
}
void overlay(CGImageMetadataRef data, DescriptiveMetadata &values) {
    if (!data)
        return;
    for (const auto &[path, destination] : {std::pair{CFSTR("dc:title"), &values.title},
                                            {CFSTR("dc:description"), &values.caption},
                                            {CFSTR("dc:rights"), &values.copyright},
                                            {CFSTR("Iptc4xmpCore:Location"), &values.location}})
        if (exists(data, path))
            *destination = scalar(data, path);
    if (exists(data, CFSTR("dc:creator")))
        values.creators = array(data, CFSTR("dc:creator"), false);
    if (exists(data, CFSTR("dc:subject")))
        values.keywords = array(data, CFSTR("dc:subject"));
    if (exists(data, CFSTR("xmp:Rating"))) {
        std::istringstream number(scalar(data, CFSTR("xmp:Rating")));
        number.imbue(std::locale::classic());
        double rating = 0;
        if (!(number >> rating) || !(number >> std::ws).eof() || !std::isfinite(rating) ||
            rating < -1 || rating > 5)
            throw std::runtime_error("Invalid XMP rating.");
        // Accept integral real spellings such as 3.0 without rounding fractions.
        if (std::trunc(rating) != rating)
            throw std::runtime_error(
                "Fractional XMP ratings cannot be represented by whole-star Library ratings.");
        values.flag = rating == -1 ? -1 : 0;
        values.rating = rating == -1 ? 0 : static_cast<int>(rating);
    }
    if (exists(data, CFSTR("xmp:Label")))
        values.label = scalar(data, CFSTR("xmp:Label"));
}
std::string escaped(std::string_view value) {
    std::string out;
    for (char c : value) {
        if (c == '&')
            out += "&amp;";
        else if (c == '<')
            out += "&lt;";
        else if (c == '>')
            out += "&gt;";
        else if (c == '"')
            out += "&quot;";
        else
            out += c;
    }
    return out;
}
} // namespace
DescriptiveMetadata readDescriptiveMetadata(const std::filesystem::path &source,
                                            std::span<const std::uint8_t> sidecar) {
    @autoreleasepool {
        CFHandle url(CFURLCreateFromFileSystemRepresentation(
            nullptr, reinterpret_cast<const UInt8 *>(source.c_str()),
            static_cast<CFIndex>(source.string().size()), false));
        CFHandle image(CGImageSourceCreateWithURL(url.get(), nullptr));
        if (!image)
            throw std::runtime_error("ImageIO cannot read original metadata.");
        CFHandle embedded(CGImageSourceCopyMetadataAtIndex(
            image.get(), CGImageSourceGetPrimaryImageIndex(image.get()), nullptr));
        DescriptiveMetadata result;
        overlay(embedded.get(), result);
        if (!sidecar.empty()) {
            auto data = parse(sidecar);
            overlay(data.get(), result);
        }
        result.validate();
        return result;
    }
}
std::vector<std::uint8_t> writeDescriptiveXMP(const DescriptiveMetadata &values,
                                              std::span<const std::uint8_t> existing) {
    @autoreleasepool {
        values.validate();
        std::ostringstream xml;
        xml.imbue(std::locale::classic());
        xml << "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\"><rdf:RDF "
               "xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">"
               "<rdf:Description rdf:about=\"\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
               "xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\" "
               "xmlns:Iptc4xmpCore=\"http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/\" xmp:Rating=\""
            << values.xmpRating() << "\" xmp:Label=\"" << escaped(values.label)
            << "\" Iptc4xmpCore:Location=\"" << escaped(values.location) << "\">";
        auto localized = [&](const char *name, const std::string &text) {
            xml << "<dc:" << name << "><rdf:Alt><rdf:li xml:lang=\"x-default\">" << escaped(text)
                << "</rdf:li></rdf:Alt></dc:" << name << ">";
        };
        localized("title", values.title);
        localized("description", values.caption);
        localized("rights", values.copyright);
        xml << "<dc:creator><rdf:Seq>";
        for (const auto &creator : values.creators)
            xml << "<rdf:li>" << escaped(creator) << "</rdf:li>";
        xml << "</rdf:Seq></dc:creator><dc:subject><rdf:Bag>";
        for (const auto &keyword : values.keywords)
            xml << "<rdf:li>" << escaped(keyword) << "</rdf:li>";
        xml << "</rdf:Bag></dc:subject></rdf:Description></rdf:RDF></x:xmpmeta>";
        const auto text = xml.str();
        auto updates =
            parse(std::span(reinterpret_cast<const std::uint8_t *>(text.data()), text.size()));
        CFHandle<CGMutableImageMetadataRef> destination(CGImageMetadataCreateMutable());
        if (!existing.empty()) {
            auto original = parse(existing);
            destination = CFHandle<CGMutableImageMetadataRef>(
                CGImageMetadataCreateMutableCopy(original.get()));
        }
        if (!destination)
            throw std::runtime_error("Cannot allocate XMP metadata.");
        CFHandle tags(CGImageMetadataCopyTags(updates.get()));
        if (!tags)
            throw std::runtime_error("Cannot serialize descriptive metadata.");
        for (CFIndex i = 0; i < CFArrayGetCount(tags.get()); ++i) {
            const auto tag = static_cast<CGImageMetadataTagRef>(
                const_cast<void *>(CFArrayGetValueAtIndex(tags.get(), i)));
            CFHandle ns(CGImageMetadataTagCopyNamespace(tag)),
                prefix(CGImageMetadataTagCopyPrefix(tag)), name(CGImageMetadataTagCopyName(tag));
            if (!ns || !prefix || !name)
                throw std::runtime_error("XMP property is missing its namespace, prefix or name.");
            if (!CGImageMetadataRegisterNamespaceForPrefix(destination.get(), ns.get(),
                                                           prefix.get(), nullptr))
                throw std::runtime_error("XMP namespace conflicts with an existing property.");
            NSString *path = [NSString stringWithFormat:@"%@:%@", (__bridge NSString *)prefix.get(),
                                                        (__bridge NSString *)name.get()];
            // Keep other language variants when editing the default text.
            if (CGImageMetadataTagGetType(tag) == kCGImageMetadataTypeAlternateText &&
                exists(destination.get(), (__bridge CFStringRef)path)) {
                CFHandle value(CGImageMetadataCopyStringValueWithPath(updates.get(), nullptr,
                                                                      (__bridge CFStringRef)path));
                NSString *localizedPath = [path stringByAppendingString:@"[x-default]"];
                if (!value || !CGImageMetadataSetValueWithPath(destination.get(), nullptr,
                                                               (__bridge CFStringRef)localizedPath,
                                                               value.get()))
                    throw std::runtime_error("Cannot update localized XMP text.");
            } else if (!CGImageMetadataSetTagWithPath(destination.get(), nullptr,
                                                      (__bridge CFStringRef)path, tag)) {
                throw std::runtime_error("Cannot update an XMP property.");
            }
        }
        CFHandle data(CGImageMetadataCreateXMPData(destination.get(), nullptr));
        if (!data || CFDataGetLength(data.get()) > 8 * 1024 * 1024)
            throw std::runtime_error("Cannot serialize XMP within its size limit.");
        return {CFDataGetBytePtr(data.get()),
                CFDataGetBytePtr(data.get()) + CFDataGetLength(data.get())};
    }
}
CFHandle<CGMutableImageMetadataRef> applyMetadataOverlay(CGImageMetadataRef source,
                                                         const MetadataOverlay &overlay) {
    @autoreleasepool {
        overlay.values.validate();
        if (overlay.includeText) {
            CFHandle<CFDataRef> packet(source ? CGImageMetadataCreateXMPData(source, nullptr)
                                              : nullptr);
            const auto bytes =
                packet ? std::span(CFDataGetBytePtr(packet.get()),
                                   static_cast<std::size_t>(CFDataGetLength(packet.get())))
                       : std::span<const std::uint8_t>{};
            const auto rewritten = writeDescriptiveXMP(overlay.values, bytes);
            auto parsed = parse(rewritten);
            return CFHandle<CGMutableImageMetadataRef>(
                CGImageMetadataCreateMutableCopy(parsed.get()));
        }
        CFHandle result(source ? CGImageMetadataCreateMutableCopy(source)
                               : CGImageMetadataCreateMutable());
        NSString *rating = [NSString stringWithFormat:@"%d", overlay.values.xmpRating()];
        NSString *label = [NSString stringWithUTF8String:overlay.values.label.c_str()];
        if (!result ||
            !CGImageMetadataRegisterNamespaceForPrefix(result.get(),
                                                       kCGImageMetadataNamespaceXMPBasic,
                                                       kCGImageMetadataPrefixXMPBasic, nullptr) ||
            !CGImageMetadataSetValueWithPath(result.get(), nullptr, CFSTR("xmp:Rating"),
                                             (__bridge CFStringRef)rating) ||
            !CGImageMetadataSetValueWithPath(result.get(), nullptr, CFSTR("xmp:Label"),
                                             (__bridge CFStringRef)label))
            throw std::runtime_error("Cannot apply catalog metadata to export.");
        return result;
    }
}
} // namespace luma::macos
