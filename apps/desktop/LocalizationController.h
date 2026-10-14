#pragma once
#include <QObject>
#include <QTranslator>
#include <QVariantList>
#include <memory>

class QQmlEngine;
class LocalizationController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY changed)
    Q_PROPERTY(QVariantList languages READ languages CONSTANT)
    Q_PROPERTY(QString error READ error NOTIFY changed)
  public:
    LocalizationController(QString preferences, QQmlEngine &engine, QObject *parent = nullptr);
    ~LocalizationController() override;
    QString language() const {
        return language_;
    }
    QString error() const {
        return error_;
    }
    QVariantList languages() const;
    Q_INVOKABLE bool selectLanguage(const QString &language);
    void setLanguage(const QString &language) {
        selectLanguage(language);
    }
  signals:
    void changed();

  private:
    QString path_, language_ = QStringLiteral("en"), error_;
    QQmlEngine &engine_;
    std::unique_ptr<QTranslator> translator_;
    void activate(const QString &language);
};
