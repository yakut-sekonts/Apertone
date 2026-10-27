#pragma once
// Test-target factory adapter. This never links into the production application.
#include "apps/desktop/UpdateController.h"
#include "tests/ux/UxDriver.h"
#include <QObject>
#include <QTimer>
#include <utility>

namespace ux {
class NativeUpdaterProbe final : public NativeUpdater {
  public:
    inline static NativeUpdaterProbe *active = nullptr;
    explicit NativeUpdaterProbe(UpdateCallbacks callbacks) : callbacks_(std::move(callbacks)) {
        require(!active, "Only one NativeUpdater probe is allowed per isolated UX process");
        active = this;
    }
    ~NativeUpdaterProbe() override {
        active = nullptr;
    }
    bool start(QString &error) override {
        error.clear();
        started_ = true;
        ++starts;
        callbacks_.availability();
        return true;
    }
    void check(bool foreground) override {
        // This method runs from a QML clicked handler: never throw across Qt dispatch.
        if (!started_ || checking_ || foreground) {
            protocolFailure =
                QStringLiteral("SDK probe received an unsupported or overlapping check");
            return;
        }
        ++checks;
        checking_ = true;
        callbacks_.status(true, QStringLiteral("Checking for updates…"));
        // Uses the actual callbacks captured by UpdateController::makeNativeUpdater.
        // Delivery context is destroyed before callbacks_, cancelling pending delivery.
        QTimer::singleShot(0, &delivery_, [this] {
            checking_ = false;
            // Match ApertoneUpdateDelegate::didFinishUpdateCycleForUpdateCheck:error:
            // for an actual non-cancellation error: failed(), then status(false,...).
            callbacks_.failed();
            callbacks_.status(
                false,
                QStringLiteral("Could not update Apertone: UX controlled SDK check failure"));
            ++failuresDelivered;
        });
    }
    bool automatic() const override {
        return automatic_;
    }
    void setAutomatic(bool enabled) override {
        automatic_ = enabled;
        callbacks_.availability();
    }
    bool canCheck() const override {
        return started_ && !checking_;
    }
    void foundAsync(const QString &version) {
        require(started_ && !version.isEmpty(), "SDK notification requires an initialized probe");
        QTimer::singleShot(0, &delivery_, [this, version] {
            // Match the scheduled-reminder standardUserDriverWillHandleShowingUpdate
            // notification path (found only; no installer or popup is invoked).
            callbacks_.found(version);
            ++foundDelivered;
        });
    }
    int starts = 0, checks = 0, failuresDelivered = 0, foundDelivered = 0;
    QString protocolFailure;

  private:
    UpdateCallbacks callbacks_;
    bool started_ = false, checking_ = false, automatic_ = true;
    QObject delivery_; // Last member: cancels queued callbacks before callback storage dies.
};
} // namespace ux
