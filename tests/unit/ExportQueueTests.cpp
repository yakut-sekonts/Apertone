#include "apps/desktop/EditorController.h"
#include "apps/desktop/ExportController.h"
#include "apps/desktop/LibraryModel.h"
#include "apps/desktop/PresetController.h"
#include "apps/desktop/UpdateController.h"
#include "apps/shared/LibraryService.h"
#include "core/image/Bayer.h"
#include "core/raw/RawDecoder.h"
#include "platform/macos/FileHasher.h"
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QQmlComponent>
#include <QQmlContext>
#include <QThread>
#include <atomic>
#include <fstream>
#include <limits>
#include <mutex>
#include <sqlite3.h>

void writeExportExifFixture(const std::filesystem::path &path);
namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <typename F> void wait(F ready) {
    QElapsedTimer timer;
    timer.start();
    while (!ready() && timer.elapsed() < 30000) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    require(ready(), "Export queue timed out.");
}
template <typename F> void rejects(F operation) {
    bool failed = false;
    try {
        operation();
    } catch (const std::exception &) {
        failed = true;
    }
    require(failed, "Invalid export settings were accepted.");
}
struct Gate {
    std::atomic<int> entered{0}, permitted{0};
    std::mutex mutex;
    std::vector<luma::AssetRecord> assets;
    std::vector<luma::ExportOptions> options;
    bool failSecond = false;
    ExportController::Runner runner() {
        return [this](const auto &asset, const auto &destination, auto settings, auto stop,
                      const auto &progress) {
            const int index = entered.fetch_add(1) + 1;
            {
                const std::lock_guard lock(mutex);
                assets.push_back(asset);
                options.push_back(settings);
            }
            progress(.25);
            while (permitted.load() < index) {
                luma::checkCancellation(stop);
                QThread::msleep(1);
            }
            luma::checkCancellation(stop);
            if (failSecond && index == 2)
                throw std::runtime_error("Injected encoder error.");
            std::ofstream file(destination);
            file << asset.edits.exposure;
            file.close();
            require(bool(file), "Cannot write queue fixture output.");
            progress(1);
        };
    }
};
void settingsTests(const QString &destination) {
    luma::ExportSettings settings;
    settings.destination = destination;
    require(luma::ExportSettings::fromJson(settings.json()).json() == settings.json(),
            "Export settings roundtrip changed defaults.");
    require(settings.filename("photo.raw", 1, "2026-10-14") == "photo.jpg",
            "Default export naming changed.");
    settings.naming = "{date}_{sequence}_{filename}";
    require(settings.filename("Фото.NEF", 12, "2026-10-14") == "2026-10-14_0012_Фото.jpg",
            "Export naming tokens changed.");
    const auto literal = QString::fromUtf8("Снимок-%{filename}-{sequence}-{date}-🌅");
    require(settings.filename(literal + ".png", 12, "2026-10-14") ==
                "2026-10-14_0012_" + literal + ".jpg",
            "Export reinterpreted naming tokens inside the source filename.");
    settings.naming = "{filename}-{filename}-{date}-{sequence}";
    require(settings.filename(literal + ".png", 7, "date-{sequence}") ==
                literal + "-" + literal + "-date-{sequence}-0007.jpg",
            "Export recursively expanded repeated source/date replacement values.");
    settings.naming = "{camera}_{lens}_{iso}_{time}";
    settings.validate();
    require(settings.filename("photo.raw", 1, "undated") ==
                "unknown-camera_unknown-lens_unknown-iso_unknown-time.jpg",
            "Missing capture metadata did not use explicit defaults.");
    luma::PhotoMetadata metadata;
    metadata.camera = " Nikon/{sequence}:Z ";
    metadata.lens = "O\u0308ptik\\50\nmm";
    metadata.iso = 125.5;
    metadata.captureTimestamp = 0;
    require(settings.filename("photo.raw", 1, "1970-01-01", metadata) ==
                QString::fromUtf8("Nikon_{sequence}_Z_\u00d6ptik_50_mm_125.5_00-00-00.jpg"),
            "Metadata naming was unsafe, locale-dependent, or recursively expanded.");
    require(luma::ExportSettings::fromJson(settings.json()).json() == settings.json(),
            "New naming tokens did not round-trip in existing profile schema.");
    metadata.camera = "..";
    metadata.lens.clear();
    metadata.iso = std::numeric_limits<double>::infinity();
    metadata.captureTimestamp.reset();
    require(settings.filename("photo.raw", 1, "undated", metadata) ==
                "unknown-camera_unknown-lens_unknown-iso_unknown-time.jpg",
            "Invalid metadata did not use explicit defaults.");
    metadata.camera = std::string(231, 'x');
    rejects([&] { settings.filename("photo.raw", 1, "undated", metadata); });
    for (const auto &name : {"../{filename}", "{unknown}", "a/b", "a:b", ""}) {
        auto invalid = settings;
        invalid.naming = QString::fromUtf8(name);
        rejects([&] { invalid.validate(); });
    }
    auto json = settings.json();
    json.insert("quality", 1.5);
    rejects([&] { luma::ExportSettings::fromJson(json); });
    json = settings.json();
    json.insert("format", "4");
    rejects([&] { luma::ExportSettings::fromJson(json); });
    json = settings.json();
    json.insert("unknown", 0);
    rejects([&] { luma::ExportSettings::fromJson(json); });
    json = settings.json();
    json.remove("metadata");
    rejects([&] { luma::ExportSettings::fromJson(json); });
}
void qmlTest(ExportController &exporter, EditorController &editor, LibraryModel &library,
             PresetController &presets, const QString &qmlPath) {
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("exporter", &exporter);
    engine.rootContext()->setContextProperty("editor", &editor);
    engine.rootContext()->setContextProperty("library", &library);
    engine.rootContext()->setContextProperty("presets", &presets);
    QStringList warnings;
    QObject::connect(&engine, &QQmlEngine::warnings, [&](const QList<QQmlError> &errors) {
        for (const auto &error : errors)
            warnings.append(error.toString());
    });
    QQmlComponent component(&engine);
    component.setData(("import QtQuick\nimport QtQuick.Controls\nimport \"" +
                       QUrl::fromLocalFile(qmlPath).toString() +
                       "\"\nApplicationWindow { width: 1000; height: 850; visible: true; "
                       "ExportPanel { id: exportPanel; Component.onCompleted: open() } }")
                          .toUtf8(),
                      QUrl());
    wait([&] { return component.status() != QQmlComponent::Loading; });
    if (component.isError())
        throw std::runtime_error(component.errorString().toStdString());
    const std::unique_ptr<QObject> window(component.create());
    require(bool(window), "Export panel did not load.");
    QCoreApplication::processEvents();
    auto *panel = window->findChild<QObject *>("exportPanel");
    auto *format = window->findChild<QObject *>("exportFormat");
    auto *naming = window->findChild<QObject *>("exportNaming");
    auto *tokens = window->findChild<QObject *>("export.namingTokens");
    require(panel && format && naming && tokens &&
                tokens->property("text").toString().contains("{camera}") &&
                tokens->property("text").toString().contains("{time}"),
            "Export panel controls or naming-token guidance are missing.");
    require(format->setProperty("currentIndex", 5), "Cannot select Adaptive HDR in QML.");
    require(QMetaObject::invokeMethod(format, "activated", Q_ARG(int, 5)),
            "Cannot activate export format.");
    naming->setProperty("text", "QML-{camera}-{time}-{sequence}");
    require(QMetaObject::invokeMethod(naming, "editingFinished"), "Cannot edit QML naming.");
    require(exporter.configure(panel->property("form").toMap()),
            "Cannot apply actual export QML settings.");
    require(exporter.settings().value("format").toInt() == 5 &&
                exporter.settings().value("colorSpace").toInt() == 0 &&
                exporter.settings().value("naming").toString() == "QML-{camera}-{time}-{sequence}",
            "QML did not update export settings.");
    if (!warnings.isEmpty())
        throw std::runtime_error(warnings.join('\n').toStdString());
}
} // namespace
void exportQueueTests(const QString &source, const QString &root, const QString &qmlPath) {
    settingsTests(root);
    const auto directory = std::filesystem::path(root.toStdString()) / "export-queue";
    std::filesystem::create_directories(directory / "output");
    const auto catalog = directory / "catalog.sqlite";
    std::vector<std::string> ids;
    {
        luma::Catalog db(catalog);
        const auto checksum = luma::macos::sha256File(source.toStdString());
        auto metadata = luma::RawDecoder().inspect(source.toStdString());
        metadata.camera = "Nikon/Z";
        metadata.lens = "50mm";
        metadata.iso = 400;
        metadata.captureTimestamp = 0;
        for (int i = 0; i < 3; ++i) {
            const auto path = directory / ("photo-" + std::to_string(i) + ".DNG");
            std::filesystem::copy_file(source.toStdString(), path);
            auto asset = db.addAndSelect(path, checksum, metadata);
            asset.edits.exposure = i;
            db.saveEdits(asset.id, asset.edits);
            ids.push_back(asset.id);
        }
    }
    PreviewProvider provider;
    EditorController editor(catalog, &provider);
    LibraryModel library(catalog);
    PresetController presets(QString::fromStdString((directory / "presets.json").string()), editor);
    Gate gate;
    gate.failSecond = true;
    ExportController exporter(catalog, editor, library, gate.runner());
    auto config = exporter.settings();
    config["destination"] = QString::fromStdString((directory / "output").string());
    require(exporter.configure(config), "Cannot configure export queue.");
    exporter.saveProfile("Web JPEG");
    require(exporter.profiles().size() == 1, "Export profile was not saved.");
    const auto profileID = exporter.profiles().first().toMap().value("id").toString();
    qmlTest(exporter, editor, library, presets, qmlPath);
    exporter.loadProfile(profileID);
    require(exporter.settings() == config, "Export profile did not restore every setting.");
    {
        ExportController restored(catalog, editor, library, gate.runner());
        require(restored.settings() == config && restored.profiles().size() == 1,
                "Export settings were not restored after restart.");
        auto changed = config;
        changed["quality"] = 91;
        require(restored.configure(changed), "Cannot save second export instance.");
        require(!exporter.configure(config), "Concurrent export settings overwrite was accepted.");
    }
    {
        ExportController saved(catalog, editor, library, gate.runner());
        auto metadataProfile = config;
        metadataProfile["naming"] = "{camera}_{lens}_{iso}_{time}";
        require(saved.configure(metadataProfile), "Cannot configure metadata export profile.");
        saved.saveProfile("Capture metadata");
        QString newProfileID;
        for (const auto &profile : saved.profiles()) {
            const auto entry = profile.toMap();
            if (entry.value("name").toString() == "Capture metadata")
                newProfileID = entry.value("id").toString();
        }
        require(!newProfileID.isEmpty(), "Metadata export profile was not saved.");
        ExportController reopened(catalog, editor, library, gate.runner());
        reopened.loadProfile(newProfileID);
        require(reopened.settings() == metadataProfile,
                "Metadata export profile did not survive reopening.");
        reopened.loadProfile(profileID);
        require(reopened.settings() == config, "Legacy export profile no longer loads.");
    }
    // Reload a fresh controller after the deliberate conflict; its queue settings are independent.
    Gate snapshotGate;
    snapshotGate.failSecond = true;
    ExportController queue(catalog, editor, library, snapshotGate.runner());
    require(queue.configure(config), "Cannot configure snapshot queue.");
    editor.openFile(
        QUrl::fromLocalFile(QString::fromStdString((directory / "photo-0.DNG").string())));
    wait([&] { return editor.hasPhoto() && !editor.busy(); });
    editor.setExposure(.75);
    queue.startIDs({ids[2], ids[0], ids[1], ids[0]});
    wait([&] { return snapshotGate.entered.load() == 1; });
    require(queue.total() == 3 && !editor.editingLocked() && !library.working(),
            "Planning did not deduplicate or release editor.");
    queue.pause();
    {
        luma::Catalog db(catalog);
        auto asset = db.assetByID(ids[2]);
        asset->edits.exposure = -3;
        db.saveEdits(asset->id, asset->edits);
    }
    auto later = config;
    later["quality"] = 17;
    require(queue.configure(later), "Cannot change settings while queue snapshot is running.");
    snapshotGate.permitted = 1;
    wait([&] { return queue.paused(); });
    require(queue.completed() == 1 && queue.succeeded() == 1 && snapshotGate.entered == 1,
            "Pause did not wait for completed current file.");
    queue.resume();
    wait([&] { return snapshotGate.entered == 2; });
    snapshotGate.permitted = 3;
    wait([&] { return !queue.working(); });
    require(queue.completed() == 3 && queue.succeeded() == 2 && queue.failures().size() == 1,
            "Queue did not continue after per-file failure.");
    {
        const std::lock_guard lock(snapshotGate.mutex);
        require(snapshotGate.assets.size() == 3 && snapshotGate.assets[0].edits.exposure == .75 &&
                    snapshotGate.assets[2].edits.exposure == 2 &&
                    snapshotGate.options[2].quality == 92,
                "Queued edit/options snapshot changed after planning.");
    }
    Gate cancelGate;
    ExportController cancelled(catalog, editor, library, cancelGate.runner());
    config["naming"] = "cancel-{filename}";
    require(cancelled.configure(config), "Cannot configure cancellation queue.");
    cancelled.startIDs(ids);
    wait([&] { return cancelGate.entered == 1; });
    cancelGate.permitted = 1;
    wait([&] { return cancelGate.entered == 2; });
    bool closed = false;
    QObject::connect(&cancelled, &ExportController::closeReady, [&] { closed = true; });
    require(!cancelled.requestClose(), "Close bypassed a running export.");
    wait([&] { return closed; });
    require(cancelled.succeeded() == 1 && cancelled.completed() == 1 && !cancelled.working(),
            "Close cancellation lost completed output or counted unfinished output.");
    require(std::filesystem::exists(directory / "output/cancel-photo-0.jpg") &&
                !std::filesystem::exists(directory / "output/cancel-photo-1.jpg"),
            "Cancelled queue wrote pending files.");
    Gate metadataGate;
    metadataGate.permitted = 3;
    ExportController metadataQueue(catalog, editor, library, metadataGate.runner());
    config["naming"] = "{camera}_{lens}_{iso}_{time}";
    require(metadataQueue.configure(config), "Cannot configure metadata naming queue.");
    metadataQueue.startIDs(ids);
    wait([&] { return !metadataQueue.working(); });
    require(metadataQueue.succeeded() == 3 && metadataQueue.failures().isEmpty() &&
                std::filesystem::exists(directory / "output/Nikon_Z_50mm_400_00-00-00.jpg") &&
                std::filesystem::exists(directory / "output/Nikon_Z_50mm_400_00-00-00-2.jpg") &&
                std::filesystem::exists(directory / "output/Nikon_Z_50mm_400_00-00-00-3.jpg"),
            "Metadata queue did not safely disambiguate identical filenames.");
    for (const auto conflict : {0, 2}) {
        config["conflict"] = conflict;
        require(metadataQueue.configure(config), "Cannot configure metadata conflict policy.");
        metadataQueue.startIDs(ids);
        wait([&] { return !metadataQueue.working(); });
        require(metadataQueue.succeeded() == 0 &&
                    metadataQueue.error().contains("duplicate destinations"),
                "Skip or Replace accepted colliding metadata-derived filenames.");
    }
    config["conflict"] = 1;
    Gate updateGate;
    ExportController updating(catalog, editor, library, updateGate.runner());
    config["naming"] = "update-{sequence}";
    require(updating.configure(config), "Cannot configure update queue.");
    updating.startIDs(ids);
    wait([&] { return updateGate.entered == 1; });
    bool installed = false;
    UpdateController updates([&] { return !updating.working(); }, false);
    QObject::connect(&updates, &UpdateController::preparingRestart, &updating,
                     &ExportController::cancel);
    updates.postponeInstallation([&] {
        require(!updating.working(), "Update restarted during export.");
        installed = true;
    });
    wait([&] { return installed; });
    require(updating.succeeded() == 0, "Update cancellation wrote an incomplete export.");
    // Real backend, skip/conflicts and a second start from an idle completed queue.
    ExportController native(catalog, editor, library);
    config["format"] = 1;
    config["naming"] = "native-{sequence}";
    config["resize"] = 3;
    config["resizeValue"] = 96;
    require(native.configure(config), "Cannot configure native queue.");
    native.startIDs(ids);
    wait([&] { return !native.working(); });
    require(native.succeeded() == 3 && native.failures().isEmpty(), "Native export queue failed.");
    config["conflict"] = 0;
    require(native.configure(config), "Cannot configure Skip.");
    native.startIDs(ids);
    wait([&] { return !native.working(); });
    require(native.skipped() == 3 && native.succeeded() == 0,
            "Skip conflict policy changed existing outputs.");
    config["conflict"] = 1;
    require(native.configure(config), "Cannot configure Unique.");
    native.startIDs(ids);
    wait([&] { return !native.working(); });
    require(native.succeeded() == 3 &&
                std::filesystem::exists(directory / "output/native-0001-2.png"),
            "Unique conflict policy overwrote existing output.");
    native.startIDs(ids);
    native.pause();
    wait([&] { return native.paused(); });
    native.cancel();
    require(!native.working(), "Paused queue cancellation did not complete.");
    {
        const auto fixture = directory / "capture.png";
        const auto fixtureCatalog = directory / "exif-catalog.sqlite";
        writeExportExifFixture(fixture);
        const auto imported = luma::LibraryService(fixtureCatalog).import({fixture}, {});
        require(imported.added == 1 && imported.failed == 0, "Cannot import EXIF fixture.");
        luma::Catalog db(fixtureCatalog);
        const auto importedIDs = db.libraryIDs({});
        require(importedIDs.size() == 1, "Imported EXIF fixture is missing from catalog.");
        const auto asset = db.assetByID(importedIDs.front());
        require(asset && asset->metadata.camera == "Luma Raster Test" &&
                    asset->metadata.lens == "Luma Test Lens" && asset->metadata.iso == 200 &&
                    asset->metadata.captureTimestamp == 1791968400,
                "Imported EXIF metadata or capture timezone changed.");
        PreviewProvider fixtureProvider;
        EditorController fixtureEditor(fixtureCatalog, &fixtureProvider);
        LibraryModel fixtureLibrary(fixtureCatalog);
        ExportController fixtureQueue(fixtureCatalog, fixtureEditor, fixtureLibrary);
        auto fixtureConfig = fixtureQueue.settings();
        fixtureConfig["destination"] = QString::fromStdString((directory / "output").string());
        fixtureConfig["format"] = 0;
        fixtureConfig["naming"] = "{camera}_{lens}_{iso}_{date}_{time}";
        require(fixtureQueue.configure(fixtureConfig), "Cannot configure EXIF naming queue.");
        wait([&] {
            return fixtureEditor.canPrepareBatch() && !fixtureLibrary.working() &&
                   !fixtureLibrary.selecting();
        });
        fixtureQueue.startIDs(importedIDs);
        wait([&] { return !fixtureQueue.working(); });
        if (fixtureQueue.succeeded() != 1 || !fixtureQueue.failures().isEmpty() ||
            !std::filesystem::exists(
                directory / "output/Luma Raster Test_Luma Test Lens_200_2026-10-14_09-00-00.jpg"))
            throw std::runtime_error(
                "Imported EXIF export failed: " + fixtureQueue.error().toStdString() + " " +
                fixtureQueue.failures().join("; ").toStdString());
    }
    const auto sql = [&](const char *statement) {
        sqlite3 *raw = nullptr;
        require(sqlite3_open(catalog.c_str(), &raw) == SQLITE_OK, "Cannot open autosave fixture.");
        const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
        require(sqlite3_exec(db.get(), statement, nullptr, nullptr, nullptr) == SQLITE_OK,
                "Cannot prepare autosave failure.");
    };
    sql("CREATE TRIGGER fail_export_save BEFORE UPDATE ON edit_documents BEGIN SELECT "
        "RAISE(ABORT,'injected autosave failure'); END");
    editor.setExposure(.9);
    native.startIDs(ids);
    wait([&] { return !editor.error().isEmpty(); });
    require(native.waitingForSave() && editor.saving(), "Export bypassed failed autosave.");
    const auto draft = editor.currentSettings();
    native.cancel();
    require(!native.working() && !editor.editingLocked() && editor.currentSettings() == draft,
            "Cancelling export preparation lost unsaved draft.");
    sql("DROP TRIGGER fail_export_save");
    editor.retrySave();
    wait([&] { return !editor.saving(); });
}
