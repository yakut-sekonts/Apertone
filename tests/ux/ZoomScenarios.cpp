#include "apps/shared/PhotoDecoder.h"
#include "core/render/CpuRenderer.h"
#include "platform/macos/DisplayProfile.h"
#include "tests/ux/LoadProbe.h"
#include "tests/ux/PreviewResultProbe.h"
#include "tests/ux/UxContext.h"
#include "tests/ux/UxPixels.h"
#include <QColorSpace>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QSignalSpy>
#include <algorithm>
#include <cmath>
#include <optional>

namespace ux {
namespace {
QByteArray sourceHash(const QString &path) {
    QFile source(path);
    require(source.open(QIODevice::ReadOnly), "Cannot read own zoom fixture");
    QCryptographicHash hash(QCryptographicHash::Sha256);
    require(hash.addData(&source), "Cannot hash complete zoom fixture");
    return hash.result().toHex();
}
QString chart(Context &c, bool pair) {
    const auto directory = c.root + "/zoom-originals";
    require(QDir().mkpath(directory), "Cannot create zoom fixture directory");
    QImage image(3072, 2048, QImage::Format_RGBA8888);
    require(!image.isNull(), "Cannot allocate bounded zoom chart");
    // Single-pixel detail in both axes, plus non-periodic coarse coordinates.
    // A 2560-wide preview stretched to 3072 cannot satisfy the scene oracle.
    for (int y = 0; y < image.height(); ++y) {
        auto *row = image.scanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            row[x * 4] = static_cast<uchar>(32 + (x % 2) * 144 + (y / 17) % 48);
            row[x * 4 + 1] = static_cast<uchar>(32 + (y % 2) * 144 + (x / 19) % 48);
            row[x * 4 + 2] = static_cast<uchar>(32 + (x / 11 + y / 13) % 192);
            row[x * 4 + 3] = 255;
        }
    }
    const auto path = directory + "/Zoom.png";
    require(image.save(path), "Cannot save zoom chart");
    if (pair) {
        image.invertPixels(QImage::InvertRgb);
        require(image.save(directory + "/Zoom2.png"), "Cannot save second zoom chart");
    }
    return path;
}
QImage reference(Context &c, const luma::DecodedImage &decoded, const luma::EditDocument &document,
                 int maximumDimension, bool displayProfile = true) {
    const auto profile = displayProfile && QGuiApplication::platformName() == "cocoa"
                             ? luma::macos::displayProfile(c.window->winId())
                             : std::vector<std::uint8_t>{};
    const auto reduced =
        maximumDimension == 0
            ? std::nullopt
            : std::optional(luma::standardPreview(decoded.image, maximumDimension));
    const auto &input = reduced ? *reduced : decoded.image;
    const double scale = std::min(double(input.width) / decoded.image.width,
                                  double(input.height) / decoded.image.height);
    const auto result =
        luma::renderCPU(input, document, decoded.metadata.orientation, profile, {},
                        decoded.calibration, luma::DetailDiagnostic::none, scale, decoded.metadata);
    return QImage(result.rgba.data(), result.width, result.height, result.width * 4,
                  QImage::Format_RGBA8888)
        .copy();
}
void ready(Context &c, const QImage &pixels, const QString &previous = {}) {
    try {
        c.driver->wait(
            [&] {
                const auto *image = c.driver->item("develop.preview");
                return !c.editor.busy() && c.editor.error().isEmpty() &&
                       c.editor.previewUrl() != previous && image->isVisible() &&
                       image->property("status").toInt() == 1 &&
                       image->property("source").toUrl().toString() == c.editor.previewUrl() &&
                       c.provider->requestImage({}, nullptr, {}) == pixels;
            },
            "Zoom Ready image equals independent full/reduced CPU reference");
    } catch (...) {
        recordPreviewDifference(c, pixels, "Zoom exact CPU reference");
        throw;
    }
    c.driver->frame();
}
bool agrees(const QImage &a, const QImage &b, int rgbBudget) {
    if (a.isNull() || b.isNull() || a.size() != b.size())
        return false;
    const auto first = a.convertToFormat(QImage::Format_RGBA8888);
    const auto second = b.convertToFormat(QImage::Format_RGBA8888);
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width() * 4; ++x)
            if (std::abs(int(first.constScanLine(y)[x]) - int(second.constScanLine(y)[x])) >
                (x % 4 == 3 ? 0 : rgbBudget))
                return false;
    return true;
}
void mapping(Context &c, const QImage &expected) {
    auto &d = *c.driver;
    auto *image = d.item("develop.preview"), *view = d.item("develop.viewport");
    const auto scale = c.window->devicePixelRatio();
    const auto origin = image->mapToScene({}) * scale;
    const auto center = image->mapFromItem(view, {view->width() / 2, view->height() / 2}) * scale;
    d.log({{"action", "zoom_pixel_geometry"},
           {"device_pixel_ratio", scale},
           {"image_width", image->width()},
           {"image_height", image->height()},
           {"origin_x_px", origin.x()},
           {"origin_y_px", origin.y()},
           {"source_center_x", center.x()},
           {"source_center_y", center.y()},
           {"content_x", view->property("contentX").toDouble()},
           {"content_y", view->property("contentY").toDouble()}});
    require(c.editor.oneToOne() && view->property("pixelMode").toBool() &&
                std::abs(image->width() * scale - expected.width()) < 1e-6 &&
                std::abs(image->height() * scale - expected.height()) < 1e-6 &&
                std::abs(image->property("paintedWidth").toDouble() - image->width()) < 1e-6 &&
                std::abs(image->property("paintedHeight").toDouble() - image->height()) < 1e-6,
            "100% does not map one output pixel to one scene backing pixel");
    require(std::abs(origin.x() - std::round(origin.x())) < 1e-6 &&
                std::abs(origin.y() - std::round(origin.y())) < 1e-6,
            "100% image is between backing pixels and resamples single-pixel detail");
    require(std::abs(center.x() - expected.width() / 2.) <= .5 &&
                std::abs(center.y() - expected.height() / 2.) <= .5,
            "Fit to 100% lost the image center");
    const auto actual = c.window->grabWindow().convertToFormat(QImage::Format_RGBA8888);
    require(!actual.isNull() && actual.width() == qRound(c.window->width() * scale) &&
                actual.height() == qRound(c.window->height() * scale),
            "Zoom scene readback dimensions disagree with the real window DPR");
    const auto sceneCenter = view->mapToScene({view->width() / 2, view->height() / 2}) * scale;
    const QRect area(qRound(sceneCenter.x()) - 128, qRound(sceneCenter.y()) - 64, 256, 128);
    const QRect sourceArea(area.topLeft() - origin.toPoint(), area.size());
    require(actual.rect().contains(area) && expected.rect().contains(sourceArea),
            "Zoom sample is outside the scene/source");
    const auto sample = actual.copy(area), target = expected.copy(sourceArea);
    require(sample.save(c.root + "/zoom-scene-sample.png") &&
                target.save(c.root + "/zoom-reference-sample.png"),
            "Cannot retain zoom samples");
    const int budget = QGuiApplication::platformName() == "cocoa" ? 1 : 0;
    auto changed = target.copy();
    changed.scanLine(0)[0] ^= 128;
    require(!agrees(changed, target, budget) &&
                !agrees(expected.copy(sourceArea.translated(1, 0)), target, budget),
            "Zoom scene oracle accepted changed or shifted single-pixel detail");
    require(agrees(sample, target, budget),
            "Actual 100% scene does not preserve full-resolution pixel detail");
    d.log({{"action", "zoom_scene_pixels"},
           {"sample_width", area.width()},
           {"sample_height", area.height()},
           {"rgb_budget", budget},
           {"alpha_budget", 0},
           {"scope", "actual Qt scene readback; hardware scanout not measured"}});
}
struct ChartSource {
    QString path;
    QByteArray hash, secondHash;
};
ChartSource openChart(Context &c, bool pair = false) {
    auto &d = *c.driver;
    // This contract isolates scaling and full-resolution decode from GPU arithmetic.
    d.click("settingsButton");
    d.wait([&] { return d.object("preferencesPanel")->property("opened").toBool(); },
           "Zoom CPU preference opened");
    require(c.editor.metalPreview(), "Fresh zoom fixture must use the default Metal preference");
    d.click("settings.metalPreview");
    d.key(Qt::Key_Escape);
    d.wait([&] { return !d.object("preferencesPanel")->property("visible").toBool(); },
           "Zoom preferences closed");
    require(!c.editor.metalPreview(), "Zoom CPU preference was not applied");
    const auto path = chart(c, pair);
    const auto hash = sourceHash(path);
    const auto secondHash =
        pair ? sourceHash(QFileInfo(path).absolutePath() + "/Zoom2.png") : QByteArray{};
    d.log({{"action", "zoom_own_original_created"},
           {"source", path},
           {"sha256", QString::fromLatin1(hash)},
           {"second_sha256", QString::fromLatin1(secondHash)}});
    const auto originalFixture = c.fixture;
    c.fixture = QFileInfo(path).absolutePath();
    c.import(pair ? 2 : 1);
    c.fixture = originalFixture;
    d.doubleClick(c.card(0));
    c.loaded();
    require(c.editor.sourcePath() == path, "Zoom import did not open the first owned chart");
    return {path, hash, secondHash};
}
void fitGeometry(Context &c) {
    const auto *view = c.driver->item("develop.viewport");
    require(!c.editor.oneToOne() && !view->property("pixelMode").toBool() &&
                view->property("contentX").toDouble() == 0 &&
                view->property("contentY").toDouble() == 0,
            "Photo switch did not reset zoom and both pan offsets to Fit");
}
luma::AssetSnapshot savedSnapshot(Context &c) {
    const auto result =
        luma::Catalog(c.catalogFile.toStdString()).assetSnapshot(c.editor.photoID().toStdString());
    require(result.has_value(), "Current chart has no catalog snapshot");
    return *result;
}
void sameSaved(Context &c, const luma::AssetSnapshot &before) {
    const auto after = luma::Catalog(c.catalogFile.toStdString()).assetSnapshot(before.asset.id);
    require(after && after->revision == before.revision &&
                after->asset.edits == before.asset.edits &&
                after->asset.path == before.asset.path &&
                after->asset.checksum == before.asset.checksum,
            "Preview delivery/zoom changed saved document, revision or original identity");
}
void filmstrip(Context &c, int row, const QString &path, const luma::EditDocument &document,
               const QImage &expected) {
    QSignalSpy opens(&c.library, &LibraryModel::openRequested);
    QSignalSpy loads(&c.editor, &EditorController::photoLoaded);
    require(opens.isValid() && loads.isValid(), "Cannot observe photo switch");
    c.driver->click(c.card(row, true));
    c.driver->wait([&] { return loads.count() == 1 && !c.editor.busy(); },
                   "Actual filmstrip photo switch");
    ready(c, expected);
    require(opens.count() == 1 && opens.at(0).at(0).toUrl().toLocalFile() == path &&
                loads.at(0).at(0).toString() == c.editor.photoID() &&
                c.editor.sourcePath() == path && c.editor.currentSettings() == document &&
                !c.editor.canUndo() && !c.editor.canRedo(),
            "Filmstrip switch lost asset identity/document or reset-history policy");
    fitGeometry(c);
}

PreviewResultSnapshot heldFinal(Context &c, PreviewResultGate &gate, const QImage &expected,
                                const QString &phase) {
    c.driver->wait(
        [&] {
            const auto state = gate.snapshot();
            return state.held && state.held->job->state.load() == luma::JobState::completed;
        },
        "Actual transient final result held: " + phase);
    const auto state = gate.snapshot();
    require(state.held && state.held->controller == &c.editor && state.held->resolution == 2560 &&
                state.held->assetID == c.editor.photoID().toStdString() && !state.hookFailed &&
                !state.released && state.deliveries == 0 && expected.width() == 2560 &&
                state.pixels == expected,
            "Held transient result differs from the exact independent CPU reference");
    c.driver->log({{"action", "progressive_transient_final_held"},
                   {"phase", phase},
                   {"exact_cpu_pixels", state.pixels == expected},
                   {"original", c.editor.original()},
                   {"preset_preview", c.editor.presetPreview()}});
    return state;
}
void rejectTransient(Context &c, PreviewResultGate &gate, const QImage &expected,
                     const QString &phase) {
    const auto url = c.editor.previewUrl();
    const auto histogram = c.editor.histogram();
    const auto document = c.editor.currentSettings();
    const auto photo = c.editor.photoID();
    const auto selection = c.library.selectionIDs();
    const bool original = c.editor.original(), hover = c.editor.presetPreview();
    const bool undo = c.editor.canUndo(), redo = c.editor.canRedo();
    require(!c.editor.histogramPending() && !c.editor.busy() && !c.editor.saving(),
            "Replacement preview is not settled before delayed transient delivery");
    QSignalSpy changes(&c.editor, &EditorController::stateChanged);
    QSignalSpy histograms(&c.editor, &EditorController::histogramChanged);
    require(changes.isValid() && histograms.isValid(), "Cannot observe transient side effects");
    gate.release();
    c.driver->wait([&] { return gate.snapshot().deliveries == 1; },
                   "Actual transient success callback delivered: " + phase);
    c.driver->frame();
    const auto result = gate.snapshot();
    require(result.held && result.delivered && result.held->sameAs(*result.delivered) &&
                result.released && !result.hookFailed && !result.deliveryBeforeRelease &&
                result.deliveries == 1 && result.rejections == 1 &&
                result.rejectionRevision > result.held->revision &&
                result.held->job->cancellation.token().isCancelled(),
            "Existing revision guard failed to reject the exact obsolete transient result");
    require(c.editor.previewUrl() == url && c.provider->requestImage({}, nullptr, {}) == expected &&
                c.editor.histogram() == histogram && !c.editor.histogramPending() &&
                c.editor.currentSettings() == document && c.editor.photoID() == photo &&
                c.library.selectionIDs() == selection && c.editor.original() == original &&
                c.editor.presetPreview() == hover && c.editor.canUndo() == undo &&
                c.editor.canRedo() == redo && changes.isEmpty() && histograms.isEmpty() &&
                !c.editor.busy() && !c.editor.saving() && c.editor.error().isEmpty(),
            "Delayed transient result changed the replacement image or durable/UI state");
    c.driver->log(
        {{"action", "progressive_transient_rejected"},
         {"phase", phase},
         {"exact_job_identity",
          result.held && result.delivered && result.held->sameAs(*result.delivered)},
         {"deliveries", int(result.deliveries)},
         {"guard_rejections", int(result.rejections)},
         {"held_revision", QString::number(result.held.value().revision)},
         {"current_revision", QString::number(result.rejectionRevision)},
         {"current_pixels_exact", c.provider->requestImage({}, nullptr, {}) == expected}});
}
void transientHistory(Context &c, const luma::EditDocument &document, const QImage &neutral,
                      const QImage &edited) {
    c.driver->click("main.undoButton");
    ready(c, neutral);
    require(c.editor.currentSettings() == luma::EditDocument{} && !c.editor.canUndo() &&
                c.editor.canRedo(),
            "Transient preview polluted the single-gesture Undo history");
    c.driver->click("main.redoButton");
    ready(c, edited);
    c.saved();
    require(c.editor.currentSettings() == document && c.editor.canUndo() && !c.editor.canRedo(),
            "Transient preview polluted Redo");
}

} // namespace
bool zoomScenario(Context &c, const QString &id) {
    if (id != "UX-P06-ZOM-001" && id != "UX-P06-ZOM-002")
        return false;
    auto &d = *c.driver;
    const auto source = openChart(c);
    const auto &path = source.path;
    const auto hash = QString::fromLatin1(source.hash);
    const auto decoded = luma::PhotoDecoder().decode(path.toStdString());
    require(decoded.image.width == 3072 && decoded.image.height == 2048,
            "Zoom fixture was not decoded at full resolution");
    c.reveal("develop.exposureSlider");
    d.drag("develop.exposureSlider", d.sliderThumb("develop.exposureSlider"), {.6, .5});
    c.saved();
    const auto document = c.editor.currentSettings();
    require(c.editor.canUndo() && !c.editor.canRedo() && document.exposure != 0,
            "Zoom fixture needs one committed UI edit");
    const auto fit = reference(c, decoded, document, 2560);
    const auto full = reference(c, decoded, document, 0);
    require(fit.width() == 2560 && full.size() == QSize(3072, 2048),
            "Zoom fixture must distinguish reduced and full-resolution rendering");
    ready(c, fit);
    const auto photo = c.editor.photoID();
    const auto before =
        luma::Catalog(c.catalogFile.toStdString()).assetSnapshot(photo.toStdString());
    require(before.has_value(), "Zoom fixture has no saved document");
    const auto selected = c.library.selectionIDs();
    QSignalSpy saves(&c.editor, &EditorController::editsSaved);
    require(saves.isValid(), "Cannot observe unintended zoom persistence");
    for (int cycle = 0; cycle < 2; ++cycle) {
        if (cycle == 1)
            d.resize({c.window->width() + 1, c.window->height() + 1});
        auto previous = c.editor.previewUrl();
        d.click("develop.actualPixels");
        ready(c, full, previous);
        mapping(c, full);
        previous = c.editor.previewUrl();
        d.click("develop.fit");
        ready(c, fit, previous);
        const auto *view = d.item("develop.viewport");
        const auto *image = d.item("develop.preview");
        const auto center = image->mapToItem(view, {image->width() / 2, image->height() / 2});
        require(!c.editor.oneToOne() && !view->property("pixelMode").toBool() &&
                    view->property("contentX").toDouble() == 0 &&
                    view->property("contentY").toDouble() == 0 &&
                    std::abs(center.x() - view->width() / 2) < 1e-6 &&
                    std::abs(center.y() - view->height() / 2) < 1e-6,
                "Returning to Fit lost the center or retained a pan offset");
    }
    const auto after =
        luma::Catalog(c.catalogFile.toStdString()).assetSnapshot(photo.toStdString());
    require(after && after->revision == before.value().revision && after->asset.edits == document &&
                after->asset.checksum == before.value().asset.checksum &&
                after->asset.path == before.value().asset.path &&
                c.editor.currentSettings() == document && c.editor.photoID() == photo &&
                c.library.count() == 1 && c.library.selectionIDs() == selected && saves.isEmpty() &&
                !c.editor.saving() && c.editor.canUndo() && !c.editor.canRedo(),
            "Zoom modified document, saved revision, identity, selection or history");
    require(QString::fromLatin1(sourceHash(path)) == hash, "Zoom changed original bytes");
    d.log({{"action", "zoom_own_original_preserved"},
           {"source", path},
           {"sha256", hash},
           {"width", 3072},
           {"height", 2048}});
    require(c.warnings.isEmpty(), "Zoom produced QML warnings: " + c.warnings.join('\n'));
    return true;
}
bool progressiveExportScenario(Context &c, const QString &id) {
    if (id != "UX-P06-PRG-002")
        return false;
    auto &d = *c.driver;
    const auto source = openChart(c);
    const auto decoded = luma::PhotoDecoder().decode(source.path.toStdString());
    const auto original = c.editor.currentSettings();
    ready(c, reference(c, decoded, original, 2560));
    c.reveal("develop.exposureSlider");
    const auto oldPreview = c.editor.previewUrl();
    d.press("develop.exposureSlider", d.sliderThumb("develop.exposureSlider"));
    luma::EditDocument document;
    try {
        d.move("develop.exposureSlider", {.6, .5});
        document = c.editor.currentSettings();
        require(document.exposure != original.exposure &&
                    d.object("develop.exposureSlider")->property("pressed").toBool(),
                "Progressive fixture did not hold an actual changed slider gesture");
        const auto interactive = reference(c, decoded, document, 1024);
        require(interactive.width() == 1024, "Expected a distinct 1024 interaction preview");
        ready(c, interactive, oldPreview);
        d.log({{"action", "progressive_interaction_observed"},
               {"width", interactive.width()},
               {"height", interactive.height()},
               {"preview_url", c.editor.previewUrl()},
               {"pressed", d.object("develop.exposureSlider")->property("pressed").toBool()},
               {"latency_qualification", false}});
    } catch (...) {
        d.release("develop.exposureSlider", {.6, .5});
        throw;
    }
    const auto interactionUrl = c.editor.previewUrl();
    d.release("develop.exposureSlider", {.6, .5});
    c.saved();
    ready(c, reference(c, decoded, document, 2560), interactionUrl);
    const auto photo = c.editor.photoID();
    const auto before =
        luma::Catalog(c.catalogFile.toStdString()).assetSnapshot(photo.toStdString());
    require(before && before.value().asset.edits == document && c.editor.canUndo() &&
                !c.editor.canRedo(),
            "Progressive final stage did not save one coherent gesture");
    const auto selected = c.library.selectionIDs();
    const auto settings = c.exporter.settings();
    for (const auto *key : {"resize", "colorSpace", "outputMedium", "watermarkKind"})
        require(settings.contains(key) && settings.value(key).toInt() == 0,
                "Progressive export requires Original/sRGB/unsharpened/unwatermarked defaults");
    const auto expected = reference(c, decoded, document, 0, false);
    const auto output = c.exportPNG("Progressive-full-resolution");
    const QImage exported(output);
    require(expected.size() == QSize(3072, 2048) && exported.size() == expected.size() &&
                exported.colorSpace() == QColorSpace(QColorSpace::SRgb),
            "Progressive export lost full resolution or its sRGB declaration");
    require(agrees(exported, expected, 0),
            "Exported PNG differs from the independent full-source CPU document");
    const auto after =
        luma::Catalog(c.catalogFile.toStdString()).assetSnapshot(photo.toStdString());
    require(after && after->revision == before.value().revision && after->asset.edits == document &&
                after->asset.path == before.value().asset.path &&
                after->asset.checksum == before.value().asset.checksum &&
                c.editor.currentSettings() == document && c.editor.photoID() == photo &&
                c.library.count() == 1 && c.library.selectionIDs() == selected &&
                sourceHash(source.path) == source.hash && !c.editor.saving(),
            "Progressive/export changed the saved document, source, identity or selection");
    d.click("main.undoButton");
    ready(c, reference(c, decoded, original, 2560));
    require(c.editor.currentSettings() == original && !c.editor.canUndo() && c.editor.canRedo(),
            "Progressive stages or export polluted single-gesture Undo");
    d.click("main.redoButton");
    ready(c, reference(c, decoded, document, 2560));
    c.saved();
    require(c.editor.currentSettings() == document && c.editor.canUndo() && !c.editor.canRedo(),
            "Progressive stages or export polluted Redo");
    d.log({{"action", "progressive_full_export"},
           {"width", exported.width()},
           {"height", exported.height()},
           {"output_sha256", QString::fromLatin1(sourceHash(output))},
           {"original_sha256", QString::fromLatin1(sourceHash(source.path))},
           {"pixel_budget", 0},
           {"display_profile_used_for_export_reference", false}});
    require(c.warnings.isEmpty(),
            "Progressive export produced QML warnings: " + c.warnings.join('\n'));
    return true;
}
bool zoomSwitchScenario(Context &c, const QString &id) {
    if (id != "UX-P06-ZOM-005")
        return false;
    auto &d = *c.driver;
    const auto source = openChart(c, true);
    const auto second = QFileInfo(source.path).absolutePath() + "/Zoom2.png";
    const auto secondHash = source.secondHash;
    const auto decoded = luma::PhotoDecoder().decode(source.path.toStdString());
    const auto nextDecoded = luma::PhotoDecoder().decode(second.toStdString());
    c.reveal("develop.exposureSlider");
    d.drag("develop.exposureSlider", d.sliderThumb("develop.exposureSlider"), {.6, .5});
    c.saved();
    const auto document = c.editor.currentSettings();
    const auto before = savedSnapshot(c);
    const auto fit = reference(c, decoded, document, 2560);
    ready(c, fit);
    d.click("develop.actualPixels");
    ready(c, reference(c, decoded, document, 0));
    mapping(c, reference(c, decoded, document, 0));
    const auto *view = d.item("develop.viewport");
    const auto x = view->property("contentX").toDouble();
    const auto y = view->property("contentY").toDouble();
    d.drag("develop.viewport", {.5, .5}, {.25, .25});
    require(view->property("contentX").toDouble() != x &&
                view->property("contentY").toDouble() != y,
            "Zoom photo switch needs actual non-central pan input");
    const luma::EditDocument neutral;
    filmstrip(c, 1, second, neutral, reference(c, nextDecoded, neutral, 2560));
    const auto nextSaved = savedSnapshot(c);
    filmstrip(c, 0, source.path, document, fit);
    sameSaved(c, before);
    sameSaved(c, nextSaved);
    require(sourceHash(source.path) == source.hash && sourceHash(second) == secondHash,
            "Photo switch changed original bytes");
    d.log({{"action", "zoom_switch_reset_fit"},
           {"first_uuid", QString::fromStdString(before.asset.id)},
           {"second_uuid", QString::fromStdString(nextSaved.asset.id)},
           {"first_sha256", QString::fromLatin1(source.hash)},
           {"second_sha256", QString::fromLatin1(secondHash)},
           {"policy", "reset zoom/pan on each photo load; preserve saved edits; reset history"}});
    require(c.warnings.isEmpty(), "Zoom switch produced QML warnings: " + c.warnings.join('\n'));
    return true;
}
bool progressiveRaceScenario(Context &c, const QString &id) {
    const bool switchAsset = id == "UX-P06-PRG-009";
    if (!switchAsset && id != "UX-P06-PRG-004")
        return false;
    auto &d = *c.driver;
    const auto source = openChart(c, switchAsset);
    const auto second = QFileInfo(source.path).absolutePath() + "/Zoom2.png";
    const auto secondHash = source.secondHash;
    const auto decoded = luma::PhotoDecoder().decode(source.path.toStdString());
    ready(c, reference(c, decoded, c.editor.currentSettings(), 2560));
    c.reveal("develop.exposureSlider");
    PreviewResultGate gate(c.editor, 2560);
    d.drag("develop.exposureSlider", d.sliderThumb("develop.exposureSlider"), {.6, .5});
    c.saved();
    const auto obsolete = c.editor.currentSettings();
    const auto oldExpected = reference(c, decoded, obsolete, 2560);
    d.wait(
        [&] {
            const auto state = gate.snapshot();
            return state.held && state.held->job->state.load() == luma::JobState::completed;
        },
        "Actual old final render finished while its success dispatch is retained");
    const auto held = gate.snapshot();
    require(!held.hookFailed && !held.released && held.deliveries == 0 &&
                held.pixels == oldExpected &&
                held.held->assetID == c.editor.photoID().toStdString(),
            "Race gate did not retain the real old final result");
    const auto firstSaved = savedSnapshot(c);
    QImage newExpected;
    if (switchAsset) {
        const auto nextDecoded = luma::PhotoDecoder().decode(second.toStdString());
        const luma::EditDocument neutral;
        newExpected = reference(c, nextDecoded, neutral, 2560);
        LoadResultGate loadGate(c.editor);
        QSignalSpy opens(&c.library, &LibraryModel::openRequested);
        QSignalSpy loads(&c.editor, &EditorController::photoLoaded);
        require(opens.isValid() && loads.isValid(), "Cannot observe pending photo switch");
        d.click(c.card(1, true));
        d.wait([&] { return loadGate.snapshot().held.has_value(); },
               "Next real session decoded, with load success still pending");
        const auto pendingLoad = loadGate.snapshot();
        require(
            c.editor.busy() && c.editor.photoID().toStdString() == firstSaved.asset.id &&
                pendingLoad.held->sourcePath == second.toStdString() &&
                pendingLoad.deliveries == 0 && !pendingLoad.timedOut && !pendingLoad.hookFailed &&
                opens.count() == 1 && opens.at(0).at(0).toUrl().toLocalFile() == second,
            "Old completion must arrive after actual switch began and before new load delivery");
        const auto pendingUrl = c.editor.previewUrl();
        const auto pendingPixels = c.provider->requestImage({}, nullptr, {});
        const auto pendingHistogram = c.editor.histogram();
        QSignalSpy pendingChanges(&c.editor, &EditorController::stateChanged);
        QSignalSpy pendingHistograms(&c.editor, &EditorController::histogramChanged);
        gate.release();
        d.wait([&] { return gate.snapshot().deliveries == 1; },
               "Old render success delivered while new load is held");
        d.frame();
        require(gate.snapshot().rejections == 1 && c.editor.busy() && loads.isEmpty() &&
                    c.editor.previewUrl() == pendingUrl &&
                    c.provider->requestImage({}, nullptr, {}) == pendingPixels &&
                    c.editor.histogram() == pendingHistogram &&
                    c.editor.currentSettings() == obsolete && pendingChanges.isEmpty() &&
                    pendingHistograms.isEmpty(),
                "Photo switch failed to invalidate the old stage before the new photo loaded");
        d.log({{"action", "progressive_rejected_during_photo_load"},
               {"busy", c.editor.busy()},
               {"new_load_deliveries", loads.count()},
               {"old_render_rejections", int(gate.snapshot().rejections)}});
        loadGate.release();
        d.wait([&] { return loads.count() == 1 && !c.editor.busy(); },
               "Retained new photo load completed");
        ready(c, newExpected);
        const auto loaded = loadGate.snapshot();
        require(loaded.deliveries == 1 && loaded.successfulArrivals == 1 && loaded.lastDelivered &&
                    loaded.held && loaded.held->sameAs(*loaded.lastDelivered) &&
                    !loaded.hookFailed && !loaded.timedOut && !loaded.deliveryBeforeRelease &&
                    loaded.mismatchedDeliveries == 0 && loaded.leftHold &&
                    c.editor.sourcePath() == second && c.editor.currentSettings() == neutral &&
                    c.editor.photoID() == loads.at(0).at(0).toString() && !c.editor.canUndo() &&
                    !c.editor.canRedo(),
                "New real photo load lost identity/document/history after old render rejection");
        fitGeometry(c);
    } else {
        d.drag("develop.exposureSlider", d.sliderThumb("develop.exposureSlider"), {.67, .5});
        c.saved();
        newExpected = reference(c, decoded, c.editor.currentSettings(), 2560);
        ready(c, newExpected);
    }
    require(oldExpected != newExpected && held.held->job->cancellation.token().isCancelled(),
            "New revision must differ and cancel the old render handle");
    const auto document = c.editor.currentSettings();
    const auto photo = c.editor.photoID();
    const auto url = c.editor.previewUrl();
    const auto selected = c.library.selectionIDs();
    const auto histogram = c.editor.histogram();
    const auto before = savedSnapshot(c);
    const bool undo = c.editor.canUndo(), redo = c.editor.canRedo();
    require(!c.editor.histogramPending(), "New final histogram is still pending");
    QSignalSpy changes(&c.editor, &EditorController::stateChanged);
    QSignalSpy saves(&c.editor, &EditorController::editsSaved);
    QSignalSpy histograms(&c.editor, &EditorController::histogramChanged);
    require(changes.isValid() && saves.isValid() && histograms.isValid(),
            "Cannot audit stale result delivery side effects");
    gate.release();
    d.wait([&] { return gate.snapshot().deliveries == 1; },
           "Retained real success callback delivered");
    d.frame();
    const auto result = gate.snapshot();
    require(result.held && result.delivered && result.held->sameAs(*result.delivered) &&
                result.deliveries == 1 && result.rejections == 1 && result.released &&
                result.rejectionRevision > result.held->revision && !result.deliveryBeforeRelease &&
                !result.hookFailed,
            "Original revision guard did not reject the exact delayed success once");
    require(c.editor.previewUrl() == url &&
                c.provider->requestImage({}, nullptr, {}) == newExpected &&
                c.editor.histogram() == histogram && !c.editor.histogramPending() &&
                c.editor.currentSettings() == document && c.editor.photoID() == photo &&
                c.library.selectionIDs() == selected && c.editor.canUndo() == undo &&
                c.editor.canRedo() == redo && changes.isEmpty() && saves.isEmpty() &&
                histograms.isEmpty() && !c.editor.saving() && c.editor.error().isEmpty(),
            "Late completion changed current image, histogram, document, selection or history");
    sameSaved(c, before);
    d.log({{"action", "progressive_stale_success_rejected"},
           {"held_revision", QString::number(result.held->revision)},
           {"current_revision", QString::number(result.rejectionRevision)},
           {"held_uuid", QString::fromStdString(result.held->assetID)},
           {"current_uuid", photo},
           {"held_resolution", result.held->resolution},
           {"release_order", switchAsset ? "during next load" : "after newer final preview"},
           {"deliveries", int(result.deliveries)},
           {"guard_rejections", int(result.rejections)},
           {"old_handle_cancelled", result.held->job->cancellation.token().isCancelled()},
           {"retained_result_equals_old_cpu", held.pixels == oldExpected},
           {"current_result_equals_new_cpu",
            c.provider->requestImage({}, nullptr, {}) == newExpected},
           {"dispatch_boundary",
            "actual render success after last cancellation check, before QueuedConnection"}});
    if (switchAsset) {
        sameSaved(c, firstSaved);
        filmstrip(c, 0, source.path, obsolete, oldExpected);
    } else {
        d.click("main.undoButton");
        ready(c, oldExpected);
        require(c.editor.currentSettings() == obsolete && c.editor.canUndo() && c.editor.canRedo(),
                "Late render completion corrupted the two-gesture Undo history");
        d.click("main.redoButton");
        ready(c, newExpected);
        require(c.editor.currentSettings() == document && c.editor.canUndo() && !c.editor.canRedo(),
                "Late render completion corrupted Redo history");
        c.saved();
    }
    require(sourceHash(source.path) == source.hash &&
                (!switchAsset || sourceHash(second) == secondHash),
            "Progressive race changed original bytes");
    d.log({{"action", "progressive_race_originals_preserved"},
           {"first_sha256", QString::fromLatin1(source.hash)},
           {"second_sha256", QString::fromLatin1(secondHash)}});
    require(c.warnings.isEmpty(),
            "Progressive race produced QML warnings: " + c.warnings.join('\n'));
    return true;
}

bool progressiveTransientScenario(Context &c, const QString &id) {
    const bool originalCase = id == "UX-P06-PRG-007";
    if (!originalCase && id != "UX-P06-PRG-008")
        return false;
    auto &d = *c.driver;
    const auto source = openChart(c);
    const auto decoded = luma::PhotoDecoder().decode(source.path.toStdString());
    c.reveal("develop.exposureSlider");
    d.drag("develop.exposureSlider", d.sliderThumb("develop.exposureSlider"), {.6, .5});
    c.saved();
    const auto document = c.editor.currentSettings();
    require(document.exposure != 0 && c.editor.canUndo() && !c.editor.canRedo(),
            "Transient test needs one saved, non-neutral UI gesture");
    const auto edited = reference(c, decoded, document, 2560);
    const auto neutral = reference(c, decoded, {}, 2560);
    require(edited != neutral, "Transient fixture cannot distinguish Original and Processed");
    ready(c, edited);
    const auto before = savedSnapshot(c);
    const auto selection = c.library.selectionIDs();
    QSignalSpy saves(&c.editor, &EditorController::editsSaved);
    require(saves.isValid(), "Cannot observe unintended transient saves");
    if (originalCase) {
        const auto originalInteraction = reference(c, decoded, {}, 1024);
        const auto editedInteraction = reference(c, decoded, document, 1024);
        for (const bool original : {true, false}) {
            // Exercise both obsolete Original -> Processed and the reverse.
            if (!original) {
                d.click("develop.originalButton");
                ready(c, neutral);
            }
            const auto phase = original ? QStringLiteral("Original-to-Processed")
                                        : QStringLiteral("Processed-to-Original");
            PreviewResultGate gate(c.editor, 2560);
            const auto previous = c.editor.previewUrl();
            d.click(original ? "develop.originalButton" : "develop.processedButton");
            ready(c, original ? originalInteraction : editedInteraction, previous);
            require(c.editor.original() == original && !c.editor.presetPreview(),
                    "Original/Processed input did not select the intended mode");
            heldFinal(c, gate, original ? neutral : edited, phase);
            d.log({{"action", "progressive_transient_interaction"},
                   {"phase", phase},
                   {"width", c.provider->requestImage({}, nullptr, {}).width()}});
            d.click(original ? "develop.processedButton" : "develop.originalButton");
            const auto &replacement = original ? edited : neutral;
            ready(c, replacement);
            require(c.editor.original() != original, "Replacement mode was not selected");
            rejectTransient(c, gate, replacement, phase);
            sameSaved(c, before);
        }
        d.click("develop.processedButton");
        ready(c, edited);
    } else {
        const auto preset = c.presets.snapshot("builtin:soft-portrait");
        const auto hoveredDocument =
            luma::mergeSettings(document, preset.settings, preset.groups, preset.parameters);
        const auto hoverInteraction = reference(c, decoded, hoveredDocument, 1024);
        const auto hoverFinal = reference(c, decoded, hoveredDocument, 2560);
        const auto builtins = c.presets.builtinItems();
        const auto userPresets = c.presets.items();
        require(hoveredDocument != document && hoverFinal != edited,
                "Hover fixture must distinguish the declared preset subset");
        for (const bool obsolete : {false, true}) {
            const auto phase = obsolete ? QStringLiteral("hover-leave-before-final")
                                        : QStringLiteral("hover-final-before-leave");
            PreviewResultGate gate(c.editor, 2560);
            const auto previous = c.editor.previewUrl();
            d.move("filter_soft-portrait", {.5, .5});
            ready(c, hoverInteraction, previous);
            require(c.editor.presetPreview() && !c.editor.original() &&
                        d.object("filter_soft-portrait")->property("hovered").toBool(),
                    "Real pointer hover did not activate the preset preview");
            heldFinal(c, gate, hoverFinal, phase);
            if (!obsolete) {
                gate.release();
                ready(c, hoverFinal);
                const auto result = gate.snapshot();
                require(result.held && result.delivered && result.held->sameAs(*result.delivered) &&
                            result.deliveries == 1 && result.rejections == 0 &&
                            !result.deliveryBeforeRelease && !result.hookFailed &&
                            c.editor.presetPreview() && c.editor.currentSettings() == document &&
                            !c.editor.saving() && saves.isEmpty(),
                        "A current final hover must display without persisting the preset");
                d.log({{"action", "progressive_transient_current_accepted"},
                       {"phase", phase},
                       {"width", c.provider->requestImage({}, nullptr, {}).width()},
                       {"deliveries", int(result.deliveries)},
                       {"rejections", int(result.rejections)}});
                sameSaved(c, before);
            }
            d.move("develop.preview", {.5, .5});
            ready(c, edited);
            require(!c.editor.presetPreview(), "Pointer leave retained a preset preview");
            if (obsolete)
                rejectTransient(c, gate, edited, phase);
            sameSaved(c, before);
        }
        require(c.presets.builtinItems() == builtins && c.presets.items() == userPresets,
                "Hover changed builtin/user preset data");
    }
    sameSaved(c, before);
    require(c.editor.currentSettings() == document &&
                c.editor.photoID().toStdString() == before.asset.id &&
                c.editor.sourcePath() == source.path && c.library.count() == 1 &&
                c.library.selectionIDs() == selection && saves.isEmpty() && !c.editor.saving() &&
                c.editor.canUndo() && !c.editor.canRedo() && !c.editor.original() &&
                !c.editor.presetPreview(),
            "Original/hover modified persisted edits, identity, membership, selection or history");
    transientHistory(c, document, neutral, edited);
    require(sourceHash(source.path) == source.hash, "Transient preview changed original bytes");
    d.log({{"action", "progressive_transient_preserved"},
           {"scenario", id},
           {"original_sha256", QString::fromLatin1(source.hash)},
           {"saved_revision_before", QString::number(before.revision)},
           {"view_modes_created_history_entries", false}});
    require(c.warnings.isEmpty(),
            "Transient preview produced QML warnings: " + c.warnings.join('\n'));
    return true;
}

} // namespace ux
