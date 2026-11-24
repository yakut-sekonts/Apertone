#include "apps/desktop/BatchController.h"
#include "apps/desktop/EditorController.h"
#include "apps/desktop/LibraryAcceptance.h"
#include "apps/desktop/LibraryModel.h"
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickItem>
#include <QScreen>
#include <QTimer>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <windows.h>

namespace {
void require(bool result, const char *reason) {
    if (!result)
        throw std::runtime_error(reason);
}

void waitFor(const std::function<bool()> &condition, const char *reason) {
    QElapsedTimer deadline;
    deadline.start();
    do {
        QEventLoop loop;
        QTimer::singleShot(30, &loop, &QEventLoop::quit);
        loop.exec();
        if (condition())
            return;
    } while (deadline.elapsed() < 15000);
    throw std::runtime_error(reason);
}

QQuickItem *findItem(QQuickItem *root, const QString &name) {
    if (root->objectName() == name)
        return root;
    for (auto *child : root->childItems())
        if (auto *item = findItem(child, name))
            return item;
    return nullptr;
}

void press(QQuickWindow &window, unsigned key) {
    const auto handle = reinterpret_cast<HWND>(window.winId());
    const auto scan = static_cast<LPARAM>(MapVirtualKeyW(key, MAPVK_VK_TO_VSC)) << 16;
    require(PostMessageW(handle, WM_KEYDOWN, key, scan | 1) &&
                PostMessageW(handle, WM_KEYUP, key, scan | 0xC0000001L),
            "Windows keyboard input failed");
}

QRectF bounds(QQuickItem *item) {
    return item->mapRectToScene(item->boundingRect());
}

QJsonArray rectangle(const QRect &rect) {
    return {rect.x(), rect.y(), rect.width(), rect.height()};
}
} // namespace

void startWindowsWorkspaceAcceptance(QGuiApplication &app, QQuickWindow &window,
                                     EditorController &editor, LibraryModel &library,
                                     BatchController &batch, const QString &evidence) {
    QTimer::singleShot(0, &app, [&app, &window, &editor, &library, &batch, evidence] {
        const QDir output(evidence);
        try {
            require(QGuiApplication::platformName() == "windows" && output.exists(),
                    "Native windows QPA and evidence directory are required");
            auto item = [&](const char *name) {
                auto *found = findItem(window.contentItem(), QString::fromLatin1(name));
                if (!found)
                    throw std::runtime_error(std::string("Missing workspace item ") + name);
                return found;
            };
            auto capture = [&](const char *file) {
                require(window.grabWindow().save(output.filePath(QString::fromLatin1(file))),
                        "Cannot save native workspace screenshot");
            };
            auto panel = [&](const char *name, unsigned key, bool show) {
                if (item(name)->isVisible() != show) {
                    press(window, key);
                    waitFor([&] { return item(name)->isVisible() == show; },
                            "Workspace panel keyboard toggle failed");
                }
            };
            auto *screen = window.screen();
            window.requestActivate();
            waitFor(
                [&] {
                    return window.isExposed() && window.isActive() && library.count() == 1 &&
                           !library.loading() && !library.working() && !library.selecting() &&
                           !library.reviewPhotoAt(0).isEmpty() &&
                           window.visibility() == QWindow::Windowed;
                },
                "Native window or isolated one-photo catalog did not become ready");
            const auto initial = window.frameGeometry();
            const auto available = screen->availableGeometry().adjusted(-2, -2, 2, 2);
            require(available.contains(initial), "Initial window extends beyond the work area");
            const auto initialDpr = window.devicePixelRatio();
            press(window, 'E');
            auto *loupe = item("library.loupe");
            waitFor(
                [&] {
                    return loupe->isVisible() && editor.comparison().value("ready").toBool() &&
                           item("library.loupe.preview.image")->property("status").toInt() == 1;
                },
                "E did not display the DNG in Library Loupe");
            const auto photo = library.reviewPhotoAt(0);
            const auto loupeBounds = bounds(loupe);
            const auto footer = bounds(item("main.footer"));
            const auto strip = bounds(item("library.filmstrip"));
            require(loupeBounds.width() >= 80 && loupeBounds.height() >= 40 &&
                        loupeBounds.bottom() <= footer.top() + 1 &&
                        strip.top() >= footer.bottom() - 1 && strip.height() > 0 &&
                        strip.bottom() <= window.height() + 1,
                    "Loupe, footer and filmstrip do not fit in vertical order");
            panel("library.leftPanel", VK_F7, true);
            waitFor(
                [&] { return item("library.navigatorPreview")->property("status").toInt() == 1; },
                "Navigator did not load the selected photo");
            if (window.width() >= 680)
                require(bounds(item("library.leftPanel")).right() <= bounds(loupe).left() + 1,
                        "Library navigator overlaps Loupe");
            panel("library.rightPanel", VK_F8, true);
            waitFor(
                [&] { return !item("library.histogram")->property("source").toUrl().isEmpty(); },
                "Inspector histogram has no selected photo");
            if (window.width() >= 680)
                require(bounds(loupe).right() <= bounds(item("library.rightPanel")).left() + 1,
                        "Library inspector overlaps Loupe");
            capture("library.png");
            auto *increase = item("library.quickExposureUp");
            require(increase->isVisible() && increase->isEnabled(),
                    "Quick Develop action unavailable");
            increase->forceActiveFocus(Qt::TabFocusReason);
            waitFor(
                [&] {
                    const auto center = increase->mapToScene(increase->boundingRect().center());
                    for (auto *ancestor = increase->parentItem(); ancestor;
                         ancestor = ancestor->parentItem())
                        if (ancestor->clip() && !bounds(ancestor).contains(center))
                            return false;
                    return increase->hasActiveFocus() && window.contentItem()->contains(center);
                },
                "Quick Develop exposure cannot be reached in the inspector");
            press(window, VK_SPACE);
            waitFor([&] { return batch.canUndo(); }, "Quick Develop exposure action failed");
            if (window.width() < 820)
                panel("library.rightPanel", VK_F8, false);
            press(window, 'D');
            waitFor(
                [&] {
                    return !window.property("libraryMode").toBool() &&
                           window.property("comparisonAvailable").toBool() &&
                           editor.photoID() == photo &&
                           std::abs(editor.exposure() - 1.0 / 3.0) < .001;
                },
                "Develop did not open the Loupe photo with the Quick Develop adjustment");
            press(window, 'Y');
            auto *before = item("develop.comparison.before");
            auto *after = item("develop.comparison.after");
            waitFor(
                [&] {
                    return window.property("beforeAfterActive").toBool() &&
                           editor.comparison().value("ready").toBool() &&
                           item("develop.comparison.before.image")->property("status").toInt() ==
                               1 &&
                           item("develop.comparison.after.image")->property("status").toInt() == 1;
                },
                "Y did not load both before/after views");
            auto checkComparison = [&] {
                auto a = bounds(before);
                auto b = bounds(after);
                require(before->isVisible() && after->isVisible() && a.width() > 30 &&
                            a.height() > 20 && a.right() <= b.left() + 1 &&
                            std::abs(a.width() - b.width()) <= 2 &&
                            std::abs(a.top() - b.top()) <= 1 &&
                            window.contentItem()
                                ->boundingRect()
                                .adjusted(-1, -1, 1, 1)
                                .contains(a.united(b)),
                        "Before/after panes are clipped, overlapping or misaligned");
            };
            checkComparison();
            capture("develop.png");
            auto fullScreen = [&] {
                press(window, VK_F11);
                waitFor(
                    [&] {
                        return window.visibility() == QWindow::FullScreen &&
                               window.geometry() == screen->geometry();
                    },
                    "F11 did not fill the native display");
                checkComparison();
            };
            fullScreen();
            const auto full = window.geometry();
            capture("fullscreen.png");
            press(window, VK_F11);
            waitFor(
                [&] {
                    return window.visibility() == QWindow::Windowed &&
                           window.frameGeometry() == initial;
                },
                "F11 did not restore the normal window");
            window.showMaximized();
            waitFor([&] { return window.visibility() == QWindow::Maximized; }, "Maximize failed");
            fullScreen();
            press(window, VK_F11);
            waitFor([&] { return window.visibility() == QWindow::Maximized; },
                    "F11 did not restore the maximized window");
            checkComparison();
            const auto maximized = window.geometry();
            // Exercise single-panel navigation even on a runner with a large native desktop.
            window.showNormal();
            waitFor([&] { return window.visibility() == QWindow::Windowed; },
                    "Restore from maximize failed");
            window.resize(window.size().boundedTo(QSize(650, 380)));
            waitFor([&] { return window.width() < 680 && !window.property("panelOnly").toBool(); },
                    "Narrow window did not start with a usable canvas");
            press(window, 'E');
            waitFor(
                [&] { return loupe->isVisible() && editor.comparison().value("ready").toBool(); },
                "Narrow window lost Library Loupe");
            panel("library.rightPanel", VK_F8, true);
            require(!item("library.leftPanel")->isVisible() && !loupe->isVisible(),
                    "Narrow inspector overlaps navigation or canvas");
            panel("library.leftPanel", VK_F7, true);
            require(!item("library.rightPanel")->isVisible() && !loupe->isVisible(),
                    "Narrow navigator overlaps inspector or canvas");
            panel("library.leftPanel", VK_F7, false);
            waitFor([&] { return loupe->isVisible() && bounds(loupe).width() > 80; },
                    "Closing narrow navigator did not restore the canvas");
            auto *importButton = item("library.importButton");
            auto *exportButton = item("exportButton");
            auto footerBounds = bounds(item("main.footer"));
            require(importButton->isVisible() && importButton->isEnabled() &&
                        exportButton->isVisible() && exportButton->isEnabled() &&
                        bounds(importButton).width() > 0 && bounds(exportButton).width() > 0 &&
                        footerBounds.contains(bounds(importButton)) &&
                        footerBounds.contains(bounds(exportButton)) &&
                        bounds(importButton).right() <= bounds(exportButton).left() + 1,
                    "Compact Library footer does not expose separate Import and Export actions");
            capture("compact-loupe.png");
            QJsonObject result{{"platform", QGuiApplication::platformName()},
                               {"dpr", initialDpr},
                               {"available", rectangle(screen->availableGeometry())},
                               {"windowed", rectangle(initial)},
                               {"fullscreen", rectangle(full)},
                               {"restoredMaximized", rectangle(maximized)},
                               {"compact", rectangle(window.frameGeometry())}};
            QFile report(output.filePath(QStringLiteral("geometry.json")));
            const auto bytes = QJsonDocument(result).toJson();
            require(report.open(QIODevice::WriteOnly) && report.write(bytes) == bytes.size() &&
                        report.flush(),
                    "Cannot write workspace geometry evidence");
            app.exit(0);
        } catch (const std::exception &failure) {
            window.grabWindow().save(output.filePath(QStringLiteral("failure.png")));
            QFile error(output.filePath(QStringLiteral("failure.txt")));
            if (error.open(QIODevice::WriteOnly))
                error.write(failure.what());
            std::cerr << "Windows workspace acceptance: " << failure.what() << '\n';
            app.exit(1);
        }
    });
}
