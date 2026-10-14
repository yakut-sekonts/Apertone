#pragma once
#include "core/export/ExportOptions.h"
#include <QJsonObject>
#include <QString>

namespace luma {
enum class ExportConflict { skip, unique, replace };
struct ExportSettings {
    ExportOptions options;
    QString destination;
    QString naming = QStringLiteral("{filename}");
    ExportConflict conflict = ExportConflict::unique;
    void validate() const;
    QJsonObject json() const;
    static ExportSettings fromJson(const QJsonObject &object);
    QString filename(const QString &source, int sequence, const QString &date) const;
};
} // namespace luma
