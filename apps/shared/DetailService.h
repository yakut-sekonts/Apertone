#pragma once
#include "core/jobs/Cancellation.h"
#include <filesystem>
#include <functional>
#include <string>

namespace luma {
enum class DetailDerivedOperation { rawDetails, detail2x };

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
