#include "apps/desktop/EditorController.h"
#include "apps/desktop/PresetController.h"
#include "core/lens/LensProfiles.h"
#include <QElapsedTimer>
#include <QImage>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QThread>
#include <memory>

namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
void wait(const std::function<bool()> &ready) {
    QElapsedTimer timer;
    timer.start();
    while (!ready() && timer.elapsed() < 30000) {
        QCoreApplication::processEvents();
        QThread::msleep(2);
    }
    require(ready(), "Lens editor wait timed out.");
}
} // namespace
void lensEditorTests(const QString &source, const QString &root, const QString &qmlPath) {
    PreviewProvider provider;
    const auto catalog = root + "/lens.sqlite";
    EditorController editor(catalog.toStdString(), &provider);
    editor.openFile(QUrl::fromLocalFile(source));
    wait([&] { return editor.hasPhoto() && !editor.busy() && !editor.previewUrl().isEmpty(); });
    const auto original = editor.currentSettings();
    require(!original.enabled(luma::Correction::lens) &&
                editor.lensState()["distortionAvailable"].toBool() &&
                editor.lensState()["vignettingAvailable"].toBool() &&
                editor.lensState()["caAvailable"].toBool(),
            "Lens metadata/calibration availability missing.");
    QQmlEngine engine;
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
         "\"\nApplicationWindow { width: 380; height: 720; visible: true; LensPanel {width: 340}}")
            .toUtf8(),
        QUrl());
    wait([&] { return component.status() != QQmlComponent::Loading; });
    if (component.isError())
        throw std::runtime_error(component.errorString().toStdString());
    const std::unique_ptr<QObject> window(component.create());
    require(window != nullptr, "Lens QML did not load.");
    const auto child = [&](const char *name) {
        auto *item = window->findChild<QObject *>(QString::fromLatin1(name));
        require(item != nullptr, "Lens control missing.");
        return item;
    };
    auto *enable = child("lensEnabled");
    require(!enable->property("checked").toBool(),
            "Optics initially enabled on old/default edits.");
    enable->setProperty("checked", true);
    require(QMetaObject::invokeMethod(enable, "toggled"), "Lens enable control failed.");
    const auto beforeUrl = editor.previewUrl();
    wait([&] { return editor.previewUrl() != beforeUrl; });
    require(editor.currentSettings().enabled(luma::Correction::lens),
            "Lens checkbox did not enable corrections.");
    const auto enabled = editor.currentSettings();
    editor.beginEdit();
    editor.setParameter("lens.distortion", 60);
    editor.setParameter("lens.distortion", 75);
    editor.endEdit();
    const auto dragged = editor.currentSettings();
    editor.undo();
    require(editor.currentSettings() == enabled, "Optics gesture created more than one Undo.");
    editor.redo();
    require(editor.currentSettings() == dragged, "Optics Redo changed settings.");
    auto *profiles = child("lensProfile");
    int chosen = -1;
    QString id;
    const auto options = profiles->property("model").toList();
    for (qsizetype i = 1; i < options.size(); ++i)
        if (options[i].toMap()["name"].toString().contains("18-140")) {
            chosen = static_cast<int>(i);
            id = options[i].toMap()["id"].toString();
            break;
        }
    require(chosen > 0 && QMetaObject::invokeMethod(profiles, "activated", Q_ARG(int, chosen)),
            "Manual lens profile cannot be chosen from QML.");
    require(editor.currentSettings().lens.profile == id.toStdString(),
            "QML selected wrong profile.");
    const auto manual = editor.currentSettings();
    auto *ca = child("lensCA");
    ca->setProperty("checked", false);
    require(QMetaObject::invokeMethod(ca, "toggled"), "Lens CA switch failed.");
    require(!editor.currentSettings().lens.chromaticAberration, "Lens CA was not disabled.");
    editor.undo();
    require(editor.currentSettings() == manual, "CA Undo changed other optics.");
    PresetController presets(root + "/lens-presets.json", editor);
    presets.copySettings(64);
    editor.resetLens();
    require(editor.currentSettings() == original, "Lens reset changed excluded groups.");
    presets.pasteSettings();
    require(editor.currentSettings() == manual, "Optics Copy/Paste failed.");
    require(presets.save({}, "Lens", {}, 64, true), "Optics preset not saved.");
    const auto presetID = presets.items()[0].toMap()["id"].toString();
    editor.resetLens();
    const auto reset = editor.currentSettings();
    presets.preview(presetID);
    require(editor.presetPreview() && editor.currentSettings() == reset,
            "Optics hover became durable.");
    presets.clearPreview();
    presets.apply(presetID);
    require(editor.currentSettings() == manual, "Optics preset apply changed selection.");
    editor.setParameter("lens.distortion", 20);
    wait([&] { return !editor.saving(); });
    require(luma::Catalog(catalog.toStdString()).activeAsset()->edits == editor.currentSettings(),
            "Optics autosave lost settings.");
    const auto saved = editor.currentSettings();
    editor.setTargetedTool(QStringLiteral("curve.red"));
    require(editor.beginTargeted(0.4, 0.5), "Targeted tool did not sample corrected optics.");
    editor.adjustTargeted(0.05);
    editor.endTargeted();
    require(editor.currentSettings() != saved, "Targeted adjustment did not change optics input.");
    editor.undo();
    require(editor.currentSettings() == saved, "Targeted Undo changed optics settings.");
    editor.setTargetedTool(QStringLiteral("none"));
    const auto normalUrl = editor.previewUrl();
    editor.setOriginal(true);
    wait([&] { return editor.previewUrl() != normalUrl; });
    editor.setOriginal(false);
    require(editor.currentSettings() == saved, "Original comparison erased optics.");
    editor.setCropMode(true);
    wait([&] { return editor.cropCanvasReady(); });
    const auto exportPath = root + "/lens-editor.png";
    editor.exportPhoto(QUrl::fromLocalFile(exportPath), 1, 92, false);
    wait([&] { return !editor.exporting(); });
    require(!editor.cropMode() && QImage(exportPath).size() == QSize(128, 96),
            "Lens export leaked Crop tool state or changed dimensions.");
    PreviewProvider restoredProvider;
    EditorController restored(catalog.toStdString(), &restoredProvider);
    restored.restore();
    wait([&] { return restored.hasPhoto() && !restored.busy(); });
    require(restored.currentSettings() == saved, "Lens restart changed durable edits.");
    QCoreApplication::processEvents();
    require(warnings.empty(), "Lens QML emitted warnings.");
}
