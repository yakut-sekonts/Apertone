#include "apps/cli/LibraryCommands.h"
#include "apps/cli/Json.h"
#include "apps/shared/LibraryService.h"
#include <charconv>
#include <iostream>
namespace luma::cli {
namespace {
int integer(const std::string &text) {
    int value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        throw std::runtime_error("Invalid integer.");
    return value;
}
void summary(const ImportSummary &value) {
    std::cout << "{\"session\":" << quoted(value.sessionID) << ",\"added\":" << value.added
              << ",\"duplicates\":" << value.duplicates << ",\"failed\":" << value.failed
              << ",\"cancelled\":" << value.cancelled << "}";
}
} // namespace
bool libraryCommand(const std::filesystem::path &catalog, const std::vector<std::string> &args) {
    if (args[0] == "import" && args.size() >= 3) {
        ImportOptions options;
        std::size_t start = 2;
        if (args[1] == "copy") {
            options.mode = ImportMode::copy;
            options.destination = args[2];
            start = 3;
        } else if (args[1] != "add")
            throw std::runtime_error("Import mode must be add or copy.");
        std::vector<std::filesystem::path> sources;
        for (std::size_t i = start; i < args.size(); ++i) {
            if (args[i] == "--no-recursive")
                options.recursive = false;
            else if (args[i] == "--allow-duplicates")
                options.skipDuplicates = false;
            else
                sources.emplace_back(args[i]);
        }
        summary(LibraryService(catalog).import(sources, options));
        std::cout << '\n';
        return true;
    }
    if (args == std::vector<std::string>{"recover"}) {
        const auto results = LibraryService(catalog).recover();
        std::cout << '[';
        for (std::size_t i = 0; i < results.size(); ++i) {
            if (i)
                std::cout << ',';
            summary(results[i]);
        }
        std::cout << "]\n";
        return true;
    }
    if (args[0] == "library") {
        LibraryFilter filter;
        int offset = 0, limit = 128;
        for (std::size_t i = 1; i < args.size(); i += 2) {
            if (i + 1 >= args.size())
                throw std::runtime_error("Library filter requires a value.");
            const auto &name = args[i], &value = args[i + 1];
            if (name == "--rating")
                filter.minimumRating = integer(value);
            else if (name == "--flag")
                filter.flag = integer(value);
            else if (name == "--label")
                filter.label = value;
            else if (name == "--search")
                filter.search = value;
            else if (name == "--collection")
                filter.collectionID = value;
            else if (name == "--offset")
                offset = integer(value);
            else if (name == "--limit")
                limit = integer(value);
            else
                throw std::runtime_error("Unknown Library filter.");
        }
        Catalog db(catalog);
        const auto rows = db.libraryPage(filter, offset, limit);
        std::cout << "{\"total\":" << db.libraryCount(filter) << ",\"items\":[";
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (i)
                std::cout << ',';
            const auto &r = rows[i];
            std::cout << "{\"id\":" << quoted(r.id) << ",\"path\":" << quoted(r.path.string())
                      << ",\"filename\":" << quoted(r.filename)
                      << ",\"checksum\":" << quoted(r.checksum) << ",\"rating\":" << r.rating
                      << ",\"flag\":" << r.flag << ",\"label\":" << quoted(r.label) << '}';
        }
        std::cout << "]}\n";
        return true;
    }
    if ((args[0] == "rate" || args[0] == "flag" || args[0] == "label") && args.size() >= 3) {
        const std::vector<std::string> ids(args.begin() + 2, args.end());
        Catalog db(catalog);
        if (args[0] == "rate")
            db.setRating(ids, integer(args[1]));
        else if (args[0] == "flag")
            db.setFlag(ids, integer(args[1]));
        else
            db.setLabel(ids, args[1]);
        std::cout << "{\"saved\":true}\n";
        return true;
    }
    if (args[0] == "collections" && args.size() == 1) {
        Catalog db(catalog);
        const auto rows = db.collections();
        std::cout << '[';
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (i)
                std::cout << ',';
            std::cout << "{\"id\":" << quoted(rows[i].id) << ",\"name\":" << quoted(rows[i].name)
                      << ",\"count\":" << rows[i].count << '}';
        }
        std::cout << "]\n";
        return true;
    }
    if (args[0] == "collection" && args.size() >= 3) {
        Catalog db(catalog);
        if (args[1] == "create" && args.size() == 3) {
            std::cout << "{\"id\":" << quoted(db.createCollection(args[2])) << "}\n";
            return true;
        }
        if (args[1] == "rename" && args.size() == 4)
            db.renameCollection(args[2], args[3]);
        else if (args[1] == "delete" && args.size() == 3)
            db.deleteCollection(args[2]);
        else if ((args[1] == "add" || args[1] == "remove") && args.size() >= 4)
            db.collectionMembership(args[2], {args.begin() + 3, args.end()}, args[1] == "add");
        else
            throw std::runtime_error("Invalid collection command.");
        std::cout << "{\"saved\":true}\n";
        return true;
    }
    return false;
}
} // namespace luma::cli
