#pragma once
#include <QCoreApplication>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <type_traits>

// Retain the source template and arguments so an existing message can be retranslated.
// Values are inserted once, verbatim: filenames containing '%' are never reinterpreted.
class LocalizedText {
  public:
    LocalizedText() = default;
    LocalizedText(QString source) : source_(std::move(source)) {}
    template <typename T> LocalizedText arg(const T &value) const {
        auto result = *this;
        if constexpr (std::is_integral_v<T>) {
            if constexpr (std::is_signed_v<T>)
                result.arguments_.append(QString::number(static_cast<qlonglong>(value)));
            else
                result.arguments_.append(QString::number(static_cast<qulonglong>(value)));
        } else if constexpr (std::is_floating_point_v<T>)
            result.arguments_.append(QString::number(value));
        else
            result.arguments_.append(QString(value));
        return result;
    }
    QString text() const {
        const auto bytes = source_.toUtf8();
        const auto translated = QCoreApplication::translate("Apertone", bytes.constData());
        QString result;
        qsizetype end = 0;
        auto matches = QRegularExpression(QStringLiteral("%([1-9][0-9]*)")).globalMatch(translated);
        while (matches.hasNext()) {
            const auto match = matches.next();
            const auto index = match.captured(1).toInt() - 1;
            result += translated.mid(end, match.capturedStart() - end);
            result +=
                index >= 0 && index < arguments_.size() ? arguments_[index] : match.captured();
            end = match.capturedEnd();
        }
        return result + translated.mid(end);
    }

  private:
    QString source_;
    QStringList arguments_;
};
