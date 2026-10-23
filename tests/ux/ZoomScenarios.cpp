#include "apps/shared/PhotoDecoder.h"
#include "core/render/CpuRenderer.h"
#include "platform/macos/DisplayProfile.h"
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
QString chart(Context &c) {
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
    QByteArray hash;
};
ChartSource openChart(Context &c) {
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
    const auto path = chart(c);
    const auto hash = sourceHash(path);
    d.log({{"action", "zoom_own_original_created"},
           {"source", path},
           {"sha256", QString::fromLatin1(hash)}});
    const auto originalFixture = c.fixture;
    c.fixture = QFileInfo(path).absolutePath();
    c.import(1);
    c.fixture = originalFixture;
    d.doubleClick(c.card(0));
    c.loaded();
    return {path, hash};
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
} // namespace ux
