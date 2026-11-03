#include "apps/shared/DetailService.h"
#include "apps/shared/DerivedImage.h"
#include "apps/shared/DerivedProvenance.h"
#include "apps/shared/PhotoDecoder.h"
#include "core/catalog/Catalog.h"
#include "core/export/DetailResolution.h"
#include "core/export/LinearOutput.h"
#include "platform/macos/FileHasher.h"
#include <cmath>
#include <iomanip>
#include <sstream>

namespace luma {
namespace {
struct OperationContract {
    const char *name;
    const char *algorithm;
};

OperationContract contract(DetailDerivedOperation operation) {
    switch (operation) {
    case DetailDerivedOperation::rawDetails:
        return {"Raw Details", "edge-aware-microcontrast"};
    case DetailDerivedOperation::detail2x:
        return {"2x Detail", "edge-aware-microcontrast+lanczos3-2x"};
    case DetailDerivedOperation::lensBlurRecovery:
        return {"Lens Blur Recovery", "bounded-first-order-gaussian-inverse"};
    case DetailDerivedOperation::colorMoireCorrection:
        return {"Color Moire Correction", "luma-guided-chroma-oscillation-suppression"};
    }
    throw std::runtime_error("Unknown Detail operation.");
}
} // namespace

std::filesystem::path DetailService::create(const std::string &id, DetailDerivedOperation operation,
                                            double amount, CancellationToken stop,
                                            const std::function<void(double)> &progress) const {
    if (!std::isfinite(amount) || amount <= 0 || amount > 1)
        throw std::runtime_error("Detail amount must be between 0 and 1.");
    Catalog catalog(catalog_);
    const auto source = catalog.assetSnapshot(id);
    if (!source)
        throw std::runtime_error("Detail source is missing from the catalog.");
    const auto path = std::filesystem::canonical(source->asset.path);
    const auto size = std::filesystem::file_size(path);
    const auto time = std::filesystem::last_write_time(path);
    if (macos::sha256File(path, stop) != source->asset.checksum)
        throw std::runtime_error("Original changed; Detail operation refused.");
    const auto decoded = PhotoDecoder().decode(path, stop, [&](double value) {
        if (progress)
            progress(.2 * value);
    });
    if (operation == DetailDerivedOperation::rawDetails &&
        decoded.diagnostics.source != DecodeSource::integerBayerRaw)
        throw std::runtime_error("Raw Details requires an original Bayer RAW photo.");
    if (size != std::filesystem::file_size(path) ||
        time != std::filesystem::last_write_time(path) ||
        macos::sha256File(path, stop) != source->asset.checksum)
        throw std::runtime_error("Original changed during Detail processing.");
    auto output = renderLinearOutput(decoded, source->asset.edits, stop, [&](double value) {
        if (progress)
            progress(.2 + .3 * value);
    });
    const auto inputContract = linearFloatContract(output.width, output.height, true) +
                               ";develop-document=v" + std::to_string(EditDocument::schemaVersion);
    const bool detail2x = operation == DetailDerivedOperation::detail2x;
    switch (operation) {
    case DetailDerivedOperation::rawDetails:
    case DetailDerivedOperation::detail2x:
        enhanceRawDetails(output, amount, stop, [&](double value) {
            if (progress)
                progress(.5 + .15 * value);
        });
        break;
    case DetailDerivedOperation::lensBlurRecovery:
        recoverLensBlur(output, amount, stop, [&](double value) {
            if (progress)
                progress(.5 + .35 * value);
        });
        break;
    case DetailDerivedOperation::colorMoireCorrection:
        suppressColorMoire(output, amount, stop, [&](double value) {
            if (progress)
                progress(.5 + .35 * value);
        });
        break;
    }
    if (detail2x) {
        output = upscaleDetail2x(std::move(output), stop, [&](double value) {
            if (progress)
                progress(.65 + .2 * value);
        });
    }
    std::ostringstream amountText;
    amountText.imbue(std::locale::classic());
    amountText << std::fixed << std::setprecision(4) << amount;
    const auto operationContract = contract(operation);
    const auto metadata = encodeDerivedProvenance(
        {.operation = operationContract.name,
         .provider = "org.apertone.local.detail",
         .providerVersion = "2",
         .algorithm = operationContract.algorithm,
         .algorithmVersion = "1",
         .inputContract = inputContract,
         .outputContract = linearFloatContract(output.width, output.height, true),
         .model = {},
         .modelVersion = {},
         .deterministic = true,
         .local = true,
         .sources = {{source->asset.id, source->asset.checksum, source->revision}},
         .parameters = {{"amount", amountText.str()}, {"develop", "rendered"}}});
    return createDerivedImage(catalog_, output, metadata, stop, [&](double value) {
        if (progress)
            progress(.85 + .15 * value);
    });
}
} // namespace luma
