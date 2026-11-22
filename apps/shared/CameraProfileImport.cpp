#include "apps/shared/CameraProfileImport.h"
#include "core/image/Bayer.h"
#include "core/io/Path.h"
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <algorithm>

namespace luma {
namespace {
QString normalized(const QString &text) {
    return text.simplified().toCaseFolded();
}
QString asString(const std::filesystem::path &path) {
    return QString::fromStdString(pathUtf8(path));
}
bool sameModel(const QString &catalog, const QString &profile) {
    return !profile.isEmpty() && (catalog == profile || catalog.endsWith(' ' + profile));
}
} // namespace

DcpSelection loadCameraProfile(const std::filesystem::path &path) {
    QFile file(asString(path));
    if (!file.open(QIODevice::ReadOnly) || file.size() < 8 ||
        file.size() > static_cast<qint64>(maximumDcpBytes))
        throw std::runtime_error("Cannot read a supported DNG camera profile.");
    const auto bytes = file.read(maximumDcpBytes + 1);
    if (bytes.size() < 8 || bytes.size() > static_cast<qsizetype>(maximumDcpBytes))
        throw std::runtime_error("Cannot read a supported DNG camera profile.");
    const auto digest =
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex().toStdString();
    return {digest, readDcpProfile(bytes.toStdString())};
}

bool cameraProfileMatches(const DcpProfile &profile, const std::string &camera) {
    return sameModel(normalized(QString::fromStdString(camera)),
                     normalized(QString::fromStdString(profile.model)));
}

CameraProfileDirectory::CameraProfileDirectory(const std::filesystem::path &folder,
                                               CancellationToken stop) {
    if (folder.empty())
        return;
    const auto root = asString(folder);
    if (!QFileInfo(root).isDir())
        throw std::runtime_error("Choose an existing folder of DNG camera profiles.");
    QDirIterator files(root, {"*.dcp", "*.DCP"}, QDir::Files | QDir::Readable | QDir::NoSymLinks,
                       QDirIterator::Subdirectories);
    while (files.hasNext()) {
        checkCancellation(stop);
        files_.push_back(pathFromUtf8(files.next().toStdString()));
        if (files_.size() == 8192)
            throw std::runtime_error(
                "Select a folder containing fewer than 8,192 DNG camera profiles.");
    }
    std::ranges::sort(files_);
}

DcpSelection CameraProfileDirectory::find(const QString &name, const std::string &camera,
                                          CancellationToken stop) {
    const auto normalizedName = normalized(name),
               model = normalized(QString::fromStdString(camera));
    if (files_.empty() || normalizedName.isEmpty() || model.isEmpty() ||
        normalizedName == "embedded")
        return {};
    const auto key = std::pair{normalizedName, model};
    if (const auto found = resolved_.find(key); found != resolved_.end())
        return found->second;
    DcpSelection result;
    for (const auto &path : files_) {
        checkCancellation(stop);
        const auto filename = normalized(asString(path.stem()));
        if (files_.size() > 128 && !filename.contains(normalizedName))
            continue;
        DcpSelection candidate;
        try {
            candidate = loadCameraProfile(path);
        } catch (const std::runtime_error &) {
            continue;
        }
        if (!cameraProfileMatches(*candidate.profile, camera) ||
            normalized(QString::fromStdString(candidate.profile->name)) != normalizedName)
            continue;
        if (!result.digest.empty() && result.digest != candidate.digest) {
            result = {};
            break;
        }
        result = std::move(candidate);
    }
    resolved_.emplace(key, result);
    return result;
}
} // namespace luma
