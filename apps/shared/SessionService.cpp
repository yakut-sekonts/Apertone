#include "apps/shared/SessionService.h"
#include "core/image/Bayer.h"
#include "core/raw/RawDecoder.h"
#include "core/render/CpuRenderer.h"
#include "platform/macos/FileHasher.h"
#include "platform/macos/ImageExporter.h"
#include <algorithm>
#include <cctype>

namespace luma {
PhotoSession SessionService::open(const std::filesystem::path &source, luma::CancellationToken stop,
                                  const std::function<void(double)> &progress) const {
    const auto path = std::filesystem::canonical(source);
    const auto originalSize = std::filesystem::file_size(path);
    const auto originalTime = std::filesystem::last_write_time(path);
    if (originalSize > 512'000'000) {
        throw std::runtime_error("RAW exceeds the 512 MB input budget.");
    }
    const auto checksum = macos::sha256File(path, stop);
    auto decoded = RawDecoder().decode(path, stop, progress);
    auto preview = std::make_shared<FloatImage>(standardPreview(decoded.image, 2560, stop));
    checkCancellation(stop);
    if (originalSize != std::filesystem::file_size(path) ||
        originalTime != std::filesystem::last_write_time(path)) {
        throw std::runtime_error(
            "Original changed while it was being decoded. Import was not committed.");
    }
    Catalog catalog(catalogPath_);
    auto asset = catalog.addAndSelect(path, checksum, decoded.metadata);
    const auto scale = std::min(static_cast<double>(preview->width) / decoded.image.width,
                                static_cast<double>(preview->height) / decoded.image.height);
    return {std::move(asset), std::move(preview), decoded.calibration, scale};
}
std::optional<PhotoSession>
SessionService::restore(luma::CancellationToken stop,
                        const std::function<void(double)> &progress) const {
    Catalog catalog(catalogPath_);
    auto active = catalog.activeAsset();
    if (!active) {
        return std::nullopt;
    }
    return open(active->path, stop, progress);
}
void SessionService::save(const std::string &assetID, double exposure) const {
    Catalog catalog(catalogPath_);
    catalog.saveExposure(assetID, exposure);
}
void SessionService::save(const std::string &assetID, EditDocument edits) const {
    Catalog(catalogPath_).saveEdits(assetID, edits);
}
void SessionService::exportPhoto(const AssetRecord &asset, EditDocument edits,
                                 const std::filesystem::path &destination, ExportOptions options,
                                 CancellationToken stop,
                                 const std::function<void(double)> &progress) const {
    edits.validate();
    options.validate();
    const auto source = std::filesystem::canonical(asset.path);
    const auto target = std::filesystem::weakly_canonical(destination);
    const auto catalog = std::filesystem::weakly_canonical(catalogPath_);
    if (target == catalog || (std::filesystem::exists(target) && std::filesystem::exists(catalog) &&
                              std::filesystem::equivalent(target, catalog)))
        throw std::runtime_error("Export cannot replace the catalog.");
    if (source == target ||
        (std::filesystem::exists(target) && std::filesystem::equivalent(source, target)))
        throw std::runtime_error("Export cannot replace the original RAW.");
    if (Catalog(catalogPath_).originalAtPath(target, stop))
        throw std::runtime_error("Export cannot replace a catalog original.");
    for (const auto &name :
         {catalog.filename().string() + "-wal", catalog.filename().string() + "-shm",
          std::string("presets.json"), std::string("export-settings.json")}) {
        const auto state = catalog.parent_path() / name;
        if (target == state || (std::filesystem::exists(target) && std::filesystem::exists(state) &&
                                std::filesystem::equivalent(target, state)))
            throw std::runtime_error("Export cannot replace application state.");
    }
    auto extension = destination.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    const bool validExtension =
        options.format == ExportFormat::jpeg  ? (extension == ".jpg" || extension == ".jpeg")
        : options.format == ExportFormat::png ? extension == ".png"
        : options.format == ExportFormat::dng ? extension == ".dng"
                                              : (extension == ".tif" || extension == ".tiff");
    if (!validExtension)
        throw std::runtime_error("Destination extension does not match export format.");
    if (!options.overwrite && std::filesystem::exists(target))
        throw std::runtime_error("Destination already exists.");
    const auto initialTime = std::filesystem::last_write_time(source);
    const auto initialSize = std::filesystem::file_size(source);
    if (macos::sha256File(source, stop) != asset.checksum)
        throw std::runtime_error("Original changed; export refused.");
    auto decoded = RawDecoder().decode(source, stop, [&](double value) {
        if (progress)
            progress(value * 0.5);
    });
    if (initialTime != std::filesystem::last_write_time(source) ||
        initialSize != std::filesystem::file_size(source))
        throw std::runtime_error("Original changed during export decoding.");
    const auto guard = [&](const std::function<void()> &install) {
        Catalog(catalogPath_).installExport(target, stop, install);
    };
    macos::writeImage(decoded, edits, destination, options, stop, progress, source, guard);
}
void SessionService::verify() const {
    Catalog(catalogPath_).verify();
}
} // namespace luma
