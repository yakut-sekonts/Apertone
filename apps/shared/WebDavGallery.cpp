#include "apps/shared/WebDavGallery.h"
#include "apps/shared/PhotoDelivery.h"
#include "core/image/Bayer.h"
#include "core/io/Path.h"
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QSaveFile>
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

constexpr qsizetype maximumIndexBytes = 4 * 1024 * 1024;
constexpr char receiptHeader[] = "Apertone WebDAV v1\n";

struct DavRequest {
    QFile *data = nullptr;
    QUrl destination{};
    QByteArray ifMatch{};
    bool readBody = false;
    int timeout = 45'000;
};

struct DavResponse {
    int status = 0;
    QByteArray body;
    QByteArray etag;
};

class WebDavClient {
  public:
    explicit WebDavClient(const WebDavGallery &upload) {
        if (loopback(upload.parent))
            manager_.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
        if (!upload.user.isEmpty())
            authorization_ =
                "Basic " + (upload.user + QLatin1Char(':') + upload.password).toUtf8().toBase64();
    }

    DavResponse send(const QByteArray &method, const QUrl &url, CancellationToken stop,
                     const DavRequest &options = {}) {
        checkCancellation(stop);
        QEventLoop wait;
        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::ManualRedirectPolicy);
        request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute,
                             QNetworkRequest::Manual);
        if (!authorization_.isEmpty())
            request.setRawHeader("Authorization", authorization_);
        if (options.readBody) {
            request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                                 QNetworkRequest::AlwaysNetwork);
            request.setRawHeader("Cache-Control", "no-cache");
            request.setRawHeader("Accept-Encoding", "identity");
        }
        if (!options.ifMatch.isEmpty())
            request.setRawHeader("If-Match", options.ifMatch);
        if (const auto data = options.data) {
            request.setHeader(QNetworkRequest::ContentLengthHeader, data->size());
            request.setHeader(QNetworkRequest::ContentTypeHeader,
                              data->fileName().endsWith(QStringLiteral(".jpg"))
                                  ? "image/jpeg"
                                  : "text/html; charset=utf-8");
        }
        if (!options.destination.isEmpty()) {
            request.setRawHeader("Destination", options.destination.toEncoded());
            request.setRawHeader("Overwrite", "F");
        }

        const std::unique_ptr<QNetworkReply> reply(
            manager_.sendCustomRequest(request, method, options.data));
        reply->setReadBufferSize(64 * 1024);
        DavResponse result;
        bool exceededLimit = false;
        const auto drain = [&] {
            if (!reply->isOpen())
                return;
            auto data = reply->readAll();
            if (!options.readBody || exceededLimit)
                return;
            if (data.size() > maximumIndexBytes - result.body.size()) {
                exceededLimit = true;
                reply->abort();
                return;
            }
            result.body += data;
        };
        QObject::connect(reply.get(), &QNetworkReply::finished, &wait, &QEventLoop::quit);
        QObject::connect(reply.get(), &QNetworkReply::readyRead, &wait, drain);
        QTimer deadline, poll;
        deadline.setSingleShot(true);
        QObject::connect(&deadline, &QTimer::timeout, reply.get(), &QNetworkReply::abort);
        QObject::connect(&poll, &QTimer::timeout, reply.get(), [&] {
            if (stop.isCancelled())
                reply->abort();
        });
        deadline.start(options.timeout);
        poll.start(75);
        if (!reply->isFinished())
            wait.exec();
        drain();
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (!exceededLimit && (reply->error() == QNetworkReply::NoError || status >= 400)) {
            result.status = status;
            result.etag = reply->rawHeader("ETag");
        }
        return result;
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

bool strongETag(const QByteArray &tag) {
    return tag.size() >= 2 && tag.size() <= 512 && tag.front() == '"' && tag.back() == '"' &&
           std::ranges::all_of(QByteArrayView(tag).sliced(1, tag.size() - 2),
                               [](char ch) { return ch > ' ' && ch < 127 && ch != '"'; });
}

QByteArray digest(QByteArrayView bytes) {
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}

QString receiptPath(const std::filesystem::path &catalog, const QUrl &index, QByteArrayView page) {
    const auto directory = QString::fromStdString(pathUtf8(catalog)) + ".luma-webdav";
    const auto key = digest(index.toEncoded() + '\n' + digest(page));
    return QDir(directory).filePath(QString::fromLatin1(key));
}

bool ownsPage(const std::filesystem::path &catalog, const QUrl &index, QByteArrayView page) {
    QFile receipt(receiptPath(catalog, index, page));
    return receipt.open(QIODevice::ReadOnly) &&
           receipt.read(sizeof(receiptHeader)) == receiptHeader;
}

void saveFile(const QString &path, QByteArrayView contents, const char *failure) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(contents.data(), contents.size()) != contents.size() || !file.commit())
        throw std::runtime_error(failure);
}

void recordPage(const std::filesystem::path &catalog, const QUrl &index, QByteArrayView page) {
    constexpr auto failure = "Cannot save the WebDAV publication receipt.";
    const auto path = receiptPath(catalog, index, page);
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        throw std::runtime_error(failure);
    saveFile(path, receiptHeader, failure);
}

QByteArray readIndex(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() > maximumIndexBytes)
        throw std::runtime_error("Cannot read the staged gallery.");
    return file.readAll();
}

int uploadFile(WebDavClient &client, const QUrl &url, const QString &path, CancellationToken stop,
               const QByteArray &etag = {}) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw std::runtime_error("Cannot read the staged gallery.");
    return client.send("PUT", url, stop, {.data = &file, .ifMatch = etag}).status;
}

QByteArray revisionLanding(QByteArray page, const QByteArray &generation) {
    page.replace("src=\"photo-", "src=\"" + generation + "/photo-");
    page.replace("href=\"photo-", "href=\"" + generation + "/photo-");
    page.replace("</main><script>", "</main><p><a href=\"" + generation +
                                        "/previous.html\">Previous gallery</a></p><script>");
    return page;
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

    const bool updating = upload.publication == WebDavPublication::updateGallery;
    const auto destination = childUrl(parent, folder);
    const auto landing = childUrl(destination, QStringLiteral("index.html"));
    const auto revision = QUuid::createUuid().toString(QUuid::Id128);
    const auto generation = QStringLiteral("apertone-revision-") + revision;
    const auto stageParent = updating ? destination : parent;
    const auto staging = childUrl(stageParent, QStringLiteral(".apertone-upload-") + revision);
    const auto stageIndex = childUrl(staging, QStringLiteral("index.html"));
    WebDavClient client(upload);
    DavResponse previous;
    if (updating) {
        previous = client.send("GET", landing, stop, {.readBody = true});
        if (previous.status == 404)
            throw std::runtime_error("Choose a gallery published by this catalog.");
        requireStatus(previous.status, {200}, stop);
        if (!ownsPage(catalog, landing, previous.body))
            throw std::runtime_error("Choose a gallery published by this catalog.");
        if (!strongETag(previous.etag))
            throw std::runtime_error("This WebDAV server does not support safe gallery updates.");
    }

    QTemporaryDir local;
    if (!local.isValid())
        throw std::runtime_error("Cannot create a temporary delivery folder.");
    const auto path = local.filePath(QStringLiteral("gallery"));
    deliverPhotos(catalog, ids, pathFromUtf8(path.toStdString()), DeliveryFormat::webGallery, title,
                  {}, stop, [&](double value) {
                      if (progress)
                          progress(value * 0.7);
                  });

    constexpr auto saveFailure = "Cannot write the gallery index.";
    const auto indexPath = QDir(path).filePath(QStringLiteral("index.html"));
    auto page = readIndex(indexPath);
    // A unique publication marker makes receipt ownership specific to this upload.
    page.insert(0, "<!-- Apertone WebDAV " + revision.toLatin1() + " -->\n");
    saveFile(indexPath, page, saveFailure);
    auto newLanding = page;
    const auto newLandingPath = local.filePath(QStringLiteral("update.html"));
    if (updating) {
        newLanding = revisionLanding(page, generation.toLatin1());
        if (newLanding.size() > maximumIndexBytes)
            throw std::runtime_error(saveFailure);
        saveFile(newLandingPath, newLanding, saveFailure);
        const auto head = previous.body.indexOf("<meta charset=");
        if (head < 0)
            throw std::runtime_error("Choose a gallery published by this catalog.");
        previous.body.insert(head, "<base href=\"../\">");
        saveFile(QDir(path).filePath(QStringLiteral("previous.html")), previous.body, saveFailure);
    }

    requireStatus(client.send("MKCOL", staging, stop).status, {201}, stop);
    bool staged = true;
    try {
        const auto files = QDir(path).entryList(QDir::Files | QDir::NoSymLinks, QDir::Name);
        if (static_cast<std::size_t>(files.size()) != ids.size() + (updating ? 2 : 1))
            throw std::runtime_error("The staged gallery is incomplete.");
        for (qsizetype position = 0; position < files.size(); ++position) {
            requireStatus(uploadFile(client, childUrl(staging, files[position]),
                                     QDir(path).filePath(files[position]), stop),
                          {200, 201, 204}, stop);
            if (progress)
                progress(0.7 + 0.25 * static_cast<double>(position + 1) / files.size());
        }
        if (updating) {
            // Verify conditional replacement on an owned staging file first.
            const auto probe = "\"apertone-probe-" + revision.toLatin1() + '"';
            if (uploadFile(client, stageIndex, newLandingPath, stop, probe) != 412) {
                checkCancellation(stop);
                throw std::runtime_error(
                    "This WebDAV server does not support safe gallery updates.");
            }
        }
        // Save the exact candidate before the network commit, which can succeed
        // even if its response is lost. Older receipts remain usable on failure.
        recordPage(catalog, landing, newLanding);
        const auto published = updating ? childUrl(destination, generation) : destination;
        requireStatus(client.send("MOVE", staging, stop, {.destination = published}).status,
                      {201, 204}, stop);
        staged = false;
        if (updating) {
            const auto status = uploadFile(client, landing, newLandingPath, stop, previous.etag);
            if (status == 412)
                throw std::runtime_error("The WebDAV gallery changed before the update.");
            requireStatus(status, {200, 204}, stop);
        }
    } catch (...) {
        // Remove only the freshly created staging collection. MOVE can have committed even if
        // its reply was lost, so never delete or overwrite the destination during recovery.
        if (staged)
            client.send("DELETE", staging, {}, {.timeout = 5000});
        throw;
    }
    if (progress)
        progress(1);
}
} // namespace luma
