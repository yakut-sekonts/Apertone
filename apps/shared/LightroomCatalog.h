#pragma once

#include "core/jobs/Cancellation.h"
#include "core/library/LightroomMigration.h"
#include <functional>

namespace luma {
struct LightroomCatalogReport {
    std::string fingerprint;
    std::size_t photos = 0;
    std::size_t available = 0;
    std::size_t missing = 0;
    std::size_t unsupported = 0;
    std::size_t virtualCopies = 0;
    std::size_t manualCollections = 0;
    std::size_t smartCollections = 0;
    std::size_t mappedEdits = 0;
    std::size_t partialEdits = 0;
    std::size_t unmappedEdits = 0;
    std::size_t unreadable = 0;
    std::vector<std::string> warnings;
    LightroomMigrationResult imported;
};

LightroomCatalogReport inspectLightroomCatalog(const std::filesystem::path &source,
                                               CancellationToken stop = {});
LightroomCatalogReport importLightroomCatalog(const std::filesystem::path &source,
                                              const std::filesystem::path &target,
                                              const std::string &expectedFingerprint,
                                              CancellationToken stop = {},
                                              const std::function<void(double)> &progress = {});
} // namespace luma
