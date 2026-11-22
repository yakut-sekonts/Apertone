#include "core/io/Path.h"
#include "apps/desktop/LibraryModel.h"
#include "apps/shared/PhotoFormats.h"
#include "apps/shared/PresetStore.h"
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSettings>
#include <QStorageInfo>
#include <algorithm>
#include <memory>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
luma::DescriptiveMetadata metadataFromMap(const QVariantMap &map) {
    luma::DescriptiveMetadata values;
    values.title = map.value("title").toString().trimmed().toStdString();
    values.caption = map.value("caption").toString().trimmed().toStdString();
    values.copyright = map.value("copyright").toString().trimmed().toStdString();
    values.location = map.value("location").toString().trimmed().toStdString();
    const auto creator = map.value("creator").toString().trimmed();
    if (!creator.isEmpty())
        values.creators.push_back(creator.toStdString());
    for (const auto &entry : map.value("keywords").toString().split(',', Qt::SkipEmptyParts)) {
        const auto keyword = entry.trimmed().toStdString();
        if (!keyword.empty() && std::find(values.keywords.begin(), values.keywords.end(),
                                          keyword) == values.keywords.end())
            values.keywords.push_back(keyword);
    }
    values.validate();
    return values;
}
std::unique_ptr<QLockFile> lockPresets() {
    const auto file = QSettings().fileName();
    QDir().mkpath(QFileInfo(file).absolutePath());
    auto lock = std::make_unique<QLockFile>(file + ".import-presets.lock");
    if (!lock->tryLock(0))
        throw std::runtime_error("Import metadata presets are being changed in another window.");
    return lock;
}
QJsonObject storedPresets() {
    const auto bytes = QSettings().value("import/metadataPresets").toByteArray();
    if (bytes.isEmpty())
        return {};
    if (bytes.size() > 1024 * 1024)
        throw std::runtime_error("Import metadata preset store exceeds its size limit.");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        throw std::runtime_error("Cannot read import metadata presets.");
    return document.object();
}
void savePresets(const QJsonObject &object) {
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    if (bytes.size() > 1024 * 1024)
        throw std::runtime_error("Import metadata preset store exceeds its size limit.");
    QSettings settings;
    settings.setValue("import/metadataPresets", bytes);
    settings.sync();
    if (settings.status() != QSettings::NoError)
        throw std::runtime_error("Cannot save import metadata presets.");
}
} // namespace
QVariantList LibraryModel::cardSources() const {
    QVariantList devices;
    for (const auto &volume : QStorageInfo::mountedVolumes()) {
        if (!volume.isValid() || !volume.isReady() || volume.isRoot())
            continue;
        const auto root = volume.rootPath();
#ifdef _WIN32
        auto device = QDir::toNativeSeparators(root);
        if (!device.endsWith(u'\\'))
            device += u'\\';
        const bool removable = GetDriveTypeW(reinterpret_cast<LPCWSTR>(device.utf16())) == DRIVE_REMOVABLE;
#else
        const bool removable = root.startsWith(QStringLiteral("/Volumes/"));
#endif
        if (!removable)
            continue;
        const auto dcim = QDir(root).absoluteFilePath(QStringLiteral("DCIM"));
        if (QFileInfo(dcim).isDir())
            devices.append(QVariantMap{{"name", volume.displayName()}, {"url", QUrl::fromLocalFile(dcim)}});
    }
    return devices;
}
void LibraryModel::startCaptureFolder(const QUrl &folder) {
    if (working() || !folder.isLocalFile())
        return;
    const auto local = QFileInfo(folder.toLocalFile()).canonicalFilePath();
    if (local.isEmpty() || !QFileInfo(local).isDir()) {
        error_ = QStringLiteral("Choose an existing capture folder.");
        emit stateChanged();
        return;
    }
    captureFolder_ = local;
    captureFiles_.clear();
    scanCaptureFolder(true);
    if (captureFolder_.isEmpty())
        return;
    captureTimer_.start();
    error_.clear();
    emit stateChanged();
}
void LibraryModel::stopCaptureFolder() {
    captureTimer_.stop();
    captureFolder_.clear();
    captureFiles_.clear();
    emit stateChanged();
}
void LibraryModel::scanCaptureFolder(bool baseline) {
    if (closing_ || captureFolder_.isEmpty() || (!baseline && working()))
        return;
    if (!QFileInfo(captureFolder_).isDir()) {
        status_ = QStringLiteral("Capture folder unavailable. Waiting for it to reconnect…");
        emit stateChanged();
        return;
    }
    QDirIterator iterator(captureFolder_, QDir::Files | QDir::Readable | QDir::NoSymLinks);
    std::vector<std::filesystem::path> stable;
    std::map<QString, CaptureFile> current;
    while (iterator.hasNext()) {
        const auto file = iterator.nextFileInfo();
        auto path = luma::pathFromUtf8(file.absoluteFilePath().toStdString());
        const auto ext = luma::lowerPhotoExtension(path);
        if (ext != ".jpg" && ext != ".jpeg" && !luma::isSupportedRawExtension(path))
            continue;
        if (current.size() >= 20'000) {
            error_ = QStringLiteral("Choose a capture folder with fewer than 20,000 photos.");
            stopCaptureFolder();
            return;
        }
        CaptureFile entry{file.size(), file.lastModified().toMSecsSinceEpoch(), baseline};
        const auto previous = captureFiles_.find(file.fileName());
        if (previous != captureFiles_.end() && previous->second.size == entry.size &&
            previous->second.modified == entry.modified) {
            entry.considered = previous->second.considered;
            if (!entry.considered && entry.size > 0 && stable.size() < 32) {
                stable.push_back(std::move(path));
                entry.considered = true;
            }
        }
        current.emplace(file.fileName(), entry);
    }
    if (!stable.empty()) {
        luma::ImportOptions options;
        options.sourceRoots = {luma::pathFromUtf8(captureFolder_.toStdString())};
        options.recursive = false;
        if (!startImport(std::move(stable), std::move(options)))
            return;
    }
    captureFiles_.swap(current);
}
QVariantList LibraryModel::importMetadataPresets() const {
    QVariantList result;
    try {
        const auto object = storedPresets();
        for (auto it = object.begin(); it != object.end(); ++it)
            result.append(
                QVariantMap{{"name", it.key()}, {"values", it.value().toObject().toVariantMap()}});
    } catch (const std::exception
                 &) { /* Mutation/import reports corruption without resetting the store. */
    }
    return result;
}
bool LibraryModel::saveImportMetadataPreset(const QString &name, const QVariantMap &values) {
    try {
        const auto lock = lockPresets();
        const auto normalized = name.trimmed();
        if (normalized.isEmpty() || normalized.toUtf8().size() > 512)
            throw std::runtime_error("Enter a metadata preset name.");
        (void)metadataFromMap(values);
        auto object = storedPresets();
        if (!object.contains(normalized) && object.size() >= 128)
            throw std::runtime_error("Import metadata preset limit reached.");
        object.insert(normalized, QJsonObject::fromVariantMap(values));
        savePresets(object);
        error_.clear();
        emit stateChanged();
        return true;
    } catch (const std::exception &failure) {
        error_ = QString::fromUtf8(failure.what());
        emit stateChanged();
        return false;
    }
}
bool LibraryModel::removeImportMetadataPreset(const QString &name) {
    try {
        const auto lock = lockPresets();
        auto object = storedPresets();
        object.remove(name);
        savePresets(object);
        error_.clear();
        emit stateChanged();
        return true;
    } catch (const std::exception &failure) {
        error_ = QString::fromUtf8(failure.what());
        emit stateChanged();
        return false;
    }
}
bool LibraryModel::importAdvanced(const QList<QUrl> &urls, const QVariantMap &settings,
                                  const QString &developPreset) {
    if (working())
        return false;
    try {
        std::vector<std::filesystem::path> sources;
        for (const auto &url : urls) {
            if (!url.isLocalFile())
                throw std::runtime_error("Import supports local files");
            sources.emplace_back(luma::pathFromUtf8(url.toLocalFile().toStdString()));
        }
        if (sources.empty())
            throw std::runtime_error("No supported photos found in the selected sources.");
        const int mode = settings.value("mode").toInt();
        if (mode < 0 || mode > 2)
            throw std::runtime_error("Invalid import mode.");
        luma::ImportOptions options;
        options.mode = static_cast<luma::ImportMode>(mode);
        const auto localPath = [](const QVariant &value) {
            const auto url = value.toUrl();
            if (url.isEmpty())
                return std::filesystem::path{};
            if (!url.isLocalFile())
                throw std::runtime_error("Import supports local files");
            return luma::pathFromUtf8(url.toLocalFile().toStdString());
        };
        options.destination = localPath(settings.value("destination"));
        options.backupDestination = localPath(settings.value("backup"));
        options.recursive = settings.value("recursive", true).toBool();
        options.skipDuplicates = settings.value("skipDuplicates", true).toBool();
        options.dateFolders = settings.value("dateFolders", false).toBool();
        options.naming = settings.value("naming", "{name}").toString().toStdString();
        options.metadata = metadataFromMap(settings.value("metadata").toMap());
        if (!developPreset.isEmpty()) {
            const auto preset = luma::decodePreset(developPreset.toUtf8());
            options.develop = luma::mergeSettings(luma::EditDocument{}, preset.settings,
                                                  preset.groups, preset.parameters);
        }
        options.validate();
        return startImport(std::move(sources), std::move(options));
    } catch (const std::exception &failure) {
        error_ = QString::fromUtf8(failure.what());
        emit stateChanged();
        return false;
    }
}
bool LibraryModel::refreshLastImport() {
    if (working() || loading_)
        return false;
    try {
        auto replay = luma::Catalog(catalog_).latestCompletedImport();
        if (!replay)
            throw std::runtime_error("No completed import is available to refresh.");
        // A refresh may discover the same files again. Never replace their catalog
        // identity or processing state, even if the original import allowed duplicates.
        replay->options.skipDuplicates = true;
        maintenanceStatus_ = QStringLiteral("Refreshing the last import…");
        return startImport(std::move(replay->sources), std::move(replay->options));
    } catch (const std::exception &failure) {
        error_ = QString::fromUtf8(failure.what());
        emit stateChanged();
        return false;
    }
}
bool LibraryModel::refreshImportSource(const QUrl &source) {
    if (working() || loading_)
        return false;
    try {
        if (!source.isLocalFile())
            throw std::runtime_error("Import supports local files");
        auto replay = luma::Catalog(catalog_).importSource(
            luma::pathFromUtf8(source.toLocalFile().toStdString()));
        if (!replay)
            throw std::runtime_error("Import source is no longer tracked.");
        replay->options.skipDuplicates = true;
        importSourceRefreshQueue_.clear();
        importSourceRefreshIndex_ = importSourceRefreshTotal_ = 1;
        managedSourceRefresh_ = true;
        maintenanceStatus_ = QStringLiteral("Refreshing import source…");
        if (!startImport(std::move(replay->sources), std::move(replay->options))) {
            managedSourceRefresh_ = false;
            return false;
        }
        return true;
    } catch (const std::exception &failure) {
        error_ = QString::fromUtf8(failure.what());
        emit stateChanged();
        return false;
    }
}
bool LibraryModel::refreshAllImportSources() {
    if (working() || loading_)
        return false;
    try {
        const auto sources = luma::Catalog(catalog_).importSources();
        if (sources.empty())
            throw std::runtime_error("No tracked import sources are available to refresh.");
        importSourceRefreshQueue_.clear();
        for (const auto &source : sources) {
            auto options = source.options;
            options.skipDuplicates = true;
            importSourceRefreshQueue_.push_back({{source.path}, std::move(options)});
        }
        importSourceRefreshIndex_ = 0;
        importSourceRefreshTotal_ = static_cast<int>(importSourceRefreshQueue_.size());
        managedSourceRefresh_ = true;
        return startNextImportSourceRefresh();
    } catch (const std::exception &failure) {
        error_ = QString::fromUtf8(failure.what());
        emit stateChanged();
        return false;
    }
}
bool LibraryModel::startNextImportSourceRefresh() {
    if (importSourceRefreshQueue_.empty())
        return false;
    auto replay = std::move(importSourceRefreshQueue_.front());
    importSourceRefreshQueue_.pop_front();
    ++importSourceRefreshIndex_;
    maintenanceStatus_ = LocalizedText(QStringLiteral("Refreshing import source %1 of %2…"))
                             .arg(importSourceRefreshIndex_)
                             .arg(importSourceRefreshTotal_);
    if (startImport(std::move(replay.sources), std::move(replay.options)))
        return true;
    importSourceRefreshQueue_.clear();
    managedSourceRefresh_ = false;
    return false;
}
