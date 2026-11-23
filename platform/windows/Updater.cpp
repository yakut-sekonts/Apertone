#include "apps/desktop/CatalogStartup.h"
#include "apps/desktop/UpdateController.h"
#include "platform/windows/UpdateRelease.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QTemporaryDir>
#include <array>
#include <exception>
#include <utility>
#include <windows.h>

namespace {
using luma::windows::WindowsUpdate;
constexpr auto preference = "windows-update/automatic";
constexpr auto helperName = "ApertoneUpdate.exe";
constexpr auto archiveLimit = 4 * 1024 * 1024;

QString systemTar() {
    std::array<wchar_t, MAX_PATH> directory{};
    const auto length = GetSystemDirectoryW(directory.data(), directory.size());
    if (!length || length >= directory.size())
        return {};
    return QDir(QString::fromWCharArray(directory.data(), static_cast<qsizetype>(length)))
        .filePath(QStringLiteral("tar.exe"));
}

QString packageRoot(const QByteArray &listing, const QVersionNumber &version) {
    const auto versioned = QStringLiteral("Apertone-%1-Windows").arg(version.toString());
    const auto generic = QStringLiteral("Apertone-windows-x64");
    const QRegularExpression safePart(QStringLiteral("^[A-Za-z0-9_+@.-]+$"));
    const QRegularExpression reserved(QStringLiteral("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$"),
                                      QRegularExpression::CaseInsensitiveOption);
    QSet<QString> entries;
    QString root;
    bool desktop = false, manifest = false;
    for (auto raw : listing.split('\n')) {
        if (raw.endsWith('\r'))
            raw.chop(1);
        if (raw.isEmpty())
            continue;
        if (raw.endsWith('/'))
            raw.chop(1);
        const auto path = QString::fromUtf8(raw);
        const auto parts = path.split('/');
        if (entries.size() >= 10000 || entries.contains(path.toCaseFolded()) ||
            (parts.front() != versioned && parts.front() != generic))
            return {};
        for (const auto &part : parts)
            if (!safePart.match(part).hasMatch() || part.endsWith('.') ||
                reserved.match(part.section('.', 0, 0)).hasMatch())
                return {};
        if (!root.isEmpty() && parts.front() != root)
            return {};
        root = parts.front();
        entries.insert(path.toCaseFolded());
        desktop |= path == root + QStringLiteral("/Apertone.exe");
        manifest |= path == root + QStringLiteral("/UPDATE-FILES.txt");
    }
    return desktop && manifest ? root : QString{};
}

class WindowsUpdater final : public QObject, public NativeUpdater {
  public:
    explicit WindowsUpdater(UpdateCallbacks callbacks) : callbacks_(std::move(callbacks)) {
        timer_.setInterval(24 * 60 * 60 * 1000);
        connect(&timer_, &QTimer::timeout, this, [this] { check(false); });
        connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart)
                fail(QStringLiteral("Could not prepare the Windows update"));
        });
        connect(&process_, &QProcess::readyReadStandardOutput, this, [this] {
            if (operation_ == Operation::listing) {
                if (process_.bytesAvailable() > archiveLimit - listing_.size()) {
                    process_.kill();
                    return;
                }
                listing_.append(process_.readAllStandardOutput());
            } else {
                process_.readAllStandardOutput();
            }
        });
        connect(&process_, &QProcess::readyReadStandardError, this,
                [this] { process_.readAllStandardError(); });
        connect(&process_, &QProcess::finished, this,
                [this](int exitCode, QProcess::ExitStatus status) {
                    processFinished(exitCode == 0 && status == QProcess::NormalExit);
                });
    }

    ~WindowsUpdater() override {
        if (reply_) {
            reply_->disconnect(this);
            reply_->abort();
        }
        process_.disconnect(this);
        if (process_.state() != QProcess::NotRunning) {
            process_.kill();
            process_.waitForFinished();
        }
    }

    bool start(QString &) override {
        started_ = true;
        if (automatic()) {
            timer_.start();
            QTimer::singleShot(0, this, [this] {
                if (automatic())
                    check(false);
            });
        }
        return true;
    }

    bool automatic() const override {
        return QSettings().value(QLatin1String(preference), true).toBool();
    }

    void setAutomatic(bool enabled) override {
        QSettings().setValue(QLatin1String(preference), enabled);
        if (started_ && enabled)
            timer_.start();
        else
            timer_.stop();
    }

    bool canCheck() const override {
        return started_ && operation_ == Operation::idle;
    }

    void check(bool foreground) override {
        if (!canCheck())
            return;
        if (foreground) {
            if (update_)
                download();
            return;
        }
        update_.reset();
        callbacks_.finished();
        operation_ = Operation::checking;
        callbacks_.status(true, QStringLiteral("Checking for updates…"));
        contents_.clear();
        request(QUrl(QStringLiteral(
                    "https://api.github.com/repos/yakut-sekonts/Apertone/releases?per_page=30")),
                false);
    }

  private:
    enum class Operation { idle, checking, downloading, listing, extracting, validating, restart };

    void request(const QUrl &url, bool downloading) {
        streamFailed_ = false;
        QNetworkRequest request(url);
        request.setRawHeader("User-Agent", "Apertone Windows Updater");
        request.setRawHeader("Accept", downloading ? "application/octet-stream"
                                                   : "application/vnd.github+json");
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::UserVerifiedRedirectPolicy);
        request.setTransferTimeout(downloading ? 120000 : 30000);
        reply_ = network_.get(request);
        reply_->setReadBufferSize(256 * 1024);
        connect(reply_, &QNetworkReply::redirected, this, [this, downloading](const QUrl &target) {
            if (downloading && luma::windows::trustedUpdateRedirect(reply_->url().resolved(target)))
                reply_->redirectAllowed();
            else
                reply_->abort();
        });
        connect(reply_, &QNetworkReply::readyRead, this, [this] { consume(); });
        connect(reply_, &QNetworkReply::finished, this, [this] { requestFinished(); });
    }

    void consume() {
        if (reply_->error() != QNetworkReply::NoError)
            return;
        const auto bytes = reply_->readAll();
        if (operation_ == Operation::checking) {
            if (bytes.size() > archiveLimit - contents_.size()) {
                streamFailed_ = true;
                reply_->abort();
            } else {
                contents_.append(bytes);
            }
        } else {
            if (bytes.size() > update_->size - received_ || output_.write(bytes) != bytes.size()) {
                streamFailed_ = true;
                reply_->abort();
                return;
            }
            digest_.addData(bytes);
            received_ += bytes.size();
        }
    }

    void requestFinished() {
        auto *finished = reply_;
        finished->disconnect(this);
        if (finished->error() == QNetworkReply::NoError)
            consume();
        reply_ = nullptr;
        const bool success =
            !streamFailed_ && finished->error() == QNetworkReply::NoError &&
            finished->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200;
        finished->deleteLater();
        if (operation_ == Operation::checking) {
            if (!success) {
                fail(QStringLiteral("Could not check for updates"));
                return;
            }
            try {
                update_ = luma::windows::latestWindowsUpdate(
                    contents_, QVersionNumber::fromString(QCoreApplication::applicationVersion()));
            } catch (const std::exception &) {
                fail(QStringLiteral("Could not check for updates"));
                return;
            }
            operation_ = Operation::idle;
            if (update_)
                callbacks_.found(update_->version.toString());
            callbacks_.status(false, update_
                                         ? QStringLiteral("A new version of Apertone is available")
                                         : QStringLiteral("No updates available"));
            return;
        }
        const bool flushed = output_.flush();
        output_.close();
        if (!success || !flushed) {
            fail(QStringLiteral("Could not download the Windows update"));
            return;
        }
        if (received_ != update_->size || digest_.result() != update_->sha256) {
            fail(QStringLiteral("Windows update verification failed"));
            return;
        }
        operation_ = Operation::listing;
        callbacks_.status(true, QStringLiteral("Preparing Windows update…"));
        listing_.clear();
        process_.start(systemTar(), {QStringLiteral("-tf"), archive_});
    }

    void download() {
        install_ = QFileInfo(QCoreApplication::applicationDirPath()).canonicalFilePath();
        const QDir installation(install_);
        const QFileInfo helper(installation.filePath(QLatin1String(helperName)));
        if (install_.isEmpty() || !helper.isFile() || helper.isSymLink() ||
            !QFileInfo::exists(installation.filePath(QStringLiteral("UPDATE-FILES.txt"))) ||
            !QFileInfo(systemTar()).isFile()) {
            fail(QStringLiteral("Could not prepare the Windows update"));
            return;
        }
        stage_ = std::make_unique<QTemporaryDir>(QFileInfo(install_).absolutePath() +
                                                 QStringLiteral("/.apertone-update-XXXXXX"));
        if (!stage_->isValid() ||
            !QFile::copy(helper.filePath(), stage_->filePath(QLatin1String(helperName)))) {
            fail(QStringLiteral("Could not prepare the Windows update"));
            return;
        }
        archive_ = stage_->filePath(QStringLiteral("update.zip"));
        output_.setFileName(archive_);
        if (!output_.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            fail(QStringLiteral("Could not prepare the Windows update"));
            return;
        }
        digest_.reset();
        received_ = 0;
        operation_ = Operation::downloading;
        callbacks_.status(true, QStringLiteral("Downloading Windows update…"));
        request(update_->url, true);
    }

    void processFinished(bool success) {
        if (!success) {
            fail(QStringLiteral("Could not prepare the Windows update"));
            return;
        }
        if (operation_ == Operation::listing) {
            const auto last = process_.readAllStandardOutput();
            if (last.size() > archiveLimit - listing_.size()) {
                fail(QStringLiteral("Windows update verification failed"));
                return;
            }
            listing_.append(last);
            root_ = packageRoot(listing_, update_->version);
            listing_.clear();
            if (root_.isEmpty()) {
                fail(QStringLiteral("Windows update verification failed"));
                return;
            }
            operation_ = Operation::extracting;
            process_.start(systemTar(),
                           {QStringLiteral("-xf"), archive_, QStringLiteral("-C"), stage_->path()});
        } else if (operation_ == Operation::extracting) {
            QDir stage(stage_->path());
            if (!stage.rename(root_, QStringLiteral("package")) || !QFile::remove(archive_)) {
                fail(QStringLiteral("Could not prepare the Windows update"));
                return;
            }
            operation_ = Operation::validating;
            process_.start(stage.filePath(QLatin1String(helperName)),
                           {QStringLiteral("--validate"), QDir::toNativeSeparators(install_)});
        } else if (operation_ == Operation::validating) {
            operation_ = Operation::restart;
            callbacks_.status(false, QStringLiteral("Preparing Windows update…"));
            callbacks_.install([this] { launchInstaller(); });
        }
    }

    void launchInstaller() {
        auto arguments = QStringList{QDir::toNativeSeparators(install_),
                                     QString::number(QCoreApplication::applicationPid())};
        const auto catalog = CatalogStartup::rememberedCatalog();
        if (!catalog.isEmpty())
            arguments.append(QDir::toNativeSeparators(catalog));
        if (!QProcess::startDetached(stage_->filePath(QLatin1String(helperName)), arguments,
                                     stage_->path())) {
            fail(QStringLiteral("Could not start the Windows update installer"));
            return;
        }
        stage_->setAutoRemove(false);
        QCoreApplication::quit();
    }

    void fail(const QString &message) {
        output_.close();
        stage_.reset();
        operation_ = Operation::idle;
        callbacks_.failed();
        callbacks_.status(false, message);
    }

    UpdateCallbacks callbacks_;
    QTimer timer_;
    QNetworkAccessManager network_;
    QNetworkReply *reply_ = nullptr;
    QProcess process_;
    Operation operation_ = Operation::idle;
    bool started_ = false;
    bool streamFailed_ = false;
    std::optional<WindowsUpdate> update_;
    QByteArray contents_, listing_;
    std::unique_ptr<QTemporaryDir> stage_;
    QString install_, archive_, root_;
    QFile output_;
    QCryptographicHash digest_{QCryptographicHash::Sha256};
    qint64 received_ = 0;
};
} // namespace

std::unique_ptr<NativeUpdater> makeNativeUpdater(UpdateCallbacks callbacks) {
    return std::make_unique<WindowsUpdater>(std::move(callbacks));
}
