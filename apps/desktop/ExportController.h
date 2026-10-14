#pragma once
#include "apps/shared/ExportSettings.h"
#include "apps/shared/SessionService.h"
#include "core/jobs/JobManager.h"
#include <QObject>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

class EditorController;
class LibraryModel;
class ExportController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool working READ working NOTIFY changed)
    Q_PROPERTY(bool waitingForSave READ waitingForSave NOTIFY changed)
    Q_PROPERTY(bool paused READ paused NOTIFY changed)
    Q_PROPERTY(bool pausePending READ pausePending NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(int completed READ completed NOTIFY changed)
    Q_PROPERTY(int total READ total NOTIFY changed)
    Q_PROPERTY(int succeeded READ succeeded NOTIFY changed)
    Q_PROPERTY(int skipped READ skipped NOTIFY changed)
    Q_PROPERTY(QString currentFile READ currentFile NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QStringList failures READ failures NOTIFY changed)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY settingsChanged)
  public:
    using Runner = std::function<void(const luma::AssetRecord &, const std::filesystem::path &,
                                      luma::ExportOptions, luma::CancellationToken,
                                      const std::function<void(double)> &)>;
    ExportController(std::filesystem::path catalog, EditorController &editor, LibraryModel &library,
                     Runner runner = {}, QObject *parent = nullptr);
    ~ExportController() override;
    bool working() const {
        return phase_ != Phase::idle;
    }
    bool waitingForSave() const {
        return phase_ == Phase::waiting;
    }
    bool paused() const {
        return phase_ == Phase::paused;
    }
    bool pausePending() const {
        return pause_ && phase_ == Phase::running;
    }
    double progress() const {
        return progress_;
    }
    int completed() const {
        return completed_;
    }
    int total() const {
        return static_cast<int>(items_.size());
    }
    int succeeded() const {
        return succeeded_;
    }
    int skipped() const {
        return skipped_;
    }
    QString currentFile() const {
        return current_;
    }
    QString status() const {
        return status_;
    }
    QString error() const {
        return error_;
    }
    QStringList failures() const {
        return failures_;
    }
    QVariantMap settings() const {
        return settings_.json().toVariantMap();
    }
    QVariantList profiles() const;
    Q_INVOKABLE QString localPath(const QUrl &url) const {
        return url.isLocalFile() ? url.toLocalFile() : QString();
    }
    Q_INVOKABLE bool configure(const QVariantMap &settings);
    Q_INVOKABLE void saveProfile(const QString &name);
    Q_INVOKABLE void loadProfile(const QString &id);
    Q_INVOKABLE void deleteProfile(const QString &id);
    Q_INVOKABLE void start(bool selection);
    // Explicit IDs are also used by deterministic acceptance tests.
    void startIDs(std::vector<std::string> ids);
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE bool requestClose();
  signals:
    void changed();
    void settingsChanged();
    void closeReady();

  private:
    enum class Phase { idle, waiting, planning, running, paused };
    struct Item {
        luma::AssetRecord asset;
        std::filesystem::path destination;
        bool skip = false;
    };
    std::filesystem::path catalog_;
    QString storePath_;
    EditorController &editor_;
    LibraryModel &library_;
    Runner runner_;
    luma::JobManager jobs_;
    luma::CancellationSource cancellation_;
    QTimer preparation_;
    QTemporaryDir watermark_;
    luma::ExportSettings settings_, snapshot_;
    QJsonObject savedProfiles_;
    QByteArray storeBytes_;
    std::vector<std::string> ids_;
    std::vector<Item> items_;
    Phase phase_ = Phase::idle;
    bool pause_ = false, closing_ = false, editorHeld_ = false;
    int completed_ = 0, succeeded_ = 0, skipped_ = 0;
    double progress_ = 0;
    QString current_, status_, error_;
    QStringList failures_;
    void readStore();
    void writeStore();
    void plan();
    void next();
    void finish(bool cancelled);
    void releaseEditor();
};
