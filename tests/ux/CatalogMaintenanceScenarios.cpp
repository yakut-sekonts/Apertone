#include "platform/macos/FileHasher.h"
#include "tests/ux/CrashScenarios.h"
#include "tests/ux/PersistenceProbe.h"
#include "tests/ux/UxContext.h"
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <map>

namespace ux {
namespace {
using Backups = std::map<std::filesystem::path, std::string>;
Backups savedBackups(const QUrl &folder) {
    Backups result;
    const QDir directory(folder.toLocalFile());
    for (const auto &name : directory.entryList({"auto-*.sqlite3"}, QDir::Files, QDir::Name)) {
        const auto path = std::filesystem::path(directory.filePath(name).toStdString());
        result.emplace(path, luma::macos::sha256File(path));
    }
    return result;
}
void openMaintenance(Context &c) {
    auto &d = *c.driver;
    d.click("settingsButton");
    d.wait([&] { return d.object("preferencesPanel")->property("opened").toBool(); },
           "Backup preferences opened");
    d.click("settings.catalogMaintenance");
    d.wait([&] { return d.object("catalogMaintenance")->property("opened").toBool(); },
           "Catalog maintenance opened through the actual settings button");
}
void closeMaintenance(Context &c) {
    auto &d = *c.driver;
    for (const auto *name : {"catalogMaintenance", "preferencesPanel"}) {
        d.key(Qt::Key_Escape);
        d.wait([&] { return !d.object(name)->property("visible").toBool(); },
               "Catalog preferences dismissed");
    }
}
} // namespace
bool catalogMaintenanceScenario(Context &c, const QString &id) {
    const bool restore = id == "UX-P09-BKP-002";
    const bool corrupt = id == "UX-P09-BKP-006";
    const bool active = id == "UX-P09-BKP-007";
    if (id != "UX-P09-BKP-001" && !restore && !corrupt && !active)
        return false;
    auto &d = *c.driver;
    c.import(1);
    d.doubleClick(c.card(0));
    c.loaded();
    c.reveal("develop.exposureSlider");
    d.drag("develop.exposureSlider", d.sliderThumb("develop.exposureSlider"), {1., .5});
    c.saved();
    require(c.editor.currentSettings().exposure == 5 && c.editor.canUndo() && !c.editor.canRedo(),
            "Backup fixture needs a saved real UI edit and one Undo entry");
    const auto photo = c.editor.photoID().toStdString();
    luma::Catalog catalog(c.catalogFile.toStdString());
    auto expected = catalog.assetSnapshot(photo);
    require(expected.has_value(), "Backup fixture has no saved asset");
    const auto originalHash = luma::macos::sha256File(expected->asset.path);
    const auto members = catalog.libraryIDs({});
    const auto selected = c.library.selectionIDs();
    QSignalSpy saves(&c.editor, &EditorController::editsSaved);
    require(saves.isValid(), "Cannot observe unexpected edits during manual backup");

    openMaintenance(c);
    std::filesystem::path backup;
    for (int repetition = 0; repetition < 2; ++repetition) {
        const auto before = savedBackups(c.library.backupFolder());
        require(!before.empty(), "Startup must already have a good backup to preserve");
        d.click("catalog.backupNow");
        d.wait([&] { return !c.library.working() && !c.library.loading(); },
               "Manual backup worker completed");
        require(c.library.error().isEmpty(), c.library.error());
        auto added = savedBackups(c.library.backupFolder());
        for (const auto &[path, hash] : before) {
            require(added.contains(path) && added.at(path) == hash,
                    "Manual backup removed or changed a previous good snapshot");
            added.erase(path);
        }
        require(added.size() == 1, "Back up now did not create exactly one new snapshot");
        const auto &path = added.begin()->first;
        backup = path;
        luma::Catalog::verifyBackup(path);
        luma::Catalog snapshot(path);
        const auto copied = snapshot.assetSnapshot(photo);
        require(copied && copied->asset.id == expected->asset.id &&
                    copied->asset.path == expected->asset.path &&
                    copied->asset.checksum == expected->asset.checksum &&
                    copied->asset.edits == expected->asset.edits &&
                    copied->revision == expected->revision && snapshot.libraryIDs({}) == members &&
                    snapshot.activeAsset() && snapshot.activeAsset()->id == photo,
                "Readable backup lost the current photo identity, saved edits or selection");
        require(c.library.status().contains(QString::fromStdString(path.string())) &&
                    d.object("catalog.backupNow")->property("enabled").toBool(),
                "Completed backup did not show its destination or re-enable its control");
        d.log({{"action", "manual_catalog_backup"},
               {"repetition", repetition + 1},
               {"snapshot", QString::fromStdString(path.string())},
               {"revision", static_cast<qint64>(copied->revision)},
               {"previous_snapshots_preserved", static_cast<int>(before.size())}});
    }
    if (restore || corrupt || active) {
        const auto backedUp = expected;
        // Different current and backed-up edits prove restore reads the selected
        // snapshot rather than silently copying the live catalog.
        closeMaintenance(c);
        c.reveal("develop.exposureSlider");
        d.drag("develop.exposureSlider", d.sliderThumb("develop.exposureSlider"), {.6, .5});
        c.saved();
        expected = catalog.assetSnapshot(photo);
        require(expected && expected->asset.edits != backedUp->asset.edits &&
                    expected->revision > backedUp->revision,
                "Restore control needs distinct live and snapshot edits");
        saves.clear();
        openMaintenance(c);
        const auto snapshots = savedBackups(c.library.backupFolder());
        auto source = backup;
        if (corrupt) {
            source = (c.root + "/corrupt-backup.sqlite3").toStdString();
            QFile damaged(QString::fromStdString(source.string()));
            const QByteArray bytes("This is not a SQLite database\n");
            require(damaged.open(QIODevice::WriteOnly | QIODevice::NewOnly) &&
                        damaged.write(bytes) == bytes.size(),
                    "Cannot create owned corrupt-backup fixture");
            damaged.close();
        }
        const auto sourceHash = luma::macos::sha256File(source);
        const auto destination = active ? c.catalogFile : c.root + "/restored-catalog.sqlite3";
        auto *dialog = d.object("catalog.restoreDestination");
        require(!dialog->property("visible").toBool() &&
                    dialog->property("title").toString() == "Restore to a new catalog file" &&
                    d.object("catalogMaintenance")
                        ->setProperty("restoreSource", QUrl::fromLocalFile(QString::fromStdString(
                                                           source.string()))) &&
                    dialog->setProperty("selectedFile", QUrl::fromLocalFile(destination)) &&
                    dialog->property("selectedFile").toUrl().toLocalFile() == destination,
                "Restore chooser fixture rejected its owned source/destination");
        d.log({{"action", "file_dialog_selection_fixture_boundary"},
               {"id", "catalog.restoreDestination"},
               {"path", destination},
               {"native_selection_exercised", false}});
        require(QMetaObject::invokeMethod(dialog, "accepted", Qt::DirectConnection),
                "Restore FileDialog has no production accepted handler");
        d.wait([&] { return !c.library.working() && !c.library.loading(); },
               "Restore worker completed");
        if (restore) {
            require(
                c.library.error().isEmpty() && c.library.status().contains(destination) &&
                    d.object("catalog.status")->property("text").toString().contains(destination),
                "Restore did not report its successful new destination");
            luma::Catalog::verifyBackup(destination.toStdString());
            luma::Catalog restored(destination.toStdString());
            const auto copied = restored.assetSnapshot(photo);
            require(
                copied && copied->asset.id == backedUp->asset.id &&
                    copied->asset.path == backedUp->asset.path &&
                    copied->asset.checksum == backedUp->asset.checksum &&
                    copied->asset.edits == backedUp->asset.edits &&
                    copied->revision == backedUp->revision && restored.libraryIDs({}) == members &&
                    restored.activeAsset() && restored.activeAsset()->id == photo,
                "Restored catalog did not reproduce the selected snapshot's edits and identity");
        } else {
            require(!c.library.error().isEmpty() &&
                        d.object("catalog.error")->property("visible").toBool() &&
                        d.object("catalog.error")->property("text").toString() ==
                            c.library.error() &&
                        !c.library.status().contains("Catalog restored to a new file"),
                    "Rejected restore did not show an error or falsely claimed success");
            if (active)
                require(c.library.error() == "Backup destination already exists.",
                        "Active catalog overwrite was not explicitly refused");
            else
                require(!QFile::exists(destination), "Corrupt backup published a destination");
        }
        require(luma::macos::sha256File(source) == sourceHash &&
                    savedBackups(c.library.backupFolder()) == snapshots &&
                    QDir(c.root).entryList({".luma-backup-*"}, QDir::Files | QDir::Hidden).empty(),
                "Restore changed its source/backups or left temporary files");
    }
    closeMaintenance(c);
    const auto after = catalog.assetSnapshot(photo);
    require(after && after->asset.edits == expected->asset.edits &&
                after->revision == expected->revision && catalog.libraryIDs({}) == members &&
                c.library.selectionIDs() == selected && c.editor.photoID().toStdString() == photo &&
                c.editor.currentSettings() == expected->asset.edits && saves.isEmpty() &&
                c.editor.canUndo() && !c.editor.canRedo() &&
                luma::macos::sha256File(expected->asset.path) == originalHash,
            "Manual backup changed originals, saved/live edits, selection or history");
    require(c.warnings.isEmpty(), "Manual backup produced QML warnings: " + c.warnings.join('\n'));
    return true;
}
bool backupCrashScenario(Context &c, const QString &id, const QString &stage) {
    if (id != "UX-P09-FLT-003")
        return false;
    auto &d = *c.driver;
    const auto expectedPath = (c.stateRoot + "/expected-backup.sqlite").toStdString();
    luma::Catalog catalog(c.catalogFile.toStdString());
    if (stage == "prepare") {
        c.import(1);
        d.doubleClick(c.card(0));
        c.loaded();
        c.saved();
        c.reveal("develop.exposureSlider");
        d.drag("develop.exposureSlider", d.sliderThumb("develop.exposureSlider"), {.6, .5});
        c.saved();
        catalog.backupTo(expectedPath);
        const auto before = savedBackups(c.library.backupFolder());
        require(!before.empty(), "Backup crash requires a prior valid snapshot");
        QJsonObject hashes;
        for (const auto &[path, hash] : before) {
            luma::Catalog::verifyBackup(path);
            hashes[QString::fromStdString(path.string())] = QString::fromStdString(hash);
        }
        openMaintenance(c);
        PersistenceGate gate(c.catalogFile.toStdString(), {"backup.after-step"});
        d.click("catalog.backupNow");
        d.wait([&] { return gate.snapshot().held; }, "Actual SQLite backup step reached");
        const auto observed = gate.snapshot();
        require(!observed.hookFailed && observed.events.size() == 1 && c.library.working(),
                "Backup kill did not retain the actual worker");
        const auto &event = observed.events.front();
        require(std::filesystem::is_regular_file(event.temporary) &&
                    !std::filesystem::exists(event.target) &&
                    savedBackups(c.library.backupFolder()) == before,
                "Incomplete backup was published or changed an earlier snapshot");
        writeJson(c.stateRoot + "/backup-crash-expected.json",
                  {{"prepare_pid", QCoreApplication::applicationPid()},
                   {"previous", hashes},
                   {"unpublished", QString::fromStdString(event.target.string())}});
        killCheckpoint(c, id, stage, "backup.after-step");
    }
    require(stage == "verify", "Invalid backup crash stage");
    const auto saved = crashExpectation(c, "backup-crash-expected.json");
    d.wait([&] { return !c.library.working() && !c.library.loading(); },
           "Backup recovery startup settled");
    require(!QFile::exists(saved["unpublished"].toString()),
            "Crash published an unfinished backup");
    const auto hashes = saved["previous"].toObject();
    for (auto it = hashes.begin(); it != hashes.end(); ++it) {
        const auto path = it.key().toStdString();
        require(luma::macos::sha256File(path) == it.value().toString().toStdString(),
                "Crash changed prior backup bytes");
        luma::Catalog::verifyBackup(path);
    }
    luma::Catalog expected(expectedPath);
    require(catalog.libraryIDs({}) == expected.libraryIDs({}),
            "Backup crash changed catalog membership");
    for (const auto &photo : expected.libraryIDs({})) {
        const auto before = expected.assetSnapshot(photo).value(),
                   now = catalog.assetSnapshot(photo).value();
        require(now.asset.edits == before.asset.edits && now.revision == before.revision &&
                    now.asset.path == before.asset.path &&
                    now.asset.checksum == before.asset.checksum &&
                    luma::macos::sha256File(now.asset.path) == before.asset.checksum,
                "Backup crash changed originals or committed documents");
    }
    openMaintenance(c);
    const auto before = savedBackups(c.library.backupFolder());
    d.click("catalog.backupNow");
    d.wait([&] { return !c.library.working() && !c.library.loading(); },
           "Post-crash backup completed");
    require(c.library.error().isEmpty(), c.library.error());
    auto added = savedBackups(c.library.backupFolder());
    for (const auto &[path, hash] : before) {
        require(added.contains(path) && added.at(path) == hash,
                "Backup retry changed a prior snapshot");
        added.erase(path);
    }
    require(added.size() == 1, "Backup retry did not publish exactly one snapshot");
    luma::Catalog::verifyBackup(added.begin()->first);
    luma::Catalog retried(added.begin()->first);
    require(retried.libraryIDs({}) == expected.libraryIDs({}),
            "Retry backup lost catalog membership");
    for (const auto &photo : expected.libraryIDs({})) {
        const auto beforePhoto = expected.assetSnapshot(photo).value(),
                   after = retried.assetSnapshot(photo).value();
        require(after.asset.edits == beforePhoto.asset.edits &&
                    after.revision == beforePhoto.revision,
                "Retry snapshot lost complete saved edits");
    }
    require(QDir(c.library.backupFolder().toLocalFile())
                .entryList({".luma-backup-*"}, QDir::Files | QDir::Hidden)
                .empty(),
            "Backup retry left an owned crash staging file");
    d.log({{"action", "backup_kill_recovered"},
           {"previous_snapshots", hashes.size()},
           {"new_snapshots", 1}});
    closeMaintenance(c);
    return true;
}

} // namespace ux
