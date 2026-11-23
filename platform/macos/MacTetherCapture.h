#pragma once
#include "core/jobs/Cancellation.h"
#include <QString>
#include <QVariantList>
#include <filesystem>
#include <functional>
#include <vector>

namespace luma::macos {
using TetherResult = std::function<void(QVariantList, std::vector<std::filesystem::path>, QString)>;

bool cameraCaptureAvailable();
// Start on the GUI thread. An empty identifier enumerates compatible local cameras.
void requestCameraCapture(const QString &id, const QString &folder, CancellationToken stop,
                          TetherResult complete);
} // namespace luma::macos
