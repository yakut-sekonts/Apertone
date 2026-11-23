#pragma once

#include <QByteArray>
#include <QUrl>
#include <QVersionNumber>
#include <optional>

namespace luma::windows {
struct WindowsUpdate {
    QVersionNumber version;
    QUrl url;
    QByteArray sha256;
    qint64 size = 0;
};

std::optional<WindowsUpdate> latestWindowsUpdate(const QByteArray &json,
                                                 const QVersionNumber &installed);
bool trustedUpdateRedirect(const QUrl &url);
} // namespace luma::windows
