#include "apps/desktop/EditorController.h"
#include "apps/desktop/LocalizationController.h"
#include "apps/desktop/PresetController.h"
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QMouseEvent>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QThread>
#include <memory>
namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void wait(const std::function<bool()> &ready) {
    QElapsedTimer t;
    t.start();
    while (!ready() && t.elapsed() < 30000) {
        QCoreApplication::processEvents();
        QThread::msleep(2);
    }
    require(ready(), "Complete local editor wait timed out.");
}
QByteArray read(const QString &path) {
    QFile f(path);
    require(f.open(QIODevice::ReadOnly), "Missing complete mask export.");
    return f.readAll();
}
} // namespace
void completeLocalEditorTests(const QString &source, const QString &root, const QString &qmlPath) {
    PreviewProvider provider;
    const auto catalog = root + "/complete-local.sqlite";
    EditorController editor(catalog.toStdString(), &provider);
    editor.openFile(QUrl::fromLocalFile(source));
    wait([&] { return editor.hasPhoto() && !editor.busy() && !editor.previewUrl().isEmpty(); });
    QQmlEngine engine;
    LocalizationController localization(root + "/mask-language.json", engine);
    engine.rootContext()->setContextProperty("editor", &editor);
    QStringList warnings;
    QObject::connect(&engine, &QQmlEngine::warnings, [&](const QList<QQmlError> &errors) {
        for (const auto &error : errors)
            warnings.append(error.toString());
    });
    QQmlComponent component(&engine);
    component.setData(
        ("import QtQuick\nimport QtQuick.Controls\nimport \"" +
         QUrl::fromLocalFile(qmlPath).toString() +
         "\"\nApplicationWindow{width:750;height:950;visible:true;LocalOverlay{width:400;height:"
         "300}ScrollView{x:420;width:280;height:930;LocalPanel{width:250}}}")
            .toUtf8(),
        QUrl());
    wait([&] { return component.status() != QQmlComponent::Loading; });
    if (component.isError())
        throw std::runtime_error(component.errorString().toStdString());
    const std::unique_ptr<QObject> window(component.create());
    require(window != nullptr, "Complete mask panel failed to load.");
    auto *quick = qobject_cast<QQuickWindow *>(window.get());
    require(quick != nullptr, "No mask test window.");
    auto child = [&](const char *name) {
        auto *object = window->findChild<QObject *>(QString::fromLatin1(name));
        if (!object)
            throw std::runtime_error(std::string("Missing mask control: ") + name);
        return object;
    };
    child("localPanelToggle")->setProperty("checked", true);
    child("localNewKind")->setProperty("currentIndex", 2);
    require(QMetaObject::invokeMethod(child("localNewMask"), "clicked"),
            "Cannot add brush through QML.");
    require(editor.currentSettings().localMasks.size() == 1 &&
                editor.currentSettings().localMasks[0].shape == luma::MaskShape::brush,
            "QML did not create a brush mask.");
    editor.setLocalParameter("exposure", .6);
    editor.localState();
    wait([&] { return editor.localCanvasReady(); });
    auto send = [&](QEvent::Type type, QPointF point, Qt::MouseButton button,
                    Qt::MouseButtons buttons) {
        QMouseEvent event(type, point, point, button, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(quick, &event);
        QCoreApplication::processEvents();
    };
    const auto before = editor.currentSettings();
    send(QEvent::MouseButtonPress, {80, 120}, Qt::LeftButton, Qt::LeftButton);
    send(QEvent::MouseMove, {200, 140}, Qt::NoButton, Qt::LeftButton);
    send(QEvent::MouseButtonRelease, {300, 160}, Qt::LeftButton, Qt::NoButton);
    const auto painted = editor.currentSettings();
    require(painted.localMasks[0].strokes.size() == 1 &&
                painted.localMasks[0].strokes[0].points.size() >= 2,
            "Real pointer did not paint a continuous brush stroke.");
    editor.undo();
    require(editor.currentSettings() == before, "Brush stroke created multiple Undo entries.");
    editor.redo();
    require(editor.currentSettings() == painted, "Brush Redo lost vector points.");
    wait([&] { return editor.localCanvasReady(); });
    send(QEvent::MouseButtonPress, {50, 50}, Qt::LeftButton, Qt::LeftButton);
    send(QEvent::MouseMove, {-20, 80}, Qt::NoButton, Qt::LeftButton);
    send(QEvent::MouseMove, {250, 100}, Qt::NoButton, Qt::LeftButton);
    send(QEvent::MouseButtonRelease, {250, 100}, Qt::LeftButton, Qt::NoButton);
    require(!editor.localGestureActive() &&
                editor.currentSettings().localMasks[0].strokes.size() == 2 &&
                editor.currentSettings().localMasks[0].strokes[1].points.size() == 1,
            "Brush continued across the image boundary.");
    editor.undo();
    require(editor.currentSettings() == painted, "Boundary-ending stroke was not one Undo.");
    require(editor.localBrushOutline(.5, .5).size() == 32, "Projected brush cursor missing.");
    child("localBrushSize")->setProperty("value", 18);
    QMetaObject::invokeMethod(child("localBrushSize"), "moved");
    child("localBrushOpacity")->setProperty("value", 50);
    QMetaObject::invokeMethod(child("localBrushOpacity"), "moved");
    child("localBrushErase")->setProperty("checked", true);
    QMetaObject::invokeMethod(child("localBrushErase"), "toggled");
    require(editor.localBrushErase() && editor.localBrushSize() == 18 &&
                editor.localBrushOpacity() == 50,
            "Brush controls not wired.");
    wait([&] { return editor.localCanvasReady(); });
    send(QEvent::MouseButtonPress, {200, 140}, Qt::LeftButton, Qt::LeftButton);
    send(QEvent::MouseButtonRelease, {220, 140}, Qt::LeftButton, Qt::NoButton);
    const auto erased = editor.currentSettings();
    require(erased.localMasks[0].strokes.size() == 2 && erased.localMasks[0].strokes[1].erase &&
                erased.localMasks[0].strokes[1].opacity == 50,
            "Eraser did not store its stroke properties.");
    editor.undo();
    require(editor.currentSettings() == painted, "Eraser Undo changed prior paint.");
    editor.redo();
    require(QMetaObject::invokeMethod(child("localClearBrush"), "clicked"), "Cannot clear brush.");
    require(editor.currentSettings().localMasks[0].strokes.empty(), "Clear brush failed.");
    editor.undo();
    require(editor.currentSettings() == erased, "Clear brush Undo failed.");
    child("localCombineToggle")->setProperty("checked", true);
    child("localComponentKind")->setProperty("currentIndex", 3);
    child("localNewOperation")->setProperty("currentIndex", 2);
    QMetaObject::invokeMethod(child("localAddComponent"), "clicked");
    require(editor.currentSettings().localMasks[0].components.size() == 1 &&
                editor.localState()["shape"].toString() == "luminance",
            "QML did not add luminance Intersect.");
    const auto rangeID = editor.localState()["componentID"].toString();
    editor.setLocalParameter("rangeLow", 10);
    editor.setLocalParameter("rangeHigh", 90);
    const auto range = editor.currentSettings();
    wait([&] { return editor.localCanvasReady(); });
    send(QEvent::MouseButtonPress, {160, 120}, Qt::LeftButton, Qt::LeftButton);
    send(QEvent::MouseButtonRelease, {160, 120}, Qt::LeftButton, Qt::NoButton);
    wait([&] { return !editor.localSampling() && editor.currentSettings() != range; });
    const auto sampled = editor.currentSettings();
    editor.undo();
    require(editor.currentSettings() == range, "Range sample was not one Undo.");
    editor.redo();
    require(editor.currentSettings() == sampled, "Range sample Redo failed.");
    editor.setLocalComponentEnabled(false);
    require(!editor.currentSettings().localMasks[0].components[0].enabled,
            "Component enable failed.");
    editor.undo();
    editor.setLocalComponentInverted(true);
    require(editor.currentSettings().localMasks[0].components[0].inverted,
            "Component invert failed.");
    editor.undo();
    child("localComponentOperation")->setProperty("currentIndex", 1);
    QMetaObject::invokeMethod(child("localComponentOperation"), "activated", Q_ARG(int, 1));
    require(editor.currentSettings().localMasks[0].components[0].operation ==
                luma::MaskOperation::subtract,
            "Component operation UI failed.");
    editor.undo();
    const auto colorID = editor.addLocalComponent("color", "subtract");
    require(!colorID.isEmpty(), "Cannot add color selection.");
    wait([&] { return editor.localCanvasReady(); });
    const auto beforeColor = editor.currentSettings();
    editor.pickLocalRange(.35, .45);
    require(editor.localSampling(), "Color picker did not start on the worker.");
    wait([&] { return !editor.localSampling(); });
    require(editor.currentSettings() != beforeColor && editor.error().isEmpty(),
            "Color picker did not update the selection.");
    editor.moveLocalComponent(-1);
    require(editor.currentSettings().localMasks[0].components[0].id == colorID.toStdString(),
            "Component reorder failed.");
    const auto composed = editor.currentSettings();
    editor.removeLocalComponent();
    editor.undo();
    require(editor.currentSettings() == composed, "Remove component Undo lost selection or order.");
    editor.selectLocalComponent(rangeID);
    wait([&] { return editor.localCanvasReady(); });
    editor.pickLocalRange(.8, .8);
    const auto pending = editor.currentSettings();
    editor.setLocalParameter("exposure", .8);
    wait([&] { return !editor.localSampling(); });
    const auto latest = editor.currentSettings();
    require(latest != pending, "Intervening local edit was lost.");
    QCoreApplication::processEvents();
    require(editor.currentSettings() == latest, "Late range sample overwrote a newer edit.");
    editor.selectLocalComponent("");
    const auto saved = editor.currentSettings();
    for (const auto *code : {"en", "ru", "zh_CN", "hy", "de", "es", "it", "fr"}) {
        require(localization.selectLanguage(QString::fromLatin1(code)),
                "Cannot switch local mask locale.");
        QCoreApplication::processEvents();
        require(child("localPanelToggle")->property("text").toString() ==
                    QCoreApplication::translate("Luma", "Local adjustments"),
                "Mask panel title did not retranslate.");
        const auto choices = child("localNewKind")->property("model").toStringList();
        require(choices.size() == 5 && choices[2] == QCoreApplication::translate("Luma", "Brush") &&
                    choices[4] == QCoreApplication::translate("Luma", "Color range"),
                "Mask type choices did not retranslate.");
        require(editor.currentSettings() == saved, "Language switch changed user mask data.");
    }
    PresetController presets(root + "/complete-mask-presets.json", editor);
    presets.copySettings(1);
    editor.setParameter("exposure", 1.2);
    presets.pasteSettings();
    require(editor.currentSettings() == saved, "Global Paste damaged composed masks.");
    editor.setLocalOverlayColor("#67e26a");
    editor.setLocalOverlayOpacity(.9);
    editor.setLocalMode(true);
    const auto first = root + "/complete-masks.png";
    editor.exportPhoto(QUrl::fromLocalFile(first), 1, 95, false);
    wait([&] { return !editor.exporting() && !editor.saving(); });
    require(!QImage(first).isNull(), "Composed mask export failed.");
    PreviewProvider secondProvider;
    EditorController restored(catalog.toStdString(), &secondProvider);
    restored.restore();
    wait([&] { return restored.hasPhoto() && !restored.busy(); });
    require(restored.currentSettings() == saved && !restored.localMode(),
            "Restart lost brush/range/boolean components or persisted a transient tool.");
    const auto second = root + "/complete-masks-restart.png";
    restored.exportPhoto(QUrl::fromLocalFile(second), 1, 95, false);
    wait([&] { return !restored.exporting(); });
    require(read(first) == read(second), "Composed mask export differs after restart.");
    if (!warnings.empty())
        throw std::runtime_error(warnings.join('\n').toStdString());
}
