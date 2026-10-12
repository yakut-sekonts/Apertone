#include "apps/desktop/EditorController.h"
#include "core/render/CpuRenderer.h"
#include "platform/macos/DisplayProfile.h"
#include <QGuiApplication>
#include <QMetaObject>
#include <QPointer>
#include <QScreen>
#include <cmath>

namespace {
QString display(std::optional<double> value, const QString &suffix = {}) {
    return value ? QString::number(*value, 'g', 4) + suffix : QStringLiteral("—");
}
} // namespace

EditorController::EditorController(std::filesystem::path catalog, PreviewProvider *provider,
                                   QObject *parent)
    : QObject(parent), catalog_(std::move(catalog)), service_(catalog_), provider_(provider) {
    saveTimer_.setSingleShot(true);
    saveTimer_.setInterval(180);
    connect(&saveTimer_, &QTimer::timeout, this, &EditorController::flushSave);
}
EditorController::~EditorController() {
    ioJobs_.shutdown();
    renderJobs_.shutdown();
}
QString EditorController::filename() const {
    return session_ ? QString::fromStdString(session_->asset.path.filename().string()) : QString();
}
QString EditorController::camera() const {
    return session_ ? QString::fromStdString(session_->asset.metadata.camera) : QStringLiteral("—");
}
QString EditorController::lens() const {
    return session_ && !session_->asset.metadata.lens.empty()
               ? QString::fromStdString(session_->asset.metadata.lens)
               : QStringLiteral("Не указан в RAW");
}
QString EditorController::iso() const {
    return session_ ? display(session_->asset.metadata.iso) : QStringLiteral("—");
}
QString EditorController::shutter() const {
    if (!session_ || !session_->asset.metadata.shutterSeconds) {
        return QStringLiteral("—");
    }
    const double seconds = *session_->asset.metadata.shutterSeconds;
    return seconds < 1 ? QStringLiteral("1/%1 с").arg(std::round(1 / seconds))
                       : display(seconds, QStringLiteral(" с"));
}
QString EditorController::aperture() const {
    return session_ ? QStringLiteral("f/") + display(session_->asset.metadata.aperture)
                    : QStringLiteral("—");
}
QString EditorController::focalLength() const {
    return session_ ? display(session_->asset.metadata.focalLength, QStringLiteral(" мм"))
                    : QStringLiteral("—");
}
QString EditorController::dimensions() const {
    if (!session_) {
        return {};
    }
    const auto &m = session_->asset.metadata;
    return QStringLiteral("%1 × %2")
        .arg(m.orientation & 4 ? m.height : m.width)
        .arg(m.orientation & 4 ? m.width : m.height);
}
QString EditorController::catalogPath() const {
    return QString::fromStdString(catalog_.string());
}
QString EditorController::previewUrl() const {
    return imageRevision_ > 0 ? QStringLiteral("image://preview/current?v=%1").arg(imageRevision_)
                              : QString();
}

void EditorController::setWindow(QQuickWindow *window) {
    window_ = window;
    connect(window, &QQuickWindow::screenChanged, this, [this] { refreshDisplayProfile(); });
    refreshDisplayProfile();
}
void EditorController::refreshDisplayProfile() {
    displayProfile_ = window_ && QGuiApplication::platformName() == QStringLiteral("cocoa")
                          ? luma::macos::displayProfile(window_->winId())
                          : std::vector<std::uint8_t>{};
    if (session_) {
        render();
    }
}
void EditorController::restore() {
    load(std::nullopt);
}
void EditorController::openFile(const QUrl &url) {
    if (!url.isLocalFile() || busy_ || closing_) {
        return;
    }
    if (saving()) {
        deferredOpen_ = url;
        if (saveFailed_) {
            retrySave();
        } else {
            flushSave();
        }
        return;
    }
    load(std::filesystem::path(url.toLocalFile().toStdString()));
}
void EditorController::load(std::optional<std::filesystem::path> source) {
    loadJob_.cancel();
    renderJob_.cancel();
    const auto revision = ++loadRevision_;
    ++renderRevision_;
    busy_ = true;
    progress_ = 0;
    error_.clear();
    status_ =
        source ? QStringLiteral("Декодирование RAW…") : QStringLiteral("Восстановление сессии…");
    emit stateChanged();
    const QPointer<EditorController> self(this);
    loadJob_ = ioJobs_.submit(luma::JobPriority::background, [this, self, source, revision](
                                                                 const luma::JobContext &context) {
        try {
            const auto progress = [self, revision, context](double value) {
                context.setProgress(value);
                if (self) {
                    QMetaObject::invokeMethod(
                        self,
                        [self, revision, value] {
                            if (self && self->loadRevision_ == revision) {
                                self->progress_ = value;
                                emit self->stateChanged();
                            }
                        },
                        Qt::QueuedConnection);
                }
            };
            auto result = source ? std::optional<luma::PhotoSession>(
                                       service_.open(*source, context.stopToken(), progress))
                                 : service_.restore(context.stopToken(), progress);
            if (self) {
                QMetaObject::invokeMethod(
                    self,
                    [self, revision, result = std::move(result)]() mutable {
                        if (!self || self->loadRevision_ != revision) {
                            return;
                        }
                        self->busy_ = false;
                        if (result) {
                            self->session_ = std::move(result);
                            self->exposure_ = self->session_->asset.edits.exposure;
                            self->original_ = false;
                            self->render();
                        }
                        self->status_ =
                            self->session_
                                ? QStringLiteral("Сессия восстановлена · правки сохранены")
                                : QStringLiteral("Откройте RAW, чтобы начать");
                        emit self->stateChanged();
                        self->checkClose();
                    },
                    Qt::QueuedConnection);
            }
        } catch (const std::exception &error) {
            const auto message = QString::fromUtf8(error.what());
            const bool cancelled = context.stopToken().isCancelled();
            if (self) {
                QMetaObject::invokeMethod(
                    self,
                    [self, revision, message, cancelled] {
                        if (!self || self->loadRevision_ != revision) {
                            return;
                        }
                        self->busy_ = false;
                        if (!cancelled) {
                            self->error_ = message;
                        }
                        self->status_ =
                            cancelled ? QStringLiteral("Операция отменена")
                                      : QStringLiteral(
                                            "Не удалось открыть RAW; каталог и правки сохранены");
                        if (self->session_) {
                            self->render();
                        }
                        emit self->stateChanged();
                        self->checkClose();
                    },
                    Qt::QueuedConnection);
            }
        }
    });
}
void EditorController::setExposure(double value) {
    if (!session_ || busy_ || closing_ || !std::isfinite(value) || value < -5 || value > 5 ||
        value == exposure_) {
        return;
    }
    exposure_ = value;
    ++editRevision_;
    pendingSave_ = true;
    saveTimer_.start();
    status_ = QStringLiteral("Сохраняем правку…");
    emit stateChanged();
    render();
}
void EditorController::setOriginal(bool value) {
    if (original_ == value || !session_) {
        return;
    }
    original_ = value;
    emit stateChanged();
    render();
}
void EditorController::render() {
    if (!session_) {
        return;
    }
    renderJob_.cancel();
    const auto revision = ++renderRevision_;
    const auto image = session_->preview;
    const int orientation = session_->asset.metadata.orientation;
    const double ev = original_ ? 0 : exposure_;
    const auto profile = displayProfile_;
    const QPointer<EditorController> self(this);
    renderJob_ = renderJobs_.submit(
        luma::JobPriority::interactive,
        [self, image, orientation, ev, profile, revision](const luma::JobContext &context) {
            try {
                const auto pixels =
                    luma::renderCPU(*image, {ev}, orientation, profile, context.stopToken());
                QImage output(pixels.rgba.data(), pixels.width, pixels.height, pixels.width * 4,
                              QImage::Format_RGBA8888);
                auto owned = output.copy();
                if (self) {
                    QMetaObject::invokeMethod(
                        self,
                        [self, revision, owned = std::move(owned)]() mutable {
                            if (!self || self->renderRevision_ != revision) {
                                return;
                            }
                            self->provider_->update(std::move(owned));
                            ++self->imageRevision_;
                            emit self->stateChanged();
                        },
                        Qt::QueuedConnection);
                }
            } catch (const std::exception &error) {
                if (context.stopToken().isCancelled()) {
                    return;
                }
                const auto message = QString::fromUtf8(error.what());
                if (self) {
                    QMetaObject::invokeMethod(
                        self,
                        [self, revision, message] {
                            if (self && self->renderRevision_ == revision) {
                                self->error_ = message;
                                emit self->stateChanged();
                            }
                        },
                        Qt::QueuedConnection);
                }
            }
        });
}
void EditorController::flushSave() {
    saveTimer_.stop();
    if (!pendingSave_ || !session_) {
        checkClose();
        return;
    }
    pendingSave_ = false;
    ++pendingWrites_;
    const auto id = session_->asset.id;
    const double ev = exposure_;
    const auto revision = editRevision_;
    const QPointer<EditorController> self(this);
    emit stateChanged();
    ioJobs_.submit(
        luma::JobPriority::background, [this, self, id, ev, revision](const luma::JobContext &) {
            QString error;
            try {
                service_.save(id, ev);
            } catch (const std::exception &failure) {
                error = QString::fromUtf8(failure.what());
            }
            if (self) {
                QMetaObject::invokeMethod(
                    self,
                    [self, ev, revision, error] {
                        if (!self) {
                            return;
                        }
                        --self->pendingWrites_;
                        if (!error.isEmpty()) {
                            self->saveFailed_ = true;
                            self->error_ = error;
                            self->status_ =
                                QStringLiteral("Правка ещё не сохранена. Повторите запись.");
                            self->closing_ = false;
                        } else if (self->editRevision_ == revision) {
                            self->saveFailed_ = false;
                            self->session_->asset.edits.exposure = ev;
                            self->status_ = QStringLiteral("Правки сохранены автоматически");
                        }
                        emit self->stateChanged();
                        if (!self->saving() && !self->deferredOpen_.isEmpty()) {
                            const auto url = self->deferredOpen_;
                            self->deferredOpen_ = QUrl();
                            self->openFile(url);
                        }
                        self->checkClose();
                    },
                    Qt::QueuedConnection);
            }
        });
}
void EditorController::retrySave() {
    if (!session_ || pendingWrites_ > 0) {
        return;
    }
    pendingSave_ = true;
    saveFailed_ = false;
    error_.clear();
    flushSave();
}
void EditorController::cancelLoad() {
    loadJob_.cancel();
}
bool EditorController::requestClose() {
    if (!busy_ && !saving()) {
        return true;
    }
    if (saveFailed_) {
        error_ = QStringLiteral("Последняя правка не сохранена. Повторите запись перед закрытием.");
        emit stateChanged();
        return false;
    }
    closing_ = true;
    loadJob_.cancel();
    renderJob_.cancel();
    flushSave();
    return false;
}
void EditorController::checkClose() {
    if (closing_ && !busy_ && !saving()) {
        emit closeReady();
    }
}
