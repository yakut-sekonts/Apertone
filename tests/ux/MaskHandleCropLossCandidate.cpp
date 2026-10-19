#include "apps/shared/PresetStore.h"
#include "tests/ux/UxContext.h"
#include "tests/ux/UxPixels.h"
#include <QCryptographicHash>
#include <QCursor>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QTest>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <exception>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <utility>
#include <vector>

// Bounded Crop/flip regression for reachable gradient controls.
// The complete official MSK-023 scenario remains unqualified.
namespace ux {
namespace {
using Document = luma::EditDocument;
QJsonObject documentJSON(const Document &document) {
    const auto parsed = QJsonDocument::fromJson(
        luma::encodePreset({"MSK023 complete document", {}, document, luma::allSettingsGroups}));
    require(parsed.isObject(), "Cannot encode complete MSK023 diagnostic document");
    auto result = parsed.object();
    result.insert("local_masks_serialized",
                  QString::fromStdString(luma::encodeLocalMasks(document.localMasks)));
    return result;
}
// Passive diagnostics only: no input, rendering, delay or acceptance changes.
struct MaskReadyTrace {
    Context &context;
    QObject observer;
    QElapsedTimer elapsed;
    QJsonArray observations;
    QJsonObject previous;
    int dropped = 0, failedSamples = 0;
    QJsonObject snapshot() const {
        const auto &editor = context.editor;
        const auto *window = context.window;
        const auto cursor = window->mapFromGlobal(QCursor::pos());
        return {{"local_mode", editor.localMode()},
                {"local_canvas_ready", editor.localCanvasReady()},
                {"local_sampling", editor.localSampling()},
                {"local_gesture_active", editor.localGestureActive()},
                {"local_overlay", editor.localOverlay()},
                {"original", editor.original()},
                {"preset_preview", editor.presetPreview()},
                {"detail_diagnostic", editor.detailDiagnostic()},
                {"crop_mode", editor.cropMode()},
                {"guided_mode", editor.guidedMode()},
                {"busy", editor.busy()},
                {"saving", editor.saving()},
                {"error", editor.error()},
                {"status", editor.status()},
                {"photo_id", editor.photoID()},
                {"source", editor.sourcePath()},
                {"preview_url", editor.previewUrl()},
                {"can_undo", editor.canUndo()},
                {"can_redo", editor.canRedo()},
                {"mask_count", static_cast<int>(editor.currentSettings().localMasks.size())},
                {"application_state", static_cast<int>(QGuiApplication::applicationState())},
                {"window_active", window->isActive()},
                {"window_visible", window->isVisible()},
                {"window_exposed", window->isExposed()},
                {"window_visibility", static_cast<int>(window->visibility())},
                {"window_has_focus", QGuiApplication::focusWindow() == window},
                {"mouse_grabber",
                 window->mouseGrabberItem() ? window->mouseGrabberItem()->objectName() : QString{}},
                {"physical_cursor_in_window_x", cursor.x()},
                {"physical_cursor_in_window_y", cursor.y()}};
    }
    void sample(const char *reason) noexcept {
        try {
            auto state = snapshot();
            if (state == previous)
                return;
            previous = state;
            state["elapsed_ms"] = elapsed.elapsed();
            state["reason"] = QString::fromLatin1(reason);
            if (observations.size() < 4096)
                observations.append(state);
            else
                ++dropped;
        } catch (...) {
            ++failedSamples;
        }
    }
    explicit MaskReadyTrace(Context &value) : context(value) {
        elapsed.start();
        QObject::connect(&context.editor, &EditorController::stateChanged, &observer,
                         [this] { sample("editor_state_changed"); });
        QObject::connect(context.window, &QQuickWindow::afterAnimating, &observer,
                         [this] { sample("window_after_animating"); });
        sample("candidate_begin");
    }
    ~MaskReadyTrace() noexcept {
        try {
            sample("candidate_end_before_cleanup");
            writeJson(context.root + "/msk023-ready-timeline.json",
                      {{"passive_only", true},
                       {"unwinding_exception", std::uncaught_exceptions() > 0},
                       {"dropped", dropped},
                       {"failed_samples", failedSamples},
                       {"observations", observations},
                       {"final_state", snapshot()},
                       {"final_document", documentJSON(context.editor.currentSettings())}});
        } catch (...) {
            std::fputs("Cannot preserve passive mask readiness diagnostics\n", stderr);
        }
    }
};
struct ReleasePointer {
    Context &context;
    QPoint last;
    bool down = false;
    QString stage = "unspecified";
    int handle = -1;
    ~ReleasePointer() {
        if (!down)
            return;
        // Persist failure state BEFORE release can repair controller/QML state.
        try {
            auto *local = context.driver->find("localMaskGestureArea");
            auto *crop = context.driver->find("cropDragArea");
            auto *grabber = context.window->mouseGrabberItem();
            writeJson(context.root + "/msk023-pointer-before-cleanup.json",
                      {{"stage", stage},
                       {"handle", handle},
                       {"acceptance_proof", false},
                       {"uncaught_exceptions", std::uncaught_exceptions()},
                       {"last_scene_x", last.x()},
                       {"last_scene_y", last.y()},
                       {"local_gesture_active", context.editor.localGestureActive()},
                       {"local_pressed", local && local->property("pressed").toBool()},
                       {"local_adjusting", local && local->property("adjusting").toBool()},
                       {"crop_pressed", crop && crop->property("pressed").toBool()},
                       {"crop_mode", context.editor.cropMode()},
                       {"grabber_name", grabber ? grabber->objectName() : QString()},
                       {"photo_id", context.editor.photoID()},
                       {"full_document", documentJSON(context.editor.currentSettings())}});
        } catch (const std::exception &failure) {
            std::fprintf(stderr, "MSK023 pre-cleanup evidence failed: %s\n", failure.what());
        } catch (...) {
            std::fputs("MSK023 pre-cleanup evidence failed; original exception retained\n", stderr);
        }
        // Failure cleanup only; never use this to satisfy an acceptance guard.
        try {
            QTest::mouseRelease(context.window, Qt::LeftButton, Qt::NoModifier, last);
            context.driver->log(
                {{"action", "geometry_failure_pointer_cleanup"}, {"acceptance_proof", false}});
        } catch (...) {
        }
    }
};
void json(const QString &path, const QJsonObject &value) {
    QFile file(path);
    require(file.open(QIODevice::WriteOnly), "Cannot create own geometry evidence: " + path);
    const auto data = QJsonDocument(value).toJson();
    require(file.write(data) == data.size() && file.flush(),
            "Cannot finish own geometry evidence: " + path);
}
QByteArray digest(const QString &path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Cannot read source for geometry SHA guard");
    QCryptographicHash hash(QCryptographicHash::Sha256);
    require(hash.addData(&file), "Cannot hash geometry source");
    return hash.result();
}
void exact(Context &c, const Document &expected, const QString &reason) {
    require(c.editor.currentSettings() == expected && c.editor.error().isEmpty(), reason);
}
void readyMask(Context &c) {
    c.driver->wait(
        [&] {
            return c.editor.localMode() && c.editor.localCanvasReady() && !c.editor.localSampling();
        },
        "Actual local editor render is ready");
    require(c.editor.error().isEmpty(), c.editor.error());
    c.driver->frame();
}
void noPointer(Context &c) {
    require(!c.editor.localGestureActive() &&
                !c.driver->object("localMaskGestureArea")->property("adjusting").toBool() &&
                !c.driver->object("localMaskGestureArea")->property("pressed").toBool(),
            "Released gradient retained a controller gesture or QML pointer ownership");
}
void noCropPointer(Context &c) {
    require(c.driver->object("cropOverlay")->property("dragMode").toString().isEmpty() &&
                !c.driver->object("cropDragArea")->property("pressed").toBool(),
            "Released/closed Crop retained transient pointer state");
}
void rendered(Context &c, const Document &expected) {
    const auto pixels = fixtureReference(c, expected, c.editor.sourcePath());
    c.driver->wait(
        [&] {
            const auto *image = c.driver->item("develop.preview");
            return !c.editor.busy() && image->isVisible() &&
                   image->property("status").toInt() == 1 &&
                   image->property("source").toUrl().toString() == c.editor.previewUrl() &&
                   c.provider->requestImage({}, nullptr, {}) == pixels;
        },
        "Full geometry document has Ready/current-source/exact CPU preview pixels");
    exact(c, expected, "Geometry render changed the document");
    c.driver->frame();
}
void durable(Context &c, const QString &asset, const Document &expected) {
    // Never call Context::saved()/endEdit to repair an unclosed input gesture.
    c.driver->wait(
        [&] {
            const auto record =
                luma::Catalog(c.catalogFile.toStdString()).assetByID(asset.toStdString());
            return !c.editor.saving() && !c.editor.busy() && record && record->edits == expected;
        },
        "Natural geometry autosave committed the complete expected document");
}
void overlayOff(Context &c) {
    c.reveal("localOverlayToggle");
    if (c.editor.localOverlay())
        c.driver->click("localOverlayToggle");
    require(!c.editor.localOverlay(), "Actual Show mask overlay toggle was not disabled");
    readyMask(c);
}
std::size_t selectedIndex(Context &c) {
    const auto id = c.editor.localState()["id"].toString().toStdString();
    const auto document = c.editor.currentSettings();
    for (std::size_t i = 0; i < document.localMasks.size(); ++i)
        if (document.localMasks[i].id == id)
            return i;
    throw std::runtime_error("Actual local mask selection has no complete document identity");
}
std::array<QPointF, 3> sourceHandles(const luma::LocalMask &mask) {
    if (mask.shape == luma::MaskShape::linear)
        return {QPointF(mask.x1, mask.y1), QPointF(mask.x2, mask.y2),
                QPointF((mask.x1 + mask.x2) / 2, (mask.y1 + mask.y2) / 2)};
    require(mask.shape == luma::MaskShape::radial, "Handle repro requires a linear/radial mask");
    const auto radians = mask.angle * std::numbers::pi / 180;
    return {QPointF(mask.x1, mask.y1),
            QPointF(mask.x1 + std::cos(radians) * mask.x2, mask.y1 + std::sin(radians) * mask.x2),
            QPointF(mask.x1 - std::sin(radians) * mask.y2, mask.y1 + std::cos(radians) * mask.y2)};
}
QPointF actualInput(Context &c, QPointF fraction) {
    auto *area = c.driver->item("localMaskGestureArea");
    const auto point = area->mapFromScene(c.driver->point("localMaskGestureArea", fraction));
    return {point.x() / area->width(), point.y() / area->height()};
}
void initialHitPositiveControls(Context &c) {
    auto &d = *c.driver;
    const auto baseline = c.editor.currentSettings();
    require(baseline.crop == luma::CropSettings{} &&
                baseline.transform == luma::TransformSettings{},
            "Independent initial handle oracle requires the untransformed generated source");
    const auto index = selectedIndex(c);
    for (int handle = 0; handle < 3; ++handle) {
        readyMask(c);
        const auto locations = sourceHandles(baseline.localMasks[index]);
        const auto start = locations[static_cast<std::size_t>(handle)];
        const auto finish = start + QPointF(.04, .03);
        const auto actualStart = actualInput(c, start), actualFinish = actualInput(c, finish);
        auto expected = baseline;
        auto &mask = expected.localMasks[index];
        const auto delta = actualFinish - actualStart;
        if (mask.shape == luma::MaskShape::linear) {
            if (handle == 0 || handle == 2) {
                mask.x1 += delta.x();
                mask.y1 += delta.y();
            }
            if (handle == 1 || handle == 2) {
                mask.x2 += delta.x();
                mask.y2 += delta.y();
            }
        } else if (handle == 0) {
            mask.x1 += delta.x();
            mask.y1 += delta.y();
        } else if (handle == 1)
            mask.x2 = std::abs(actualFinish.x() - mask.x1);
        else
            mask.y2 = std::abs(actualFinish.y() - mask.y1);
        ReleasePointer pointer{c, d.point("localMaskGestureArea", start)};
        pointer.stage = "initial-handle";
        pointer.handle = handle;
        d.press("localMaskGestureArea", start);
        pointer.down = true;
        require(c.editor.localGestureActive() &&
                    d.object("localMaskGestureArea")->property("adjusting").toBool(),
                "An independently located initial handle did not accept real pointer input");
        for (int step = 1; step <= 12; ++step) {
            const auto fraction = start + (finish - start) * (static_cast<double>(step) / 12);
            pointer.last = d.point("localMaskGestureArea", fraction);
            d.move("localMaskGestureArea", fraction);
        }
        d.release("localMaskGestureArea", finish);
        pointer.down = false;
        readyMask(c);
        noPointer(c);
        const auto actual = c.editor.currentSettings();
        // QTest rounds to an input pixel; expected uses the actual dispatched
        // start/end coordinates. Floating computation order alone may differ.
        auto normalized = actual;
        auto &geometry = normalized.localMasks[index];
        require(
            std::abs(geometry.x1 - mask.x1) < 1e-12 && std::abs(geometry.y1 - mask.y1) < 1e-12 &&
                std::abs(geometry.x2 - mask.x2) < 1e-12 && std::abs(geometry.y2 - mask.y2) < 1e-12,
            "Actual initial handle hit did not follow its independent complete geometry oracle");
        geometry.x1 = mask.x1;
        geometry.y1 = mask.y1;
        geometry.x2 = mask.x2;
        geometry.y2 = mask.y2;
        require(actual != baseline && normalized == expected,
                "Initial handle hit changed an unrelated document field");
        rendered(c, actual);
        d.click("main.undoButton");
        exact(c, baseline, "Initial handle positive control was not exactly one Undo");
        d.click("main.redoButton");
        exact(c, actual, "Initial handle positive-control Redo changed the complete document");
        d.click("main.undoButton");
        exact(c, baseline,
              "Cannot restore the unchanged pre-restriction fixture after positive control");
        d.log({{"action", "gradient_initial_real_handle_hit_positive_control"},
               {"mask_id", QString::fromStdString(baseline.localMasks[index].id)},
               {"handle", handle}});
    }
}
struct Hit {
    int handle = -1;
    QPointF fraction;
    bool proxy = false; // From independent raw projection, never the production proxy flag.
};
struct Availability {
    QJsonArray evidence;
    std::vector<Hit> hits;
};
bool insideClips(Context &c, QQuickItem *area, const QPointF &point) {
    const auto scene = area->mapToScene(point);
    if (!c.window->contentItem()->contains(scene))
        return false;
    for (auto *item = area; item; item = item->parentItem())
        if (!item->isVisible() || !item->isEnabled() ||
            (item->clip() && !item->contains(item->mapFromScene(scene))))
            return false;
    return area->contains(point);
}
// Independent known256x192 integer Crop geometry + optional H/V flips.
// Photo dimensions come from the actual Image, not controller handle projection.
struct PlainCropInput {
    int left, top, width, height;
    bool flipH, flipV, declaredPhotoRect = false;
    double photoX, photoY, photoWidth, photoHeight, areaWidth, areaHeight;
    PlainCropInput(Context &c, const Document &document) {
        require(document.transform == luma::TransformSettings{} && document.crop.angle == 0 &&
                    document.crop.aspect == 0 && document.crop.quarterTurns == 0,
                "Independent MSK023 mapping requires plain Crop and optional H/V flips");
        left = std::clamp(static_cast<int>(std::lround(document.crop.x * 256)), 0, 255);
        top = std::clamp(static_cast<int>(std::lround(document.crop.y * 192)), 0, 191);
        const int right =
            std::clamp(static_cast<int>(std::lround((document.crop.x + document.crop.width) * 256)),
                       left + 1, 256);
        const int bottom = std::clamp(
            static_cast<int>(std::lround((document.crop.y + document.crop.height) * 192)), top + 1,
            192);
        width = right - left;
        height = bottom - top;
        require(width >= 2 && height >= 2,
                "MSK023 fixture requires at least two source pixels on each Crop axis");
        flipH = document.crop.flipHorizontal;
        flipV = document.crop.flipVertical;
        auto *area = c.driver->item("localMaskGestureArea");
        auto *overlay = c.driver->item("localPhotoOverlay");
        auto *preview = c.driver->item("develop.preview");
        areaWidth = area->width();
        areaHeight = area->height();
        photoWidth = preview->property("paintedWidth").toDouble();
        photoHeight = preview->property("paintedHeight").toDouble();
        photoX = (areaWidth - photoWidth) / 2;
        photoY = (areaHeight - photoHeight) / 2;
        require(photoWidth > 0 && photoHeight > 0 && areaWidth > 0 && areaHeight > 0,
                "Actual preview/input rectangle has invalid dimensions");
        int declarations = 0;
        for (const auto &[name, expected] :
             std::array<std::pair<const char *, double>, 4>{{{"photoX", photoX},
                                                             {"photoY", photoY},
                                                             {"photoWidth", photoWidth},
                                                             {"photoHeight", photoHeight}}}) {
            const auto value = overlay->property(name);
            if (value.isValid()) {
                ++declarations;
                require(std::abs(value.toDouble() - expected) < 1e-6,
                        "Declared photo rectangle differs from actual centered Image geometry: " +
                            QString::fromLatin1(name));
            }
        }
        declaredPhotoRect = declarations == 4;
        // Permit original whole-photo geometry to preserve its strict zero-hit
        // failure. Expanded fixed geometry must expose the complete photo rect.
        require(declarations == 4 ||
                    (declarations == 0 && std::abs(photoX) < 1e-6 && std::abs(photoY) < 1e-6),
                "Unknown partial/expanded overlay photo rectangle contract");
        const auto actualOrigin = area->mapToScene(QPointF(photoX, photoY));
        const auto expectedOrigin = preview->mapToScene(
            {(preview->width() - photoWidth) / 2, (preview->height() - photoHeight) / 2});
        require(std::abs(actualOrigin.x() - expectedOrigin.x()) < 1e-6 &&
                    std::abs(actualOrigin.y() - expectedOrigin.y()) < 1e-6,
                "Overlay photo origin is not aligned with the actual preview");
    }
    QPointF rawPhoto(const QPointF &source) const {
        const double x = flipH ? 1 - source.x() : source.x();
        const double y = flipV ? 1 - source.y() : source.y();
        return {(x * 256 - left) / width, (y * 192 - top) / height};
    }
    QPointF rawOverlay(const QPointF &source) const {
        const auto photo = rawPhoto(source);
        return {(photoX + photo.x() * photoWidth) / areaWidth,
                (photoY + photo.y() * photoHeight) / areaHeight};
    }
    QPointF localInput(Context &c, const QPointF &fraction) const {
        auto *area = c.driver->item("localMaskGestureArea");
        return area->mapFromScene(c.driver->point("localMaskGestureArea", fraction));
    }
    QPointF photoInput(Context &c, const QPointF &fraction) const {
        const auto point = localInput(c, fraction);
        return {std::clamp((point.x() - photoX) / photoWidth, 0., 1.),
                std::clamp((point.y() - photoY) / photoHeight, 0., 1.)};
    }
    QPointF sourceInput(Context &c, const QPointF &fraction) const {
        const auto photo = photoInput(c, fraction);
        double x = (left + std::clamp(photo.x() * width - .5, 0., width - 1.) + .5) / 256;
        double y = (top + std::clamp(photo.y() * height - .5, 0., height - 1.) + .5) / 192;
        if (flipH)
            x = 1 - x;
        if (flipV)
            y = 1 - y;
        return {x, y};
    }
    QPointF proxySourceDelta(Context &c, const QPointF &from, const QPointF &to) const {
        const auto pixels = localInput(c, to) - localInput(c, from);
        return {(flipH ? -1 : 1) * pixels.x() / photoWidth * width / 256,
                (flipV ? -1 : 1) * pixels.y() / photoHeight * height / 192};
    }
    QPointF overlayInput(double x, double y) const {
        return {(photoX + x * photoWidth) / areaWidth, (photoY + y * photoHeight) / areaHeight};
    }
};
Availability availability(Context &c, bool independentPlainCrop) {
    auto &d = *c.driver;
    readyMask(c);
    auto *area = d.item("localMaskGestureArea");
    require(area->width() > 2 && area->height() > 2 && area->isVisible() && area->isEnabled(),
            "Availability oracle requires the actual visible, enabled gradient input surface");
    const auto controller = c.editor.localHandles();
    auto property = d.object("localPhotoOverlay")->property("handles");
    if (property.metaType() == QMetaType::fromType<QJSValue>())
        property = property.value<QJSValue>().toVariant();
    const auto qml = property.toList();
    require(controller.size() == 3 && qml.size() == 3,
            "Gradient projection lost its three typed handles");
    QMap<int, QVariantMap> controllerByID, paintedByID;
    for (const auto &entry : controller) {
        const auto handle = entry.toMap();
        const auto key = handle.value("handle", -1).toInt();
        require(handle.contains("x") && handle.contains("y") && key >= 0 && key <= 2 &&
                    !controllerByID.contains(key),
                "Controller gradient handles lost their unique typed identity/coordinates");
        controllerByID.insert(key, handle);
    }
    for (const auto &entry : qml) {
        const auto handle = entry.toMap();
        require(handle.contains("handle") && handle.contains("x") && handle.contains("y"),
                "A painted gradient handle lacks its typed identity/coordinates");
        const auto key = handle["handle"].toInt();
        require(key >= 0 && key <= 2 && !paintedByID.contains(key),
                "Painted gradient handles duplicate/lose an identity");
        paintedByID.insert(key, handle);
    }
    const auto document = c.editor.currentSettings();
    const PlainCropInput mapping(c, document);
    const auto source = sourceHandles(document.localMasks[selectedIndex(c)]);
    Availability result;
    for (int i = 0; i < 3; ++i) {
        const auto handle = controllerByID[i], painted = paintedByID[i];
        const auto rawX = handle["x"].toDouble(), rawY = handle["y"].toDouble();
        const auto x = painted["x"].toDouble(), y = painted["y"].toDouble();
        require(
            handle["handle"].toInt() == i && std::isfinite(rawX) && std::isfinite(rawY) &&
                std::isfinite(x) && std::isfinite(y),
            "Controller or actual QML painted handles lost their identities/finite coordinates");
        QJsonObject projection;
        if (independentPlainCrop) {
            // Direct integer Crop translation/scaling for this known 256x192,
            // unrotated, unflipped fixture; never calls localHandles/CropGeometry
            // as the expected projection generator.
            const auto rawPhoto = mapping.rawPhoto(source[static_cast<std::size_t>(i)]);
            const auto rawOverlay = mapping.rawOverlay(source[static_cast<std::size_t>(i)]);
            const auto expectedX = rawPhoto.x(), expectedY = rawPhoto.y();
            projection = {
                {"expected_raw_x", expectedX},
                {"expected_raw_y", expectedY},
                {"controller_matches_raw_projection",
                 std::abs(rawX - expectedX) < 1e-9 && std::abs(rawY - expectedY) < 1e-9},
                {"expected_raw_overlay_x", rawOverlay.x()},
                {"expected_raw_overlay_y", rawOverlay.y()},
                {"painted_matches_raw_projection",
                 std::abs(x - rawOverlay.x()) < 1e-9 && std::abs(y - rawOverlay.y()) < 1e-9}};
            // Deliberately evidence, not equality precondition: an edge proxy or
            // clamped displayed handle may legitimately differ from the raw source
            // projection. Its actual pointer hit and typed geometry effect below
            // must still succeed without changing the stored mask merely to reveal it.
        }
        // Search real integer window pixels in the exact 14px hit neighborhood.
        // One clamped pixel (especially an arbitrary 1px inset) cannot prove that
        // every available point loses to another handle or lies outside a clip.
        const QPointF desired(std::clamp(x * area->width(), 1.0, area->width() - 1),
                              std::clamp(y * area->height(), 1.0, area->height() - 1));
        QPointF fraction(desired.x() / area->width(), desired.y() / area->height());
        auto dispatched = area->mapFromScene(d.point("localMaskGestureArea", fraction));
        const auto nearestAt = [&](const QPointF &point) {
            int nearest = -1;
            double distance = 14 * 14;
            for (const auto &entry : qml) {
                const auto value = entry.toMap();
                const double dx = value["x"].toDouble() * area->width() - point.x();
                const double dy = value["y"].toDouble() * area->height() - point.y();
                const double squared = dx * dx + dy * dy;
                if (squared < distance) {
                    distance = squared;
                    nearest = value["handle"].toInt();
                }
            }
            return std::pair{nearest, distance};
        };
        const auto region =
            area->mapRectToScene(QRectF(x * area->width() - 14, y * area->height() - 14, 28, 28))
                .intersected(area->mapRectToScene(area->boundingRect()))
                .intersected(QRectF(0, 0, c.window->width() - 1, c.window->height() - 1));
        bool hit = false;
        int checkedPixels = 0;
        double bestDistance = std::numeric_limits<double>::infinity();
        if (!region.isEmpty()) {
            for (int sceneY = static_cast<int>(std::ceil(region.top()));
                 sceneY <= static_cast<int>(std::floor(region.bottom())); ++sceneY)
                for (int sceneX = static_cast<int>(std::ceil(region.left()));
                     sceneX <= static_cast<int>(std::floor(region.right())); ++sceneX) {
                    ++checkedPixels;
                    const QPoint scene(sceneX, sceneY);
                    if (scene.isNull()) // QTest interprets QPoint(0,0) as window center.
                        continue;
                    const auto point = area->mapFromScene(scene);
                    const QPointF candidate(point.x() / area->width(), point.y() / area->height());
                    if (!insideClips(c, area, point) ||
                        d.point("localMaskGestureArea", candidate) != scene)
                        continue;
                    const auto [nearest, distance] = nearestAt(point);
                    if (nearest == i && distance < bestDistance) {
                        hit = true;
                        bestDistance = distance;
                        fraction = candidate;
                        dispatched = point;
                    }
                }
        }
        const auto [nearest, distance] = nearestAt(dispatched);
        const double ownDX = x * area->width() - dispatched.x();
        const double ownDY = y * area->height() - dispatched.y();
        result.evidence.append(QJsonObject{{"handle", i},
                                           {"controller_x", rawX},
                                           {"controller_y", rawY},
                                           {"painted_hit_x", x},
                                           {"painted_hit_y", y},
                                           {"independent_projection", projection},
                                           {"logical_width", area->width()},
                                           {"logical_height", area->height()},
                                           {"nearest_at_real_input_pixel", nearest},
                                           {"nearest_distance_squared", distance},
                                           {"own_distance_squared", ownDX * ownDX + ownDY * ownDY},
                                           {"actual_input_x", dispatched.x()},
                                           {"actual_input_y", dispatched.y()},
                                           {"candidate_integer_pixels_checked", checkedPixels},
                                           {"overlay_clip", d.item("localPhotoOverlay")->clip()},
                                           {"hit_available", hit}});
        if (hit)
            result.hits.push_back(
                {i, fraction,
                 independentPlainCrop && !projection["painted_matches_raw_projection"].toBool()});
    }
    return result;
}
void select(Context &c, int index) {
    c.reveal("localMaskChoice");
    c.driver->choose("localMaskChoice", index);
    readyMask(c);
    require(c.editor.localState()["index"].toInt() == index,
            "Actual mask choice did not select the intended projected geometry");
}
QJsonObject postCropActualHandle(Context &c, const Hit &hit, const QString &phase) {
    auto &d = *c.driver;
    readyMask(c);
    noPointer(c);
    const auto before = c.editor.currentSettings();
    const auto index = selectedIndex(c);
    require(hit.handle >= 0 && hit.handle <= 2 && before.localMasks[index].angle == 0,
            "Post-Crop typed oracle requires an identified unrotated gradient handle");
    const PlainCropInput mapping(c, before);
    require(mapping.declaredPhotoRect && mapping.areaWidth >= 84 && mapping.areaHeight >= 84,
            "Fixed post-Crop controls require the declared expanded photo/input rectangle");
    const auto fromPhoto = mapping.photoInput(c, hit.fraction);
    const auto target =
        mapping.overlayInput(fromPhoto.x() < .5 ? .9 : .1, fromPhoto.y() < .5 ? .9 : .1);
    const auto sourceFrom = mapping.sourceInput(c, hit.fraction);
    const auto clampedSourceTo = mapping.sourceInput(c, target);
    const auto delta = hit.proxy ? mapping.proxySourceDelta(c, hit.fraction, target)
                                 : clampedSourceTo - sourceFrom;
    auto expected = before;
    auto &mask = expected.localMasks[index];
    if (mask.shape == luma::MaskShape::linear) {
        if (hit.handle == 0 || hit.handle == 2) {
            mask.x1 += delta.x();
            mask.y1 += delta.y();
        }
        if (hit.handle == 1 || hit.handle == 2) {
            mask.x2 += delta.x();
            mask.y2 += delta.y();
        }
    } else {
        require(mask.shape == luma::MaskShape::radial,
                "Post-Crop oracle reached another primitive");
        if (hit.handle == 0) {
            mask.x1 += delta.x();
            mask.y1 += delta.y();
        } else if (hit.handle == 1)
            mask.x2 = std::abs(hit.proxy ? mask.x2 + delta.x() : clampedSourceTo.x() - mask.x1);
        else
            mask.y2 = std::abs(hit.proxy ? mask.y2 + delta.y() : clampedSourceTo.y() - mask.y1);
    }
    expected.validate();
    require(expected != before, "Post-Crop target did not create a positive typed geometry edit");
    ReleasePointer pointer{c, d.point("localMaskGestureArea", hit.fraction)};
    pointer.stage = phase;
    pointer.handle = hit.handle;
    d.press("localMaskGestureArea", hit.fraction);
    pointer.down = true;
    require(c.editor.localGestureActive() &&
                d.object("localMaskGestureArea")->property("pressed").toBool() &&
                d.object("localMaskGestureArea")->property("adjusting").toBool(),
            "Predicted post-Crop handle did not accept an actual pointer press");
    exact(c, before, "Pressing a post-Crop handle jumped/changed the full document");
    for (int step = 1; step <= 8; ++step) {
        const auto point = hit.fraction + (target - hit.fraction) * (static_cast<double>(step) / 8);
        pointer.last = d.point("localMaskGestureArea", point);
        d.move("localMaskGestureArea", point);
    }
    pointer.last = d.point("localMaskGestureArea", target);
    d.release("localMaskGestureArea", target);
    pointer.down = false;
    noPointer(c);
    readyMask(c);
    const auto after = c.editor.currentSettings();
    auto normalized = after;
    auto &actual = normalized.localMasks[index];
    require(std::abs(actual.x1 - mask.x1) < 1e-12 && std::abs(actual.y1 - mask.y1) < 1e-12 &&
                std::abs(actual.x2 - mask.x2) < 1e-12 && std::abs(actual.y2 - mask.y2) < 1e-12,
            "Actual post-Crop handle drag disagrees with independent typed/source-delta oracle");
    actual.x1 = mask.x1;
    actual.y1 = mask.y1;
    actual.x2 = mask.x2;
    actual.y2 = mask.y2;
    require(after != before && normalized == expected,
            "Post-Crop handle changed an unrelated complete document field or redrew geometry");
    rendered(c, after);
    durable(c, c.editor.photoID(), after);
    d.click("main.undoButton");
    exact(c, before, "One post-Crop handle drag was not exactly one Undo");
    readyMask(c);
    noPointer(c);
    rendered(c, before);
    durable(c, c.editor.photoID(), before);
    d.click("main.redoButton");
    exact(c, after, "Post-Crop handle Redo did not restore the exact complete document");
    readyMask(c);
    noPointer(c);
    rendered(c, after);
    durable(c, c.editor.photoID(), after);
    d.click("main.undoButton");
    exact(c, before, "Final Undo failed to restore the unchanged Crop/mask baseline");
    readyMask(c);
    noPointer(c);
    rendered(c, before);
    durable(c, c.editor.photoID(), before);
    return {{"phase", phase},
            {"handle", hit.handle},
            {"proxy_by_independent_projection", hit.proxy},
            {"source_from_x", sourceFrom.x()},
            {"source_from_y", sourceFrom.y()},
            {"clamped_source_to_x", clampedSourceTo.x()},
            {"clamped_source_to_y", clampedSourceTo.y()},
            {"source_delta_x", delta.x()},
            {"source_delta_y", delta.y()},
            {"before", documentJSON(before)},
            {"expected", documentJSON(expected)},
            {"actual", documentJSON(after)},
            {"final_restored", documentJSON(c.editor.currentSettings())}};
}
void actualPostCropControls(Context &c, const Document &expected, const QString &phase,
                            QJsonArray &progress) {
    for (int kind = 0; kind <= 1; ++kind) {
        select(c, kind);
        exact(c, expected, "Selecting the post-Crop shape changed the full document");
        for (int handle = 0; handle < 3; ++handle) {
            // Fresh availability after the preceding final Undo: never use stale targets.
            const auto current = availability(c, true);
            progress.append(
                QJsonObject{{"phase", phase},
                            {"kind", kind},
                            {"handle", handle},
                            {"status", "before-actual-press"},
                            {"availability", current.evidence},
                            {"full_document", documentJSON(c.editor.currentSettings())}});
            writeJson(c.root + "/msk023-postcrop-progress.json", {{"controls", progress}});
            const auto found =
                std::find_if(current.hits.begin(), current.hits.end(),
                             [=](const Hit &value) { return value.handle == handle; });
            require(found != current.hits.end(),
                    "A post-Crop typed handle has no actual hit pixel: " + phase + "/" +
                        QString::number(kind) + "/" + QString::number(handle));
            const auto effect = postCropActualHandle(c, *found, phase);
            progress.append(QJsonObject{{"phase", phase},
                                        {"kind", kind},
                                        {"handle", handle},
                                        {"status", "actual-drag-history-pixels-durable-passed"},
                                        {"effect", effect}});
            writeJson(c.root + "/msk023-postcrop-progress.json", {{"controls", progress}});
            exact(c, expected, "Post-Crop handle control failed to preserve its final baseline");
        }
    }
}
void seededMask(Context &c, int kind) {
    auto &d = *c.driver;
    c.addMask(kind);
    overlayOff(c);
    const auto before = c.editor.currentSettings();
    const auto index = selectedIndex(c);
    const QPointF from(.1, .1), to(.22, .22);
    const auto inputFrom = actualInput(c, from), inputTo = actualInput(c, to);
    d.drag("localMaskGestureArea", from, to);
    readyMask(c);
    noPointer(c);
    auto expected = before;
    auto &mask = expected.localMasks[index];
    mask.x1 = inputFrom.x();
    mask.y1 = inputFrom.y();
    mask.x2 = kind == 0 ? inputTo.x() : std::abs(inputTo.x() - inputFrom.x());
    mask.y2 = kind == 0 ? inputTo.y() : std::abs(inputTo.y() - inputFrom.y());
    mask.angle = 0;
    auto normalized = c.editor.currentSettings();
    auto &actual = normalized.localMasks[index];
    require(std::abs(actual.x1 - mask.x1) < 1e-12 && std::abs(actual.y1 - mask.y1) < 1e-12 &&
                std::abs(actual.x2 - mask.x2) < 1e-12 && std::abs(actual.y2 - mask.y2) < 1e-12,
            "Actual initial gradient redraw differs from dispatched input geometry");
    actual.x1 = mask.x1;
    actual.y1 = mask.y1;
    actual.x2 = mask.x2;
    actual.y2 = mask.y2;
    require(normalized == expected, "Gradient setup changed unrelated complete document fields");

    // Give geometry a real rendered effect, instead of testing neutral-mask pixels.
    const auto neutral = c.editor.currentSettings();
    const auto neutralPixels = fixtureReference(c, neutral, c.editor.sourcePath());
    c.reveal("local_exposure");
    d.drag("local_exposure", d.sliderThumb("local_exposure"), {.64, .5});
    expected = neutral;
    expected.localMasks[index].adjustments[0] =
        d.object("local_exposure")->property("value").toDouble();
    require(std::abs(expected.localMasks[index].adjustments[0]) > .5,
            "Gradient fixture has no visible local adjustment");
    exact(c, expected, "Real local Exposure input changed unrelated complete document fields");
    rendered(c, expected);
    require(c.provider->requestImage({}, nullptr, {}) != neutralPixels,
            "Nonneutral local mask did not produce a positive rendered-pixel control");
    initialHitPositiveControls(c); // Real hit/effect/history proof for each of the three handles.
    readyMask(c);
    noPointer(c);
}
void thinPaddingRejectsOrdinaryInput(Context &c, const Document &baseline, const QString &asset) {
    auto &d = *c.driver;
    QJsonArray results;
    // Four distinct tools share the expanded canvas, but only a real gradient
    // proxy may accept input outside the actual photo rectangle.
    for (const int kind : {0, 2, 3, 4}) {
        c.addMask(kind);
        overlayOff(c);
        const auto before = c.editor.currentSettings();
        require(before.localMasks.size() == baseline.localMasks.size() + 1,
                "Padding negative-control fixture did not add exactly one mask");
        const std::array<const char *, 5> kinds{"linear", "radial", "brush", "luminance", "color"};
        const auto state = c.editor.localState();
        require(before.localMasks.back().shape == static_cast<luma::MaskShape>(kind) &&
                    state["id"].toString() == QString::fromStdString(before.localMasks.back().id) &&
                    state["shape"].toString() ==
                        QString::fromLatin1(kinds[static_cast<std::size_t>(kind)]),
                "Padding negative control did not select the intended actual primitive");
        auto expectedAdded = baseline;
        expectedAdded.localMasks.push_back(before.localMasks.back());
        exact(c, expectedAdded, "Adding a padding-control mask changed unrelated document fields");
        rendered(c, before);
        const PlainCropInput mapping(c, before);
        require(mapping.declaredPhotoRect && mapping.photoX > 20 && mapping.areaWidth >= 84,
                "Padding rejection needs a real expanded thin-photo rectangle");
        auto *area = d.item("localMaskGestureArea");
        QJsonArray handles;
        if (kind == 0)
            handles = availability(c, true).evidence;
        // A padding point may legitimately contain a proxy. Choose an actual
        // empty pixel rather than treating a valid handle as ordinary canvas.
        // These four distinct rows are farther than 28px apart in this tall
        // thin-photo fixture, so three 14px handle neighborhoods cannot fill all.
        require(mapping.areaHeight > 112,
                "Thin padding fixture needs four disjoint candidate input neighborhoods");
        QPointF from, point;
        bool found = false;
        for (const double row : {.125, .375, .625, .875}) {
            const QPointF candidate(mapping.photoX / 2 / mapping.areaWidth, row);
            const auto input = mapping.localInput(c, candidate);
            if (input.x() < 0 || input.x() >= mapping.photoX || !insideClips(c, area, input))
                continue;
            bool empty = true;
            for (const auto &entry : handles) {
                const auto handle = entry.toObject();
                const double dx = handle["painted_hit_x"].toDouble() * area->width() - input.x();
                const double dy = handle["painted_hit_y"].toDouble() * area->height() - input.y();
                if (dx * dx + dy * dy < 14 * 14) {
                    empty = false;
                    break;
                }
            }
            if (empty) {
                from = candidate;
                point = input;
                found = true;
                break;
            }
        }
        require(found, "Empty-padding negative control cannot find a real non-handle input pixel");
        const QPointF to(from.x(), from.y() + 8 / mapping.areaHeight);
        bool samplingObserved = false, gestureObserved = false;
        QObject observer;
        QObject::connect(&c.editor, &EditorController::stateChanged, &observer, [&] {
            samplingObserved = samplingObserved || c.editor.localSampling();
            gestureObserved = gestureObserved || c.editor.localGestureActive();
        });
        ReleasePointer pointer{c, d.point("localMaskGestureArea", from)};
        d.press("localMaskGestureArea", from);
        pointer.down = true;
        exact(c, before, "Ordinary padding press changed its full document");
        require(!c.editor.localGestureActive() && !c.editor.localSampling() &&
                    !d.object("localMaskGestureArea")->property("adjusting").toBool(),
                "Ordinary padding press started a transient edit or sample without changing the "
                "document");
        pointer.last = d.point("localMaskGestureArea", to);
        d.move("localMaskGestureArea", to);
        d.release("localMaskGestureArea", to);
        pointer.down = false;
        d.frame();
        noPointer(c);
        require(!samplingObserved && !gestureObserved && !c.editor.localSampling(),
                "Ordinary padding input started a brush/gradient edit or range sample");
        exact(c, before, "Ordinary padding motion edited outside the photo rectangle");
        rendered(c, before);
        durable(c, asset, before);
        d.click("main.undoButton");
        exact(c, baseline, "Rejected padding input added an unexpected Undo entry");
        d.click("main.redoButton");
        exact(c, before, "Padding fixture Redo did not restore exactly its added mask");
        d.click("main.undoButton");
        exact(c, baseline, "Padding negative control did not restore the prior full document");
        rendered(c, baseline);
        durable(c, asset, baseline);
        results.append(QJsonObject{{"kind", kind},
                                   {"sampling_observed", samplingObserved},
                                   {"gesture_observed", gestureObserved},
                                   {"outside_photo_x", point.x()},
                                   {"photo_left", mapping.photoX}});
    }
    json(c.root + "/msk023-thin-padding.json", {{"tools", results}});
}
} // namespace

bool maskHandleCropLossCandidate(Context &c, const QString &id) {
    const bool thin = id == "CANDIDATE-MSK023-CROP-LOSS-THIN";
    if (id != "CANDIDATE-MSK023-CROP-LOSS" && !thin)
        return false;
    auto &d = *c.driver;
    MaskReadyTrace readyTrace(c);
    c.import(1);
    d.doubleClick(c.card(0));
    c.loaded();
    const auto asset = c.editor.photoID(), source = c.editor.sourcePath();
    const auto originalHash = digest(source);
    const auto record = luma::Catalog(c.catalogFile.toStdString()).assetByID(asset.toStdString());
    require(record && record->metadata.width == 256 && record->metadata.height == 192 &&
                record->metadata.orientation == 0 &&
                c.editor.currentSettings().crop == luma::CropSettings{} &&
                c.editor.currentSettings().transform == luma::TransformSettings{} &&
                !c.window->property("libraryMode").toBool(),
            "Crop-loss independent oracle requires the known upright untransformed256x192 source");
    seededMask(c, 0);
    seededMask(c, 1);
    const auto masks = c.editor.currentSettings();
    require(masks.localMasks.size() == 2 && masks.localMasks[0].shape == luma::MaskShape::linear &&
                masks.localMasks[1].shape == luma::MaskShape::radial,
            "Crop-loss fixture lacks the two actual gradient shapes");
    durable(c, asset, masks);

    c.reveal("cropTool");
    d.click("cropTool");
    d.wait([&] { return c.editor.cropMode() && c.editor.cropCanvasReady(); },
           "Actual Crop canvas ready before inset grip input");
    auto *cropArea = d.item("cropDragArea");
    const QPointF from(.001, .001), to(thin ? .99 : .55, .5);
    const auto normalizedInput = [&](const QPointF &fraction) {
        const auto point = cropArea->mapFromScene(d.point("cropDragArea", fraction));
        return QPointF(std::clamp(point.x() / cropArea->width(), 0., 1.),
                       std::clamp(point.y() / cropArea->height(), 0., 1.));
    };
    const auto inputFrom = normalizedInput(from), inputTo = normalizedInput(to);
    ReleasePointer pointer{c, d.point("cropDragArea", from)};
    pointer.stage = thin ? "thin-crop-grip" : "normal-crop-grip";
    d.press("cropDragArea", from);
    pointer.down = true;
    require(d.object("cropOverlay")->property("dragMode").toString() == "lt",
            "Actual inset input did not acquire the existing top-left Crop grip");
    for (int step = 1; step <= 8; ++step) {
        const auto point = from + (to - from) * (static_cast<double>(step) / 8);
        pointer.last = d.point("cropDragArea", point);
        d.move("cropDragArea", point);
    }
    d.release("cropDragArea", to);
    pointer.down = false;
    noCropPointer(c);
    auto expected = masks;
    expected.crop.x = inputTo.x() - inputFrom.x();
    expected.crop.y = inputTo.y() - inputFrom.y();
    expected.crop.width = 1 - expected.crop.x;
    expected.crop.height = 1 - expected.crop.y;
    const auto cropped = c.editor.currentSettings();
    auto normalized = cropped;
    require(std::abs(cropped.crop.x - expected.crop.x) < 1e-12 &&
                std::abs(cropped.crop.y - expected.crop.y) < 1e-12 &&
                std::abs(cropped.crop.width - expected.crop.width) < 1e-12 &&
                std::abs(cropped.crop.height - expected.crop.height) < 1e-12,
            "Actual Crop grip differs from the independent dispatched-input crop oracle");
    normalized.crop = expected.crop;
    require(normalized == expected && cropped.crop.x > .5 && cropped.crop.y > .45,
            "Crop inset changed masks/unrelated fields or did not exclude their original region");
    c.reveal("cropTool");
    d.click("cropTool");
    require(!c.editor.cropMode() && !d.item("cropOverlay")->isVisible(),
            "Actual Crop Done did not leave the geometry tool");
    noCropPointer(c);

    // Collect BOTH shapes and preserve full data evidence before the strict failure.
    QJsonArray checks;
    bool everyShapeHasHandle = true;
    for (int kind = 0; kind <= 1; ++kind) {
        select(c, kind);
        const auto result = availability(c, true);
        everyShapeHasHandle = everyShapeHasHandle && !result.hits.empty();
        checks.append(QJsonObject{
            {"kind", kind},
            {"mask_id",
             QString::fromStdString(cropped.localMasks[static_cast<std::size_t>(kind)].id)},
            {"reachable_handles", static_cast<qint64>(result.hits.size())},
            {"handles", result.evidence}});
        exact(c, cropped, "Actual mask re-entry/availability read changed the full document");
        rendered(c, cropped);
        d.screenshot(c.root + QString("/msk023-crop-mask-%1.png").arg(kind));
    }
    durable(c, asset, cropped);
    require(c.editor.photoID() == asset && c.editor.sourcePath() == source &&
                digest(source) == originalHash,
            "Crop-loss reproduction changed original bytes or source identity");
    const luma::Catalog catalog(c.catalogFile.toStdString());
    const auto persisted = catalog.assetByID(asset.toStdString());
    require(catalog.libraryIDs({}) == std::vector<std::string>{asset.toStdString()} && persisted &&
                persisted->edits == cropped &&
                QString::fromStdString(persisted->path.string()) == source,
            "Crop-loss reproduction changed durable membership/source/full document");
    c.checkData();
    json(c.root + "/msk023-crop-handles.json",
         {{"candidate", id},
          {"scope", "actual Crop/Done and fresh mask re-entry; no Transform/Undo/redraw recovery"},
          {"expected_uuid", asset},
          {"source", source},
          {"source_sha256", QString::fromLatin1(originalHash.toHex())},
          {"crop_x", cropped.crop.x},
          {"crop_y", cropped.crop.y},
          {"crop_width", cropped.crop.width},
          {"crop_height", cropped.crop.height},
          {"masks", checks},
          {"all_shapes_have_real_hit_target", everyShapeHasHandle}});
    // Deliberately BEFORE any Transform/Undo/reset or attempted rescue. A zero-target
    // mask cannot accept a real handle press in the current clipped input surface.
    require(
        everyShapeHasHandle,
        "After actual Crop/Done and mask re-entry a selected gradient has no hit-testable handle");
    if (thin) {
        const PlainCropInput mapping(c, cropped);
        require(mapping.photoWidth < 48 && mapping.areaWidth >= 84 &&
                    mapping.areaWidth > mapping.photoWidth && mapping.photoX > 0,
                "Thin-Crop fixture did not exercise the expanded proxy input canvas");
    }
    QJsonArray progress;
    actualPostCropControls(c, cropped, thin ? "thin-base" : "normal-base", progress);
    auto finalDocument = cropped;
    const std::array<std::pair<const char *, const char *>, 3> flips{
        {{"cropFlipHorizontal", "H"}, {"cropFlipVertical", "HV"}, {"cropFlipHorizontal", "V"}}};
    for (const auto &[control, phase] : flips) {
        c.reveal(QString::fromLatin1(control));
        const auto before = finalDocument;
        d.click(QString::fromLatin1(control));
        if (QString::fromLatin1(control) == "cropFlipHorizontal")
            finalDocument.crop.flipHorizontal = !before.crop.flipHorizontal;
        else
            finalDocument.crop.flipVertical = !before.crop.flipVertical;
        exact(c, finalDocument, "Actual Flip input changed more than the one expected Crop flag");
        rendered(c, finalDocument);
        durable(c, asset, finalDocument);
        actualPostCropControls(c, finalDocument, QString::fromLatin1(phase), progress);
    }
    if (thin)
        thinPaddingRejectsOrdinaryInput(c, finalDocument, asset);
    exact(c, finalDocument, "Final post-Crop/Flip controls changed the complete expected baseline");
    durable(c, asset, finalDocument);
    const luma::Catalog finalCatalog(c.catalogFile.toStdString());
    const auto finalRecord = finalCatalog.assetByID(asset.toStdString());
    require(c.editor.photoID() == asset && c.editor.sourcePath() == source &&
                digest(source) == originalHash &&
                finalCatalog.libraryIDs({}) == std::vector<std::string>{asset.toStdString()} &&
                finalRecord && finalRecord->edits == finalDocument &&
                finalRecord->checksum == record->checksum &&
                QString::fromStdString(finalRecord->path.string()) == source,
            "Post-Crop input/history/Flip checks changed source identity or final durable state");
    c.checkData();
    return true;
}
} // namespace ux
