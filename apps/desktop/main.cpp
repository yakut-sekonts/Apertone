#include "apps/desktop/EditorController.h"
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QTimer>
#include <cmath>
#include <iostream>

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Luma"));
    app.setApplicationName(QStringLiteral("Luma"));
    app.setApplicationVersion(QStringLiteral("0.2.0"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Luma — non-destructive RAW editor for macOS"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(
        {QStringLiteral("catalog"), QStringLiteral("Catalog path"), QStringLiteral("file")});
    parser.addOption({QStringLiteral("smoke-test"), QStringLiteral("Verify QML shell then exit")});
    parser.addOption({QStringLiteral("test-edit"), QStringLiteral("Integration test: open RAW"),
                      QStringLiteral("raw")});
    parser.addOption({QStringLiteral("test-exposure"), QStringLiteral("Integration test: exposure"),
                      QStringLiteral("ev")});
    parser.addOption({QStringLiteral("test-restore"),
                      QStringLiteral("Integration test: verify restored exposure"),
                      QStringLiteral("ev")});
    parser.addOption({QStringLiteral("test-screenshot"),
                      QStringLiteral("Integration test: save the rendered app window"),
                      QStringLiteral("png")});
    parser.addOption(
        {QStringLiteral("test-develop"), QStringLiteral("Integration test: Develop and history")});
    parser.addOption({QStringLiteral("test-export"), QStringLiteral("Integration test: export PNG"),
                      QStringLiteral("png")});
    parser.addOption({QStringLiteral("test-cancel-export"),
                      QStringLiteral("Integration test: cancel queued export")});
    parser.addOption({QStringLiteral("test-close-during-export"),
                      QStringLiteral("Integration test: close with queued load/export")});
    parser.process(app);
    const auto catalog =
        parser.isSet(QStringLiteral("catalog"))
            ? parser.value(QStringLiteral("catalog"))
            : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                  QStringLiteral("/catalog.sqlite");
    auto *provider = new PreviewProvider;
    EditorController editor(std::filesystem::path(catalog.toStdString()), provider);
    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("preview"), provider);
    engine.rootContext()->setContextProperty(QStringLiteral("editor"), &editor);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("Luma.Desktop"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window) {
        return 1;
    }
    editor.setWindow(window);
    if (parser.isSet(QStringLiteral("smoke-test"))) {
        QTimer::singleShot(300, &app, &QCoreApplication::quit);
    } else if (parser.isSet(QStringLiteral("test-edit")) ||
               parser.isSet(QStringLiteral("test-restore"))) {
        const bool editing = parser.isSet(QStringLiteral("test-edit"));
        bool parsed = false;
        const double target =
            parser.value(editing ? QStringLiteral("test-exposure") : QStringLiteral("test-restore"))
                .toDouble(&parsed);
        if (!parsed) {
            std::cerr << "Invalid test exposure\n";
            return 2;
        }
        auto applied = std::make_shared<bool>(false);
        auto finished = std::make_shared<bool>(false);
        const bool closeDuringExport = parser.isSet(QStringLiteral("test-close-during-export"));
        const auto testSource = parser.value(QStringLiteral("test-edit"));
        const bool cancelExport = parser.isSet(QStringLiteral("test-cancel-export"));
        const bool develop = parser.isSet(QStringLiteral("test-develop"));
        const auto exportPath = parser.value(QStringLiteral("test-export"));
        auto exportStarted = std::make_shared<bool>(false);
        const auto screenshot = parser.value(QStringLiteral("test-screenshot"));
        QObject::connect(
            &editor, &EditorController::stateChanged, &app,
            [&editor, &app, window, editing, target, applied, finished, screenshot, develop,
             exportPath, exportStarted, cancelExport, closeDuringExport, testSource] {
                if (!editor.error().isEmpty()) {
                    std::cerr << editor.error().toStdString() << '\n';
                    app.exit(1);
                    return;
                }
                if (!editor.busy() && editor.hasPhoto() && !*applied) {
                    *applied = true;
                    if (editing) {
                        editor.setExposure(target);
                        if (develop) {
                            editor.beginEdit();
                            editor.setParameter(QStringLiteral("temperature"), 5100);
                            editor.setParameter(QStringLiteral("tint"), 12);
                            editor.endEdit();
                            editor.setParameter(QStringLiteral("contrast"), 18);
                            editor.setParameter(QStringLiteral("highlights"), -40);
                            editor.setParameter(QStringLiteral("shadows"), 25);
                            const auto previousVibrance =
                                editor.parameters().value(QStringLiteral("vibrance")).toDouble();
                            editor.setParameter(QStringLiteral("vibrance"), 35);
                            editor.undo();
                            if (editor.parameters().value(QStringLiteral("vibrance")).toDouble() !=
                                previousVibrance) {
                                std::cerr << "Undo did not restore Vibrance\n";
                                app.exit(1);
                                return;
                            }
                            editor.redo();
                            editor.setCorrectionEnabled(QStringLiteral("shadows"), false);
                        }
                    } else if (std::abs(editor.exposure() - target) > 1e-9) {
                        std::cerr << "Exposure restoration mismatch\n";
                        app.exit(1);
                        return;
                    }
                }
                if (*applied && develop && !editing && !editor.busy() && editor.hasPhoto()) {
                    if (editor.parameters().value(QStringLiteral("temperature")).toDouble() !=
                            5100 ||
                        editor.parameters().value(QStringLiteral("vibrance")).toDouble() != 35 ||
                        editor.correctionStates().value(QStringLiteral("shadows")).toBool()) {
                        std::cerr << "Develop restoration mismatch\n";
                        app.exit(1);
                        return;
                    }
                }
                if (*applied && !*finished && !editor.saving() && !editor.busy() &&
                    !editor.previewUrl().isEmpty() && !editor.exporting()) {
                    if (!exportPath.isEmpty() && !*exportStarted) {
                        *exportStarted = true;
                        editor.exportPhoto(QUrl::fromLocalFile(exportPath), 1, 92, false);
                        if (cancelExport)
                            editor.cancelExport();
                        if (closeDuringExport) {
                            editor.openFile(QUrl::fromLocalFile(testSource));
                            window->close();
                        }
                        return;
                    }
                    *finished = true;
                    QTimer::singleShot(120, &app, [window, &app, screenshot] {
                        if (!screenshot.isEmpty()) {
                            const auto frame = window->grabWindow();
                            if (frame.isNull() || !frame.save(screenshot)) {
                                std::cerr << "Cannot capture the rendered viewer\n";
                                app.exit(1);
                                return;
                            }
                        }
                        std::cout << "RAW viewer and Exposure persistence verified\n";
                        window->close();
                    });
                }
            });
        QTimer::singleShot(90'000, &app, [&app] {
            std::cerr << "GUI acceptance test timed out\n";
            app.exit(1);
        });
        if (editing) {
            editor.openFile(QUrl::fromLocalFile(parser.value(QStringLiteral("test-edit"))));
        } else {
            editor.restore();
        }
    } else {
        editor.restore();
    }
    return app.exec();
}
