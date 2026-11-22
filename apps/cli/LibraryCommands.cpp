#include "apps/cli/LibraryCommands.h"
#include "apps/cli/Json.h"
#include "apps/shared/LibraryService.h"
#include "apps/shared/LightroomCatalog.h"
#include "core/io/Path.h"
#include <charconv>
#include <iostream>
#ifdef _WIN32
#include "platform/windows/TetherCapture.h"
#include <QString>
#include <future>
#endif
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
#ifdef _WIN32
    if (args == std::vector<std::string>{"camera", "list"}) {
        const auto devices =
            std::async(std::launch::async, [] { return windows::tetherCameras(); }).get();
        std::cout << '[';
        for (std::size_t i = 0; i < devices.size(); ++i) {
            if (i)
                std::cout << ',';
            const auto &device = devices[i];
            std::cout << "{\"id\":" << quoted(QString::fromStdWString(device.id).toStdString())
                      << ",\"name\":" << quoted(QString::fromStdWString(device.name).toStdString())
                      << '}';
        }
        std::cout << "]\n";
        return true;
    }
    if (args[0] == "camera" && args.size() == 4 && args[1] == "capture") {
        auto files = std::async(std::launch::async, [&] {
                         return windows::captureTetheredPhotos(
                             QString::fromStdString(args[2]).toStdWString(),
                             std::filesystem::absolute(pathFromUtf8(args[3])));
                     }).get();
        summary(LibraryService(catalog).import(files, {}));
        std::cout << '\n';
        return true;
    }
#endif
    if (args[0] == "copy" && args.size() >= 3) {
        Catalog db(catalog);
        if (args[1] == "create" && args.size() == 4)
            std::cout << "{\"id\":" << quoted(db.createVirtualCopy(args[2], args[3])) << "}\n";
        else if (args[1] == "rename" && args.size() == 4)
            db.renameVirtualCopy(args[2], args[3]);
        else if (args[1] == "delete" && args.size() == 3)
            db.deleteVirtualCopy(args[2]);
        else
            throw std::runtime_error(
                "Use copy create ID NAME, copy rename ID NAME, or copy delete ID.");
        return true;
    }
    if (args[0] == "lightroom" && args.size() >= 3 &&
        (args[1] == "inspect" || args[1] == "import")) {
        const bool apply = args[1] == "import";
        if (args.size() != 3 && !(apply && args.size() == 5 && args[3] == "--profiles"))
            throw std::runtime_error("Use lightroom import SOURCE [--profiles FOLDER].");
        const auto source = pathFromUtf8(args[2]);
        if (apply && catalog.empty())
            throw std::runtime_error("Specify a destination Apertone catalog with --catalog.");
        const auto profileFolder =
            args.size() == 5 ? pathFromUtf8(args[4]) : std::filesystem::path{};
        const auto report = apply
                                ? importLightroomCatalog(source, catalog, {}, {}, {}, profileFolder)
                                : inspectLightroomCatalog(source);
        std::cout << "{\"photos\":" << report.photos << ",\"available\":" << report.available
                  << ",\"missing\":" << report.missing << ",\"unsupported\":" << report.unsupported
                  << ",\"virtualCopies\":" << report.virtualCopies
                  << ",\"manualCollections\":" << report.manualCollections
                  << ",\"smartCollections\":" << report.smartCollections
                  << ",\"smartRulesMapped\":" << report.smartRulesMapped
                  << ",\"mappedEdits\":" << report.mappedEdits
                  << ",\"partialEdits\":" << report.partialEdits
                  << ",\"unmappedEdits\":" << report.unmappedEdits
                  << ",\"masksMapped\":" << report.masksMapped
                  << ",\"masksSkipped\":" << report.masksSkipped
                  << ",\"profilesMapped\":" << report.profilesMapped
                  << ",\"profilesSkipped\":" << report.profilesSkipped
                  << ",\"historyMapped\":" << report.historyMapped
                  << ",\"historyPartial\":" << report.historyPartial
                  << ",\"historySkipped\":" << report.historySkipped
                  << ",\"historySteps\":" << report.imported.historySteps
                  << ",\"snapshotsMapped\":" << report.snapshotsMapped
                  << ",\"snapshotsPartial\":" << report.snapshotsPartial << ",\"snapshotsSkipped\":"
                  << report.snapshotsSkipped + report.imported.snapshotsWithoutRoom
                  << ",\"snapshots\":" << report.imported.snapshots
                  << ",\"unreadable\":" << report.unreadable
                  << ",\"added\":" << report.imported.added
                  << ",\"reused\":" << report.imported.reused
                  << ",\"versions\":" << report.imported.versions
                  << ",\"copies\":" << report.imported.copies
                  << ",\"unchanged\":" << report.imported.unchanged
                  << ",\"collections\":" << report.imported.collections
                  << ",\"collectionSets\":" << report.imported.collectionSets
                  << ",\"smartCollectionsImported\":" << report.imported.smartCollections
                  << ",\"fingerprint\":" << quoted(report.fingerprint) << "}\n";
        return true;
    }
    if (args[0] == "import" && args.size() >= 3) {
        ImportOptions options;
        std::size_t start = 2;
        if (args[1] == "copy" || args[1] == "move") {
            options.mode = args[1] == "copy" ? ImportMode::copy : ImportMode::move;
            options.destination = pathFromUtf8(args[2]);
            start = 3;
        } else if (args[1] != "add")
            throw std::runtime_error("Import mode must be add, copy or move.");
        std::vector<std::filesystem::path> sources;
        for (std::size_t i = start; i < args.size(); ++i) {
            if (args[i] == "--no-recursive")
                options.recursive = false;
            else if (args[i] == "--allow-duplicates")
                options.skipDuplicates = false;
            else
                sources.emplace_back(pathFromUtf8(args[i]));
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
            else if (name == "--stack")
                filter.stackID = value;
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
            std::cout << "{\"id\":" << quoted(r.id)
                      << ",\"path\":" << quoted(luma::pathUtf8(r.path))
                      << ",\"filename\":" << quoted(r.filename)
                      << ",\"checksum\":" << quoted(r.checksum) << ",\"rating\":" << r.rating
                      << ",\"flag\":" << r.flag << ",\"label\":" << quoted(r.label)
                      << ",\"copyName\":" << quoted(r.copyName)
                      << ",\"originalAsset\":" << quoted(r.originalAssetID)
                      << ",\"stack\":" << quoted(r.stackID) << ",\"stackSize\":" << r.stackSize
                      << ",\"stackCover\":" << (r.stackCover ? "true" : "false") << '}';
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
                      << ",\"count\":" << rows[i].count << ",\"set\":" << quoted(rows[i].setID)
                      << '}';
        }
        std::cout << "]\n";
        return true;
    }
    if (args == std::vector<std::string>{"sets"}) {
        const auto rows = Catalog(catalog).collectionSets();
        std::cout << '[';
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (i)
                std::cout << ',';
            const auto &row = rows[i];
            std::cout << "{\"id\":" << quoted(row.id) << ",\"name\":" << quoted(row.name)
                      << ",\"parent\":" << quoted(row.parentID) << ",\"path\":" << quoted(row.path)
                      << '}';
        }
        std::cout << "]\n";
        return true;
    }
    if (args[0] == "set" && args.size() >= 3) {
        Catalog db(catalog);
        if (args[1] == "create" && args.size() <= 4) {
            const auto id = db.createCollectionSet(args[2], args.size() == 4 ? args[3] : "");
            std::cout << "{\"id\":" << quoted(id) << "}\n";
            return true;
        }
        if (args[1] == "update" && (args.size() == 4 || args.size() == 5))
            db.updateCollectionSet(args[2], args[3], args.size() == 5 ? args[4] : "");
        else if (args[1] == "delete" && args.size() == 3)
            db.deleteCollectionSet(args[2]);
        else
            throw std::runtime_error("Invalid collection set command.");
        std::cout << "{\"saved\":true}\n";
        return true;
    }
    if (args == std::vector<std::string>{"keywords"}) {
        const auto rows = Catalog(catalog).catalogKeywords();
        std::cout << '[';
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (i)
                std::cout << ',';
            const auto &row = rows[i];
            std::cout << "{\"path\":" << quoted(row.path) << ",\"photos\":" << row.photos
                      << ",\"aliases\":[";
            for (std::size_t alias = 0; alias < row.aliases.size(); ++alias) {
                if (alias)
                    std::cout << ',';
                std::cout << quoted(row.aliases[alias]);
            }
            std::cout << "]}";
        }
        std::cout << "]\n";
        return true;
    }
    if (args[0] == "keyword" && args.size() >= 3) {
        Catalog db(catalog);
        if (args[1] == "create")
            db.saveCatalogKeyword({}, args[2], {args.begin() + 3, args.end()});
        else if (args[1] == "update" && args.size() >= 4)
            db.saveCatalogKeyword(args[2], args[3], {args.begin() + 4, args.end()});
        else if (args[1] == "delete" && args.size() == 3)
            db.deleteCatalogKeyword(args[2]);
        else if ((args[1] == "add" || args[1] == "remove") && args.size() >= 4)
            db.keywordMembership(args[2], {args.begin() + 3, args.end()}, args[1] == "add");
        else
            throw std::runtime_error("Invalid keyword command.");
        std::cout << "{\"saved\":true}\n";
        return true;
    }
    if (args[0] == "stack" && args.size() >= 3) {
        Catalog db(catalog);
        if (args[1] == "create" && args.size() >= 4) {
            std::cout << "{\"id\":" << quoted(db.createPhotoStack({args.begin() + 2, args.end()}))
                      << "}\n";
        } else if (args[1] == "cover" && args.size() == 3) {
            db.setPhotoStackCover(args[2]);
            std::cout << "{\"saved\":true}\n";
        } else if (args[1] == "remove" && args.size() == 3) {
            db.removePhotoStack(args[2]);
            std::cout << "{\"saved\":true}\n";
        } else {
            throw std::runtime_error("Invalid stack command.");
        }
        return true;
    }
    if (args[0] == "collection" && args.size() >= 3) {
        Catalog db(catalog);
        if (args[1] == "create" && args.size() <= 4) {
            const auto id = db.createCollection(args[2], args.size() == 4 ? args[3] : "");
            std::cout << "{\"id\":" << quoted(id) << "}\n";
            return true;
        }
        if (args[1] == "rename" && args.size() == 4)
            db.renameCollection(args[2], args[3]);
        else if (args[1] == "move" && args.size() <= 4)
            db.moveCollection(args[2], args.size() == 4 ? args[3] : "");
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
