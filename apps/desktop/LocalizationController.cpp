#include "apps/desktop/LocalizationController.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QLockFile>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QSaveFile>
#include <algorithm>
#include <array>
#include <stdexcept>

static void initializeLanguageResources() {
    Q_INIT_RESOURCE(luma_languages);
}
namespace {
struct Language {
    const char *code;
    const char *name;
};
constexpr std::array supported{Language{"en", "English"},     Language{"ru", "Русский"},
                               Language{"zh_CN", "简体中文"}, Language{"hy", "Հայերեն"},
                               Language{"de", "Deutsch"},     Language{"es", "Español"},
                               Language{"it", "Italiano"},    Language{"fr", "Français"}};
bool supportedLanguage(const QString &code) {
    return std::any_of(supported.begin(), supported.end(),
                       [&](const auto &language) { return code == QLatin1String(language.code); });
}
QStringList placeholders(const QString &text) {
    QStringList result;
    auto matches = QRegularExpression(QStringLiteral("%[1-9][0-9]*")).globalMatch(text);
    while (matches.hasNext())
        result.append(matches.next().captured());
    result.sort();
    return result;
}
class LanguagePack final : public QTranslator {
  public:
    explicit LanguagePack(const QString &language) {
        QFile file(QStringLiteral(":/luma/i18n/%1.json").arg(language));
        if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024)
            throw std::runtime_error("Cannot read language pack.");
        QJsonParseError parse;
        const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
        const auto root = document.object();
        if (parse.error != QJsonParseError::NoError || root.size() != 3 ||
            root.value("schema").toDouble() != 1 || root.value("locale").toString() != language ||
            !root.value("messages").isObject())
            throw std::runtime_error("Invalid language pack.");
        const auto messages = root.value("messages").toObject();
        for (auto it = messages.begin(); it != messages.end(); ++it) {
            if (it.key().isEmpty() || !it.value().isString() || it.value().toString().isEmpty() ||
                it.key().contains(QChar(0)) || it.value().toString().contains(QChar(0)) ||
                placeholders(it.key()) != placeholders(it.value().toString()))
                throw std::runtime_error("Invalid language-pack entry or placeholders.");
            entries_.insert(it.key(), it.value().toString());
        }
    }
    bool isEmpty() const override {
        return entries_.isEmpty();
    }
    QString translate(const char *, const char *source, const char *, int) const override {
        if (!source)
            return {};
        const auto text = QString::fromUtf8(source);
        if (auto entry = entries_.constFind(text); entry != entries_.cend())
            return entry.value();
        return {};
    }

  private:
    QHash<QString, QString> entries_;
};
} // namespace
LocalizationController::LocalizationController(QString preferences, QQmlEngine &engine,
                                               QObject *parent)
    : QObject(parent), path_(std::move(preferences)), engine_(engine) {
    initializeLanguageResources();
    activate(QStringLiteral("en"));
    try {
        QFile file(path_);
        if (!file.exists())
            return;
        if (!file.open(QIODevice::ReadOnly) || file.size() > 4096)
            throw std::runtime_error("Cannot read language preferences.");
        QJsonParseError parse;
        const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
        const auto root = document.object();
        if (parse.error != QJsonParseError::NoError || root.size() != 2 ||
            root.value("schema").toDouble() != 1 || !root.value("language").isString() ||
            !supportedLanguage(root.value("language").toString()))
            throw std::runtime_error("Invalid language preferences; English is active.");
        activate(root.value("language").toString());
    } catch (const std::exception &error) {
        error_ = QString::fromUtf8(error.what());
    }
}
LocalizationController::~LocalizationController() {
    QCoreApplication::removeTranslator(translator_.get());
}
QVariantList LocalizationController::languages() const {
    QVariantList result;
    for (const auto &language : supported)
        result.append(QVariantMap{{"code", QString::fromLatin1(language.code)},
                                  {"name", QString::fromUtf8(language.name)}});
    return result;
}
void LocalizationController::activate(const QString &language) {
    auto next = std::make_unique<LanguagePack>(language);
    if (translator_)
        QCoreApplication::removeTranslator(translator_.get());
    translator_ = std::move(next);
    QCoreApplication::installTranslator(translator_.get());
    language_ = language;
    QLocale::setDefault(QLocale(language));
    engine_.setUiLanguage(language);
    engine_.retranslate();
}
bool LocalizationController::selectLanguage(const QString &language) {
    try {
        if (!supportedLanguage(language))
            throw std::runtime_error("Unsupported interface language.");
        // Validate before writing, so an unreadable pack cannot become a persisted choice.
        const LanguagePack validated(language);
        if (!QDir().mkpath(QFileInfo(path_).absolutePath()))
            throw std::runtime_error("Cannot create preferences folder.");
        QLockFile lock(path_ + QStringLiteral(".lock"));
        if (!lock.tryLock(0))
            throw std::runtime_error("Language preferences are in use.");
        const auto bytes =
            QJsonDocument(QJsonObject{{"schema", 1}, {"language", language}}).toJson();
        QSaveFile file(path_);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
            throw std::runtime_error("Cannot save language preferences.");
        activate(language);
        error_.clear();
        emit changed();
        return true;
    } catch (const std::exception &error) {
        error_ = QString::fromUtf8(error.what());
        emit changed();
        return false;
    }
}
