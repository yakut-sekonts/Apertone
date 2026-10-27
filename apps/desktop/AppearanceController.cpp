#include "apps/desktop/AppearanceController.h"

#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QStyleHints>
#include <algorithm>
#include <array>
#include <stdexcept>

namespace {
constexpr std::array supportedModes{QLatin1StringView("system"), QLatin1StringView("day"),
                                    QLatin1StringView("night")};

bool supportedMode(const QString &mode) {
    return std::ranges::any_of(supportedModes,
                               [&](const auto candidate) { return mode == candidate; });
}
} // namespace

AppearanceController::AppearanceController(QString preferences, QObject *parent)
    : QObject(parent), path_(std::move(preferences)) {
    try {
        QFile file(path_);
        if (file.exists()) {
            if (!file.open(QIODevice::ReadOnly) || file.size() > 4096)
                throw std::runtime_error("Cannot read appearance preferences.");
            QJsonParseError parse;
            const auto root = QJsonDocument::fromJson(file.readAll(), &parse).object();
            if (parse.error != QJsonParseError::NoError || root.size() != 2 ||
                root.value(QStringLiteral("schema")).toInt() != 1 ||
                !root.value(QStringLiteral("mode")).isString() ||
                !supportedMode(root.value(QStringLiteral("mode")).toString()))
                throw std::runtime_error("Invalid appearance preferences; system theme is active.");
            mode_ = root.value(QStringLiteral("mode")).toString();
        }
    } catch (const std::exception &exception) {
        error_ = QString::fromUtf8(exception.what());
    }
    applyColorScheme();
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (mode_ == QLatin1String("system"))
            emit changed();
    });
}
bool AppearanceController::dark() const {
    if (mode_ == QLatin1String("night"))
        return true;
    if (mode_ == QLatin1String("day"))
        return false;
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

void AppearanceController::applyColorScheme() {
    auto scheme = Qt::ColorScheme::Unknown;
    if (mode_ == QLatin1String("day"))
        scheme = Qt::ColorScheme::Light;
    else if (mode_ == QLatin1String("night"))
        scheme = Qt::ColorScheme::Dark;
    QGuiApplication::styleHints()->setColorScheme(scheme);
}

bool AppearanceController::selectMode(const QString &mode) {
    if (!supportedMode(mode)) {
        error_ = QStringLiteral("Unsupported appearance mode.");
        emit changed();
        return false;
    }
    if (mode == mode_) {
        if (!error_.isEmpty()) {
            error_.clear();
            emit changed();
        }
        return true;
    }
    QLockFile lock(path_ + QStringLiteral(".lock"));
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        error_ = QStringLiteral("Appearance preferences are busy.");
        emit changed();
        return false;
    }
    QSaveFile file(path_);
    const auto document =
        QJsonDocument(QJsonObject{{QStringLiteral("schema"), 1}, {QStringLiteral("mode"), mode}});
    const auto bytes = document.toJson(QJsonDocument::Compact);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        error_ = QStringLiteral("Cannot save appearance preferences.");
        emit changed();
        return false;
    }
    mode_ = mode;
    error_.clear();
    applyColorScheme();
    emit changed();
    return true;
}
