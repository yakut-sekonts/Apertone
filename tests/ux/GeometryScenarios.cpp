#include "tests/ux/UxContext.h"
#include <cmath>

namespace ux {
namespace {
void compareDevelopEdits(Context &c) {
    auto &d = *c.driver;
    d.resize({1320, 840});
    c.openFirst();
    const auto sourceSize = c.editor.previewPixelSize();
    c.reveal("cropAspect");
    d.choose("cropAspect", sourceSize.width() == sourceSize.height() ? 5 : 2);
    c.saved();
    d.key(Qt::Key_Y);
    auto *view = d.item("develop.comparison");
    auto *before = d.item("develop.comparison.before");
    auto *after = d.item("develop.comparison.after");
    auto *wipe = d.item("develop.comparison.wipe");
    d.wait([&] { return view->property("ready").toBool(); }, "inline Before/After ready");
    require(!c.window->property("workspacePopupOpen").toBool() && before->isVisible() &&
                after->isVisible() && !wipe->isVisible() && before->width() > 100 &&
                after->width() > 100 &&
                before->mapToScene({before->width(), 0}).x() <= after->mapToScene({0, 0}).x(),
            "Y must show two separate, adjacent photo panes in the Develop workspace");
    for (const auto *name : {"develop.navigator", "develop.adjustmentsPane", "develop.filmstrip"})
        require(d.item(name)->isVisible() && d.item(name)->isEnabled(),
                QString(name) + " must remain available while comparing");

    const auto pixels = [&](const char *id) { return c.provider->requestImage(id, nullptr, {}); };
    const auto original = pixels("compare-0");
    const auto firstPreview = pixels("current");
    require(!original.isNull() && !firstPreview.isNull(), "Comparison must have image pixels");
    const char *slider = "develop.exposureSlider";
    c.reveal(slider);
    d.drag(slider, d.sliderThumb(slider), {.68, .5});
    d.wait([&] { return !c.editor.saving() && pixels("current") != firstPreview; },
           "Fit shows the edited preview");
    require(!c.editor.comparison()["fullResolution"].toBool() && original == pixels("compare-0") &&
                after->property("source").toString() == c.editor.previewUrl(),
            "Fit must display the live editor preview next to the unchanged original");
    d.screenshot(c.root + "/compare-fit.png");

    d.click("develop.actualPixels");
    d.wait(
        [&] {
            return c.editor.comparison()["fullResolution"].toBool() &&
                   view->property("ready").toBool() && !c.editor.comparison()["busy"].toBool();
        },
        "100% Before/After ready");
    const auto fullBefore = pixels("compare-0");
    const auto fullAfter = pixels("compare-1");
    const auto previousSource = after->property("source").toString();
    require(!fullAfter.isNull() && before->property("pixelScale").toBool() &&
                after->property("pixelScale").toBool(),
            "100% panes must show full-resolution pixels");
    d.drag(slider, d.sliderThumb(slider), {.32, .5});
    d.wait(
        [&] {
            return after->property("source").toString() != previousSource &&
                   !c.editor.comparison()["busy"].toBool();
        },
        "100% After refreshes on save");
    require(after->property("source") == c.editor.comparison()["right"] &&
                pixels("compare-0") == fullBefore && pixels("compare-1") != fullAfter,
            "Saving a new exposure must change only the After image at 100%");
    d.click("develop.splitComparison");
    d.wait([&] { return wipe->isVisible() && !after->isVisible() && !before->isVisible(); },
           "two-up switches to the wipe view");
    auto *leftImage = d.item("develop.comparison.wipe.beforePane.image");
    auto *rightImage = d.item("develop.comparison.wipe.afterImage");
    const auto checkShape = [](QQuickItem *item, const QImage &frame) {
        return item->width() > 0 && item->height() > 0 &&
               std::abs(item->width() / item->height() - double(frame.width()) / frame.height()) <
                   .001;
    };
    require(fullBefore.size() != fullAfter.size(), "Crop must produce distinct comparison sizes");
    d.wait([&] { return checkShape(leftImage, fullBefore) && checkShape(rightImage, fullAfter); },
           "wipe preserves the aspect ratio of the cropped edit and the full original");
    require(
        !leftImage->property("smooth").toBool() && !rightImage->property("smooth").toBool() &&
            std::abs(leftImage->width() * c.window->devicePixelRatio() - fullBefore.width()) < 1 &&
            std::abs(rightImage->width() * c.window->devicePixelRatio() - fullAfter.width()) < 1,
        "100% must map one image pixel to one physical pixel for both wipe frames");
    d.drag("develop.comparison.wipe", {.5, .55}, {.75, .55});
    require(wipe->property("divider").toReal() > .65, "Wipe divider must follow the mouse");
    d.screenshot(c.root + "/compare-wipe.png");
    d.click("develop.fit");
    d.wait(
        [&] {
            return view->property("ready").toBool() &&
                   !c.editor.comparison()["fullResolution"].toBool() &&
                   checkShape(leftImage, fullBefore) && checkShape(rightImage, fullAfter);
        },
        "wipe Fit preserves both aspect ratios");
    d.key(Qt::Key_Escape);
    d.wait(
        [&] { return !view->isVisible() && c.editor.comparison()["ids"].toStringList().isEmpty(); },
        "Escape returns to Develop");
    d.key(Qt::Key_Y);
    d.wait([&] { return view->isVisible(); }, "comparison reopened");
    d.click("main.libraryButton");
    d.wait(
        [&] {
            return c.window->property("libraryMode").toBool() && !view->isVisible() &&
                   !c.editor.comparison()["busy"].toBool() &&
                   c.editor.comparison()["ids"].toStringList().isEmpty();
        },
        "Library releases the comparison");
}
} // namespace
bool geometryScenario(Context &c, const QString &id) {
    if (id == "UX-PRV-016") {
        compareDevelopEdits(c);
        return true;
    }
    if (id == "UX-DSC-007") {
        c.openFirst();
        require(c.localization->selectLanguage("ru"), "Switch Develop to long localized labels");
        auto &d = *c.driver;
        d.resize({1110, 700});
        auto *pane = d.item("develop.adjustmentsPane");
        auto *viewport = d.item("develop.adjustmentsScroll");
        for (const int width : {250, 380, 250}) {
            const double initial = pane->width();
            d.dragBetween("develop.adjustmentsPane", {-1 / initial, .5}, "develop.adjustmentsPane",
                          {(initial - width - 1) / initial, .5});
            d.wait([&] { return std::abs(pane->width() - width) <= 1; },
                   "SplitView width reaches requested bound");
            for (const auto *selector :
                 {"develop.exposureSlider", "develop.saturationSlider", "cropAngleSlider",
                  "develop.transform.horizontalSlider", "sharpen.amountSlider",
                  "noise.colorSmoothnessSlider", "cropFlipVertical", "detailDerived.deblur"}) {
                c.reveal(selector);
                auto *control = d.item(selector);
                const auto left = control->mapToItem(viewport, QPointF()).x();
                const auto right = control->mapToItem(viewport, QPointF(control->width(), 0)).x();
                d.log({{"action", "narrow_panel_control"},
                       {"id", selector},
                       {"pane_width", pane->width()},
                       {"viewport_width", viewport->width()},
                       {"left", left},
                       {"right", right}});
                require(left >= -.5 && right <= viewport->width() + .5,
                        "Adjustment control is clipped horizontally: " + QString(selector));
                if (control->objectName() == "detailDerived.deblur") {
                    const auto *caption = control->property("contentItem").value<QQuickItem *>();
                    if (caption && !caption->property("paintedWidth").isValid())
                        caption = caption->findChild<QQuickItem *>("label");
                    require(
                        caption && caption->property("paintedWidth").isValid() &&
                            !caption->property("truncated").toBool() &&
                            caption->property("paintedWidth").toReal() <= caption->width() + 1 &&
                            caption->property("paintedHeight").toReal() <= caption->height() + 1,
                        "The derived DNG action must display its full localized label");
                }
            }
        }
        return true;
    }
    if (id != "UX-CRP-001" && id != "UX-CRP-002" && id != "UX-UND-006")
        return false;
    auto &d = *c.driver;
    c.openFirst();
    c.reveal("cropTool");
    d.click("cropTool");
    d.wait([&] { return c.editor.cropMode() && c.editor.cropCanvasReady(); }, "Crop canvas ready");
    const QPointF grips[]{{0, 0}, {.5, 0}, {1, 0}, {0, .5}, {1, .5}, {0, 1}, {.5, 1}, {1, 1}};
    const int count = id == "UX-CRP-001" ? 8 : 1;
    for (int i = 0; i < count; ++i) {
        // An inset frame is a precondition; the acceptance input is a real drag.
        c.editor.setCropRect(.2, .2, .6, .6);
        c.saved();
        d.wait([&] { return c.editor.cropCanvasReady(); }, "inset Crop fixture ready");
        d.frame();
        const auto before = c.editor.currentSettings();
        const bool move = id == "UX-CRP-002";
        const auto start =
            move ? QPointF(.5, .5) : QPointF(.2 + .6 * grips[i].x(), .2 + .6 * grips[i].y());
        const auto finish =
            move ? QPointF(.6, .6)
                 : start + QPointF((.5 - grips[i].x()) * .2, (.5 - grips[i].y()) * .2);
        // The displayed frame is snapped to source pixels, and Qt delivers integer
        // window coordinates even when the fitted photo spans fractional logical pixels.
        const auto frame = c.editor.cropState();
        auto expected = before.crop;
        expected.x = frame["x"].toDouble();
        expected.y = frame["y"].toDouble();
        expected.width = frame["width"].toDouble();
        expected.height = frame["height"].toDouble();
        auto *area = d.item("cropDragArea");
        const auto delta = area->mapFromScene(d.point("cropDragArea", finish)) -
                           area->mapFromScene(d.point("cropDragArea", start));
        const double dx = delta.x() / area->width();
        const double dy = delta.y() / area->height();
        d.press("cropDragArea", start);
        for (int step = 1; step <= 12; ++step) {
            d.move("cropDragArea", start + (finish - start) * (static_cast<double>(step) / 12));
            d.frame();
        }
        d.release("cropDragArea", finish);
        d.wait([&] { return c.editor.cropCanvasReady(); }, "Crop drag final render");
        const auto after = c.editor.currentSettings();
        if (move || grips[i].x() == 0)
            expected.x += dx;
        if (move || grips[i].y() == 0)
            expected.y += dy;
        if (!move) {
            if (grips[i].x() == 0)
                expected.width -= dx;
            if (grips[i].x() == 1)
                expected.width += dx;
            if (grips[i].y() == 0)
                expected.height -= dy;
            if (grips[i].y() == 1)
                expected.height += dy;
        }
        d.log({{"action", "crop_drag_oracle"},
               {"grip", i},
               {"actual_x", after.crop.x},
               {"expected_x", expected.x},
               {"actual_y", after.crop.y},
               {"expected_y", expected.y},
               {"actual_width", after.crop.width},
               {"expected_width", expected.width}});
        require(std::abs(after.crop.x - expected.x) < .005 &&
                    std::abs(after.crop.y - expected.y) < .005 &&
                    std::abs(after.crop.width - expected.width) < .005 &&
                    std::abs(after.crop.height - expected.height) < .005,
                "Crop frame did not follow the complete pointer drag");
        d.click("main.undoButton");
        require(c.editor.currentSettings() == before, "Crop drag was not exactly one Undo");
        d.click("main.redoButton");
        require(c.editor.currentSettings() == after, "Crop Redo changed final geometry");
    }
    return true;
}
} // namespace ux
