#pragma once
#include <QString>
class QGuiApplication;
class QQuickWindow;
class EditorController;
class LibraryModel;
class BatchController;
#ifdef _WIN32
void startWindowsWorkspaceAcceptance(QGuiApplication &app, QQuickWindow &window,
                                     EditorController &editor, LibraryModel &library,
                                     BatchController &batch, const QString &evidence);
#endif
void startLibraryAcceptance(QGuiApplication &app, QQuickWindow *window, EditorController &editor,
                            LibraryModel &library, const QString &source, const QString &screenshot,
                            bool closeImport);
