#include "apps/desktop/UpdateController.h"
#include "platform/macos/ApplicationAlert.h"
#include "platform/macos/CrashCapture.h"
#include <windows.h>

namespace luma::macos {
void showApplicationError(const std::string &title, const std::string &message) {
    const auto wideTitle = QString::fromUtf8(title).toStdWString();
    const auto wideMessage = QString::fromUtf8(message).toStdWString();
    MessageBoxW(nullptr, wideMessage.c_str(), wideTitle.c_str(), MB_OK | MB_ICONERROR);
}

bool enableCrashCapture(const std::filesystem::path &) noexcept { return false; }
bool formatCrashReport(const std::filesystem::path &) noexcept { return false; }
} // namespace luma::macos

std::unique_ptr<NativeUpdater> makeNativeUpdater(UpdateCallbacks) { return {}; }
