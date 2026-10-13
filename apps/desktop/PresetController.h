#pragma once
#include "apps/shared/PresetStore.h"
#include <QObject>
#include <QUrl>
#include <QVariantList>

class EditorController;
class PresetController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(bool hasCopiedSettings READ hasCopiedSettings NOTIFY stateChanged)
  public:
    PresetController(QString path, EditorController &editor, QObject *parent = nullptr);
    QVariantList items() const;
    QString error() const {
        return error_;
    }
    bool hasCopiedSettings() const {
        return copied_.has_value();
    }
    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool save(const QString &id, const QString &name, const QString &folder, int groups,
                          bool captureCurrent);
    Q_INVOKABLE bool remove(const QString &id);
    Q_INVOKABLE bool importPreset(const QUrl &url);
    Q_INVOKABLE bool exportPreset(const QString &id, const QUrl &url);
    Q_INVOKABLE void apply(const QString &id);
    Q_INVOKABLE void preview(const QString &id);
    Q_INVOKABLE void clearPreview();
    Q_INVOKABLE void copySettings(int groups);
    Q_INVOKABLE void pasteSettings();
  signals:
    void itemsChanged();
    void stateChanged();

  private:
    QString path_;
    luma::PresetStore store_;
    EditorController &editor_;
    std::vector<luma::StoredPreset> presets_;
    std::optional<luma::DevelopPreset> copied_;
    QString error_;
    const luma::DevelopPreset &find(const QString &id) const;
    bool failure(const std::exception &error);
};
