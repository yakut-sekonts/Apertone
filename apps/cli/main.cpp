#include "apps/shared/SessionService.h"
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
std::string quoted(const std::string &value) {
    std::ostringstream output;
    output << '"';
    for (const unsigned char c : value) {
        if (c == '"' || c == '\\') {
            output << '\\' << c;
        } else if (c < 32) {
            output << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c)
                   << std::dec;
        } else {
            output << c;
        }
    }
    output << '"';
    return output.str();
}
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
              << ",\"checksum\":" << quoted(asset.checksum) << "}\n";
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
            std::cout << "Luma M0\nUsage: luma open RAW --catalog FILE\n"
                      << "       luma exposure EV --catalog FILE\n"
                      << "       luma restore --catalog FILE\n"
                      << "       luma catalog verify --catalog FILE\n";
            return 0;
        }
        if (catalog.empty()) {
            throw std::runtime_error("Specify --catalog FILE explicitly.");
        }
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
