#include "platform/macos/PhotoMetadataIO.h"
#include "platform/macos/CFHandle.h"
#import <Foundation/Foundation.h>
#include <ImageIO/ImageIO.h>
#include <iostream>
#include <string>

std::span<const std::uint8_t> bytes(const std::string &s) {
    return {reinterpret_cast<const std::uint8_t *>(s.data()), s.size()};
}
std::string packet(const std::string &properties) {
    return "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\"><rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"><rdf:Description rdf:about=\"\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\">" + properties + "</rdf:Description></rdf:RDF></x:xmpmeta>";
}
void paths(std::span<const std::uint8_t> input) {
    using namespace luma::macos;
    CFHandle data(CFDataCreate(nullptr,input.data(),input.size()));
    CFHandle meta(CGImageMetadataCreateFromXMPData(data.get()));
    if (!meta) { std::cout << "PARSE NULL\n"; return; }
    for (NSString *path in @[@"dc:title",@"dc:title[x-default]",@"dc:title[de-DE]",@"qa:Note",@"crs:Exposure2012",@"dc:creator",@"dc:subject"]) {
        CFHandle tag(CGImageMetadataCopyTagWithPath(meta.get(),nullptr,(__bridge CFStringRef)path));
        CFHandle text(CGImageMetadataCopyStringValueWithPath(meta.get(),nullptr,(__bridge CFStringRef)path));
        std::cout << path.UTF8String << " type=" << (tag ? CGImageMetadataTagGetType(tag.get()) : -999) << " string=" << (text ? [(__bridge NSString *)text.get() UTF8String] : "<NULL>") << '\n';
        if (tag) { CFHandle v(CGImageMetadataTagCopyValue(tag.get())); if(v) CFShow(v.get()); }
    }
}
void values(const luma::DescriptiveMetadata &v) {
    std::cout << "title=" << v.title << " caption=" << v.caption << " copyright=" << v.copyright << " location=" << v.location << " rating=" << v.rating << " flag=" << v.flag << " label=" << v.label << '\n';
    for(const auto &x:v.creators) std::cout << "creator=" << x << '\n';
    for(const auto &x:v.keywords) std::cout << "keyword=" << x << '\n';
}
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    @autoreleasepool {
        const auto original=packet("<dc:title><rdf:Alt><rdf:li xml:lang=\"x-default\">Before</rdf:li><rdf:li xml:lang=\"de-DE\">Deutscher Titel</rdf:li></rdf:Alt></dc:title><qa:Note xmlns:qa=\"https://example.invalid/luma-qa/\">Keep &amp; preserve</qa:Note><crs:Exposure2012 xmlns:crs=\"http://ns.adobe.com/camera-raw-settings/1.0/\">1.25</crs:Exposure2012>");
        std::cout << "BEFORE\n"; paths(bytes(original));
        luma::DescriptiveMetadata replacement;replacement.title="Север & 夜景";replacement.creators={"Անի Սարգսյան","山田 太郎","Անի Սարգսյան"};replacement.keywords={"ночь","빛","café"};
        const auto output=luma::macos::writeDescriptiveXMP(replacement,bytes(original));
        std::cout << "AFTER XML\n" << std::string(output.begin(),output.end()) << "\nAFTER PATHS\n";paths(output);
        const auto external=std::string("\n")+packet("<xmp:Rating>3</xmp:Rating><xmp:Label>External</xmp:Label><dc:title><rdf:Alt><rdf:li xml:lang=\"x-default\">Внешний 夜景 &amp; café</rdf:li></rdf:Alt></dc:title><dc:description><rdf:Alt><rdf:li xml:lang=\"x-default\">Отдельный XMP</rdf:li></rdf:Alt></dc:description><dc:creator><rdf:Seq><rdf:li>Անի</rdf:li></rdf:Seq></dc:creator><dc:subject><rdf:Bag><rdf:li>外部</rdf:li></rdf:Bag></dc:subject>");
        const std::string serialized(output.begin(),output.end());
        std::cout << "BEFORE WITH DECLARATION\n";paths(bytes("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"+original));
        std::cout << "AFTER READBACK\n";
        try { values(luma::macos::readDescriptiveMetadata(argv[1],output)); }
        catch(const std::exception &e) {std::cout << "READ ERROR " << e.what() << '\n';}
        std::cout << "EXTERNAL\n"; paths(bytes(external));
        try { values(luma::macos::readDescriptiveMetadata(argv[1],bytes(external))); }
        catch(const std::exception &e) {std::cout << "READ ERROR " << e.what() << '\n';}
    }
}
