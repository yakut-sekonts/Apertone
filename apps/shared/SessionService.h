#pragma once
#include "core/catalog/Catalog.h"
#include "core/color/RawColorCalibration.h"
#include "core/export/ExportOptions.h"
#include "core/image/FloatImage.h"
#include "core/jobs/Cancellation.h"
#include <functional>
#include <memory>

namespace luma {
struct PhotoSession {
    AssetRecord asset;
    std::shared_ptr<const FloatImage> preview;
    RawColorCalibration calibration;
};
class SessionService {
  public:
    explicit SessionService(std::filesystem::path catalog) : catalogPath_(std::move(catalog)) {}
    PhotoSession open(const std::filesystem::path &source, luma::CancellationToken stop = {},
                      const std::function<void(double)> &progress = {}) const;
    std::optional<PhotoSession> restore(luma::CancellationToken stop = {},
                                        const std::function<void(double)> &progress = {}) const;
    void save(const std::string &assetID, double exposure) const;
    void save(const std::string &assetID, EditDocument edits) const;
    void exportPhoto(const AssetRecord &asset, EditDocument edits,
                     const std::filesystem::path &destination, ExportOptions options,
                     CancellationToken stop = {},
                     const std::function<void(double)> &progress = {}) const;
    void verify() const;

  private:
    std::filesystem::path catalogPath_;
};
} // namespace luma
