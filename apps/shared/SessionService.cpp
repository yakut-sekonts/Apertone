#include "apps/shared/SessionService.h"
#include "core/image/Bayer.h"
#include "core/raw/RawDecoder.h"
#include "core/render/CpuRenderer.h"
#include "platform/macos/FileHasher.h"

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
    return {std::move(asset), std::move(preview)};
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
void SessionService::verify() const {
    Catalog(catalogPath_).verify();
}
} // namespace luma
