#pragma once
#include "core/jobs/Cancellation.h"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace luma {
enum class DetailDerivedOperation : std::uint8_t {
    rawDetails,
    detail2x,
    lensBlurRecovery,
    colorMoireCorrection
};

class DetailService {
  public:
    explicit DetailService(std::filesystem::path catalog) : catalog_(std::move(catalog)) {}
    std::filesystem::path create(const std::string &id, DetailDerivedOperation operation,
                                 double amount, CancellationToken stop = {},
                                 const std::function<void(double)> &progress = {}) const;

  private:
    std::filesystem::path catalog_;
};
} // namespace luma
