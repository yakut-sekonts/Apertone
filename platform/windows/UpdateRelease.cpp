#include "platform/windows/UpdateRelease.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <stdexcept>

namespace luma::windows {
namespace {
QVersionNumber stableVersion(const QString &tag) {
    if (!tag.startsWith(u'v'))
        return {};
    const auto version = QVersionNumber::fromString(QStringView(tag).sliced(1));
    if (version.segmentCount() != 3 || tag != u'v' + version.toString())
        return {};
    return version;
}

std::optional<WindowsUpdate> windowsAsset(const QJsonArray &assets, const QString &tag,
                                          const QVersionNumber &version) {
    const auto name = QStringLiteral("Apertone-%1-windows-x64.zip").arg(version.toString());
    const auto expectedUrl =
        QStringLiteral("https://github.com/yakut-sekonts/Apertone/releases/download/%1/%2")
            .arg(tag, name);
    std::optional<WindowsUpdate> update;
    bool matched = false;
    for (const auto &value : assets) {
        const auto asset = value.toObject();
        const auto assetName = asset.value(QStringLiteral("name"));
        if (!assetName.isString())
            throw std::runtime_error("Invalid GitHub release asset");
        if (assetName.toString() != name)
            continue;
        const auto state = asset.value(QStringLiteral("state"));
        if (matched || !state.isString())
            throw std::runtime_error("Invalid GitHub Windows release asset");
        matched = true;
        if (state.toString() != QStringLiteral("uploaded"))
            continue;
        const auto url = asset.value(QStringLiteral("browser_download_url")).toString();
        const auto digest = asset.value(QStringLiteral("digest")).toString();
        const auto size = asset.value(QStringLiteral("size")).toInteger();
        static const QRegularExpression sha256(QStringLiteral("\\Asha256:[a-fA-F0-9]{64}\\z"));
        constexpr qint64 maximumArchiveSize = 2LL * 1024 * 1024 * 1024;
        if (url != expectedUrl || !sha256.match(digest).hasMatch() || size <= 0 ||
            size > maximumArchiveSize)
            throw std::runtime_error("Invalid GitHub Windows release metadata");
        update = WindowsUpdate{version, QUrl(url), QByteArray::fromHex(digest.sliced(7).toLatin1()),
                               size};
    }
    return update;
}
} // namespace

std::optional<WindowsUpdate> latestWindowsUpdate(const QByteArray &json,
                                                 const QVersionNumber &installed) {
    const auto document = QJsonDocument::fromJson(json);
    if (!document.isArray())
        throw std::runtime_error("Invalid GitHub release list");
    std::optional<WindowsUpdate> latest;
    const auto current = installed.normalized();
    for (const auto &entry : document.array()) {
        const auto release = entry.toObject();
        const auto tag = release.value(QStringLiteral("tag_name"));
        const auto draft = release.value(QStringLiteral("draft"));
        const auto prerelease = release.value(QStringLiteral("prerelease"));
        if (!tag.isString() || !draft.isBool() || !prerelease.isBool())
            throw std::runtime_error("Invalid GitHub release metadata");
        if (draft.toBool() || prerelease.toBool())
            continue;
        const auto version = stableVersion(tag.toString());
        if (version.isNull() || version.normalized() <= current)
            continue;
        const auto assets = release.value(QStringLiteral("assets"));
        if (!assets.isArray())
            throw std::runtime_error("Invalid GitHub release assets");
        const auto update = windowsAsset(assets.toArray(), tag.toString(), version);
        if (!update)
            continue;
        if (latest && latest->version == version)
            throw std::runtime_error("Duplicate GitHub Windows release version");
        if (!latest || version > latest->version)
            latest = update;
    }
    return latest;
}

bool trustedUpdateRedirect(const QUrl &url) {
    if (!url.isValid() || url.scheme() != QStringLiteral("https") || url.port(443) != 443 ||
        url.authority(QUrl::FullyEncoded).contains(u'@') || url.hasFragment())
        return false;
    return url.host() == QStringLiteral("release-assets.githubusercontent.com") ||
           url.host() == QStringLiteral("objects.githubusercontent.com");
}
} // namespace luma::windows
