#pragma once
#include "core/catalog/Catalog.h"
#include "core/image/FloatImage.h"
#include <functional>
#include <memory>
#include <stop_token>

namespace luma {
struct PhotoSession {
    AssetRecord asset;
    std::shared_ptr<const FloatImage> preview;
};
class SessionService {
  public:
    explicit SessionService(std::filesystem::path catalog) : catalogPath_(std::move(catalog)) {}
    PhotoSession open(const std::filesystem::path &source, std::stop_token stop = {},
                      const std::function<void(double)> &progress = {}) const;
    std::optional<PhotoSession> restore(std::stop_token stop = {},
                                        const std::function<void(double)> &progress = {}) const;
    void save(const std::string &assetID, double exposure) const;
    void verify() const;

  private:
    std::filesystem::path catalogPath_;
};
} // namespace luma
