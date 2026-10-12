#pragma once
#include "core/catalog/Catalog.h"
#include "core/jobs/Cancellation.h"
#include <functional>

namespace luma {
class LibraryService {
  public:
    explicit LibraryService(std::filesystem::path catalog) : catalog_(std::move(catalog)) {}
    ImportSummary
    import(const std::vector<std::filesystem::path> &sources, ImportOptions options,
           CancellationToken stop = {},
           const std::function<void(double, const std::string &)> &progress = {}) const;
    std::vector<ImportSummary> recover(CancellationToken stop = {}) const;

  private:
    std::filesystem::path catalog_;
};
} // namespace luma
