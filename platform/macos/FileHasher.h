#pragma once
#include "core/jobs/Cancellation.h"
#include <filesystem>
#include <string>

namespace luma::macos {
std::string sha256File(const std::filesystem::path &path, luma::CancellationToken stop = {});
} // namespace luma::macos
