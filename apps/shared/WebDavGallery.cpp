#include "apps/shared/WebDavGallery.h"
#include "apps/shared/PhotoDelivery.h"
#include "core/image/Bayer.h"
#include "core/io/Path.h"
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QSslSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <algorithm>
#include <memory>

namespace luma {
namespace {
bool controlCharacter(QChar character) {
    return character.category() == QChar::Other_Control;
}

bool loopback(const QUrl &url) {
    return url.host() == QStringLiteral("127.0.0.1") || url.host() == QStringLiteral("::1");
}

QUrl childUrl(const QUrl &parent, const QString &filename) {
    auto child = parent.adjusted(QUrl::StripTrailingSlash);
    child.setPath(child.path(QUrl::FullyDecoded) + QLatin1Char('/') + filename, QUrl::DecodedMode);
    return child;
}

class WebDavClient {
  public:
    explicit WebDavClient(const WebDavGallery &upload) {
        if (loopback(upload.parent))
            manager_.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
        if (!upload.user.isEmpty())
            authorization_ =
                "Basic " + (upload.user + QLatin1Char(':') + upload.password).toUtf8().toBase64();
    }

    int send(const QByteArray &method, const QUrl &url, CancellationToken stop,
             QFile *data = nullptr, const QUrl &destination = {}, int timeout = 45'000) {
        checkCancellation(stop);
        QEventLoop wait;
        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::ManualRedirectPolicy);
        request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute,
                             QNetworkRequest::Manual);
        if (!authorization_.isEmpty())
            request.setRawHeader("Authorization", authorization_);
        if (data) {
            request.setHeader(QNetworkRequest::ContentLengthHeader, data->size());
            request.setHeader(QNetworkRequest::ContentTypeHeader,
                              data->fileName().endsWith(QStringLiteral(".jpg"))
                                  ? "image/jpeg"
                                  : "text/html; charset=utf-8");
        }
        if (!destination.isEmpty()) {
            request.setRawHeader("Destination", destination.toEncoded());
            request.setRawHeader("Overwrite", "F");
        }

        const std::unique_ptr<QNetworkReply> reply(
            manager_.sendCustomRequest(request, method, data));
        QObject::connect(reply.get(), &QNetworkReply::finished, &wait, &QEventLoop::quit);
        QObject::connect(reply.get(), &QNetworkReply::readyRead, &wait, [&] { reply->readAll(); });
        QTimer deadline, poll;
        deadline.setSingleShot(true);
        QObject::connect(&deadline, &QTimer::timeout, reply.get(), &QNetworkReply::abort);
        QObject::connect(&poll, &QTimer::timeout, reply.get(), [&] {
            if (stop.isCancelled())
                reply->abort();
        });
        deadline.start(timeout);
        poll.start(75);
        if (!reply->isFinished())
            wait.exec();
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        return reply->error() == QNetworkReply::NoError || status >= 400 ? status : 0;
    }

  private:
    // The delivery scheduler uses a std::thread. Set up its Qt dispatcher before networking.
    QEventLoop dispatcher_;
    QNetworkAccessManager manager_;
    QByteArray authorization_;
};

void requireStatus(int status, std::initializer_list<int> accepted, CancellationToken stop) {
    if (std::ranges::find(accepted, status) != accepted.end())
        return;
    checkCancellation(stop);
    if (status == 401 || status == 403)
        throw std::runtime_error("WebDAV authorization failed.");
    if (status == 412)
        throw std::runtime_error("A gallery already exists at the WebDAV destination.");
    if (status >= 300 && status < 400)
        throw std::runtime_error("Use the final WebDAV server URL.");
    if (status == 0)
        throw std::runtime_error("Could not complete a request to the WebDAV server.");
    throw std::runtime_error("The WebDAV server did not accept the gallery.");
}
} // namespace

void publishWebDavGallery(const std::filesystem::path &catalog, const std::vector<std::string> &ids,
                          const std::string &title, const WebDavGallery &upload,
                          CancellationToken stop, const std::function<void(double)> &progress) {
    const auto &parent = upload.parent;
    const bool tls = parent.scheme() == QStringLiteral("https");
    if (!parent.isValid() || parent.host().isEmpty() || parent.hasQuery() || parent.hasFragment() ||
        !parent.userInfo().isEmpty() || (!tls && !(parent.scheme() == "http" && loopback(parent))))
        throw std::runtime_error("Enter an HTTPS WebDAV folder URL without credentials.");
    const auto &folder = upload.folder;
    if (folder.isEmpty() || folder.size() > 80 || folder.front() == QLatin1Char('.') ||
        folder.back() == QLatin1Char('.') || folder != folder.trimmed() ||
        folder.contains(QLatin1Char('/')) || folder.contains(QLatin1Char('\\')) ||
        std::ranges::any_of(folder, controlCharacter))
        throw std::runtime_error("Enter a new WebDAV gallery folder name.");
    if (upload.user.contains(QLatin1Char(':')) || upload.user.size() > 512 ||
        upload.password.size() > 4096 || std::ranges::any_of(upload.user, controlCharacter) ||
        std::ranges::any_of(upload.password, controlCharacter) ||
        (upload.user.isEmpty() && !upload.password.isEmpty()))
        throw std::runtime_error("Enter a valid WebDAV username and password.");
    if (tls && !QSslSocket::supportsSsl())
        throw std::runtime_error("This installation cannot open HTTPS connections.");
    checkCancellation(stop);

    QTemporaryDir local;
    if (!local.isValid())
        throw std::runtime_error("Cannot create a temporary delivery folder.");
    const auto path = local.filePath(QStringLiteral("gallery"));
    deliverPhotos(catalog, ids, pathFromUtf8(path.toStdString()), DeliveryFormat::webGallery, title,
                  {}, stop, [&](double value) {
                      if (progress)
                          progress(value * 0.7);
                  });

    const auto destination = childUrl(parent, folder);
    const auto staging = childUrl(parent, QStringLiteral(".apertone-upload-") +
                                              QUuid::createUuid().toString(QUuid::Id128));
    WebDavClient client(upload);
    requireStatus(client.send("MKCOL", staging, stop), {201}, stop);
    try {
        const auto files = QDir(path).entryList(QDir::Files | QDir::NoSymLinks, QDir::Name);
        if (static_cast<std::size_t>(files.size()) != ids.size() + 1)
            throw std::runtime_error("The staged gallery is incomplete.");
        for (qsizetype index = 0; index < files.size(); ++index) {
            QFile file(QDir(path).filePath(files[index]));
            if (!file.open(QIODevice::ReadOnly))
                throw std::runtime_error("Cannot read the staged gallery.");
            requireStatus(client.send("PUT", childUrl(staging, files[index]), stop, &file),
                          {200, 201, 204}, stop);
            if (progress)
                progress(0.7 + 0.25 * static_cast<double>(index + 1) / files.size());
        }
        requireStatus(client.send("MOVE", staging, stop, nullptr, destination), {201, 204}, stop);
    } catch (...) {
        // Remove only the freshly created staging collection. MOVE can have committed even if
        // its reply was lost, so never delete or overwrite the destination during recovery.
        client.send("DELETE", staging, {}, nullptr, {}, 5000);
        throw;
    }
    if (progress)
        progress(1);
}
} // namespace luma
