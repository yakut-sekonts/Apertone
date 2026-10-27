#pragma once

#include <QObject>

class AppearanceController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY changed)
    Q_PROPERTY(bool dark READ dark NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)

  public:
    explicit AppearanceController(QString preferences, QObject *parent = nullptr);

    QString mode() const {
        return mode_;
    }
    bool dark() const;
    QString error() const {
        return error_;
    }

    Q_INVOKABLE bool selectMode(const QString &mode);
    void setMode(const QString &mode) {
        selectMode(mode);
    }

  signals:
    void changed();

  private:
    QString path_;
    QString mode_ = QStringLiteral("system");
    QString error_;

    void applyColorScheme();
};
