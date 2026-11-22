#pragma once

#include "core/export/LinearOutput.h"

namespace luma::windows {
void writeUltraHdrJpeg(const LinearOutput &image, const std::filesystem::path &path, int quality,
                       CancellationToken stop = {});
} // namespace luma::windows
