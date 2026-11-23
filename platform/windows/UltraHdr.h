#pragma once

#include "core/export/LinearOutput.h"
#include <QByteArray>

namespace luma::windows {
void writeUltraHdrJpeg(const LinearOutput &image, const std::filesystem::path &path, int quality,
                       CancellationToken stop = {}, const QByteArray &metadata = {});
} // namespace luma::windows
