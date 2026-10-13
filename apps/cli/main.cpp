#include "apps/cli/Json.h"
#include "apps/cli/LibraryCommands.h"
#include "apps/shared/SessionService.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
using luma::cli::quoted;
void printSession(const luma::PhotoSession &session) {
    const auto &asset = session.asset;
    auto number = [](std::optional<double> value) {
        return value ? std::to_string(*value) : std::string("null");
    };
    std::cout << "{\"asset\":" << quoted(asset.id)
              << ",\"camera\":" << quoted(asset.metadata.camera)
              << ",\"lens\":" << quoted(asset.metadata.lens)
              << ",\"iso\":" << number(asset.metadata.iso)
              << ",\"shutter\":" << number(asset.metadata.shutterSeconds)
              << ",\"aperture\":" << number(asset.metadata.aperture)
              << ",\"focalLength\":" << number(asset.metadata.focalLength)
              << ",\"width\":" << asset.metadata.width << ",\"height\":" << asset.metadata.height
              << ",\"exposure\":" << std::setprecision(17) << asset.edits.exposure
              << ",\"temperature\":" << asset.edits.temperature << ",\"tint\":" << asset.edits.tint
              << ",\"customWhiteBalance\":" << (asset.edits.customWhiteBalance ? "true" : "false")
              << ",\"contrast\":" << asset.edits.contrast
              << ",\"highlights\":" << asset.edits.highlights
              << ",\"shadows\":" << asset.edits.shadows << ",\"whites\":" << asset.edits.whites
              << ",\"blacks\":" << asset.edits.blacks << ",\"vibrance\":" << asset.edits.vibrance
              << ",\"saturation\":" << asset.edits.saturation
              << ",\"enabledCorrections\":" << asset.edits.enabledCorrections
              << ",\"editSchemaVersion\":" << luma::EditDocument::schemaVersion
              << ",\"curveParametric\":{\"shadows\":" << asset.edits.curveShadows
              << ",\"darks\":" << asset.edits.curveDarks
              << ",\"lights\":" << asset.edits.curveLights
              << ",\"highlights\":" << asset.edits.curveHighlights << "},\"curves\":{";
    for (std::size_t c = 0; c < luma::curveChannels.size(); ++c) {
        if (c != 0)
            std::cout << ',';
        std::cout << quoted(std::string(luma::curveChannels[c])) << ":[";
        const auto &points = asset.edits.curves[c].points;
        for (std::size_t i = 0; i < points.size(); ++i) {
            if (i != 0)
                std::cout << ',';
            std::cout << '[' << points[i].x << ',' << points[i].y << ']';
        }
        std::cout << ']';
    }
    std::cout << "},\"hsl\":{";
    for (std::size_t b = 0; b < luma::hslBands.size(); ++b) {
        if (b != 0)
            std::cout << ',';
        const auto &band = asset.edits.hsl[b];
        std::cout << quoted(std::string(luma::hslBands[b])) << ":{\"hue\":" << band.hue
                  << ",\"saturation\":" << band.saturation << ",\"luminance\":" << band.luminance
                  << '}';
    }
    std::cout << "},\"sharpening\":{\"amount\":" << asset.edits.sharpenAmount
              << ",\"radius\":" << asset.edits.sharpenRadius
              << ",\"detail\":" << asset.edits.sharpenDetail
              << ",\"masking\":" << asset.edits.sharpenMasking
              << "},\"checksum\":" << quoted(asset.checksum) << "}\n";
}
} // namespace

int main(int argc, char **argv) {
    try {
        std::filesystem::path catalog;
        std::vector<std::string> arguments;
        for (int i = 1; i < argc; ++i) {
            if (std::string(argv[i]) == "--catalog" && i + 1 < argc) {
                catalog = argv[++i];
            } else {
                arguments.emplace_back(argv[i]);
            }
        }
        if (arguments.empty() || arguments[0] == "--help") {
            std::cout << "Luma Library and Develop\nUsage: luma open RAW --catalog FILE\n"
                      << "       luma exposure EV --catalog FILE\n"
                      << "       luma restore --catalog FILE\n"
                      << "       luma develop PARAM=VALUE... --catalog FILE\n"
                      << "       luma curve rgb|red|green|blue X:Y X:Y... --catalog FILE\n"
                      << "       luma enable|disable CORRECTION --catalog FILE\n"
                      << "       luma wb as-shot --catalog FILE\n"
                      << "       luma export DEST [--quality N] [--overwrite] --catalog FILE\n"
                      << "       luma catalog verify --catalog FILE\n"
                      << "       luma import add SOURCES... | import copy DEST SOURCES... "
                         "--catalog FILE\n"
                      << "       luma library [--search TEXT] [--rating N] [--flag N] [--label "
                         "COLOR] [--collection ID] [--offset N] [--limit N] --catalog FILE\n"
                      << "       luma rate|flag|label VALUE IDS... --catalog FILE\n"
                      << "       luma collections | collection create NAME | collection rename ID "
                         "NAME --catalog FILE\n"
                      << "       luma collection add|remove ID ASSETS... | collection delete ID "
                         "--catalog FILE\n"
                      << "       luma recover --catalog FILE\n";
            return 0;
        }
        if (catalog.empty()) {
            throw std::runtime_error("Specify --catalog FILE explicitly.");
        }
        if (luma::cli::libraryCommand(catalog, arguments))
            return 0;
        luma::SessionService service(catalog);
        if (arguments[0] == "open" && arguments.size() == 2) {
            printSession(service.open(arguments[1]));
        } else if (arguments[0] == "restore" && arguments.size() == 1) {
            const auto session = service.restore();
            if (session) {
                printSession(*session);
            } else {
                std::cout << "{\"asset\":null}\n";
            }
        } else if (arguments[0] == "exposure" && arguments.size() == 2) {
            std::size_t consumed = 0;
            const double ev = std::stod(arguments[1], &consumed);
            if (consumed != arguments[1].size()) {
                throw std::runtime_error("Invalid exposure number.");
            }
            luma::Catalog connection(catalog);
            const auto asset = connection.activeAsset();
            if (!asset) {
                throw std::runtime_error("No active photo in this catalog.");
            }
            service.save(asset->id, ev);
            std::cout << "{\"saved\":true}\n";
        } else if (arguments[0] == "develop" && arguments.size() >= 2) {
            luma::Catalog connection(catalog);
            auto asset = connection.activeAsset();
            if (!asset)
                throw std::runtime_error("No active photo in this catalog.");
            for (std::size_t i = 1; i < arguments.size(); ++i) {
                const auto separator = arguments[i].find('=');
                if (separator == std::string::npos)
                    throw std::runtime_error("Expected PARAM=VALUE.");
                const auto name = arguments[i].substr(0, separator);
                const auto text = arguments[i].substr(separator + 1);
                std::size_t consumed = 0;
                const auto value = std::stod(text, &consumed);
                if (consumed != text.size())
                    throw std::runtime_error("Invalid edit number.");
                asset->edits.setParameter(name, value);
            }
            service.save(asset->id, asset->edits);
            std::cout << "{\"saved\":true}\n";
        } else if (arguments[0] == "curve" && arguments.size() >= 4) {
            luma::Catalog connection(catalog);
            auto asset = connection.activeAsset();
            if (!asset)
                throw std::runtime_error("No active photo in this catalog.");
            auto &points =
                asset->edits.curves[luma::EditDocument::curveChannel(arguments[1])].points;
            points.clear();
            for (std::size_t i = 2; i < arguments.size(); ++i) {
                const auto separator = arguments[i].find(':');
                if (separator == std::string::npos)
                    throw std::runtime_error("Expected curve points as X:Y.");
                auto number = [](const std::string &text) {
                    std::size_t count = 0;
                    const double value = std::stod(text, &count);
                    if (count != text.size())
                        throw std::runtime_error("Invalid curve coordinate.");
                    return value;
                };
                points.push_back({number(arguments[i].substr(0, separator)),
                                  number(arguments[i].substr(separator + 1))});
            }
            service.save(asset->id, asset->edits);
            std::cout << "{\"saved\":true}\n";
        } else if ((arguments[0] == "enable" || arguments[0] == "disable") &&
                   arguments.size() == 2) {
            luma::Catalog connection(catalog);
            auto asset = connection.activeAsset();
            if (!asset)
                throw std::runtime_error("No active photo in this catalog.");
            asset->edits.setEnabled(luma::EditDocument::correction(arguments[1]),
                                    arguments[0] == "enable");
            service.save(asset->id, asset->edits);
            std::cout << "{\"saved\":true}\n";
        } else if (arguments == std::vector<std::string>{"wb", "as-shot"}) {
            luma::Catalog connection(catalog);
            auto asset = connection.activeAsset();
            if (!asset)
                throw std::runtime_error("No active photo in this catalog.");
            asset->edits.customWhiteBalance = false;
            service.save(asset->id, asset->edits);
            std::cout << "{\"saved\":true}\n";
        } else if (arguments[0] == "export" && arguments.size() >= 2) {
            luma::Catalog connection(catalog);
            const auto asset = connection.activeAsset();
            if (!asset)
                throw std::runtime_error("No active photo in this catalog.");
            std::filesystem::path destination(arguments[1]);
            auto extension = destination.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            luma::ExportOptions options;
            if (extension == ".png")
                options.format = luma::ExportFormat::png;
            else if (extension == ".tif" || extension == ".tiff")
                options.format = luma::ExportFormat::tiff16;
            for (std::size_t i = 2; i < arguments.size(); ++i) {
                if (arguments[i] == "--overwrite")
                    options.overwrite = true;
                else if (arguments[i] == "--quality" && i + 1 < arguments.size()) {
                    std::size_t consumed = 0;
                    const auto text = arguments[++i];
                    options.quality = std::stoi(text, &consumed);
                    if (consumed != text.size())
                        throw std::runtime_error("Invalid JPEG quality.");
                } else
                    throw std::runtime_error("Unknown export option.");
            }
            service.exportPhoto(*asset, asset->edits, destination, options);
            std::cout << "{\"exported\":" << quoted(destination.string()) << "}\n";
        } else if (arguments == std::vector<std::string>{"catalog", "verify"}) {
            service.verify();
            std::cout << "{\"integrity\":\"ok\"}\n";
        } else {
            throw std::runtime_error("Unknown command. Use --help.");
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Luma: " << error.what() << '\n';
        return 1;
    }
}
