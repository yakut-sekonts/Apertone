#pragma once
#include "apps/desktop/PreviewProvider.h"
#include "apps/shared/SessionService.h"
#include "core/jobs/JobManager.h"
#include <QObject>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>

class EditorController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool saving READ saving NOTIFY stateChanged)
    Q_PROPERTY(bool hasPhoto READ hasPhoto NOTIFY stateChanged)
    Q_PROPERTY(bool original READ original WRITE setOriginal NOTIFY stateChanged)
    Q_PROPERTY(double exposure READ exposure WRITE setExposure NOTIFY stateChanged)
    Q_PROPERTY(double progress READ progress NOTIFY stateChanged)
    Q_PROPERTY(QString filename READ filename NOTIFY stateChanged)
    Q_PROPERTY(QString camera READ camera NOTIFY stateChanged)
    Q_PROPERTY(QString lens READ lens NOTIFY stateChanged)
    Q_PROPERTY(QString iso READ iso NOTIFY stateChanged)
    Q_PROPERTY(QString shutter READ shutter NOTIFY stateChanged)
    Q_PROPERTY(QString aperture READ aperture NOTIFY stateChanged)
    Q_PROPERTY(QString focalLength READ focalLength NOTIFY stateChanged)
    Q_PROPERTY(QString dimensions READ dimensions NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString previewUrl READ previewUrl NOTIFY stateChanged)
    Q_PROPERTY(QString catalogPath READ catalogPath CONSTANT)

  public:
    EditorController(std::filesystem::path catalog, PreviewProvider *provider,
                     QObject *parent = nullptr);
    ~EditorController() override;
    bool busy() const {
        return busy_;
    }
    bool saving() const {
        return pendingSave_ || pendingWrites_ > 0 || saveFailed_;
    }
    bool hasPhoto() const {
        return session_.has_value();
    }
    bool original() const {
        return original_;
    }
    double exposure() const {
        return exposure_;
    }
    double progress() const {
        return progress_;
    }
    QString filename() const;
    QString camera() const;
    QString lens() const;
    QString iso() const;
    QString shutter() const;
    QString aperture() const;
    QString focalLength() const;
    QString dimensions() const;
    QString status() const {
        return status_;
    }
    QString error() const {
        return error_;
    }
    QString previewUrl() const;
    QString catalogPath() const;
    void restore();
    void setWindow(QQuickWindow *window);
    Q_INVOKABLE void openFile(const QUrl &url);
    Q_INVOKABLE void setExposure(double value);
    Q_INVOKABLE void setOriginal(bool value);
    Q_INVOKABLE bool requestClose();
    Q_INVOKABLE void retrySave();
    Q_INVOKABLE void cancelLoad();
  signals:
    void stateChanged();
    void closeReady();

  private:
    std::filesystem::path catalog_;
    luma::SessionService service_;
    PreviewProvider *provider_;
    luma::JobManager ioJobs_;
    luma::JobManager renderJobs_;
    luma::JobHandle loadJob_;
    luma::JobHandle renderJob_;
    std::optional<luma::PhotoSession> session_;
    QTimer saveTimer_;
    QQuickWindow *window_ = nullptr;
    std::vector<std::uint8_t> displayProfile_;
    bool busy_ = false;
    bool original_ = false;
    bool pendingSave_ = false;
    bool saveFailed_ = false;
    bool closing_ = false;
    int pendingWrites_ = 0;
    double exposure_ = 0;
    double progress_ = 0;
    std::uint64_t loadRevision_ = 0;
    std::uint64_t renderRevision_ = 0;
    std::uint64_t imageRevision_ = 0;
    std::uint64_t editRevision_ = 0;
    QUrl deferredOpen_;
    QString status_ = QStringLiteral("Готово к работе");
    QString error_;
    void load(std::optional<std::filesystem::path> source);
    void render();
    void flushSave();
    void checkClose();
    void refreshDisplayProfile();
};
