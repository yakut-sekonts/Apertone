#include "apps/desktop/UpdateController.h"
#include <utility>

UpdateController::UpdateController(std::function<bool()> ready, bool enabled, QObject *parent)
    : QObject(parent), ready_(std::move(ready)) {
    restartTimer_.setInterval(100);
    connect(&restartTimer_, &QTimer::timeout, this, [this] {
        if (!install_ || !ready_())
            return;
        restartTimer_.stop();
        auto action = std::exchange(install_, {});
        message_ = QStringLiteral("Installing update and restarting Luma…");
        emit changed();
        action();
    });
    if (!enabled) {
        message_ = QStringLiteral("Updates are disabled in test mode");
        return;
    }
    native_ = makeNativeUpdater(
        {[this](QString version) {
             version_ = std::move(version);
             message_ = QStringLiteral("A new version of Luma is available");
             emit changed();
         },
         [this](bool checking, QString message) {
             checking_ = checking;
             message_ = std::move(message);
             emit changed();
         },
         [this] {
             version_.clear();
             emit changed();
         },
         [this](std::function<void()> action) { postponeInstallation(std::move(action)); },
         [this] { emit changed(); },
         [this] {
             restartTimer_.stop();
             install_ = {};
             restarting_ = false;
             emit changed();
         }});
    QTimer::singleShot(0, this, [this] {
        QString error;
        if (!native_->start(error))
            message_ = error;
        emit changed();
    });
}
UpdateController::~UpdateController() = default;
bool UpdateController::canCheck() const {
    return native_ && native_->canCheck() && !restartPending();
}
bool UpdateController::automatic() const {
    return native_ && native_->automatic();
}
void UpdateController::setAutomatic(bool enabled) {
    if (native_)
        native_->setAutomatic(enabled);
    emit changed();
}
void UpdateController::check() {
    if (canCheck() && !checking_)
        native_->check(false);
}
void UpdateController::install() {
    if (native_ && !version_.isEmpty() && !restartPending())
        native_->check(true);
}
void UpdateController::postponeInstallation(std::function<void()> action) {
    if (restarting_ || !action)
        return;
    restarting_ = true;
    install_ = std::move(action);
    message_ = QStringLiteral("Waiting for edits and active operations before updating…");
    emit preparingRestart();
    emit changed();
    restartTimer_.start();
}
