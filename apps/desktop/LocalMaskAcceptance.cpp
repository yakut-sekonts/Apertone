#include "apps/desktop/LocalMaskAcceptance.h"
#include "apps/desktop/EditorController.h"
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QQuickItem>
#include <QTimer>
#include <iostream>
#include <memory>
namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
struct State {
    int phase = 0;
    bool finished = false;
    luma::EditDocument expected;
};
} // namespace
void startLocalMaskAcceptance(QGuiApplication &app, QQuickWindow *window, EditorController &editor,
                              const QString &source, bool restore, double exposure,
                              const QString &output, const QString &screenshot) {
    auto state = std::make_shared<State>();
    auto *timer = new QTimer(&app);
    timer->setInterval(20);
    QObject::connect(
        timer, &QTimer::timeout, &app,
        [state, timer, &app, window, &editor, restore, exposure, output, screenshot] {
            try {
                if (state->finished)
                    return;
                if (!editor.error().isEmpty())
                    throw std::runtime_error(editor.error().toStdString());
                if (editor.busy() || !editor.hasPhoto() || editor.previewUrl().isEmpty())
                    return;
                switch (state->phase) {
                case 0:
                    if (restore) {
                        const auto edits = editor.currentSettings();
                        require(edits.exposure == exposure && edits.localMasks.size() == 2,
                                "Complete mask restart lost global exposure or masks.");
                        require(
                            edits.localMasks[0].shape == luma::MaskShape::brush &&
                                edits.localMasks[0].strokes.size() == 2 &&
                                edits.localMasks[0].strokes[1].erase &&
                                edits.localMasks[0].components.size() == 1 &&
                                edits.localMasks[1].shape == luma::MaskShape::color &&
                                edits.localMasks[1].components.size() == 2 && !editor.localMode(),
                            "Complete mask restart lost strokes, ranges or boolean components.");
                        state->phase = 9;
                        break;
                    }
                    editor.setExposure(exposure);
                    editor.setCropRect(.08, .06, .84, .88);
                    editor.setParameter("transform.rotate", 6);
                    editor.setParameter("transform.horizontal", 16);
                    require(!editor.addLocalMask("brush").isEmpty(), "Cannot create Brush mask.");
                    editor.setLocalParameter("exposure", .7);
                    editor.setLocalBrushSize(24);
                    editor.setLocalBrushOpacity(80);
                    editor.setLocalBrushErase(false);
                    state->phase = 1;
                    break;
                case 1:
                    if (!editor.localCanvasReady())
                        return;
                    require(editor.beginLocalGesture(-1, .2, .3), "Brush press failed.");
                    editor.adjustLocalGesture(.45, .45);
                    editor.adjustLocalGesture(.8, .65);
                    editor.endEdit();
                    state->phase = 2;
                    break;
                case 2:
                    if (!editor.localCanvasReady())
                        return;
                    editor.setLocalBrushErase(true);
                    editor.setLocalBrushSize(10);
                    editor.setLocalBrushOpacity(50);
                    require(editor.beginLocalGesture(-1, .5, .48), "Eraser press failed.");
                    editor.adjustLocalGesture(.57, .5);
                    editor.endEdit();
                    require(!editor.addLocalComponent("luminance", "intersect").isEmpty(),
                            "Cannot intersect a luminance range.");
                    editor.setLocalParameter("rangeLow", 0);
                    editor.setLocalParameter("rangeHigh", 100);
                    state->phase = 3;
                    break;
                case 3:
                    if (!editor.localCanvasReady())
                        return;
                    editor.pickLocalRange(.45, .5);
                    require(editor.localSampling(), "Luminance picker did not start.");
                    state->phase = 4;
                    break;
                case 4:
                    if (editor.localSampling() || !editor.localCanvasReady())
                        return;
                    require(!editor.addLocalMask("color").isEmpty(), "Cannot create Color Range.");
                    state->phase = 5;
                    break;
                case 5:
                    if (!editor.localCanvasReady())
                        return;
                    editor.pickLocalRange(.6, .5);
                    require(editor.localSampling(), "Color picker did not start.");
                    state->phase = 6;
                    break;
                case 6:
                    if (editor.localSampling() || !editor.localCanvasReady())
                        return;
                    editor.setLocalParameter("colorTolerance", 35);
                    editor.setLocalParameter("exposure", -.4);
                    require(!editor.addLocalComponent("radial", "subtract").isEmpty(),
                            "Cannot subtract a radial gradient.");
                    state->phase = 7;
                    break;
                case 7:
                    if (!editor.localCanvasReady())
                        return;
                    require(editor.beginLocalGesture(-1, .35, .35),
                            "Radial component press failed.");
                    editor.adjustLocalGesture(.65, .7);
                    editor.endEdit();
                    require(!editor.addLocalComponent("linear", "add").isEmpty(),
                            "Cannot add a linear gradient.");
                    state->phase = 8;
                    break;
                case 8:
                    if (!editor.localCanvasReady())
                        return;
                    require(editor.beginLocalGesture(-1, .2, .2), "Linear component press failed.");
                    editor.adjustLocalGesture(.8, .6);
                    editor.endEdit();
                    editor.setLocalComponentInverted(true);
                    editor.moveLocalComponent(-1);
                    editor.moveLocalComponent(1);
                    state->phase = 9;
                    break;
                case 9:
                    if (editor.saving())
                        return;
                    state->expected = editor.currentSettings();
                    editor.exportPhoto(QUrl::fromLocalFile(output), 1, 95, false);
                    state->phase = 10;
                    break;
                case 10:
                    if (editor.exporting() || editor.saving())
                        return;
                    require(!QImage(output).isNull(), "Complete mask export failed.");
                    require(editor.currentSettings() == state->expected, "Export mutated masks.");
                    require(
                        luma::Catalog(editor.catalogPath().toStdString()).activeAsset()->edits ==
                            state->expected,
                        "Autosave did not retain complete masks.");
                    if (screenshot.isEmpty()) {
                        state->finished = true;
                        timer->stop();
                        window->close();
                        return;
                    }
                    {
                        auto *panel = window->findChild<QObject *>("localPanelToggle");
                        require(panel && panel->setProperty("checked", true),
                                "Cannot expand local mask panel.");
                        editor.selectLocalMask(
                            QString::fromStdString(state->expected.localMasks[0].id));
                        editor.setLocalMode(true);
                    }
                    state->phase = 11;
                    break;
                case 11:
                case 12:
                case 13:
                case 14:
                    if (!editor.localCanvasReady())
                        return;
                    timer->stop();
                    QTimer::singleShot(
                        120, &app, [state, timer, window, &app, &editor, screenshot] {
                            auto fail = [&](const char *message) {
                                state->finished = true;
                                std::cerr << message << '\n';
                                app.exit(1);
                            };
                            auto *panel = window->findChild<QQuickItem *>("localPanel");
                            if (!panel || panel->mapToScene(QPointF(panel->width(), 0)).x() >
                                              window->width()) {
                                fail("Complete mask panel escaped the window");
                                return;
                            }
                            for (const auto *name :
                                 {"localNewMask", "localMaskChoice", "localComponentChoice",
                                  "localComponentOperation", "localBrushSize", "localBrushOpacity",
                                  "local_colorTolerance", "local_rangeLow", "local_rangeHigh",
                                  "local_exposure", "local_opacity"}) {
                                auto *item =
                                    window->findChild<QQuickItem *>(QString::fromLatin1(name));
                                if (!item ||
                                    (item->isVisible() &&
                                     (item->width() < 150 ||
                                      item->mapToItem(panel, QPointF(item->width(), 0)).x() >
                                          panel->width() + 1))) {
                                    std::cerr << name << '\n';
                                    fail("Complete mask control escaped sidebar");
                                    return;
                                }
                            }
                            for (auto *item : panel->findChildren<QQuickItem *>()) {
                                if (item->isVisible() &&
                                    QString::fromLatin1(item->metaObject()->className())
                                        .contains("LocalCheckBox") &&
                                    item->mapToItem(panel, QPointF(item->width(), 0)).x() >
                                        panel->width() + 1) {
                                    fail("Localized mask checkbox escaped sidebar");
                                    return;
                                }
                            }
                            const QString suffix =
                                state->phase == 11   ? QString()
                                : state->phase == 12 ? QStringLiteral(".range.png")
                                : state->phase == 13 ? QStringLiteral(".color.png")
                                                     : QStringLiteral(".radial.png");
                            if (!window->grabWindow().save(screenshot + suffix)) {
                                fail("Cannot capture complete masks");
                                return;
                            }
                            if (state->phase == 14) {
                                state->finished = true;
                                std::cout << "Complete masks: Brush/Erase, ranges, "
                                             "Add/Subtract/Intersect, all selection panels, save "
                                             "and export passed\n";
                                window->close();
                                return;
                            }
                            if (state->phase == 11)
                                editor.selectLocalComponent(QString::fromStdString(
                                    state->expected.localMasks[0].components[0].id));
                            else if (state->phase == 12)
                                editor.selectLocalMask(
                                    QString::fromStdString(state->expected.localMasks[1].id));
                            else
                                editor.selectLocalComponent(QString::fromStdString(
                                    state->expected.localMasks[1].components[0].id));
                            ++state->phase;
                            timer->start();
                        });
                    break;
                default:
                    throw std::runtime_error("Invalid mask acceptance state.");
                }
            } catch (const std::exception &error) {
                state->finished = true;
                timer->stop();
                std::cerr << error.what() << '\n';
                app.exit(1);
            }
        });
    QTimer::singleShot(120000, &app, [state, &app] {
        if (!state->finished) {
            std::cerr << "Complete mask acceptance timed out\n";
            app.exit(1);
        }
    });
    timer->start();
    if (restore)
        editor.restore();
    else
        editor.openFile(QUrl::fromLocalFile(source));
}
