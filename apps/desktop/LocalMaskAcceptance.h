#pragma once
#include <QString>
class QGuiApplication;
class QQuickWindow;
class EditorController;
void startLocalMaskAcceptance(QGuiApplication &app, QQuickWindow *window, EditorController &editor,
                              const QString &source, bool restore, double exposure,
                              const QString &output, const QString &screenshot);
