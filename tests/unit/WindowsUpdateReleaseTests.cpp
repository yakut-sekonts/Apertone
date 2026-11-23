#include "platform/windows/UpdateRelease.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <stdexcept>

namespace {
using luma::windows::latestWindowsUpdate;
using luma::windows::trustedUpdateRedirect;

void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}

void rejects(const auto &operation) {
    try {
        operation();
    } catch (const std::runtime_error &) {
        return;
    }
    throw std::runtime_error("Invalid Windows release metadata was accepted.");
}

const auto digest =
    QStringLiteral("sha256:7011b3f1b4ec4e07976432e3ad2e4d090b3162387a1782c0c84ef9d918c4f774");

QJsonObject windowsZip(const QString &version) {
    const auto name = QStringLiteral("Apertone-%1-windows-x64.zip").arg(version);
    const auto url =
        QStringLiteral("https://github.com/yakut-sekonts/Apertone/releases/download/v%1/%2")
            .arg(version, name);
    return {{"name", name},
            {"state", "uploaded"},
            {"size", 375371972},
            {"digest", digest},
            {"browser_download_url", url}};
}

QJsonObject release(const QString &version, const QJsonArray &assets) {
    return {
        {"tag_name", u'v' + version}, {"draft", false}, {"prerelease", false}, {"assets", assets}};
}

QJsonObject windowsRelease(const QString &version) {
    return release(version, {windowsZip(version)});
}

QByteArray encode(const QJsonArray &releases) {
    return QJsonDocument(releases).toJson(QJsonDocument::Compact);
}

void selectNewestWindowsRelease() {
    auto draft = windowsRelease("7.0.0");
    draft["draft"] = true;
    auto preview = windowsRelease("6.0.0");
    preview["prerelease"] = true;
    auto pending = windowsZip("4.0.0");
    pending["state"] = "starter";
    pending["digest"] = QJsonValue::Null;
    auto macAsset = QJsonObject{{"name", "Apertone-3.0.0-macos-arm64.zip"}};
    const auto update = latestWindowsUpdate(
        encode({windowsRelease("2.10.0"), windowsRelease("2.6.1"), windowsRelease("2.5.0"),
                release("3.0.0", {macAsset}), release("4.0.0", {pending}),
                windowsRelease("5.0.0-rc.1"), preview, draft}),
        QVersionNumber(2, 5, 0));
    require(update && update->version == QVersionNumber(2, 10, 0),
            "Did not numerically select the newest stable Windows x64 release.");
    require(update->size == 375371972 &&
                update->sha256 == QByteArray::fromHex(digest.sliced(7).toLatin1()) &&
                update->sha256.size() == 32 &&
                update->url == QUrl(windowsZip("2.10.0")["browser_download_url"].toString()),
            "The selected asset lost its URL, size, or binary SHA-256.");
    require(!latestWindowsUpdate(encode({windowsRelease("2.5.0"), windowsRelease("2.4.9")}),
                                 QVersionNumber(2, 5)),
            "Reoffered an installed or older release.");
    require(!latestWindowsUpdate(encode({}), QVersionNumber(2, 5, 0)),
            "An empty release list should produce no update.");
}

void rejectInvalidMetadata() {
    const QVersionNumber installed(2, 5, 0);
    for (const auto &body : {"{", "{}", "[null]", R"([{"tag_name":"v3.0.0","draft":"false"}])"})
        rejects([&] { (void)latestWindowsUpdate(body, installed); });

    auto broken = windowsRelease("3.0.0");
    broken["assets"] = QJsonObject{};
    rejects([&] { (void)latestWindowsUpdate(encode({broken}), installed); });
    broken["assets"] = QJsonArray{windowsZip("3.0.0"), windowsZip("3.0.0")};
    rejects([&] { (void)latestWindowsUpdate(encode({broken}), installed); });

    const auto invalidAsset = [&](const QString &field, const QJsonValue &value) {
        auto invalid = windowsZip("3.0.0");
        invalid[field] = value;
        rejects(
            [&] { (void)latestWindowsUpdate(encode({release("3.0.0", {invalid})}), installed); });
    };
    invalidAsset("browser_download_url", "https://github.com/someone-else/Apertone/releases/"
                                         "download/v3.0.0/Apertone-3.0.0-windows-x64.zip");
    invalidAsset("browser_download_url",
                 windowsZip("3.0.0")["browser_download_url"].toString() + "?other=asset");
    invalidAsset("digest", "sha256:f00d");
    invalidAsset("digest", digest + '0');
    invalidAsset("digest", QStringLiteral("sha256:%1z").arg(QString(63, u'0')));
    invalidAsset("size", 0);
    invalidAsset("size", -3);
    invalidAsset("size", 4.25);
    invalidAsset("size", 2147483649LL);
}

void verifyReleaseRedirects() {
    for (const auto &url : {"https://release-assets.githubusercontent.com/file?sig=test",
                            "https://objects.githubusercontent.com:443/asset"})
        require(trustedUpdateRedirect(QUrl(url)), "A trusted GitHub release redirect was denied.");
    for (const auto &url :
         {"http://release-assets.githubusercontent.com/file",
          "https://release-assets.githubusercontent.com.invalid/file",
          "https://release-assets.githubusercontent.com:8080/file",
          "https://user:pass@release-assets.githubusercontent.com/file",
          "https://@release-assets.githubusercontent.com/file",
          "https://release-assets.githubusercontent.com/file#fragment",
          "https://github.com/yakut-sekonts/Apertone/releases/download/v3.0.0/",
          "https://raw.githubusercontent.com/file", "//release-assets.githubusercontent.com/file"})
        require(!trustedUpdateRedirect(QUrl(url)), "An untrusted release redirect was permitted.");
}
} // namespace

int main() {
    try {
        selectNewestWindowsRelease();
        rejectInvalidMetadata();
        verifyReleaseRedirects();
        std::cout
            << "Stable Windows release selection, metadata checks and redirect policy passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
