#include "apps/desktop/BatchController.h"
#include "apps/desktop/EditorController.h"
#include "apps/desktop/LibraryModel.h"
#include "apps/desktop/PresetController.h"
#include "apps/desktop/UpdateController.h"
#include "core/presets/SettingsSubset.h"
#include "core/raw/RawDecoder.h"
#include "platform/macos/FileHasher.h"
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QTemporaryDir>
#include <QThread>
#include <iostream>
#include <sqlite3.h>

std::unique_ptr<NativeUpdater> makeNativeUpdater(UpdateCallbacks) {
    return {};
}
namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <typename F> void throws(F work) {
    bool failed = false;
    try {
        work();
    } catch (const std::exception &) {
        failed = true;
    }
    require(failed, "Expected batch failure.");
}
template <typename F> void wait(F ready) {
    QElapsedTimer timer;
    timer.start();
    while (!ready() && timer.elapsed() < 30000) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    require(ready(), "Batch acceptance timed out.");
}
void sql(const std::filesystem::path &path, const std::string &statement) {
    sqlite3 *raw = nullptr;
    require(sqlite3_open(path.c_str(), &raw) == SQLITE_OK, "Cannot open batch fixture.");
    const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
    require(sqlite3_exec(db.get(), statement.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK,
            "Cannot prepare batch fixture.");
}
struct Fixture {
    std::filesystem::path catalog;
    std::vector<std::filesystem::path> paths;
    std::vector<std::string> ids;
    std::vector<luma::EditDocument> edits;
};
Fixture fixture(const std::filesystem::path &source, const std::filesystem::path &root, int count) {
    std::filesystem::create_directories(root);
    Fixture result{root / "catalog.sqlite", {}, {}, {}};
    luma::Catalog db(result.catalog);
    const auto metadata = luma::RawDecoder().inspect(source);
    const auto checksum = luma::macos::sha256File(source);
    for (int i = 0; i < count; ++i) {
        const auto name =
            i == 0 ? std::string(90, 'a') + ".DNG" : "photo-" + std::to_string(i) + ".DNG";
        const auto path = root / name;
        std::filesystem::copy_file(source, path);
        const auto asset = db.addAndSelect(path, checksum, metadata);
        luma::EditDocument edits;
        edits.exposure = static_cast<double>(i) / 100;
        edits.temperature = 4500 + i * 10;
        edits.customWhiteBalance = i % 2 != 0;
        edits.hsl[0].hue = static_cast<double>(i % 50);
        edits.sharpenAmount = static_cast<double>(i % 150);
        edits.sharpenRadius = 1.5;
        edits.crop = {
            0.1,        0.15, 0.7, 0.6, static_cast<double>(i % 15), 1.5, static_cast<int>(i % 4),
            i % 2 == 0, false};
        edits.setEnabled(luma::Correction::crop, i % 3 == 0);
        edits.lens = {"0123456789abcdef0123456789abcdef", static_cast<double>(i), 80, i % 2 == 0};
        edits.setEnabled(luma::Correction::lens, i % 3 == 0);
        edits.transform.rotate = static_cast<double>(i % 15);
        edits.transform.scale = 110 + static_cast<double>(i % 50);
        edits.setEnabled(luma::Correction::transform, i % 3 == 0);
        edits.noiseLuminance = static_cast<double>(i % 100);
        edits.noiseColor = 15 + static_cast<double>(i % 80);
        edits.setEnabled(luma::Correction::colorNoise, i % 3 == 0);
        edits.setEnabled(luma::Correction::sharpening, i % 2 == 0);
        edits.curves[0].points = {{0, 0.02}, {0.5, 0.6}, {1, 1}};
        edits.setEnabled(luma::Correction::highlights, i % 2 == 0);
        db.saveEdits(asset.id, edits);
        result.paths.push_back(path);
        result.ids.push_back(asset.id);
        result.edits.push_back(edits);
    }
    db.setRating(result.ids, 4);
    const auto collection = db.createCollection("Batch fixture");
    db.collectionMembership(collection, result.ids, true);
    return result;
}
std::vector<luma::EditDocument> documents(const Fixture &fixture) {
    luma::Catalog db(fixture.catalog);
    std::vector<luma::EditDocument> result;
    for (const auto &id : fixture.ids)
        result.push_back(db.assetByID(id)->edits);
    return result;
}
void coreTests(const std::filesystem::path &source, const std::filesystem::path &root) {
    const auto f = fixture(source, root, 40);
    luma::Catalog db(f.catalog);
    luma::EditDocument sourceEdits;
    sourceEdits.exposure = 1.25;
    sourceEdits.temperature = 6200;
    sourceEdits.customWhiteBalance = false;
    sourceEdits.hsl[0].hue = -35;
    sourceEdits.curves[1].points = {{0, 0}, {0.4, 0.6}, {1, 1}};
    sourceEdits.setEnabled(luma::Correction::shadows, false);
    bool observed = false;
    const auto applied = db.applyBatchEdits(
        f.ids, sourceEdits, 1 | 4, {},
        [&](double) {
            if (!observed) {
                require(documents(f) == f.edits, "Reader observed a partially committed batch.");
                observed = true;
            }
        },
        f.ids[0]);
    require(applied.changed == 40 && applied.active && db.canUndoBatch(),
            "Batch missed selected photos.");
    const auto after = documents(f);
    for (std::size_t i = 0; i < after.size(); ++i) {
        require(after[i].exposure == 1.25 && after[i].temperature == f.edits[i].temperature &&
                    after[i].hsl == f.edits[i].hsl && after[i].curves == sourceEdits.curves &&
                    after[i].sharpenAmount == f.edits[i].sharpenAmount &&
                    after[i].enabled(luma::Correction::sharpening) ==
                        f.edits[i].enabled(luma::Correction::sharpening),
                "Batch changed excluded groups or missed included ones.");
    }
    const auto noop = db.applyBatchEdits(f.ids, sourceEdits, 1 | 4);
    require(noop.changed == 0 && db.canUndoBatch(), "No-op erased the previous batch journal.");
    require(db.restoreBatchEdits(false, {}, f.ids[0]).active == f.edits[0],
            "Undo active snapshot failed.");
    require(documents(f) == f.edits && db.canRedoBatch(),
            "Batch Undo did not restore every photo.");
    db.restoreBatchEdits(true);
    require(documents(f) == after, "Batch Redo changed its saved result.");
    throws([&] { db.applyBatchEdits({f.ids[0], "missing"}, sourceEdits, 15); });
    throws([&] { db.applyBatchEdits({f.ids[0], f.ids[0]}, sourceEdits, 15); });
    require(documents(f) == after && db.canUndoBatch(),
            "Failed selection changed photos or old journal.");
    luma::CancellationSource cancelled;
    throws([&] {
        db.applyBatchEdits(f.ids, sourceEdits, 15, cancelled.token(), [&](double progress) {
            if (progress >= 0.25)
                cancelled.cancel();
        });
    });
    require(documents(f) == after && db.canUndoBatch(),
            "Cancellation partially committed or erased Undo.");
    sql(f.catalog, "CREATE TRIGGER fail_batch BEFORE INSERT ON edit_hsl_bands WHEN NEW.band=3 "
                   "BEGIN SELECT RAISE(ABORT,'injected'); END");
    throws([&] { db.applyBatchEdits(f.ids, sourceEdits, 15); });
    throws([&] { db.restoreBatchEdits(false); });
    require(documents(f) == after && db.canUndoBatch(), "Failed batch/Undo partially committed.");
    sql(f.catalog, "DROP TRIGGER fail_batch");
    auto changed = after[5];
    changed.hsl[0].hue += 7;
    luma::Catalog(f.catalog).saveEdits(f.ids[5], changed);
    const auto conflict = documents(f);
    throws([&] { db.restoreBatchEdits(false); });
    require(documents(f) == conflict, "Batch Undo overwrote newer edits.");
    luma::Catalog(f.catalog).saveEdits(f.ids[5], after[5]);
    db.restoreBatchEdits(false);
    require(documents(f) == f.edits, "Retrying conflict-free Undo failed.");
    // A failed replacement must also retain the redo journal, not only Undo.
    throws([&] { db.applyBatchEdits({f.ids[0], "missing"}, sourceEdits, 15); });
    require(db.canRedoBatch(), "Failed replacement erased Redo.");
    db.restoreBatchEdits(true);
    require(luma::Catalog(f.catalog).libraryPage({}, 0, 40)[0].rating == 4 &&
                db.collections()[0].count == 40,
            "Batch altered Library metadata or membership.");
    db.verify();
    for (const auto &path : f.paths)
        require(luma::macos::sha256File(path) == luma::macos::sha256File(source),
                "Batch changed original RAW.");
    luma::Catalog restarted(f.catalog);
    require(!restarted.canUndoBatch() && documents(f) == after,
            "Batch edits/journal restart contract failed.");
    auto detailSource = sourceEdits;
    detailSource.sharpenAmount = 85;
    detailSource.sharpenMasking = 60;
    detailSource.noiseLuminance = 70;
    detailSource.noiseLuminanceDetail = 30;
    detailSource.noiseLuminanceContrast = 45;
    detailSource.noiseColor = 80;
    detailSource.noiseColorDetail = 25;
    detailSource.noiseColorSmoothness = 90;
    detailSource.setEnabled(luma::Correction::colorNoise, false);
    const auto detailResult = db.applyBatchEdits({f.ids[0], f.ids[2]}, detailSource, 16);
    require(detailResult.changed == 2, "Detail batch missed its selected subset.");
    const auto detailed = documents(f);
    for (std::size_t i = 0; i < detailed.size(); ++i)
        require(detailed[i] ==
                    (i == 0 || i == 2 ? luma::mergeSettings(after[i], detailSource, 16) : after[i]),
                "Detail batch changed excluded photos/groups.");
    db.restoreBatchEdits(false);
    require(documents(f) == after, "Detail batch Undo lost new columns.");
    db.restoreBatchEdits(true);
    require(documents(f) == detailed, "Detail batch Redo lost new columns.");
    auto cropSource = detailSource;
    cropSource.crop = {0.2, 0.1, 0.5, 0.7, -7, 4.0 / 3, 2, true, true};
    cropSource.setEnabled(luma::Correction::crop, false);
    require(db.applyBatchEdits({f.ids[1], f.ids[3]}, cropSource, 32).changed == 2,
            "Crop batch missed targets.");
    const auto cropped = documents(f);
    for (std::size_t i = 0; i < cropped.size(); ++i)
        require(cropped[i] == (i == 1 || i == 3 ? luma::mergeSettings(detailed[i], cropSource, 32)
                                                : detailed[i]),
                "Crop batch changed excluded groups/photos.");
    db.restoreBatchEdits(false);
    require(documents(f) == detailed, "Crop batch Undo lost geometry.");
    db.restoreBatchEdits(true);
    require(documents(f) == cropped, "Crop batch Redo lost geometry.");
    auto opticsSource = cropSource;
    opticsSource.lens = {"abcdef0123456789abcdef0123456789", 135, 45, false};
    opticsSource.setEnabled(luma::Correction::lens, true);
    require(db.applyBatchEdits({f.ids[0], f.ids[2]}, opticsSource, 64).changed == 2,
            "Optics batch missed targets.");
    const auto corrected = documents(f);
    for (std::size_t i = 0; i < corrected.size(); ++i)
        require(
            corrected[i] ==
                (i == 0 || i == 2 ? luma::mergeSettings(cropped[i], opticsSource, 64) : cropped[i]),
            "Optics batch changed excluded settings/photos.");
    db.restoreBatchEdits(false);
    require(documents(f) == cropped, "Optics batch Undo lost settings.");
    db.restoreBatchEdits(true);
    require(documents(f) == corrected, "Optics batch Redo lost settings.");
    auto geometrySource = opticsSource;
    geometrySource.transform.rotate = 8;
    geometrySource.transform.horizontal = 25;
    geometrySource.transform.vertical = -15;
    geometrySource.transform.scale = 120;
    geometrySource.transform.guided = true;
    geometrySource.transform.guide = {1, 0.02, 0, -0.01, 1, 0, 0.03, -0.02};
    geometrySource.setEnabled(luma::Correction::transform, true);
    require(db.applyBatchEdits({f.ids[1], f.ids[3]}, geometrySource, 128).changed == 2,
            "Geometry batch missed selected targets.");
    const auto transformed = documents(f);
    for (std::size_t i = 0; i < transformed.size(); ++i)
        require(transformed[i] == (i == 1 || i == 3
                                       ? luma::mergeSettings(corrected[i], geometrySource, 128)
                                       : corrected[i]),
                "Geometry batch changed excluded groups/photos.");
    db.restoreBatchEdits(false);
    require(documents(f) == corrected, "Geometry batch Undo lost matrix/parameters.");
    db.restoreBatchEdits(true);
    require(documents(f) == transformed, "Geometry batch Redo changed matrix/parameters.");
}
void qmlTests(EditorController &editor, LibraryModel &library, PresetController &presets,
              BatchController &batch, const QString &qmlPath) {
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("editor", &editor);
    engine.rootContext()->setContextProperty("library", &library);
    engine.rootContext()->setContextProperty("presets", &presets);
    engine.rootContext()->setContextProperty("batch", &batch);
    QStringList warnings;
    QObject::connect(&engine, &QQmlEngine::warnings, [&](const QList<QQmlError> &errors) {
        for (const auto &error : errors)
            warnings.append(error.toString());
    });
    QQmlComponent component(&engine);
    component.setData(("import QtQuick\nimport QtQuick.Controls\nimport \"" +
                       QUrl::fromLocalFile(qmlPath).toString() +
                       "\"\nApplicationWindow { width: 1000; height: 500; visible: true; "
                       "font.family: \"Helvetica Neue\"; BatchSettings { anchors.fill: parent } }")
                          .toUtf8(),
                      QUrl());
    wait([&] { return component.status() != QQmlComponent::Loading; });
    if (component.isError())
        throw std::runtime_error(component.errorString().toStdString());
    const std::unique_ptr<QObject> window(component.create());
    require(window != nullptr, "Batch QML did not load.");
    const auto beforeFilter = editor.currentSettings();
    auto *choice = window->findChild<QObject *>("batchPresetChoice");
    require(choice && choice->property("count").toInt() == presets.items().size() + 8,
            "Batch QML did not expose built-in filters and user presets.");
    choice->setProperty("currentIndex", 3);
    auto *apply = window->findChild<QObject *>("batchApplyPreset");
    require(apply && QMetaObject::invokeMethod(apply, "clicked"),
            "Cannot apply built-in from batch QML.");
    wait([&] { return !batch.working(); });
    require(batch.error().isEmpty() &&
                editor.currentSettings() ==
                    luma::mergeSettings(beforeFilter,
                                        presets.snapshot("builtin:warm-film").settings, 12),
            "Batch QML applied the wrong filter/subset.");
    batch.undo();
    wait([&] { return !batch.working(); });
    require(editor.currentSettings() == beforeFilter,
            "Batch QML filter Undo changed excluded settings.");
    auto *button = window->findChild<QObject *>("batchSync");
    require(button && button->property("enabled").toBool(), "Sync control is not available.");
    require(QMetaObject::invokeMethod(button, "clicked"), "Cannot open Sync dialog.");
    auto *dialog = window->findChild<QObject *>("batchSyncDialog");
    require(dialog && dialog->property("visible").toBool(), "Sync dialog did not open.");
    dialog->setProperty("groups", 2 | 8 | 16 | 32 | 64 | 128);
    auto *confirm = window->findChild<QObject *>("batchSyncConfirm");
    require(confirm && QMetaObject::invokeMethod(confirm, "clicked"),
            "Cannot apply Sync from QML.");
    require(batch.working(), "QML did not start batch operation.");
    wait([&] { return !batch.working(); });
    require(batch.error().isEmpty(), "QML Sync failed.");
    QCoreApplication::processEvents();
    if (!warnings.isEmpty())
        throw std::runtime_error(warnings.join('\n').toStdString());
}
void controllerTests(const std::filesystem::path &source, const std::filesystem::path &root,
                     const QString &qmlPath) {
    const auto f = fixture(source, root, 6);
    PreviewProvider provider;
    EditorController editor(f.catalog, &provider);
    LibraryModel library(f.catalog);
    PresetController presets(QString::fromStdString((root / "presets.json").string()), editor);
    BatchController batch(f.catalog, editor, library, presets);
    editor.openFile(QUrl::fromLocalFile(QString::fromStdString(f.paths[0].string())));
    wait([&] {
        return !editor.busy() && editor.hasPhoto() && !library.working() && library.count() == 6;
    });
    library.selectAll();
    wait([&] { return library.selectedCount() == 6 && !library.selecting(); });
    editor.setExposure(1.5);
    require(presets.save({}, "Batch <b>Light</b>", {}, 1, true), "Cannot create batch preset.");
    const auto presetID = presets.items()[0].toMap()["id"].toString();
    editor.setExposure(0.75);
    editor.setParameter("temperature", 7500);
    batch.applyPreset(presetID);
    require(editor.editingLocked() && library.working() && batch.waitingForSave(),
            "Batch did not lock/wait for autosave.");
    editor.setExposure(-2);
    require(editor.exposure() == 0.75, "Editing remained enabled while batch was waiting.");
    wait([&] { return !batch.working(); });
    require(batch.error().isEmpty() && batch.canUndo() && editor.exposure() == 1.5 &&
                !editor.saving(),
            "Batch failed or autosave overwrote the applied edit.");
    auto after = documents(f);
    require(after[0].temperature == 7500 && after[0].customWhiteBalance,
            "Batch lost an excluded pending WB edit.");
    for (const auto &doc : after)
        require(doc.exposure == 1.5, "Preset missed a selected photo.");
    batch.undo();
    wait([&] { return !batch.working(); });
    require(editor.exposure() == 0.75 && batch.canRedo(),
            "Batch Undo did not refresh the active photo.");
    batch.redo();
    wait([&] { return !batch.working(); });
    require(documents(f) == after, "Controller Redo changed applied settings.");
    editor.setParameter("vibrance", 12);
    wait([&] { return !editor.saving(); });
    batch.undo();
    wait([&] { return !batch.working(); });
    require(!batch.error().isEmpty() && editor.parameters()["vibrance"].toDouble() == 12,
            "Batch Undo overwrote a newer manual edit.");
    editor.undo();
    wait([&] { return !editor.saving(); });
    batch.undo();
    wait([&] { return !batch.working(); });
    require(batch.error().isEmpty() && editor.exposure() == 0.75,
            "Undo did not recover after resolving conflict.");
    presets.copySettings(2 | 8);
    batch.paste();
    wait([&] { return !batch.working(); });
    const auto pasted = documents(f);
    for (std::size_t i = 0; i < pasted.size(); ++i)
        require(pasted[i].temperature == 7500 &&
                    pasted[i].exposure == (i == 0 ? 0.75 : f.edits[i].exposure),
                "Batch Paste changed excluded Light settings.");
    const auto beforeFilter = documents(f);
    batch.applyPreset("builtin:warm-film");
    wait([&] { return !batch.working(); });
    require(batch.error().isEmpty(), "Built-in filter batch failed.");
    const auto filtered = documents(f);
    for (std::size_t i = 0; i < filtered.size(); ++i)
        require(filtered[i] == luma::mergeSettings(beforeFilter[i],
                                                   presets.snapshot("builtin:warm-film").settings,
                                                   12),
                "Built-in batch overwrote per-photo excluded settings.");
    batch.undo();
    wait([&] { return !batch.working(); });
    require(documents(f) == beforeFilter, "Built-in batch Undo did not restore target settings.");
    batch.redo();
    wait([&] { return !batch.working(); });
    require(documents(f) == filtered, "Built-in batch Redo changed target settings.");
    batch.undo();
    wait([&] { return !batch.working(); });
    qmlTests(editor, library, presets, batch, qmlPath);
    const auto synced = documents(f);
    for (std::size_t i = 0; i < synced.size(); ++i)
        require(synced[i].sharpenAmount == editor.currentSettings().sharpenAmount &&
                    synced[i].enabled(luma::Correction::sharpening) ==
                        editor.currentSettings().enabled(luma::Correction::sharpening) &&
                    synced[i].crop == editor.currentSettings().crop &&
                    synced[i].enabled(luma::Correction::crop) ==
                        editor.currentSettings().enabled(luma::Correction::crop) &&
                    synced[i].transform == editor.currentSettings().transform &&
                    synced[i].enabled(luma::Correction::transform) ==
                        editor.currentSettings().enabled(luma::Correction::transform) &&
                    synced[i].noiseColor == editor.currentSettings().noiseColor &&
                    synced[i].noiseLuminance == editor.currentSettings().noiseLuminance &&
                    synced[i].enabled(luma::Correction::colorNoise) ==
                        editor.currentSettings().enabled(luma::Correction::colorNoise) &&
                    synced[i].exposure == pasted[i].exposure,
                "QML Sync missed Detail or changed excluded Light.");
    // A failed active-photo save cannot be overtaken by a batch write.
    sql(f.catalog, "CREATE TRIGGER fail_save BEFORE UPDATE ON edit_documents BEGIN SELECT "
                   "RAISE(ABORT,'save blocked'); END");
    editor.setParameter("contrast", 23);
    wait([&] { return !editor.error().isEmpty() && editor.saving(); });
    const auto durable = documents(f);
    const auto draft = editor.currentSettings();
    require(batch.requestClose() && !editor.requestClose(),
            "Failed autosave must reject close while an idle batch is ready.");
    batch.sync(15);
    QCoreApplication::processEvents();
    require(batch.waitingForSave() && editor.currentSettings() == draft,
            "Batch bypassed a failed autosave.");
    batch.cancel();
    require(!batch.working() && !editor.editingLocked() && editor.currentSettings() == draft &&
                documents(f) == durable,
            "Cancelling save preparation lost the draft or changed targets.");
    sql(f.catalog, "DROP TRIGGER fail_save");
    editor.retrySave();
    wait([&] { return !editor.saving(); });
    bool installed = false;
    UpdateController updates(
        [&] { return !batch.working() && !editor.saving() && !library.working(); }, false);
    QObject::connect(&updates, &UpdateController::preparingRestart, &batch,
                     &BatchController::cancel);
    batch.sync(15);
    updates.postponeInstallation([&] {
        require(!batch.working() && !editor.saving() && !editor.editingLocked(),
                "Update restarted during batch/autosave.");
        installed = true;
    });
    wait([&] { return installed; });
    require(!batch.working() && documents(f)[0] == draft, "Update preparation lost manual edits.");
    const auto beforeClose = documents(f);
    bool closeRequested = false, closed = false;
    QObject::connect(&batch, &BatchController::changed, [&] {
        if (batch.working() && !batch.waitingForSave() && !closeRequested) {
            closeRequested = true;
            require(!batch.requestClose(), "Close did not wait for queued/running rollback.");
        }
    });
    QObject::connect(&batch, &BatchController::closeReady, [&] { closed = true; });
    batch.sync(15);
    wait([&] { return closed; });
    require(documents(f) == beforeClose && !editor.editingLocked(),
            "Close committed or retained a cancelled batch.");
    // Another close participant can reject closing after batch readiness.
    batch.sync(15);
    require(batch.working(), "Batch remained locked after deferred close readiness.");
    batch.cancel();
    require(editor.error().isEmpty(), "Unexpected editor error after batch acceptance.");
}
} // namespace
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");
    try {
        require(argc == 3, "Expected synthetic RAW and QML directory.");
        QTemporaryDir temporary;
        require(temporary.isValid(), "Cannot create batch test directory.");
        coreTests(argv[1], std::filesystem::path(temporary.path().toStdString()) / "core");
        controllerTests(argv[1],
                        std::filesystem::path(temporary.path().toStdString()) / "controller",
                        QString::fromUtf8(argv[2]));
        std::cout << "Atomic batch, cancellation, conflict-aware Undo, autosave, QML and update "
                     "guards passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
