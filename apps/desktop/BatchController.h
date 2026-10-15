#pragma once
#include "apps/desktop/LocalizedText.h"
#include "apps/shared/PresetStore.h"
#include "core/catalog/Catalog.h"
#include "core/jobs/JobManager.h"
#include <QObject>
#include <QTimer>

class EditorController;
class LibraryModel;
class PresetController;
class BatchController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool working READ working NOTIFY changed)
    Q_PROPERTY(bool waitingForSave READ waitingForSave NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
  public:
    BatchController(std::filesystem::path catalog, EditorController &editor, LibraryModel &library,
                    PresetController &presets, QObject *parent = nullptr);
    ~BatchController() override;
    bool working() const {
        return phase_ != Phase::idle;
    }
    bool waitingForSave() const {
        return phase_ == Phase::waiting;
    }
    bool canUndo() const {
        return undo_ && !working();
    }
    bool canRedo() const {
        return redo_ && !working();
    }
    double progress() const {
        return progress_;
    }
    QString status() const {
        return status_.text();
    }
    QString error() const {
        return error_;
    }
    Q_INVOKABLE void applyPreset(const QString &id);
    Q_INVOKABLE void paste();
    Q_INVOKABLE void sync(int groups);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE bool requestClose();
  signals:
    void changed();
    void closeReady();

  private:
    enum class Phase { idle, waiting, running };
    enum class Action { apply, undo, redo };
    struct Request {
        Action action;
        std::vector<std::string> ids;
        luma::EditDocument source;
        std::uint32_t groups = 0;
        QString label;
        std::vector<std::string> parameters{};
    };
    std::filesystem::path path_;
    EditorController &editor_;
    LibraryModel &library_;
    PresetController &presets_;
    luma::JobManager jobs_;
    // Created, queried and destroyed on the one batch worker.
    std::unique_ptr<luma::Catalog> catalog_;
    luma::CancellationSource cancellation_;
    QTimer preparation_;
    std::optional<Request> request_;
    Phase phase_ = Phase::idle;
    bool undo_ = false, redo_ = false, closing_ = false;
    double progress_ = 0;
    LocalizedText status_;
    QString error_;
    void apply(const luma::DevelopPreset &preset);
    void start(Request request);
    void run();
    void finish(QString error, LocalizedText status, bool undo, bool redo,
                std::optional<luma::EditDocument> active, bool committedChanges = false);
    void failure(const std::exception &error);
};
