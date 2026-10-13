#include "apps/desktop/UpdateController.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <iostream>
#include <stdexcept>

namespace {
UpdateCallbacks callbacks;
bool enabled = true, checkingAllowed = true;
int checks = 0, installs = 0;
class FakeUpdater final : public NativeUpdater {
  public:
    bool start(QString &) override {
        return true;
    }
    void check(bool foreground) override {
        if (foreground)
            ++installs;
        else {
            ++checks;
            callbacks.status(true, QStringLiteral("checking"));
        }
    }
    bool automatic() const override {
        return enabled;
    }
    void setAutomatic(bool value) override {
        enabled = value;
    }
    bool canCheck() const override {
        return checkingAllowed;
    }
};
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
void wait(int milliseconds) {
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}
} // namespace
std::unique_ptr<NativeUpdater> makeNativeUpdater(UpdateCallbacks value) {
    callbacks = std::move(value);
    return std::make_unique<FakeUpdater>();
}
int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    try {
        bool saved = false, exportDone = false, importDone = false;
        UpdateController updates([&] { return saved && exportDone && importDone; });
        wait(1);
        updates.check();
        updates.check();
        require(checks == 1 && updates.checking(), "Duplicate check was not suppressed");
        callbacks.status(false, QStringLiteral("offline"));
        updates.check();
        require(checks == 2, "A failed check could not be retried");
        callbacks.found(QStringLiteral("0.4.1"));
        callbacks.status(false, QStringLiteral("available"));
        require(updates.availableVersion() == QStringLiteral("0.4.1"),
                "Bell lost available version");
        updates.install();
        require(installs == 1, "Install did not reach native updater");
        updates.setAutomatic(false);
        require(!updates.automatic(), "Automatic check preference ignored");
        int restarted = 0;
        updates.postponeInstallation([&] { ++restarted; });
        updates.postponeInstallation([&] { restarted += 100; });
        require(updates.restartPending() && !updates.canCheck(), "Restart did not lock new checks");
        wait(120);
        require(restarted == 0, "Restart ran before save");
        saved = true;
        wait(120);
        require(restarted == 0, "Restart ran during export");
        exportDone = true;
        wait(120);
        require(restarted == 0, "Restart ran during import");
        importDone = true;
        wait(240);
        require(restarted == 1 && !updates.restartPending(), "Restart did not run exactly once");
        callbacks.finished();
        require(updates.availableVersion().isEmpty(), "Completed session kept stale bell badge");
        {
            UpdateController destroyed([&] { return false; }, false);
            destroyed.postponeInstallation([&] { ++restarted; });
        }
        wait(120);
        require(restarted == 1, "Destroyed updater called deferred installer");
        std::cout
            << "Bell state, retries, preferences and save/export/import restart guard passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
