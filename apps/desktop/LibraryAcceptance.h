#pragma once
#include <QString>
class QGuiApplication;
class QQuickWindow;
class EditorController;
class LibraryModel;
void startLibraryAcceptance(QGuiApplication &app, QQuickWindow *window, EditorController &editor,
                            LibraryModel &library, const QString &source, const QString &screenshot,
                            bool closeImport);
