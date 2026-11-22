#pragma once

#include "core/jobs/Cancellation.h"
#include <filesystem>

class QImage;

namespace luma::windows {
void writeZipTiff(const QImage &image, const std::filesystem::path &path, CancellationToken stop);
} // namespace luma::windows
