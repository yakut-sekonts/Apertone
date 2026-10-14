#include "apps/desktop/ExportController.h"
#include "apps/desktop/EditorController.h"
#include "apps/desktop/LibraryModel.h"
#include "core/image/Bayer.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QPointer>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimeZone>
#include <QUuid>
#include <algorithm>
#include <set>
#include <tuple>

namespace {
QByteArray readFile(const QString &path) {
    QFile file(path);
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024)
        throw std::runtime_error("Cannot read export settings (1 MB limit).");
    return file.readAll();
}
} // namespace
ExportController::ExportController(std::filesystem::path catalog, EditorController &editor,
                                   LibraryModel &library, Runner runner, QObject *parent)
    : QObject(parent), catalog_(std::move(catalog)),
      storePath_(QString::fromStdString(catalog_.parent_path().string()) +
                 QStringLiteral("/export-settings.json")),
      editor_(editor), library_(library), runner_(std::move(runner)) {
    settings_.destination = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (settings_.destination.isEmpty())
        settings_.destination = QDir::homePath();
    if (!runner_)
        runner_ = [path = catalog_](const auto &asset, const auto &destination, auto options,
                                    auto stop, const auto &progress) {
            luma::SessionService(path).exportPhoto(asset, asset.edits, destination, options, stop,
                                                   progress);
        };
    try {
        readStore();
    } catch (const std::exception &error) {
        error_ = QString::fromUtf8(error.what());
    }
    preparation_.setInterval(40);
    connect(&preparation_, &QTimer::timeout, this, &ExportController::plan);
}
ExportController::~ExportController() {
    cancellation_.cancel();
    jobs_.shutdown();
}
void ExportController::readStore() {
    const auto bytes = readFile(storePath_);
    if (bytes.isEmpty()) {
        storeBytes_ = bytes;
        return;
    }
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(bytes, &parse);
    const auto root = document.object();
    if (parse.error != QJsonParseError::NoError || !document.isObject() || root.size() != 3 ||
        root.value("schema").toDouble() != 1 || !root.value("last").isObject() ||
        !root.value("profiles").isObject())
        throw std::runtime_error("Invalid export settings file.");
    const auto last = luma::ExportSettings::fromJson(root.value("last").toObject());
    const auto profiles = root.value("profiles").toObject();
    if (profiles.size() > 64)
        throw std::runtime_error("Too many export profiles.");
    for (auto it = profiles.begin(); it != profiles.end(); ++it) {
        const auto profile = it.value().toObject();
        if (QUuid(it.key()).isNull() || profile.size() != 2 || !profile.value("name").isString() ||
            profile.value("name").toString().trimmed().isEmpty() ||
            profile.value("name").toString().size() > 80 || !profile.value("settings").isObject())
            throw std::runtime_error("Invalid export profile.");
        luma::ExportSettings::fromJson(profile.value("settings").toObject());
    }
    settings_ = last;
    savedProfiles_ = profiles;
    storeBytes_ = bytes;
}
void ExportController::writeStore() {
    QDir().mkpath(QFileInfo(storePath_).absolutePath());
    QLockFile lock(storePath_ + QStringLiteral(".lock"));
    if (!lock.tryLock(0))
        throw std::runtime_error("Export settings are in use by another instance.");
    if (readFile(storePath_) != storeBytes_)
        throw std::runtime_error(
            "Export settings changed in another instance. Reopen Luma before saving.");
    const auto bytes =
        QJsonDocument(
            QJsonObject{{"schema", 1}, {"last", settings_.json()}, {"profiles", savedProfiles_}})
            .toJson();
    if (bytes.size() > 1024 * 1024)
        throw std::runtime_error("Export settings exceed 1 MB.");
    QSaveFile file(storePath_);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        throw std::runtime_error("Cannot save export settings atomically.");
    storeBytes_ = bytes;
}
QVariantList ExportController::profiles() const {
    QVariantList result;
    for (auto it = savedProfiles_.begin(); it != savedProfiles_.end(); ++it)
        result.append(QVariantMap{{"id", it.key()},
                                  {"name", it.value().toObject().value("name").toString()}});
    return result;
}
bool ExportController::configure(const QVariantMap &settings) {
    try {
        const auto parsed = luma::ExportSettings::fromJson(QJsonObject::fromVariantMap(settings));
        const auto previous = settings_;
        settings_ = parsed;
        try {
            writeStore();
        } catch (...) {
            settings_ = previous;
            throw;
        }
        error_.clear();
        emit settingsChanged();
        emit changed();
        return true;
    } catch (const std::exception &error) {
        error_ = QString::fromUtf8(error.what());
        emit changed();
        return false;
    }
}
void ExportController::saveProfile(const QString &name) {
    try {
        if (name.trimmed().isEmpty() || name.size() > 80 || name.contains(QChar(0)) ||
            savedProfiles_.size() >= 64)
            throw std::runtime_error(
                "Use a profile name of 1–80 characters (maximum 64 profiles).");
        const auto previous = savedProfiles_;
        savedProfiles_.insert(
            QUuid::createUuid().toString(QUuid::WithoutBraces),
            QJsonObject{{"name", name.trimmed()}, {"settings", settings_.json()}});
        try {
            writeStore();
        } catch (...) {
            savedProfiles_ = previous;
            throw;
        }
        error_.clear();
        emit settingsChanged();
        emit changed();
    } catch (const std::exception &error) {
        error_ = QString::fromUtf8(error.what());
        emit changed();
    }
}
void ExportController::loadProfile(const QString &id) {
    if (!savedProfiles_.contains(id))
        return;
    configure(savedProfiles_.value(id).toObject().value("settings").toObject().toVariantMap());
}
void ExportController::deleteProfile(const QString &id) {
    if (!savedProfiles_.contains(id))
        return;
    const auto previous = savedProfiles_;
    savedProfiles_.remove(id);
    try {
        writeStore();
        error_.clear();
        emit settingsChanged();
    } catch (const std::exception &error) {
        savedProfiles_ = previous;
        error_ = QString::fromUtf8(error.what());
    }
    emit changed();
}
void ExportController::start(bool selection) {
    startIDs(selection ? library_.selectionIDs()
                       : std::vector<std::string>{editor_.photoID().toStdString()});
}
void ExportController::startIDs(std::vector<std::string> ids) {
    if (working() || closing_)
        return;
    try {
        settings_.validate();
        if (ids.empty() ||
            std::any_of(ids.begin(), ids.end(), [](const auto &id) { return id.empty(); }))
            throw std::runtime_error("Select photos before exporting.");
        if (library_.working() || library_.selecting() || !editor_.canPrepareBatch())
            throw std::runtime_error("Wait for import, photo loading or batch processing.");
        if (ids.size() > 100000)
            throw std::runtime_error("Export queue exceeds 100000 photos.");
        snapshot_ = settings_;
        ids_ = std::move(ids);
        items_.clear();
        failures_.clear();
        completed_ = succeeded_ = skipped_ = 0;
        progress_ = 0;
        pause_ = false;
        current_.clear();
        error_.clear();
        cancellation_ = luma::CancellationSource{};
        phase_ = Phase::waiting;
        editorHeld_ = true;
        library_.setExternalBusy(true);
        editor_.prepareExternalEdits();
        status_ = QStringLiteral("Ожидаем сохранения правок…");
        preparation_.start();
        emit changed();
    } catch (const std::exception &error) {
        error_ = QString::fromUtf8(error.what());
        emit changed();
    }
}
void ExportController::releaseEditor() {
    if (!editorHeld_)
        return;
    editorHeld_ = false;
    editor_.finishExternalEdits({});
    library_.setExternalBusy(false);
}
void ExportController::plan() {
    if (phase_ != Phase::waiting || editor_.saving())
        return;
    preparation_.stop();
    phase_ = Phase::planning;
    status_ = QStringLiteral("Фиксируем очередь…");
    emit changed();
    const auto ids = ids_;
    const auto settings = snapshot_;
    const auto stop = cancellation_.token();
    const auto path = catalog_;
    const auto watermarkFolder = watermark_.path();
    const QPointer<ExportController> self(this);
    jobs_.submit(luma::JobPriority::background, [self, ids, settings, stop, path,
                                                 watermarkFolder](const auto &) mutable {
        std::vector<Item> items;
        QString error;
        auto frozen = settings;
        try {
            const auto folder = std::filesystem::canonical(settings.destination.toStdString());
            if (!std::filesystem::is_directory(folder))
                throw std::runtime_error("Export folder is not a directory.");
            luma::Catalog db(path);
            std::set<std::string> seen;
            for (const auto &id : ids) {
                luma::checkCancellation(stop);
                if (!seen.insert(id).second)
                    continue;
                auto snapshot = db.assetSnapshot(id);
                if (!snapshot)
                    throw std::runtime_error("Queued photo no longer exists.");
                items.push_back({std::move(snapshot->asset), {}, false});
            }
            std::sort(items.begin(), items.end(), [](const auto &a, const auto &b) {
                return std::tie(a.asset.path, a.asset.id) < std::tie(b.asset.path, b.asset.id);
            });
            std::set<QString> targets;
            const auto targetKey = [](const std::filesystem::path &target) {
                return QString::fromStdString(target.string())
                    .normalized(QString::NormalizationForm_C)
                    .toCaseFolded();
            };
            int sequence = 0;
            for (auto &item : items) {
                luma::checkCancellation(stop);
                const auto stamp = item.asset.metadata.captureTimestamp;
                const auto date = stamp ? QDateTime::fromSecsSinceEpoch(*stamp, QTimeZone::UTC)
                                              .toString(QStringLiteral("yyyy-MM-dd"))
                                        : QStringLiteral("undated");
                auto target =
                    folder / settings
                                 .filename(QString::fromStdString(item.asset.path.string()),
                                           ++sequence, date)
                                 .toStdString();
                const auto base = target;
                if (settings.conflict == luma::ExportConflict::unique) {
                    int suffix = 1;
                    while (std::filesystem::exists(target) || targets.contains(targetKey(target))) {
                        luma::checkCancellation(stop);
                        if (++suffix > 100000)
                            throw std::runtime_error("Cannot find a unique export filename.");
                        target = base.parent_path() /
                                 (base.stem().string() + "-" + std::to_string(suffix) +
                                  base.extension().string());
                    }
                } else if (targets.contains(targetKey(target)))
                    throw std::runtime_error(
                        "Filename template produces duplicate destinations. Add {sequence}.");
                targets.insert(targetKey(target));
                item.destination = target;
                item.skip = settings.conflict == luma::ExportConflict::skip &&
                            std::filesystem::exists(target);
            }
            if (frozen.options.watermark.kind == luma::WatermarkKind::image &&
                frozen.options.watermark.opacity > 0) {
                if (watermarkFolder.isEmpty())
                    throw std::runtime_error("Cannot create watermark snapshot folder.");
                const auto original = std::filesystem::canonical(frozen.options.watermark.image);
                if (std::filesystem::file_size(original) > 64000000)
                    throw std::runtime_error("Watermark exceeds 64 MB.");
                const auto copy =
                    std::filesystem::path(watermarkFolder.toStdString()) / "watermark";
                std::filesystem::copy_file(original, copy,
                                           std::filesystem::copy_options::overwrite_existing);
                frozen.options.watermark.image = copy;
                for (const auto &item : items)
                    if (std::filesystem::exists(item.destination) &&
                        std::filesystem::equivalent(original, item.destination))
                        throw std::runtime_error("Export cannot replace the watermark input.");
            }
            frozen.options.overwrite = settings.conflict == luma::ExportConflict::replace;
            luma::checkCancellation(stop);
        } catch (const std::exception &exception) {
            error = QString::fromUtf8(exception.what());
        }
        if (self)
            QMetaObject::invokeMethod(
                self,
                [self, items = std::move(items), frozen, error, stop]() mutable {
                    if (!self)
                        return;
                    self->releaseEditor();
                    if (!error.isEmpty() || stop.isCancelled()) {
                        if (!stop.isCancelled())
                            self->error_ = error;
                        self->finish(stop.isCancelled());
                        return;
                    }
                    self->items_ = std::move(items);
                    self->snapshot_ = frozen;
                    self->phase_ = Phase::running;
                    self->next();
                },
                Qt::QueuedConnection);
    });
}
void ExportController::next() {
    if (cancellation_.isCancelled()) {
        finish(true);
        return;
    }
    if (completed_ == total()) {
        finish(false);
        return;
    }
    if (pause_) {
        phase_ = Phase::paused;
        status_ = QStringLiteral("Очередь приостановлена");
        emit changed();
        return;
    }
    phase_ = Phase::running;
    const auto item = items_[static_cast<std::size_t>(completed_)];
    current_ = QString::fromStdString(item.destination.filename().string());
    progress_ = 0;
    status_ = QStringLiteral("Экспорт %1 / %2").arg(completed_ + 1).arg(total());
    emit changed();
    const auto stop = cancellation_.token();
    const auto options = snapshot_.options;
    const auto conflict = snapshot_.conflict;
    const QPointer<ExportController> self(this);
    jobs_.submit(
        luma::JobPriority::background, [this, self, item, stop, options, conflict](const auto &) {
            bool skipped = item.skip, committed = false;
            QString error;
            try {
                luma::checkCancellation(stop);
                // A file created after planning must also follow Skip; Unique never overwrites it.
                skipped = skipped || (conflict == luma::ExportConflict::skip &&
                                      std::filesystem::exists(item.destination));
                if (!skipped) {
                    runner_(item.asset, item.destination, options, stop, [self](double value) {
                        if (self)
                            QMetaObject::invokeMethod(
                                self,
                                [self, value] {
                                    if (self) {
                                        self->progress_ = value;
                                        emit self->changed();
                                    }
                                },
                                Qt::QueuedConnection);
                    });
                    committed = true;
                }
            } catch (const std::exception &exception) {
                error = QString::fromUtf8(exception.what());
            }
            if (self)
                QMetaObject::invokeMethod(
                    self,
                    [self, skipped, committed, error, stop, item] {
                        if (!self)
                            return;
                        if (committed || skipped || !stop.isCancelled()) {
                            ++self->completed_;
                            if (committed)
                                ++self->succeeded_;
                            else if (skipped)
                                ++self->skipped_;
                            else
                                self->failures_.append(
                                    QString::fromStdString(item.destination.filename().string()) +
                                    QStringLiteral(": ") + error);
                        }
                        self->next();
                    },
                    Qt::QueuedConnection);
        });
}
void ExportController::finish(bool cancelled) {
    preparation_.stop();
    releaseEditor();
    phase_ = Phase::idle;
    pause_ = false;
    progress_ = cancelled ? 0 : 1;
    status_ = (cancelled ? QStringLiteral("Отменено") : QStringLiteral("Готово")) +
              QStringLiteral(" · экспортировано %1 · пропущено %2 · ошибок %3")
                  .arg(succeeded_)
                  .arg(skipped_)
                  .arg(failures_.size());
    emit changed();
    if (closing_) {
        closing_ = false;
        QMetaObject::invokeMethod(this, [this] { emit closeReady(); }, Qt::QueuedConnection);
    }
}
void ExportController::pause() {
    if (working()) {
        pause_ = true;
        emit changed();
    }
}
void ExportController::resume() {
    pause_ = false;
    if (paused())
        next();
    else
        emit changed();
}
void ExportController::cancel() {
    if (!working())
        return;
    cancellation_.cancel();
    status_ = QStringLiteral("Отмена экспорта…");
    if (phase_ == Phase::waiting || phase_ == Phase::paused)
        finish(true);
    else
        emit changed();
}
bool ExportController::requestClose() {
    if (!working())
        return true;
    closing_ = true;
    cancel();
    return !working();
}
