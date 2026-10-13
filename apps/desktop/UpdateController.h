#pragma once
#include <QObject>
#include <QTimer>
#include <functional>
#include <memory>

struct UpdateCallbacks {
    std::function<void(QString)> found;
    std::function<void(bool, QString)> status;
    std::function<void()> finished;
    std::function<void(std::function<void()>)> install;
};
class NativeUpdater {
  public:
    virtual ~NativeUpdater() = default;
    virtual bool start(QString &error) = 0;
    virtual void check(bool foreground) = 0;
    virtual bool automatic() const = 0;
    virtual void setAutomatic(bool enabled) = 0;
    virtual bool canCheck() const = 0;
};
std::unique_ptr<NativeUpdater> makeNativeUpdater(UpdateCallbacks callbacks);

class UpdateController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString availableVersion READ availableVersion NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(bool checking READ checking NOTIFY changed)
    Q_PROPERTY(bool canCheck READ canCheck NOTIFY changed)
    Q_PROPERTY(bool automatic READ automatic WRITE setAutomatic NOTIFY changed)
    Q_PROPERTY(bool restartPending READ restartPending NOTIFY changed)
  public:
    // A readiness guard must include pending/failed saves and active jobs.
    explicit UpdateController(std::function<bool()> ready, bool enabled = true,
                              QObject *parent = nullptr);
    ~UpdateController() override;
    QString availableVersion() const {
        return version_;
    }
    QString message() const {
        return message_;
    }
    bool checking() const {
        return checking_;
    }
    bool canCheck() const;
    bool automatic() const;
    bool restartPending() const {
        return bool(install_);
    }
    void setAutomatic(bool enabled);
    Q_INVOKABLE void check();
    Q_INVOKABLE void install();
    // Used by the native delegate; always waits at least one event-loop turn.
    void postponeInstallation(std::function<void()> action);
  signals:
    void changed();
    void preparingRestart();

  private:
    QString version_, message_ = QStringLiteral("Обновления ещё не проверялись");
    bool checking_ = false;
    std::unique_ptr<NativeUpdater> native_;
    std::function<bool()> ready_;
    std::function<void()> install_;
    QTimer restartTimer_;
};
