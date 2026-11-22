#include "core/edits/EditSnapshot.h"
#include "core/io/Path.h"
#include "core/library/LibraryTypes.h"
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
namespace luma {
namespace {
void require(bool condition) {
    if (!condition)
        throw std::runtime_error("Invalid advanced import options.");
}
void writeList(std::ostream &out, const std::vector<std::string> &values) {
    out << values.size() << ' ';
    for (const auto &value : values)
        out << std::quoted(value) << ' ';
}
std::vector<std::string> readList(std::istream &in, unsigned maximum = 256) {
    unsigned count = 0;
    in >> count;
    require(static_cast<bool>(in) && count <= maximum);
    std::vector<std::string> values;
    for (unsigned i = 0; i < count; ++i) {
        std::string value;
        in >> std::quoted(value);
        require(static_cast<bool>(in));
        values.push_back(std::move(value));
    }
    return values;
}
void validFilename(const std::string &name) {
    require(!name.empty() && name != "." && name != ".." && name.size() <= 230 &&
            name.front() != '.');
    for (unsigned char c : name)
        require(c >= 32 && c != 127 && c != '/' && c != '\\' && c != ':');
}
} // namespace
void ImportOptions::validate() const {
    require(mode == ImportMode::add || mode == ImportMode::copy || mode == ImportMode::move);
    require(naming.size() <= 230 && !naming.empty() && startedAt >= 0 &&
            startedAt <= 253402300799LL);
    require(sourceRoots.size() <= 65536);
    for (const auto &path : {destination, backupDestination})
        require(luma::pathUtf8(path).size() <= 4096 &&
                luma::pathUtf8(path).find('\0') == std::string::npos);
    for (const auto &path : sourceRoots)
        require(!path.empty() && luma::pathUtf8(path).size() <= 4096 &&
                luma::pathUtf8(path).find('\0') == std::string::npos);
    if (mode != ImportMode::add)
        require(!destination.empty());
    auto example = naming;
    for (const auto &token : {"{name}", "{date}", "{seq}"}) {
        std::size_t pos = 0;
        while ((pos = example.find(token, pos)) != std::string::npos) {
            example.replace(pos, std::char_traits<char>::length(token), "x");
            ++pos;
        }
    }
    require(example.find_first_of("{}") == std::string::npos);
    validFilename(example);
    metadata.validate();
    if (develop)
        develop->validate();
}
std::string encodeImportOptions(const ImportOptions &o) {
    o.validate();
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "LUMA_IMPORT 2 " << static_cast<int>(o.mode) << ' '
        << std::quoted(luma::pathUtf8(o.destination)) << ' ' << o.recursive << ' '
        << o.skipDuplicates << ' ' << o.dateFolders << ' ' << std::quoted(o.naming) << ' '
        << std::quoted(luma::pathUtf8(o.backupDestination)) << ' ' << o.startedAt << ' '
        << std::quoted(o.metadata.title) << ' ' << std::quoted(o.metadata.caption) << ' '
        << std::quoted(o.metadata.copyright) << ' ' << std::quoted(o.metadata.location) << ' '
        << o.metadata.rating << ' ' << o.metadata.flag << ' ' << std::quoted(o.metadata.label)
        << ' ';
    writeList(out, o.metadata.creators);
    writeList(out, o.metadata.keywords);
    out << std::quoted(o.develop ? encodeEditSnapshot(*o.develop) : std::string{}) << ' ';
    std::vector<std::string> roots;
    roots.reserve(o.sourceRoots.size());
    for (const auto &path : o.sourceRoots)
        roots.push_back(luma::pathUtf8(path));
    writeList(out, roots);
    out << '\n';
    return out.str();
}
ImportOptions decodeImportOptions(const std::string &document) {
    require(!document.empty() && document.size() <= maximumEditSnapshotBytes + 131072);
    std::istringstream in(document);
    in.imbue(std::locale::classic());
    std::string magic, destination, backup, develop;
    int version = 0, mode = -1;
    ImportOptions o;
    in >> magic >> version >> mode >> std::quoted(destination) >> o.recursive >> o.skipDuplicates >>
        o.dateFolders >> std::quoted(o.naming) >> std::quoted(backup) >> o.startedAt;
    require(magic == "LUMA_IMPORT" && (version == 1 || version == 2) && mode >= 0 && mode <= 2);
    o.mode = static_cast<ImportMode>(mode);
    o.destination = pathFromUtf8(destination);
    o.backupDestination = pathFromUtf8(backup);
    in >> std::quoted(o.metadata.title) >> std::quoted(o.metadata.caption) >>
        std::quoted(o.metadata.copyright) >> std::quoted(o.metadata.location) >>
        o.metadata.rating >> o.metadata.flag >> std::quoted(o.metadata.label);
    o.metadata.creators = readList(in);
    o.metadata.keywords = readList(in);
    in >> std::quoted(develop);
    require(static_cast<bool>(in));
    if (!develop.empty())
        o.develop = decodeEditSnapshot(develop);
    o.sourceRootsRecorded = version == 2;
    if (o.sourceRootsRecorded)
        for (auto &path : readList(in, 65536))
            o.sourceRoots.emplace_back(pathFromUtf8(path));
    in >> std::ws;
    require(in.eof());
    o.validate();
    return o;
}
std::filesystem::path importRelativePath(const std::filesystem::path &source,
                                         std::optional<std::int64_t> capturedAt, int index,
                                         const ImportOptions &options) {
    options.validate();
    require(index >= 0);
    const auto timestamp = capturedAt.value_or(options.startedAt);
    require(timestamp >= -62135596800LL && timestamp <= 253402300799LL);
    const std::time_t seconds = timestamp;
    std::tm utc{};
#ifdef _WIN32
    require(gmtime_s(&utc, &seconds) == 0);
#else
    require(gmtime_r(&seconds, &utc) != nullptr);
#endif
    std::ostringstream date, folder, sequence;
    date << std::put_time(&utc, "%Y-%m-%d");
    folder << std::put_time(&utc, "%Y/%m/%d");
    sequence << std::setw(6) << std::setfill('0') << (static_cast<std::int64_t>(index) + 1);
    // Expand only template tokens. Source filenames containing braces remain literal.
    std::string name;
    for (std::size_t i = 0; i < options.naming.size();) {
        if (options.naming.compare(i, 6, "{name}") == 0) {
            name += luma::pathUtf8(source.stem());
            i += 6;
        } else if (options.naming.compare(i, 6, "{date}") == 0) {
            name += date.str();
            i += 6;
        } else if (options.naming.compare(i, 5, "{seq}") == 0) {
            name += sequence.str();
            i += 5;
        } else
            name += options.naming[i++];
    }
    name += luma::pathUtf8(source.extension());
    validFilename(name);
    const auto filename = pathFromUtf8(name);
    return options.dateFolders ? std::filesystem::path(folder.str()) / filename : filename;
}
DescriptiveMetadata importMetadata(DescriptiveMetadata original,
                                   const DescriptiveMetadata &preset) {
    preset.validate();
    if (!preset.title.empty())
        original.title = preset.title;
    if (!preset.caption.empty())
        original.caption = preset.caption;
    if (!preset.copyright.empty())
        original.copyright = preset.copyright;
    if (!preset.location.empty())
        original.location = preset.location;
    if (!preset.creators.empty())
        original.creators = preset.creators;
    for (const auto &keyword : preset.keywords)
        if (std::find(original.keywords.begin(), original.keywords.end(), keyword) ==
            original.keywords.end())
            original.keywords.push_back(keyword);
    original.validate();
    return original;
}
} // namespace luma
