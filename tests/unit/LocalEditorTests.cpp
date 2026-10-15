#include "apps/desktop/EditorController.h"
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
    require(ready(), "Local editor wait timed out.");
}
QByteArray read(const QString &path) {
    QFile f(path);
    require(f.open(QIODevice::ReadOnly), "Missing local export.");
    return f.readAll();
}
} // namespace
void localEditorTests(const QString &source, const QString &root, const QString &qmlPath) {
    PreviewProvider provider;
    const auto catalog = root + "/local-editor.sqlite";
    EditorController editor(catalog.toStdString(), &provider);
    editor.openFile(QUrl::fromLocalFile(source));
    wait([&] { return editor.hasPhoto() && !editor.busy() && !editor.previewUrl().isEmpty(); });
    const auto initial = editor.currentSettings();
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("editor", &editor);
    QStringList warnings;
    QObject::connect(&engine, &QQmlEngine::warnings, [&](const QList<QQmlError> &errors) {
        for (const auto &e : errors)
            warnings.append(e.toString());
    });
    QQmlComponent component(&engine);
    component.setData(
        ("import QtQuick\nimport QtQuick.Controls\nimport \"" +
         QUrl::fromLocalFile(qmlPath).toString() +
         "\"\nApplicationWindow {width:800;height:1000;visible:true;LocalOverlay "
         "{width:400;height:300} ScrollView {x:420;width:350;height:980;LocalPanel {width:330}}}")
            .toUtf8(),
        QUrl());
    wait([&] { return component.status() != QQmlComponent::Loading; });
    if (component.isError())
        throw std::runtime_error(component.errorString().toStdString());
    const std::unique_ptr<QObject> window(component.create());
    require(window != nullptr, "Local QML did not load.");
    auto *quick = qobject_cast<QQuickWindow *>(window.get());
    require(quick != nullptr, "Local test window missing.");
    auto child = [&](const char *name) {
        auto *item = window->findChild<QObject *>(QString::fromLatin1(name));
        if (!item)
            throw std::runtime_error(std::string("Missing local QML control: ") + name);
        return item;
    };
    require(child("localPanelToggle")->setProperty("checked", true), "Cannot expand local panel.");
    require(QMetaObject::invokeMethod(child("addLinearMask"), "clicked"),
            "Cannot add mask through QML.");
    require(editor.currentSettings().localMasks.size() == 1 && editor.localMode(),
            "QML add failed.");
    require(!editor.localCanvasReady(), "Local tool accepted stale render.");
    wait([&] { return editor.localCanvasReady(); });
    const auto added = editor.currentSettings();
    auto *slider = child("local_exposure");
    require(slider->property("enabled").toBool(), "Local exposure disabled.");
    slider->setProperty("pressed", true);
    slider->setProperty("value", .3);
    QMetaObject::invokeMethod(slider, "moved");
    slider->setProperty("value", .7);
    QMetaObject::invokeMethod(slider, "moved");
    slider->setProperty("pressed", false);
    require(editor.currentSettings().localMasks[0].parameter("exposure") == .7,
            "Local QML slider failed.");
    const auto adjusted = editor.currentSettings();
    editor.undo();
    require(editor.currentSettings() == added, "Slider drag not one Undo.");
    editor.redo();
    require(editor.currentSettings() == adjusted, "Local Redo failed.");
    wait([&] { return editor.localCanvasReady(); });
    const auto handles = editor.localHandles();
    require(handles.size() == 3, "Local handles missing.");
    const auto handle = handles[2].toMap();
    const QPointF start(handle["x"].toDouble() * 400, handle["y"].toDouble() * 300);
    auto send = [&](QEvent::Type type, const QPointF &p, Qt::MouseButton button,
                    Qt::MouseButtons buttons) {
        QMouseEvent e(type, p, p, button, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(quick, &e);
        QCoreApplication::processEvents();
    };
    send(QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    send(QEvent::MouseMove, start + QPointF(22, 14), Qt::NoButton, Qt::LeftButton);
    send(QEvent::MouseMove, start + QPointF(31, 21), Qt::NoButton, Qt::LeftButton);
    send(QEvent::MouseButtonRelease, start + QPointF(31, 21), Qt::LeftButton, Qt::NoButton);
    const auto moved = editor.currentSettings();
    require(moved.localMasks[0].x1 != adjusted.localMasks[0].x1,
            "Actual pointer drag did not move mask.");
    editor.undo();
    require(editor.currentSettings() == adjusted, "Geometry drag not one Undo.");
    editor.redo();
    require(editor.currentSettings() == moved, "Geometry Redo failed.");
    editor.renameLocalMask("Свет на лице");
    const auto renamed = editor.currentSettings();
    editor.renameLocalMask(" ");
    require(editor.currentSettings() == renamed && !editor.error().isEmpty(),
            "Invalid rename mutated mask.");
    editor.setLocalInverted(true);
    editor.setLocalEnabled(false);
    require(!editor.currentSettings().localMasks[0].enabled, "Local flags failed.");
    editor.undo();
    editor.undo();
    require(editor.currentSettings() == renamed, "Local flags Undo failed.");
    const auto firstID = editor.localState()["id"].toString();
    const auto secondID = editor.addLocalMask("radial");
    require(!secondID.isEmpty(), "Cannot add radial mask.");
    editor.setLocalParameter("exposure", -.4);
    editor.setLocalParameter("angle", 35);
    editor.moveLocalMask(-1);
    require(editor.currentSettings().localMasks[0].id == secondID.toStdString(),
            "Mask reordering failed.");
    wait([&] { return editor.localCanvasReady(); });
    const auto radial = editor.currentSettings();
    require(editor.beginLocalGesture(-1, .3, .3), "Cannot start radial geometry gesture.");
    editor.adjustLocalGesture(.6, .65);
    editor.endEdit();
    require(editor.currentSettings() != radial, "Radial redraw failed.");
    editor.undo();
    require(editor.currentSettings() == radial, "Radial gesture not one Undo.");
    editor.selectLocalMask(firstID);
    editor.removeLocalMask();
    require(editor.currentSettings().localMasks.size() == 1, "Mask removal failed.");
    editor.undo();
    require(editor.currentSettings() == radial, "Delete Undo lost mask/order.");
    editor.selectLocalMask(secondID);
    editor.setLocalMode(true);
    wait([&] { return editor.localCanvasReady(); });
    require(editor.beginLocalGesture(0, .5, .5), "Cannot begin deactivation gesture.");
    editor.adjustLocalGesture(.52, .52);
    const auto deactivated = editor.currentSettings();
    QEvent deactivate(QEvent::ApplicationDeactivate);
    QCoreApplication::sendEvent(QCoreApplication::instance(), &deactivate);
    editor.adjustLocalGesture(.7, .7);
    require(editor.currentSettings() == deactivated, "Deactivated gesture continued writing.");
    editor.undo();
    require(editor.currentSettings() == radial, "Deactivation did not end one gesture.");
    editor.setLocalMode(true);
    editor.setCropMode(true);
    require(!editor.localMode() && editor.cropMode(), "Crop and local overlap.");
    editor.setLocalMode(true);
    require(!editor.cropMode(), "Local tool did not close Crop.");
    editor.setGuidedMode(true);
    require(!editor.localMode(), "Guided did not close local.");
    editor.setLocalMode(true);
    require(!editor.guidedMode(), "Local did not close Guided.");
    editor.setOriginal(true);
    require(!editor.localMode(), "Original did not close local.");
    editor.setOriginal(false);
    editor.setLocalMode(true);
    editor.setTargetedTool("curve.red");
    require(!editor.localMode(), "Targeted tool did not close local.");
    editor.setTargetedTool("none");
    PresetController presets(root + "/local-presets.json", editor);
    presets.copySettings(1);
    editor.setParameter("exposure", 1);
    presets.pasteSettings();
    require(editor.currentSettings() == radial, "Global preset copy erased masks.");
    editor.setLocalMode(true);
    editor.setLocalOverlay(true);
    editor.setLocalOverlayColor("#00cbea");
    editor.setLocalOverlayOpacity(.8);
    const auto exported = root + "/local-overlay.png";
    editor.exportPhoto(QUrl::fromLocalFile(exported), 1, 95, false);
    wait([&] { return !editor.exporting() && !editor.saving(); });
    require(!editor.localMode() && !QImage(exported).isNull(),
            "Local export failed or left tool open.");
    require(luma::Catalog(catalog.toStdString()).activeAsset()->edits == radial,
            "Autosave lost masks.");
    PreviewProvider restoredProvider;
    EditorController restored(catalog.toStdString(), &restoredProvider);
    restored.restore();
    wait([&] { return restored.hasPhoto() && !restored.busy(); });
    require(restored.currentSettings() == radial && !restored.localMode(),
            "Restart lost masks or retained overlay mode.");
    const auto after = root + "/local-restored.png";
    restored.exportPhoto(QUrl::fromLocalFile(after), 1, 95, false);
    wait([&] { return !restored.exporting(); });
    require(read(exported) == read(after), "Overlay or restart changed exported bytes.");
    editor.undo(); // Global paste (masks must survive full document history).
    require(editor.currentSettings().localMasks == radial.localMasks,
            "Global Undo lost local masks.");
    require(initial.localMasks.empty(), "New photo unexpectedly has masks.");
    QCoreApplication::processEvents();
    if (!warnings.empty())
        throw std::runtime_error(warnings.join('\n').toStdString());
}
