#include "apps/shared/DetailService.h"
#include "apps/shared/PhotoDecoder.h"
#include "apps/shared/SessionService.h"
#include "core/catalog/Catalog.h"
#include "core/catalog/Identity.h"
#include "core/image/DecodeDiagnostics.h"
#include "platform/macos/FileHasher.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}

struct TemporaryDirectory {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() / ("apertone-detail-service-" + luma::newID());
    TemporaryDirectory() {
        std::filesystem::create_directories(path);
    }
    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};
} // namespace

int main(int argc, char **argv) {
    try {
        require(argc == 2, "Detail service test requires one raster fixture.");
        TemporaryDirectory folder;
        const auto source = std::filesystem::canonical(argv[1]);
        const auto sourceHash = luma::macos::sha256File(source);
        const auto catalogPath = folder.path / "catalog.sqlite";
        luma::SessionService sessions(catalogPath);
        const auto session = sessions.open(source);
        sessions.save(session.asset.id, 1.0);

        luma::DetailService service(catalogPath);
        bool rawRejected = false;
        try {
            (void)service.create(session.asset.id, luma::DetailDerivedOperation::rawDetails, .5);
        } catch (const std::exception &) {
            rawRejected = true;
        }
        require(rawRejected, "Raw Details accepted a raster original.");
        luma::Catalog before(catalogPath);
        require(before.libraryCount() == 1 && before.pendingDerived().empty(),
                "Rejected Raw Details left a derived journal or catalog asset.");

        const auto created =
            service.create(session.asset.id, luma::DetailDerivedOperation::detail2x, .5);
        require(std::filesystem::is_regular_file(created),
                "2x Detail did not publish its managed DNG.");
        const auto result = luma::PhotoDecoder().decode(created);
        require(result.diagnostics.source == luma::DecodeSource::renderedLinearDNG &&
                    result.image.width == session.asset.metadata.width * 2 &&
                    result.image.height == session.asset.metadata.height * 2,
                "2x Detail DNG has the wrong decoder contract or dimensions.");
        std::ifstream file(created, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(file)), {});
        require(bytes.find("luma:ProviderVersion=\"2\"") != std::string::npos &&
                    bytes.find("luma:AlgorithmVersion=\"1\"") != std::string::npos &&
                    bytes.find("luma:InputContract=") != std::string::npos &&
                    bytes.find("luma:OutputContract=") != std::string::npos &&
                    bytes.find("edge-aware-microcontrast+lanczos3-2x") != std::string::npos,
                "2x Detail DNG lost its explicit processing provenance.");
        const auto deblurred =
            service.create(session.asset.id, luma::DetailDerivedOperation::lensBlurRecovery, .5);
        const auto corrected = service.create(
            session.asset.id, luma::DetailDerivedOperation::colorMoireCorrection, .5);
        require(std::filesystem::is_regular_file(deblurred) &&
                    std::filesystem::is_regular_file(corrected),
                "Deterministic Detail corrections did not publish managed DNG files.");
        std::ifstream deblurFile(deblurred, std::ios::binary),
            moireFile(corrected, std::ios::binary);
        const std::string deblurBytes((std::istreambuf_iterator<char>(deblurFile)), {});
        const std::string moireBytes((std::istreambuf_iterator<char>(moireFile)), {});
        require(deblurBytes.find("bounded-first-order-gaussian-inverse") != std::string::npos &&
                    moireBytes.find("luma-guided-chroma-oscillation-suppression") !=
                        std::string::npos,
                "Detail corrections lost their explicit algorithm provenance.");
        luma::Catalog after(catalogPath);
        require(after.libraryCount() == 4 && after.pendingDerived().empty(),
                "Detail corrections did not atomically register their managed DNG files.");
        require(luma::macos::sha256File(source) == sourceHash,
                "Detail processing changed the original file.");
        after.verify();
        std::cout
            << "Raw rejection, deterministic Detail DNG registration and provenance passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
