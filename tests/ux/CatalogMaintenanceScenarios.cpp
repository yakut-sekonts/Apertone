#include "platform/macos/FileHasher.h"
#include "tests/ux/UxContext.h"
#include <QDir>
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
} // namespace
bool catalogMaintenanceScenario(Context &c, const QString &id) {
    if (id != "UX-P09-BKP-001")
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
    const auto expected = catalog.assetSnapshot(photo);
    require(expected.has_value(), "Backup fixture has no saved asset");
    const auto originalHash = luma::macos::sha256File(expected->asset.path);
    const auto members = catalog.libraryIDs({});
    const auto selected = c.library.selectionIDs();
    QSignalSpy saves(&c.editor, &EditorController::editsSaved);
    require(saves.isValid(), "Cannot observe unexpected edits during manual backup");

    d.click("settingsButton");
    d.wait([&] { return d.object("preferencesPanel")->property("opened").toBool(); },
           "Backup preferences opened");
    d.click("settings.catalogMaintenance");
    d.wait([&] { return d.object("catalogMaintenance")->property("opened").toBool(); },
           "Catalog maintenance opened through the actual settings button");
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
    d.key(Qt::Key_Escape);
    d.wait([&] { return !d.object("catalogMaintenance")->property("visible").toBool(); },
           "Catalog maintenance dismissed");
    d.key(Qt::Key_Escape);
    d.wait([&] { return !d.object("preferencesPanel")->property("visible").toBool(); },
           "Preferences dismissed");
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
} // namespace ux
