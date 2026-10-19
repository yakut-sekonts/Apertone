#include "apps/shared/PresetStore.h"
#include "tests/ux/UxContext.h"
#include "tests/ux/UxPixels.h"
#include <QColor>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMouseEvent>
#include <QPointer>
#include <QSGRendererInterface>
#include <QSignalSpy>
#include <QTest>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <vector>

// Actual scene/history regression for the approved Done-only contract.
// The historical supplemental ID retains its separate evidence domain.
// Scene readback is the composited Qt window, not physical display scanout.
namespace ux {
namespace {
constexpr auto candidateID = "CANDIDATE-CRP010-DONE-RESIDUE";
constexpr auto officialID = "UX-CRP-010";
using Document = luma::EditDocument;
QString sha(const QString &path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Cannot read CRP010 original");
    QCryptographicHash hash(QCryptographicHash::Sha256);
    require(hash.addData(&file), "Cannot hash complete CRP010 original");
    return QString::fromLatin1(hash.result().toHex());
}
QJsonObject documentJSON(const Document &value) {
    const auto encoded = QJsonDocument::fromJson(
        luma::encodePreset({"CRP010 full document", {}, value, luma::allSettingsGroups}));
    require(encoded.isObject(), "Cannot encode full CRP010 document evidence");
    auto result = encoded.object();
    result["local_masks_serialized"] =
        QString::fromStdString(luma::encodeLocalMasks(value.localMasks));
    return result;
}
QJsonObject rectJSON(const QRectF &r) {
    return {{"x", r.x()}, {"y", r.y()}, {"width", r.width()}, {"height", r.height()}};
}
QJsonObject metadataJSON(const luma::PhotoMetadata &m) {
    return {{"camera", QString::fromStdString(m.camera)},
            {"lens", QString::fromStdString(m.lens)},
            {"iso", m.iso ? QJsonValue(*m.iso) : QJsonValue(QJsonValue::Null)},
            {"shutter",
             m.shutterSeconds ? QJsonValue(*m.shutterSeconds) : QJsonValue(QJsonValue::Null)},
            {"aperture", m.aperture ? QJsonValue(*m.aperture) : QJsonValue(QJsonValue::Null)},
            {"focal", m.focalLength ? QJsonValue(*m.focalLength) : QJsonValue(QJsonValue::Null)},
            {"capture", m.captureTimestamp ? QJsonValue(QString::number(*m.captureTimestamp))
                                           : QJsonValue(QJsonValue::Null)},
            {"width", m.width},
            {"height", m.height},
            {"orientation", m.orientation}};
}
QJsonObject rowJSON(const luma::LibraryItem &r) {
    return {{"id", QString::fromStdString(r.id)},
            {"path", QString::fromStdString(r.path.string())},
            {"filename", QString::fromStdString(r.filename)},
            {"checksum", QString::fromStdString(r.checksum)},
            {"width", r.width},
            {"height", r.height},
            {"orientation", r.orientation},
            {"rating", r.rating},
            {"flag", r.flag},
            {"label", QString::fromStdString(r.label)},
            {"camera", QString::fromStdString(r.camera)}};
}
QJsonArray readRows(Context &c, const char *query) {
    sqlite3 *opened = nullptr;
    const int result =
        sqlite3_open_v2(c.catalogFile.toUtf8().constData(), &opened, SQLITE_OPEN_READONLY, nullptr);
    const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(opened, sqlite3_close);
    require(result == SQLITE_OK && db, "Cannot open readonly CRP010 journal");
    require(sqlite3_busy_timeout(db.get(), 1000) == SQLITE_OK, "Cannot bound CRP010 journal read");
    sqlite3_stmt *prepared = nullptr;
    const int preparation = sqlite3_prepare_v2(db.get(), query, -1, &prepared, nullptr);
    const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(prepared,
                                                                               sqlite3_finalize);
    require(preparation == SQLITE_OK && statement, "Cannot prepare CRP010 journal query");
    QJsonArray rows;
    int step = SQLITE_OK;
    while ((step = sqlite3_step(statement.get())) == SQLITE_ROW) {
        QJsonArray row;
        for (int column = 0; column < sqlite3_column_count(statement.get()); ++column)
            row.append(sqlite3_column_type(statement.get(), column) == SQLITE_NULL
                           ? QJsonValue(QJsonValue::Null)
                           : QJsonValue(QString::fromUtf8(reinterpret_cast<const char *>(
                                 sqlite3_column_text(statement.get(), column)))));
        rows.append(QJsonValue(row));
    }
    require(step == SQLITE_DONE, "Cannot finish CRP010 readonly journal query");
    return rows;
}
QJsonObject journal(Context &c) {
    return {
        {"imports",
         readRows(
             c, "SELECT id,mode,destination,state,owner_pid,skip_duplicates,created_at,finished_at "
                "FROM import_sessions ORDER BY id")},
        {"items",
         readRows(c, "SELECT "
                     "session_id,position,source_path,target_path,temporary_path,checksum,filesize,"
                     "state,asset_id,error FROM import_items ORDER BY session_id,position")},
        {"session", readRows(c, "SELECT slot,asset_id FROM session ORDER BY slot")}};
}
QJsonObject state(Context &c) {
    return {{"photo_id", c.editor.photoID()},
            {"source", c.editor.sourcePath()},
            {"full_document", documentJSON(c.editor.currentSettings())},
            {"crop_mode", c.editor.cropMode()},
            {"crop_canvas_ready", c.editor.cropCanvasReady()},
            {"crop_overlay_visible", c.driver->item("cropOverlay")->isVisible()},
            {"crop_drag_mode", c.driver->object("cropOverlay")->property("dragMode").toString()},
            {"crop_pressed", c.driver->object("cropDragArea")->property("pressed").toBool()},
            {"history_gesture_active", c.editor.uxHistoryGestureActive()},
            {"can_undo", c.editor.canUndo()},
            {"can_redo", c.editor.canRedo()},
            {"local_mode", c.editor.localMode()},
            {"local_sampling", c.editor.localSampling()},
            {"local_gesture", c.editor.localGestureActive()},
            {"guided_mode", c.editor.guidedMode()},
            {"targeted_tool", c.editor.targetedTool()},
            {"preset_preview", c.editor.presetPreview()},
            {"detail_diagnostic", c.editor.detailDiagnostic()},
            {"saving", c.editor.saving()},
            {"preview_url", c.editor.previewUrl()},
            {"error", c.editor.error()}};
}
struct ReleasePointer {
    Context &c;
    QPoint last{};
    bool down = false;
    ~ReleasePointer() {
        if (!down)
            return;
        try {
            auto evidence = state(c);
            evidence["acceptance_proof"] = false;
            writeJson(c.root + "/crp010-pointer-before-cleanup.json", evidence);
        } catch (...) {
            std::fputs("Cannot preserve CRP010 pre-release failure evidence\n", stderr);
        }
        try {
            QTest::mouseRelease(c.window, Qt::LeftButton, Qt::NoModifier, last);
        } catch (...) {
            std::fputs("CRP010 failure-only pointer cleanup failed\n", stderr);
        }
    }
};
class InputTrace final : public QObject {
  public:
    explicit InputTrace(Context &value) : c(value) {
        c.window->installEventFilter(this);
    }
    ~InputTrace() override {
        c.window->removeEventFilter(this);
    }
    void begin(const QString &value) {
        label = value;
        presses = releases = moves = 0;
        malformed = false;
    }
    QJsonObject json() const {
        return {{"label", label},
                {"presses", presses},
                {"held_moves", moves},
                {"releases", releases},
                {"malformed", malformed}};
    }
    void complete(bool moved) const {
        require(presses == 1 && releases == 1 && (!moved || moves > 0) && !malformed,
                "CRP010 real window input sequence was not paired");
    }

  private:
    Context &c;
    QString label;
    int presses = 0, releases = 0, moves = 0;
    bool malformed = false;
    bool eventFilter(QObject *receiver, QEvent *event) override {
        if (receiver != c.window || label.isEmpty())
            return false;
        if (event->type() == QEvent::MouseButtonPress) {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                malformed = malformed || presses != 0 || releases != 0 ||
                            !mouse->buttons().testFlag(Qt::LeftButton);
                ++presses;
            }
        } else if (event->type() == QEvent::MouseMove) {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->buttons().testFlag(Qt::LeftButton) && presses == 1 && releases == 0)
                ++moves;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                malformed = malformed || presses != 1 || releases != 0 ||
                            mouse->buttons().testFlag(Qt::LeftButton);
                ++releases;
            }
        }
        return false;
    }
};
void native(Context &c) {
    require(QGuiApplication::platformName() == "cocoa" && c.window->isActive() &&
                c.window->isExposed() && c.window->rendererInterface() &&
                c.window->rendererInterface()->graphicsApi() == QSGRendererInterface::Metal,
            "CRP010 native candidate requires the exposed active Cocoa/Metal scene");
    for (const char *name : {"QT_SCALE_FACTOR", "QT_SCREEN_SCALE_FACTORS", "QT_DEVICE_PIXEL_RATIO"})
        require(!qEnvironmentVariableIsSet(name),
                "Forced Qt scale cannot qualify native CRP010 scene mapping");
}
bool near(double a, double b) {
    return std::abs(a - b) < 1e-9;
}
QRect sourceRect(const Document &doc) {
    require(doc.crop.angle == 0 && doc.crop.quarterTurns == 0 && !doc.crop.flipHorizontal &&
                !doc.crop.flipVertical && doc.crop.aspect == 0 &&
                doc.transform == luma::TransformSettings{},
            "CRP010 independent oracle requires its upright untransformed free crop");
    const int left = static_cast<int>(std::lround(doc.crop.x * 256));
    const int top = static_cast<int>(std::lround(doc.crop.y * 192));
    const int right = static_cast<int>(std::lround((doc.crop.x + doc.crop.width) * 256));
    const int bottom = static_cast<int>(std::lround((doc.crop.y + doc.crop.height) * 192));
    require(left >= 0 && top >= 0 && right <= 256 && bottom <= 192 && right > left && bottom > top,
            "CRP010 independent integer crop escaped its source");
    return {left, top, right - left, bottom - top};
}
void ready(Context &c, const Document &expected, bool cropCanvas) {
    auto renderedDocument = expected;
    if (cropCanvas)
        renderedDocument.crop = {};
    const auto reference = fixtureReference(c, renderedDocument, c.editor.sourcePath());
    c.driver->wait(
        [&] {
            const auto *image = c.driver->item("develop.preview");
            return !c.editor.busy() && c.editor.cropMode() == cropCanvas &&
                   (!cropCanvas || c.editor.cropCanvasReady()) && image->isVisible() &&
                   image->property("status").toInt() == 1 &&
                   image->property("source").toUrl().toString() == c.editor.previewUrl() &&
                   c.provider->requestImage({}, nullptr, {}) == reference;
        },
        "CRP010 actual Ready/current source/full-document provider acknowledgement");
    require(c.editor.currentSettings() == expected && c.editor.error().isEmpty(),
            "CRP010 Ready changed the full document");
    c.driver->frame();
}
struct View {
    QRectF item, photo;
    qreal dpr = 0;
    QSize window;
};
View view(Context &c, QSize output) {
    native(c);
    auto *image = c.driver->item("develop.preview");
    require(image->width() > 20 && image->height() > 20 && output.width() > 0 &&
                output.height() > 0,
            "CRP010 has no scene image geometry");
    const auto scale = std::min(image->width() / output.width(), image->height() / output.height());
    const QSizeF painted(output.width() * scale, output.height() * scale);
    require(near(image->property("paintedWidth").toDouble(), painted.width()) &&
                near(image->property("paintedHeight").toDouble(), painted.height()),
            "Actual photo fit does not match independent output aspect/size");
    return {image->mapRectToScene(image->boundingRect()),
            {image->mapToScene({(image->width() - painted.width()) / 2,
                                (image->height() - painted.height()) / 2}),
             painted},
            c.window->effectiveDevicePixelRatio(),
            c.window->size()};
}
bool sameView(const View &a, const View &b) {
    return a.item == b.item && a.photo == b.photo && a.window == b.window && near(a.dpr, b.dpr);
}
QImage grab(Context &c, const View &before, QSize output) {
    const QPointer<QQuickItem> image(c.driver->item("develop.preview"));
    const auto url = image->property("source").toUrl();
    auto frame = c.window->grabWindow().convertToFormat(QImage::Format_RGBA8888);
    require(image && !frame.isNull() && sameView(before, view(c, output)) &&
                std::isfinite(before.dpr) && before.dpr > 0 &&
                frame.size() == before.window * before.dpr &&
                image->property("status").toInt() == 1 &&
                image->property("source").toUrl() == url && url.toString() == c.editor.previewUrl(),
            "CRP010 composited scene/readback geometry changed or has invalid native pixel size");
    return frame;
}
int distance(const QColor &a, const QColor &b) {
    return std::max({std::abs(a.red() - b.red()), std::abs(a.green() - b.green()),
                     std::abs(a.blue() - b.blue()), std::abs(a.alpha() - b.alpha())});
}
// Literal fixture oracle, independent of PhotoDecoder/renderCPU/provider output.
int stripe(double sourceX) {
    return (static_cast<int>(std::floor(sourceX)) / 64) % 2 == 0 ? 255 : 0;
}
QJsonObject census(Context &c, const Document &document, const QString &tag) {
    ready(c, document, false);
    require(!c.driver->item("cropOverlay")->isVisible(),
            "Done has not hidden the actual Crop overlay");
    const auto crop = sourceRect(document);
    const auto geometry = view(c, crop.size());
    const auto frame = grab(c, geometry, crop.size());
    require(frame.save(c.root + '/' + tag + ".png"), "Cannot retain CRP010 composited scene");
    int black = 0, white = 0, background = 0, mismatches = 0, maxDelta = 0, excluded = 0;
    QJsonArray failures;
    const QRect region =
        QRectF(geometry.item.x() * geometry.dpr, geometry.item.y() * geometry.dpr,
               geometry.item.width() * geometry.dpr, geometry.item.height() * geometry.dpr)
            .toAlignedRect();
    require(frame.rect().contains(region), "CRP010 actual preview item escaped the native scene");
    for (int py = region.top(); py <= region.bottom(); ++py) {
        for (int px = region.left(); px <= region.right(); ++px) {
            const QPointF logical((px + .5) / geometry.dpr, (py + .5) / geometry.dpr);
            if (!geometry.item.adjusted(2, 2, -2, -2).contains(logical)) {
                ++excluded;
                continue;
            }
            QColor expected(17, 19, 24); // Literal current preview canvas, not an observed pixel.
            const auto expanded = geometry.photo.adjusted(-2, -2, 2, 2);
            const auto interior = geometry.photo.adjusted(2, 2, -2, -2);
            if (expanded.contains(logical) && !interior.contains(logical)) {
                ++excluded;
                continue;
            }
            if (interior.contains(logical)) {
                const double sourceX = crop.left() + (logical.x() - geometry.photo.left()) /
                                                         geometry.photo.width() * crop.width();
                const double sourceY = crop.top() + (logical.y() - geometry.photo.top()) /
                                                        geometry.photo.height() * crop.height();
                require(sourceX >= 0 && sourceX < 256 && sourceY >= 0 && sourceY < 192,
                        "Independent CRP010 output-to-source mapping escaped its fixture");
                // Exclude only expected source interpolation boundaries, never actual mismatches.
                if (std::abs(sourceX - 64) < 3 || std::abs(sourceX - 128) < 3 ||
                    std::abs(sourceX - 192) < 3) {
                    ++excluded;
                    continue;
                }
                const int channel = stripe(sourceX);
                expected = QColor(channel, channel, channel);
                if (channel)
                    ++white;
                else
                    ++black;
            } else
                ++background;
            const auto actual = frame.pixelColor(px, py);
            const int delta = distance(actual, expected);
            maxDelta = std::max(maxDelta, delta);
            if (delta > 1) {
                ++mismatches;
                if (failures.size() < 16)
                    failures.append(
                        QJsonObject{{"native_x", px},
                                    {"native_y", py},
                                    {"actual_rgba", static_cast<qint64>(actual.rgba())},
                                    {"expected_rgba", static_cast<qint64>(expected.rgba())}});
            }
        }
    }
    const QJsonObject evidence{{"tag", tag},
                               {"scene", tag + ".png"},
                               {"photo", rectJSON(geometry.photo)},
                               {"item", rectJSON(geometry.item)},
                               {"device_pixel_ratio", geometry.dpr},
                               {"native_width", frame.width()},
                               {"native_height", frame.height()},
                               {"integer_source_crop", rectJSON(crop)},
                               {"black_samples", black},
                               {"white_samples", white},
                               {"letterbox_samples", background},
                               {"excluded_boundary_samples", excluded},
                               {"max_channel_delta", maxDelta},
                               {"mismatches", mismatches},
                               {"first_mismatches", failures},
                               {"literal_expected_rgb", true},
                               {"physical_scanout_measured", false}};
    writeJson(c.root + '/' + tag + ".json", evidence);
    require(black > 100 && white > 100 && background > 100 && mismatches == 0,
            "CRP010 composited scene contains residual pixels or does not match independent "
            "literal crop/letterbox oracle");
    return evidence;
}
QJsonObject positiveOverlay(Context &c, const Document &document) {
    ready(c, document, true);
    const auto crop = sourceRect(document);
    const auto geometry = view(c, {256, 192});
    const auto *overlay = c.driver->item("cropOverlay");
    require(overlay->isVisible() &&
                overlay->mapRectToScene(overlay->boundingRect()) == geometry.photo &&
                c.editor.cropOverlay() == "thirds",
            "CRP010 active Canvas does not cover its independently fitted uncropped source");
    require(crop.left() > 32 && crop.left() < 60 && crop.top() > 16 && crop.top() < 40,
            "CRP010 positive probe fixture requires its reviewed small top-left inset");
    const auto sourceToScene = [&](QPointF source) {
        return QPointF(geometry.photo.left() + source.x() / 256 * geometry.photo.width(),
                       geometry.photo.top() + source.y() / 192 * geometry.photo.height());
    };
    const QPointF dim = sourceToScene({16, 96});
    const QPointF guide = sourceToScene({crop.left() + crop.width() / 3.0, 110});
    const QPointF grip =
        sourceToScene({crop.left() + crop.width() / 2.0, static_cast<double>(crop.top())});
    require(stripe(16) == 255 && stripe(crop.left() + crop.width() / 3.0) == 0,
            "CRP010 guide/dim independent source probes are not white/black respectively");
    QImage frame;
    int dimHits = 0, guideHits = 0, goldHits = 0;
    try {
        c.driver->wait(
            [&] {
                frame = grab(c, geometry, {256, 192});
                dimHits = guideHits = goldHits = 0;
                const auto count = [&](QPointF point, double radius, const auto &matches) {
                    int hits = 0;
                    const int cx = static_cast<int>(std::floor(point.x() * geometry.dpr));
                    const int cy = static_cast<int>(std::floor(point.y() * geometry.dpr));
                    const int extent = static_cast<int>(std::ceil(radius * geometry.dpr));
                    for (int dy = -extent; dy <= extent; ++dy)
                        for (int dx = -extent; dx <= extent; ++dx) {
                            const QPoint p(cx + dx, cy + dy);
                            if (frame.rect().contains(p) && matches(frame.pixelColor(p)))
                                ++hits;
                        }
                    return hits;
                };
                dimHits = count(dim, 1, [](const QColor &color) {
                    return distance(color, QColor(115, 115, 115)) <= 2;
                });
                guideHits = count(guide, 2, [](const QColor &color) {
                    return color.alpha() == 255 && color.red() >= 100 && color.red() <= 245 &&
                           std::abs(color.red() - color.green()) <= 1 &&
                           std::abs(color.red() - color.blue()) <= 1;
                });
                goldHits = count(grip, 5, [](const QColor &color) {
                    return distance(color, QColor(223, 174, 119)) <= 2;
                });
                return dimHits >= 4 && guideHits >= 2 && goldHits >= 8;
            },
            "Actual composited Crop Canvas displays independent dim/guide/gold positive controls",
            5000);
    } catch (...) {
        if (!frame.isNull())
            frame.save(c.root + "/crp010-active-unmatched.png");
        writeJson(c.root + "/crp010-active-unmatched.json", {{"dim_hits", dimHits},
                                                             {"guide_hits", guideHits},
                                                             {"gold_hits", goldHits},
                                                             {"state", state(c)}});
        throw;
    }
    require(frame.save(c.root + "/crp010-active-overlay.png"),
            "Cannot retain actual Crop Canvas positive scene");
    const QJsonObject evidence{
        {"dim_hits", dimHits},
        {"guide_hits", guideHits},
        {"gold_hits", goldHits},
        {"photo", rectJSON(geometry.photo)},
        {"integer_source_crop", rectJSON(crop)},
        {"device_pixel_ratio", geometry.dpr},
        {"dim_probe_scene", QJsonObject{{"x", dim.x()}, {"y", dim.y()}}},
        {"guide_probe_scene", QJsonObject{{"x", guide.x()}, {"y", guide.y()}}},
        {"grip_probe_scene", QJsonObject{{"x", grip.x()}, {"y", grip.y()}}},
        {"literal_dim_rgb", 115},
        {"white_guide_over_black", true},
        {"literal_gold_rgb", "223,174,119"}};
    writeJson(c.root + "/crp010-active-overlay.json", evidence);
    return evidence;
}
void noTransient(Context &c, bool done) {
    require(!c.editor.uxHistoryGestureActive() && !c.editor.localGestureActive() &&
                !c.editor.localSampling() && !c.editor.localMode() && !c.editor.guidedMode() &&
                c.editor.targetedTool() == "none" && !c.editor.original() &&
                !c.editor.presetPreview() && c.editor.detailDiagnostic().isEmpty() &&
                c.driver->object("cropOverlay")->property("dragMode").toString().isEmpty() &&
                !c.driver->object("cropDragArea")->property("pressed").toBool(),
            "CRP010 retained transient input/preview/diagnostic state after a natural boundary");
    if (done)
        require(!c.editor.cropMode() && !c.driver->item("cropOverlay")->isVisible() &&
                    c.window->mouseGrabberItem() != c.driver->item("cropDragArea"),
                "Actual Done retained Crop visibility or input ownership");
}
} // namespace

bool cropDoneResidueCandidate(Context &c, const QString &id) {
    if (id != candidateID && id != officialID)
        return false;
    native(c);
    auto &d = *c.driver;
    require(!c.editor.hasPhoto() && c.library.count() == 0 &&
                QDir(c.fixture).entryList({"*.png"}, QDir::Files).size() == 1,
            "CRP010 requires a fresh Context and exactly one reviewed generated PNG");
    const QString input = c.fixture + "/Photo-0000.png";
    require(QFileInfo(input).isFile() && !QFileInfo(input).isSymLink(),
            "CRP010 source must be a regular original");
    const QImage declared(input);
    require(declared.size() == QSize(256, 192),
            "CRP010 fixture must have the exact independent dimensions");
    for (int y = 0; y < 192; ++y)
        for (int x = 0; x < 256; ++x)
            require(declared.pixelColor(x, y) == QColor(stripe(x), stripe(x), stripe(x)),
                    "CRP010 declared source does not contain its exact literal stripes");
    d.log({{"action", "declared_chooser_result_setup"},
           {"native_chooser_exercised", false},
           {"input_boundary", "QT_QTEST_WINDOW_INPUT"},
           {"hardware_pointer_events_exercised", false}});
    c.import(1);
    d.wait([&] { return !d.object("library.importDialog")->property("visible").toBool(); },
           "CRP010 Import modal closed");
    d.doubleClick(c.card(0));
    c.loaded();
    d.wait([&] { return !c.editor.saving() && !c.library.loading(); },
           "CRP010 neutral setup naturally settled");
    const QString assetID = c.editor.photoID(), source = c.editor.sourcePath(),
                  originalSHA = sha(input);
    require(source == input && !c.window->property("libraryMode").toBool(),
            "Real CRP010 input did not open its original in Develop");
    luma::Catalog initialCatalog(c.catalogFile.toStdString());
    const auto initial = initialCatalog.assetSnapshot(assetID.toStdString());
    const auto initialRows = initialCatalog.libraryPage({}, 0, 2);
    const Document neutral;
    require(initial && initial->revision == 0 && initial->asset.edits == neutral &&
                initialRows.size() == 1 && initialRows[0].id == assetID.toStdString() &&
                initialRows[0].rating == 0 && initialRows[0].flag == 0 &&
                initialRows[0].label.empty() &&
                initial->asset.path == std::filesystem::path(source.toStdString()) &&
                initial->asset.checksum == originalSHA.toStdString() &&
                initial->asset.metadata.width == 256 && initial->asset.metadata.height == 192 &&
                initial->asset.metadata.orientation == 0 && c.editor.currentSettings() == neutral &&
                !c.editor.canUndo() && !c.editor.canRedo(),
            "CRP010 source/default document/neutral history setup is invalid");
    const auto journalBefore = journal(c);
    QJsonArray expectedSession;
    expectedSession.append(QJsonValue(QJsonArray{QStringLiteral("1"), assetID}));
    require(journalBefore["imports"].toArray().size() == 1 &&
                journalBefore["items"].toArray().size() == 1 &&
                journalBefore["session"].toArray() == expectedSession,
            "CRP010 setup Import/session journal does not identify its one source");
    const auto imported = journalBefore["imports"].toArray().at(0).toArray();
    const auto importedItem = journalBefore["items"].toArray().at(0).toArray();
    require(imported.size() == 8 && imported[1].toString() == "add" &&
                imported[2].toString().isEmpty() && imported[3].toString() == "completed" &&
                imported[4].toString().toLongLong() > 0 && imported[5].toString() == "0" &&
                !imported[6].toString().isEmpty() && !imported[7].toString().isEmpty() &&
                importedItem.size() == 10 && importedItem[0] == imported[0] &&
                importedItem[1].toString() == "0" && importedItem[2].toString() == source &&
                importedItem[3].toString() == source && importedItem[4].toString().isEmpty() &&
                importedItem[5].toString() == originalSHA &&
                importedItem[6].toString().toLongLong() == QFileInfo(source).size() &&
                importedItem[7].toString() == "committed" &&
                importedItem[8].toString() == assetID && importedItem[9].toString().isEmpty(),
            "CRP010 setup journal is not the exact completed one-source Add transaction");
    const auto immutableSource = metadataJSON(initial->asset.metadata),
               immutableRow = rowJSON(initialRows[0]);
    QSignalSpy saves(&c.editor, &EditorController::editsSaved);
    require(saves.isValid(), "Cannot observe real CRP010 natural save completions");
    InputTrace trace(c);
    ReleasePointer pointer{c}; // Declared before try: catch records pressed state before cleanup.
    QString phase = "baseline";
    QJsonArray checkpoints, scenes, inputs;
    const auto checkpoint = [&](const QString &tag) {
        auto value = state(c);
        value["tag"] = tag;
        value["save_success_count"] = saves.count();
        const auto snapshot =
            luma::Catalog(c.catalogFile.toStdString()).assetSnapshot(assetID.toStdString());
        if (snapshot) {
            value["durable_document"] = documentJSON(snapshot->asset.edits);
            value["durable_revision"] = QString::number(snapshot->revision);
        }
        checkpoints.append(value);
        writeJson(c.root + "/crp010-checkpoints.json",
                  {{"phase", phase}, {"checkpoints", checkpoints}});
    };
    const auto durable = [&](const Document &expected, std::optional<std::int64_t> revision) {
        d.wait(
            [&] {
                const auto snapshot =
                    luma::Catalog(c.catalogFile.toStdString()).assetSnapshot(assetID.toStdString());
                return !c.editor.saving() && !c.editor.busy() && snapshot &&
                       snapshot->asset.edits == expected;
            },
            "CRP010 exact full document persisted naturally without endEdit/save rescue");
        luma::Catalog catalog(c.catalogFile.toStdString());
        const auto snapshot = catalog.assetSnapshot(assetID.toStdString());
        const auto rows = catalog.libraryPage({}, 0, 2);
        require(snapshot && (!revision || snapshot->revision == *revision) &&
                    snapshot->asset.path == initial->asset.path &&
                    snapshot->asset.checksum == originalSHA.toStdString() &&
                    metadataJSON(snapshot->asset.metadata) == immutableSource && rows.size() == 1 &&
                    rowJSON(rows[0]) == immutableRow &&
                    catalog.libraryIDs({}) == std::vector<std::string>{assetID.toStdString()} &&
                    catalog.collections().empty() && journal(c) == journalBefore &&
                    source == c.editor.sourcePath() && assetID == c.editor.photoID() &&
                    sha(source) == originalSHA && c.editor.currentSettings() == expected &&
                    c.editor.error().isEmpty(),
                "CRP010 persistence/revision/source/metadata/row/journal identity mismatch");
        require(saves.count() == snapshot->revision,
                "CRP010 durable revision and real successful save callbacks differ");
        return snapshot->revision;
    };
    const auto clickTool = [&](const QString &label) {
        c.reveal("cropTool");
        trace.begin(label);
        d.click("cropTool");
        trace.complete(false);
        inputs.append(trace.json());
    };
    try {
        noTransient(c, true);
        durable(neutral, 0);
        scenes.append(census(c, neutral, "crp010-neutral-scene"));
        checkpoint("neutral_literal_scene_positive");
        phase = "real_crop_input";
        clickTool("enter_crop");
        ready(c, neutral, true);
        auto *area = d.item("cropDragArea");
        require(area->width() > 100 && area->height() > 100,
                "CRP010 reviewed handle fixture needs an ordinary supported photo surface");
        // Keep the first integer-logical input inside the actual lt hit area.
        // A .001 fraction can round outside a fractionally positioned photo.
        const QPointF from(std::max(.001, 2.0 / area->width()),
                           std::max(.001, 2.0 / area->height()));
        const QPointF to(.18, .15);
        const QPointF startLocal = area->mapFromScene(d.point("cropDragArea", from));
        const QPointF finishLocal = area->mapFromScene(d.point("cropDragArea", to));
        const QPointF start(std::clamp(startLocal.x() / area->width(), 0.0, 1.0),
                            std::clamp(startLocal.y() / area->height(), 0.0, 1.0));
        const QPointF finish(std::clamp(finishLocal.x() / area->width(), 0.0, 1.0),
                             std::clamp(finishLocal.y() / area->height(), 0.0, 1.0));
        const QPointF delta = finish - start;
        d.log({{"action", "crp010_independent_crop_input_oracle"},
               {"area_width", area->width()},
               {"area_height", area->height()},
               {"start_local_x", startLocal.x()},
               {"start_local_y", startLocal.y()},
               {"finish_local_x", finishLocal.x()},
               {"finish_local_y", finishLocal.y()},
               {"start_normalized_x", start.x()},
               {"start_normalized_y", start.y()},
               {"finish_normalized_x", finish.x()},
               {"finish_normalized_y", finish.y()},
               {"independent_delta_x", delta.x()},
               {"independent_delta_y", delta.y()}});
        auto expected = neutral;
        expected.crop.x = delta.x();
        expected.crop.y = delta.y();
        expected.crop.width = 1 - delta.x();
        expected.crop.height = 1 - delta.y();
        trace.begin("crop_top_left_real_drag");
        pointer.last = d.point("cropDragArea", from);
        d.press("cropDragArea", from);
        pointer.down = true;
        require(d.object("cropOverlay")->property("dragMode").toString() == "lt" &&
                    d.object("cropDragArea")->property("pressed").toBool() &&
                    c.editor.uxHistoryGestureActive(),
                "CRP010 actual top-left handle did not accept the held gesture");
        for (int step = 1; step <= 12; ++step) {
            const auto fraction = from + (to - from) * (static_cast<double>(step) / 12);
            pointer.last = d.point("cropDragArea", fraction);
            d.move("cropDragArea", fraction);
        }
        d.release("cropDragArea", to);
        pointer.down = false;
        trace.complete(true);
        inputs.append(trace.json());
        const auto cropped = c.editor.currentSettings();
        require(near(cropped.crop.x, expected.crop.x) && near(cropped.crop.y, expected.crop.y) &&
                    near(cropped.crop.width, expected.crop.width) &&
                    near(cropped.crop.height, expected.crop.height),
                "CRP010 Crop does not follow the independently dispatched top-left delta");
        expected.crop.x = cropped.crop.x;
        expected.crop.y = cropped.crop.y;
        expected.crop.width = cropped.crop.width;
        expected.crop.height = cropped.crop.height;
        require(cropped == expected && cropped != neutral && c.editor.canUndo() &&
                    !c.editor.canRedo(),
                "CRP010 actual crop changed unrelated typed settings/history");
        noTransient(c, false);
        const auto revision = durable(cropped, {});
        require(revision > 0, "CRP010 crop has no natural durable revision");
        phase = "positive_actual_canvas_before_done";
        const auto positive = positiveOverlay(c, cropped);
        checkpoint("active_canvas_literal_dim_guide_gold_positive");
        phase = "done";
        clickTool("actual_done");
        noTransient(c, true);
        durable(cropped, revision);
        scenes.append(census(c, cropped, "crp010-done-scene"));
        checkpoint("done_no_residue_before_any_reentry_or_undo");
        phase = "post_done_empty_preview_input";
        trace.begin("ordinary_preview_drag_after_done");
        pointer.last = d.point("develop.preview", {.3, .25});
        d.press("develop.preview", {.3, .25});
        pointer.down = true;
        for (int step = 1; step <= 6; ++step) {
            const auto fraction =
                QPointF(.3, .25) + QPointF(.3, .45) * (static_cast<double>(step) / 6);
            pointer.last = d.point("develop.preview", fraction);
            d.move("develop.preview", fraction);
            require(!c.editor.uxHistoryGestureActive() && c.editor.currentSettings() == cropped,
                    "Closed Crop captured fresh ordinary preview input");
        }
        d.release("develop.preview", {.6, .7});
        pointer.down = false;
        trace.complete(true);
        inputs.append(trace.json());
        noTransient(c, true);
        durable(cropped, revision);
        phase = "reenter_and_done_without_edit";
        clickTool("reenter_without_edit");
        ready(c, cropped, true);
        noTransient(c, false);
        clickTool("second_done_without_edit");
        noTransient(c, true);
        for (int frame = 0; frame < 3; ++frame)
            d.frame();
        durable(cropped, revision);
        scenes.append(census(c, cropped, "crp010-second-done-scene"));
        checkpoint("second_done_no_empty_history_or_revision");
        phase = "single_undo";
        d.click("main.undoButton");
        require(c.editor.currentSettings() == neutral && !c.editor.canUndo() && c.editor.canRedo(),
                "Done/reentry/fresh input added a phantom history entry before the one Crop Undo");
        noTransient(c, true);
        durable(neutral, revision + 1);
        scenes.append(census(c, neutral, "crp010-undo-scene"));
        checkpoint("single_undo_exact_neutral");
        phase = "single_redo";
        d.click("main.redoButton");
        require(c.editor.currentSettings() == cropped && c.editor.canUndo() && !c.editor.canRedo(),
                "CRP010 single Redo did not restore only the exact crop gesture");
        noTransient(c, true);
        durable(cropped, revision + 2);
        scenes.append(census(c, cropped, "crp010-redo-scene"));
        checkpoint("single_redo_exact_crop");
        luma::Catalog(c.catalogFile.toStdString()).verify();
        writeJson(c.root + "/crp010-summary.json",
                  {{"candidate", id},
                   {"scenario", id},
                   {"inventory_domain", id == officialID ? "specification" : "supplemental"},
                   {"intended_contract", "UX-CRP-010 Done-only approved"},
                   {"official_registered", id == officialID},
                   {"full_release_qualified", false},
                   {"input_boundary", "QT_QTEST_WINDOW_INPUT"},
                   {"hardware_pointer_events_exercised", false},
                   {"physical_scanout_measured", false},
                   {"source", source},
                   {"source_sha256", originalSHA},
                   {"full_neutral_document", documentJSON(neutral)},
                   {"full_cropped_document", documentJSON(cropped)},
                   {"source_metadata", immutableSource},
                   {"library_row", immutableRow},
                   {"initial_journal", journalBefore},
                   {"crop_natural_revision", QString::number(revision)},
                   {"final_revision", QString::number(revision + 2)},
                   {"save_success_count", saves.count()},
                   {"active_overlay_positive", positive},
                   {"scene_censuses", scenes},
                   {"inputs", inputs}});
    } catch (...) {
        try {
            checkpoint("failure_before_any_automatic_pointer_release");
            writeJson(c.root + "/crp010-failure.json", {{"phase", phase},
                                                        {"state", state(c)},
                                                        {"input", trace.json()},
                                                        {"pointer_cleanup_pending", pointer.down},
                                                        {"source_sha256", sha(source)},
                                                        {"journal", journal(c)}});
        } catch (...) {
            std::fputs(
                "Cannot preserve complete CRP010 failure evidence; original failure retained\n",
                stderr);
        }
        throw;
    }
    return true;
}
} // namespace ux
