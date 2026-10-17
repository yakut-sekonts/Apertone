#pragma once
#include <QJsonObject>
#include <QString>
namespace ux {
void createPrivacySource(const QString &path);
QJsonObject encodedMetadata(const QString &path);
} // namespace ux
