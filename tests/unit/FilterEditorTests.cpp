#include "apps/desktop/EditorController.h"
#include "apps/desktop/PresetController.h"
#include "apps/shared/BuiltinPresets.h"
#include <QElapsedTimer>
#include <QFile>
#include <QMouseEvent>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QThread>
#include <memory>
#include <source_location>

namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void wait(const std::function<bool()> &ready,
          std::source_location location = std::source_location::current()) {
    QElapsedTimer timer;
    timer.start();
    while (!ready() && timer.elapsed() < 30000) {
        QCoreApplication::processEvents();
        QThread::msleep(2);
    }
    if (!ready())
        throw std::runtime_error("Filter editor wait timed out at line " +
                                 std::to_string(location.line()));
}
} // namespace
void filterEditorTests(const QString &source, const QString &root, const QString &qmlPath) {
    PreviewProvider provider;
    const auto catalog = root + "/filter.sqlite", collection = root + "/filter-presets.json";
    EditorController editor(catalog.toStdString(), &provider);
    PresetController presets(collection, editor);
    require(presets.items().isEmpty() && presets.builtinItems().size() == 8 &&
                !QFile::exists(collection),
            "Built-ins wrote user storage or replaced user preset items.");
    const QString id = "builtin:soft-portrait";
    require(!presets.save(id, "Modified", {}, 12, false) && !presets.remove(id),
            "Built-in mutation was allowed.");
    require(!QFile::exists(collection), "Refused built-in mutation wrote storage.");
    require(presets.refresh(), "Cannot clear expected read-only errors.");
    editor.openFile(QUrl::fromLocalFile(source));
    wait([&] { return editor.hasPhoto() && !editor.busy() && !editor.previewUrl().isEmpty(); });
    editor.beginEdit();
    editor.setExposure(0.35);
    editor.setParameter("temperature", 5100);
    editor.setParameter("tint", 12);
    editor.setParameter("sharpen.amount", 60);
    editor.setParameter("noise.color", 40);
    editor.setParameter("crop.x", 0.1);
    editor.setParameter("crop.width", 0.8);
    editor.setParameter("transform.rotate", 4);
    editor.setCorrectionEnabled("curves", false);
    editor.endEdit();
    const auto baseline = editor.currentSettings();
    wait([&] { return !editor.saving(); });
    const auto baselinePixels = provider.requestImage({}, nullptr, {});
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("editor", &editor);
    engine.rootContext()->setContextProperty("presets", &presets);
    QStringList warnings;
    QObject::connect(&engine, &QQmlEngine::warnings, [&](const QList<QQmlError> &errors) {
        for (const auto &error : errors)
            warnings.append(error.toString());
    });
    QQmlComponent component(&engine);
    component.setData(("import QtQuick\nimport QtQuick.Controls\nimport \"" +
                       QUrl::fromLocalFile(qmlPath).toString() +
                       "\"\nApplicationWindow {width: 400; height: 800; visible: true; PresetPanel "
                       "{ anchors.fill: parent; anchors.margins: 20 }}")
                          .toUtf8(),
                      QUrl());
    wait([&] { return component.status() != QQmlComponent::Loading; });
    if (component.isError())
        throw std::runtime_error(component.errorString().toStdString());
    const std::unique_ptr<QObject> window(component.create());
    auto *quick = qobject_cast<QQuickWindow *>(window.get());
    require(quick != nullptr, "Filter QML window did not load.");
    const std::function<QQuickItem *(QQuickItem *, const QString &)> find =
        [&](QQuickItem *parent, const QString &name) -> QQuickItem * {
        if (parent->objectName() == name)
            return parent;
        for (auto *item : parent->childItems())
            if (auto *found = find(item, name))
                return found;
        return nullptr;
    };
    wait([&] { return find(quick->contentItem(), "filter_soft-portrait") != nullptr; });
    auto *row = find(quick->contentItem(), "filter_soft-portrait");
    wait([&] { return quick->isExposed() && row->width() > 0; });
    const auto point = row->mapToScene(QPointF(row->width() / 2, row->height() / 2));
    const auto pointer = [&](QEvent::Type type, QPointF p, Qt::MouseButton button = Qt::NoButton,
                             Qt::MouseButtons buttons = Qt::NoButton) {
        QMouseEvent event(type, p, p, button, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(quick, &event);
        QCoreApplication::processEvents();
    };
    const auto initialPreview = editor.previewUrl();
    QEnterEvent entered(point, point, point);
    QCoreApplication::sendEvent(quick, &entered);
    pointer(QEvent::MouseMove, point);
    wait([&] { return editor.presetPreview() && editor.previewUrl() != initialPreview; });
    const auto hoveredStored = luma::Catalog(catalog.toStdString()).activeAsset();
    require(editor.currentSettings() == baseline && !editor.saving() && hoveredStored &&
                hoveredStored->edits == baseline,
            "Actual filter hover modified edits/history or autosave.");
    const auto hoveredPixels = provider.requestImage({}, nullptr, {});
    require(hoveredPixels != baselinePixels, "Filter hover did not render the look.");
    const auto hoveredURL = editor.previewUrl();
    pointer(QEvent::MouseMove, QPointF(390, 790));
    wait([&] { return !editor.presetPreview() && editor.previewUrl() != hoveredURL; });
    require(provider.requestImage({}, nullptr, {}) == baselinePixels,
            "Leaving filter hover did not restore pixels.");
    pointer(QEvent::MouseMove, point);
    pointer(QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    pointer(QEvent::MouseButtonRelease, point, Qt::LeftButton);
    const auto applied = luma::mergeSettings(baseline, presets.snapshot(id).settings, 12);
    wait([&] { return editor.currentSettings() == applied && !editor.saving(); });
    require(!QFile::exists(collection), "Applying a built-in wrote the user preset collection.");
    editor.undo();
    require(editor.currentSettings() == baseline, "Filter application was not one Undo.");
    editor.redo();
    require(editor.currentSettings() == applied, "Filter Redo changed excluded settings.");
    presets.apply(id);
    editor.undo();
    require(editor.currentSettings() == baseline, "Idempotent filter application added Undo.");
    editor.redo();
    pointer(QEvent::MouseMove, QPointF(390, 790));
    wait([&] { return !editor.saving(); });
    const auto json = root + "/exported-filter.json";
    require(presets.exportPreset(id, QUrl::fromLocalFile(json)) &&
                presets.importPreset(QUrl::fromLocalFile(json)),
            "Built-in JSON export/import failed.");
    require(presets.items().size() == 1, "Import did not create an editable user copy.");
    const auto userID = presets.items()[0].toMap()["id"].toString();
    require(!luma::builtinPresetID(userID) &&
                presets.snapshot(userID).settings == presets.snapshot(id).settings,
            "Imported filter ID or values are incorrect.");
    require(presets.save(userID, "My look", "Custom", 12, false), "Imported look is not editable.");
    require(presets.builtinItems()[1].toMap()["name"].toString() == "Soft Portrait",
            "User rename changed a built-in.");
    wait([&] { return !editor.saving(); });
    const auto appliedStored = luma::Catalog(catalog.toStdString()).activeAsset();
    require(appliedStored && appliedStored->edits == applied,
            "Applied filter did not persist exactly.");
    PreviewProvider restoredProvider;
    EditorController restored(catalog.toStdString(), &restoredProvider);
    restored.restore();
    wait([&] {
        return restored.hasPhoto() && !restored.busy() && !restored.previewUrl().isEmpty();
    });
    require(restored.currentSettings() == applied, "Restart changed applied filter settings.");
    const auto firstExport = root + "/filter-first.png", secondExport = root + "/filter-second.png";
    editor.exportPhoto(QUrl::fromLocalFile(firstExport), 1, 92, false);
    wait([&] { return !editor.exporting(); });
    restored.exportPhoto(QUrl::fromLocalFile(secondExport), 1, 92, false);
    wait([&] { return !restored.exporting(); });
    QFile first(firstExport), second(secondExport);
    require(first.open(QIODevice::ReadOnly) && second.open(QIODevice::ReadOnly) &&
                first.readAll() == second.readAll(),
            "Applied filter exports differ after restart.");
    require(editor.error().isEmpty() && restored.error().isEmpty(), "Filter export failed.");
    if (!warnings.isEmpty())
        throw std::runtime_error(warnings.join('\n').toStdString());
}
