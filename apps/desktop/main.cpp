#include "apps/desktop/BatchController.h"
#include "apps/desktop/EditorController.h"
#include "apps/desktop/LibraryAcceptance.h"
#include "apps/desktop/LibraryModel.h"
#include "apps/desktop/PresetController.h"
#include "apps/desktop/ThumbnailProvider.h"
#include "apps/desktop/UpdateController.h"
#include <QCommandLineParser>
#include <QFileInfo>
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
    app.setApplicationVersion(QStringLiteral("0.4.7-dev"));
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
    parser.addOption({QStringLiteral("test-advanced"),
                      QStringLiteral("Integration test: Curves, HSL and targeted gestures")});
    parser.addOption({QStringLiteral("test-lens"),
                      QStringLiteral("Integration test: calibrated Lens Corrections")});
    parser.addOption({QStringLiteral("test-transform"),
                      QStringLiteral("Integration test: perspective geometry and history")});
    parser.addOption({QStringLiteral("test-export"), QStringLiteral("Integration test: export PNG"),
                      QStringLiteral("png")});
    parser.addOption({QStringLiteral("test-cancel-export"),
                      QStringLiteral("Integration test: cancel queued export")});
    parser.addOption({QStringLiteral("test-close-during-export"),
                      QStringLiteral("Integration test: close with queued load/export")});
    parser.addOption({QStringLiteral("test-library"),
                      QStringLiteral("Integration test: Library import and organization"),
                      QStringLiteral("folder")});
    parser.addOption({QStringLiteral("test-close-import"),
                      QStringLiteral("Integration test: close during import")});
    parser.process(app);
    const auto catalog =
        parser.isSet(QStringLiteral("catalog"))
            ? parser.value(QStringLiteral("catalog"))
            : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                  QStringLiteral("/catalog.sqlite");
    auto *provider = new PreviewProvider;
    EditorController editor(std::filesystem::path(catalog.toStdString()), provider);
    PresetController presets(QFileInfo(catalog).absolutePath() + QStringLiteral("/presets.json"),
                             editor);
    LibraryModel library(std::filesystem::path(catalog.toStdString()));
    BatchController batch(std::filesystem::path(catalog.toStdString()), editor, library, presets);
    const bool testing = parser.isSet(QStringLiteral("smoke-test")) ||
                         parser.isSet(QStringLiteral("test-edit")) ||
                         parser.isSet(QStringLiteral("test-restore")) ||
                         parser.isSet(QStringLiteral("test-library"));
    UpdateController updates(
        [&] {
            return !editor.transformAutoBusy() && !editor.busy() && !editor.saving() &&
                   !editor.exporting() && !library.working() && !batch.working();
        },
        !testing);
    QObject::connect(&updates, &UpdateController::preparingRestart, &editor,
                     &EditorController::endEdit);
    QObject::connect(&updates, &UpdateController::preparingRestart, &batch,
                     &BatchController::cancel);
    QObject::connect(&library, &LibraryModel::openRequested, &editor, &EditorController::openFile);
    QObject::connect(&editor, &EditorController::photoLoaded, &library, &LibraryModel::photoOpened);
    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("thumbnail"),
                            new ThumbnailProvider(std::filesystem::path(catalog.toStdString())));
    engine.addImageProvider(QStringLiteral("preview"), provider);
    engine.rootContext()->setContextProperty(QStringLiteral("editor"), &editor);
    engine.rootContext()->setContextProperty(QStringLiteral("library"), &library);
    engine.rootContext()->setContextProperty(QStringLiteral("updates"), &updates);
    engine.rootContext()->setContextProperty(QStringLiteral("presets"), &presets);
    engine.rootContext()->setContextProperty(QStringLiteral("batch"), &batch);
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
    } else if (parser.isSet(QStringLiteral("test-library"))) {
        startLibraryAcceptance(app, window, editor, library,
                               parser.value(QStringLiteral("test-library")),
                               parser.value(QStringLiteral("test-screenshot")),
                               parser.isSet(QStringLiteral("test-close-import")));
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
        const bool advanced = parser.isSet(QStringLiteral("test-advanced"));
        const bool lens = parser.isSet(QStringLiteral("test-lens"));
        const bool transform = parser.isSet(QStringLiteral("test-transform"));
        const auto exportPath = parser.value(QStringLiteral("test-export"));
        auto exportStarted = std::make_shared<bool>(false);
        const auto screenshot = parser.value(QStringLiteral("test-screenshot"));
        QObject::connect(
            &editor, &EditorController::stateChanged, &app,
            [&editor, &app, window, editing, target, applied, finished, screenshot, develop,
             exportPath, exportStarted, cancelExport, closeDuringExport, testSource, advanced, lens,
             transform] {
                if (!editor.error().isEmpty()) {
                    std::cerr << editor.error().toStdString() << '\n';
                    app.exit(1);
                    return;
                }
                if (!editor.busy() && editor.hasPhoto() && !*applied) {
                    *applied = true;
                    if (editing) {
                        editor.setExposure(target);
                        if (lens) {
                            if (!editor.lensState().value("distortionAvailable").toBool() ||
                                !editor.lensState().value("vignettingAvailable").toBool() ||
                                !editor.lensState().value("caAvailable").toBool()) {
                                std::cerr << "Known lens calibration unavailable\n";
                                app.exit(1);
                                return;
                            }
                            editor.setCorrectionEnabled(QStringLiteral("lens"), true);
                        }
                        if (transform) {
                            const auto before = editor.parameters();
                            editor.beginEdit();
                            editor.setParameter(QStringLiteral("transform.rotate"), 8);
                            editor.setParameter(QStringLiteral("transform.horizontal"), 24);
                            editor.setParameter(QStringLiteral("transform.vertical"), -18);
                            editor.setParameter(QStringLiteral("transform.aspect"), 12);
                            editor.setParameter(QStringLiteral("transform.xOffset"), 5);
                            editor.endEdit();
                            const auto after = editor.parameters();
                            editor.undo();
                            if (editor.parameters() != before || after == before) {
                                std::cerr << "Transform gesture Undo failed\n";
                                app.exit(1);
                                return;
                            }
                            editor.redo();
                            if (editor.parameters() != after) {
                                std::cerr << "Transform gesture Redo failed\n";
                                app.exit(1);
                                return;
                            }
                        }
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
                        if (advanced) {
                            const auto originalCurves = editor.curvePoints();
                            editor.beginEdit();
                            const int point =
                                editor.addCurvePoint(QStringLiteral("rgb"), 0.4, 0.55);
                            editor.moveCurvePoint(QStringLiteral("rgb"), point, 0.4, 0.6);
                            editor.moveCurvePoint(QStringLiteral("rgb"), point, 0.4, 0.62);
                            editor.endEdit();
                            editor.undo();
                            if (editor.curvePoints() != originalCurves) {
                                std::cerr << "Curve gesture Undo failed\n";
                                app.exit(1);
                                return;
                            }
                            editor.redo();
                            const auto beforeHSL = editor.parameters();
                            editor.beginEdit();
                            editor.setParameter(QStringLiteral("hsl.red.hue"), 12);
                            editor.setParameter(QStringLiteral("hsl.blue.luminance"), 25);
                            editor.endEdit();
                            editor.undo();
                            if (editor.parameters() != beforeHSL) {
                                std::cerr << "HSL gesture Undo failed\n";
                                app.exit(1);
                                return;
                            }
                            editor.redo();
                            editor.setParameter(QStringLiteral("curve.lights"), 15);
                            const auto beforeTarget = editor.curvePoints();
                            editor.setTargetedTool(QStringLiteral("curve.red"));
                            if (!editor.beginTargeted(0.4, 0.5)) {
                                std::cerr << "Curve target did not begin\n";
                                app.exit(1);
                                return;
                            }
                            editor.adjustTargeted(0.05);
                            editor.adjustTargeted(0.08);
                            editor.endTargeted();
                            if (editor.curvePoints() == beforeTarget) {
                                std::cerr << "Curve target did not change pixels\n";
                                app.exit(1);
                                return;
                            }
                            editor.undo();
                            if (editor.curvePoints() != beforeTarget) {
                                std::cerr << "Targeted curve Undo failed\n";
                                app.exit(1);
                                return;
                            }
                            const auto beforeColorTarget = editor.parameters();
                            editor.setTargetedTool(QStringLiteral("hsl.saturation"));
                            if (!editor.beginTargeted(0.4, 0.5)) {
                                std::cerr << "HSL target did not begin\n";
                                app.exit(1);
                                return;
                            }
                            editor.adjustTargeted(-0.15);
                            editor.adjustTargeted(-0.2);
                            editor.endTargeted();
                            if (editor.parameters() == beforeColorTarget) {
                                std::cerr << "HSL target did not change parameters\n";
                                app.exit(1);
                                return;
                            }
                            editor.undo();
                            if (editor.parameters() != beforeColorTarget) {
                                std::cerr << "Targeted HSL Undo failed\n";
                                app.exit(1);
                                return;
                            }
                            editor.setTargetedTool(QStringLiteral("none"));
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
                if (*applied && advanced && !editing && !editor.busy() && editor.hasPhoto()) {
                    const auto points = editor.curvePoints().value(QStringLiteral("rgb")).toList();
                    if (points.size() != 3 ||
                        points[1].toMap().value(QStringLiteral("y")).toDouble() != 0.62 ||
                        editor.parameters().value(QStringLiteral("curve.lights")).toDouble() !=
                            15 ||
                        editor.parameters().value(QStringLiteral("hsl.red.hue")).toDouble() != 12 ||
                        editor.parameters()
                                .value(QStringLiteral("hsl.blue.luminance"))
                                .toDouble() != 25) {
                        std::cerr << "Advanced restoration mismatch\n";
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
