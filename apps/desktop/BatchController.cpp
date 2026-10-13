#include "apps/desktop/BatchController.h"
#include "apps/desktop/EditorController.h"
#include "apps/desktop/LibraryModel.h"
#include "apps/desktop/PresetController.h"
#include <QPointer>
#include <future>

BatchController::BatchController(std::filesystem::path catalog, EditorController &editor,
                                 LibraryModel &library, PresetController &presets, QObject *parent)
    : QObject(parent), path_(std::move(catalog)), editor_(editor), library_(library),
      presets_(presets) {
    preparation_.setInterval(40);
    connect(&preparation_, &QTimer::timeout, this, &BatchController::run);
}
BatchController::~BatchController() {
    cancellation_.cancel();
    try {
        auto completed = std::make_shared<std::promise<void>>();
        auto ready = completed->get_future();
        jobs_.submit(luma::JobPriority::background, [this, completed](const auto &) {
            catalog_.reset();
            completed->set_value();
        });
        ready.wait();
    } catch (...) {
        // If cleanup cannot be queued, join before releasing the connection.
        // NOMUTEX permits transfer after all access on its worker has stopped.
        jobs_.shutdown();
        catalog_.reset();
        return;
    }
    jobs_.shutdown();
}
void BatchController::failure(const std::exception &error) {
    error_ = QString::fromUtf8(error.what());
    emit changed();
}
void BatchController::apply(const luma::DevelopPreset &preset) {
    preset.validate();
    start({Action::apply, library_.selectionIDs(), preset.settings, preset.groups, preset.name});
}
void BatchController::applyPreset(const QString &id) {
    try {
        apply(presets_.snapshot(id));
    } catch (const std::exception &error) {
        failure(error);
    }
}
void BatchController::paste() {
    try {
        const auto copied = presets_.copiedSettings();
        if (!copied)
            throw std::runtime_error("Copy settings before pasting to the selection.");
        apply(*copied);
    } catch (const std::exception &error) {
        failure(error);
    }
}
void BatchController::sync(int groups) {
    try {
        if (!editor_.hasPhoto())
            throw std::runtime_error("Open the source photo before syncing settings.");
        const auto settings = editor_.currentSettings();
        const auto selectedGroups = static_cast<std::uint32_t>(groups);
        luma::validateSettingsGroups(selectedGroups);
        settings.validate();
        start(
            {Action::apply, library_.selectionIDs(), settings, selectedGroups, editor_.filename()});
    } catch (const std::exception &error) {
        failure(error);
    }
}
void BatchController::undo() {
    if (canUndo())
        start({Action::undo, {}, {}, 0, {}});
}
void BatchController::redo() {
    if (canRedo())
        start({Action::redo, {}, {}, 0, {}});
}
void BatchController::start(Request request) {
    if (working() || closing_)
        return;
    if (library_.working() || library_.selecting() || !editor_.canPrepareBatch()) {
        error_ = QStringLiteral("Дождитесь импорта, переключения фото или экспорта.");
        emit changed();
        return;
    }
    if (request.action == Action::apply && request.ids.empty()) {
        error_ = QStringLiteral("Выберите фотографии в Library.");
        emit changed();
        return;
    }
    request_ = std::move(request);
    cancellation_ = luma::CancellationSource{};
    phase_ = Phase::waiting;
    progress_ = 0;
    error_.clear();
    status_ = QStringLiteral("Ожидаем сохранения активного фото…");
    library_.setExternalBusy(true);
    editor_.prepareExternalEdits();
    preparation_.start();
    emit changed();
}
void BatchController::run() {
    if (phase_ != Phase::waiting || !request_ || editor_.saving())
        return;
    const auto request = std::move(*request_);
    request_.reset();
    preparation_.stop();
    const auto active = editor_.photoID().toStdString();
    const auto stop = cancellation_.token();
    const QPointer<BatchController> self(this);
    phase_ = Phase::running;
    status_ = request.action == Action::apply
                  ? QStringLiteral("%1 · %2 фото").arg(request.label).arg(request.ids.size())
                  : QStringLiteral("Пакетный Undo/Redo…");
    emit changed();
    jobs_.submit(luma::JobPriority::background, [this, self, request, active, stop](const auto &) {
        QString error, status;
        std::optional<luma::EditDocument> updated;
        try {
            if (!catalog_)
                catalog_ = std::make_unique<luma::Catalog>(path_);
            const auto progress = [self](double value) {
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
            };
            const auto result =
                request.action == Action::apply
                    ? catalog_->applyBatchEdits(request.ids, request.source, request.groups, stop,
                                                progress, active)
                    : catalog_->restoreBatchEdits(request.action == Action::redo, stop, active);
            updated = result.active;
            status = result.changed
                         ? QStringLiteral("Готово · изменено %1 фото").arg(result.changed)
                         : QStringLiteral("Настройки уже совпадают · фото не изменены");
        } catch (const std::exception &exception) {
            error = stop.isCancelled() ? QString() : QString::fromUtf8(exception.what());
            status = stop.isCancelled()
                         ? QStringLiteral("Пакетная операция отменена · изменения откатились")
                         : QStringLiteral("Пакетная операция не выполнена");
        }
        const bool undo = catalog_ && catalog_->canUndoBatch();
        const bool redo = catalog_ && catalog_->canRedoBatch();
        if (self)
            QMetaObject::invokeMethod(
                self,
                [self, error, status, undo, redo, updated] {
                    if (self)
                        self->finish(error, status, undo, redo, updated);
                },
                Qt::QueuedConnection);
    });
}
void BatchController::finish(QString error, QString status, bool undo, bool redo,
                             std::optional<luma::EditDocument> active) {
    phase_ = Phase::idle;
    undo_ = undo;
    redo_ = redo;
    error_ = std::move(error);
    status_ = std::move(status);
    progress_ = 1;
    editor_.finishExternalEdits(active);
    library_.setExternalBusy(false);
    emit changed();
    if (closing_)
        QMetaObject::invokeMethod(
            this,
            [this] {
                closing_ = false;
                emit closeReady();
            },
            Qt::QueuedConnection);
}
void BatchController::cancel() {
    if (!working())
        return;
    cancellation_.cancel();
    if (phase_ == Phase::waiting) {
        preparation_.stop();
        request_.reset();
        finish({}, QStringLiteral("Пакетная операция отменена до записи"), undo_, redo_, {});
    } else {
        status_ = QStringLiteral("Отмена и откат изменений…");
        emit changed();
    }
}
bool BatchController::requestClose() {
    if (!working())
        return true;
    closing_ = true;
    cancel();
    return !working();
}
