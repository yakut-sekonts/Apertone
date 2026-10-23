#include "tests/ux/UxContext.h"
#include "tests/ux/UxPixels.h"
#include <QFileInfo>
#include <QSettings>
#include <QSignalSpy>

namespace ux {
namespace {
void ready(Context &c, const luma::EditDocument &expected, const QString &previous = {}) {
    const auto pixels = fixtureReference(c, expected, c.editor.sourcePath());
    c.driver->wait(
        [&] {
            auto *image = c.driver->item("develop.preview");
            return !c.editor.busy() && !c.editor.previewUrl().isEmpty() &&
                   c.editor.previewUrl() != previous && image->isVisible() &&
                   image->property("status").toInt() == 1 &&
                   image->property("source").toUrl().toString() == c.editor.previewUrl() &&
                   c.provider->requestImage({}, nullptr, {}) == pixels;
        },
        "Backend toggle produced a new Ready preview matching the independent CPU fixture");
    require(c.editor.currentSettings() == expected && c.editor.error().isEmpty(),
            "Preview backend changed the complete document or reported an error");
}
} // namespace
bool previewBackendScenario(Context &c, const QString &id) {
    if (id != "UX-P06-GPU-001")
        return false;
    auto &d = *c.driver;
    c.openFirst();
    c.saved();
    const auto original = c.editor.currentSettings();
    ready(c, original);
    c.reveal("develop.exposureSlider");
    d.drag("develop.exposureSlider", d.sliderThumb("develop.exposureSlider"), {1., .5});
    auto edited = original;
    edited.exposure = 5;
    c.saved();
    ready(c, edited);
    require(c.editor.canUndo() && !c.editor.canRedo(), "Backend fixture needs one real edit");
    const auto photo = c.editor.photoID(), source = c.editor.sourcePath();
    const auto before =
        luma::Catalog(c.catalogFile.toStdString()).assetSnapshot(photo.toStdString());
    require(before.has_value(), "Backend fixture has no saved asset");
    const auto members = luma::Catalog(c.catalogFile.toStdString()).libraryIDs({});
    const auto selected = c.library.selectionIDs();
    QSignalSpy saves(&c.editor, &EditorController::editsSaved);
    require(saves.isValid(), "Cannot observe unintended persistence during a backend change");
    for (const bool enabled : {false, true, false, true}) {
        require(c.editor.metalPreview() != enabled,
                "Fixture must exercise an actual backend toggle");
        d.click("settingsButton");
        d.wait([&] { return d.object("preferencesPanel")->property("opened").toBool(); },
               "Backend preferences opened");
        const auto previous = c.editor.previewUrl();
        d.click("settings.metalPreview");
        require(c.editor.metalPreview() == enabled &&
                    d.object("settings.metalPreview")->property("checked").toBool() == enabled,
                "Actual Metal checkbox did not acknowledge its new value");
        d.key(Qt::Key_Escape);
        d.wait([&] { return !d.object("preferencesPanel")->property("visible").toBool(); },
               "Backend preferences closed");
        ready(c, edited, previous);
        const auto after =
            luma::Catalog(c.catalogFile.toStdString()).assetSnapshot(photo.toStdString());
        require(after && after->revision == before->revision &&
                    after->asset.edits == before->asset.edits &&
                    after->asset.id == before->asset.id &&
                    after->asset.path == before->asset.path &&
                    after->asset.checksum == before->asset.checksum &&
                    luma::Catalog(c.catalogFile.toStdString()).libraryIDs({}) == members &&
                    c.library.selectionIDs() == selected && c.editor.photoID() == photo &&
                    c.editor.sourcePath() == source && !c.editor.saving() && saves.isEmpty() &&
                    c.editor.canUndo() && !c.editor.canRedo(),
                "Backend preference changed catalog identity, saved edits, selection or history");
        QSettings preferences(QFileInfo(c.catalogFile).absolutePath() + "/preview.ini",
                              QSettings::IniFormat);
        require(preferences.value("metal").toBool() == enabled,
                "Metal preference was not persisted independently of catalog edits");
        d.log({{"action", "preview_backend_toggle"},
               {"metal_preference", enabled},
               {"catalog_revision", static_cast<qint64>(after->revision)},
               {"preview_url", c.editor.previewUrl()},
               {"scope", "UI preference and state preservation; not proof of GPU execution"}});
    }
    d.click("main.undoButton");
    ready(c, original);
    require(!c.editor.canUndo() && c.editor.canRedo(), "Backend toggle polluted Undo history");
    d.click("main.redoButton");
    ready(c, edited);
    require(c.editor.canUndo() && !c.editor.canRedo(), "Backend toggle polluted Redo history");
    require(c.warnings.isEmpty(), "Backend toggle produced QML warnings: " + c.warnings.join('\n'));
    return true;
}
} // namespace ux
