#include "tests/ux/UxPixels.h"
#include "apps/shared/PhotoDecoder.h"
#include "core/render/CpuRenderer.h"
#include "platform/macos/DisplayProfile.h"
#include "tests/ux/UxContext.h"
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QSignalSpy>
#include <algorithm>
#include <cmath>
namespace ux {
void recordPreviewDifference(Context &context, const QImage &expected, const QString &reason) {
    const auto actual = context.provider->requestImage({}, nullptr, {});
    require(expected.save(context.root + "/expected-preview.png") &&
                actual.save(context.root + "/actual-preview.png"),
            "Cannot preserve failed preview pixel evidence");
    QJsonObject evidence{{"reason", reason},
                         {"actual_width", actual.width()},
                         {"actual_height", actual.height()},
                         {"expected_width", expected.width()},
                         {"expected_height", expected.height()},
                         {"asset_id", context.editor.photoID()},
                         {"source", context.editor.sourcePath()},
                         {"preview_url", context.editor.previewUrl()}};
    if (!expected.isNull() && actual.size() == expected.size()) {
        const auto a = actual.convertToFormat(QImage::Format_RGBA8888);
        const auto b = expected.convertToFormat(QImage::Format_RGBA8888);
        qint64 differences = 0, total = 0;
        int maximum = 0;
        for (int y = 0; y < a.height(); ++y)
            for (int x = 0; x < a.width() * 4; ++x) {
                const int difference =
                    std::abs(int(a.constScanLine(y)[x]) - int(b.constScanLine(y)[x]));
                differences += difference != 0;
                total += difference;
                maximum = std::max(maximum, difference);
            }
        evidence.insert("different_channels", differences);
        evidence.insert("maximum_channel_difference", maximum);
        evidence.insert("mean_channel_difference", double(total) / (a.width() * a.height() * 4.));
    }
    writeJson(context.root + "/preview-pixel-difference.json", evidence);
}
QSharedPointer<QQuickItemGrabResult> readyItemGrab(Context &context, QQuickItem &item,
                                                   const QString &reason, int timeout) {
    require(context.unfinishedItemGrab.isNull(),
            "Cannot start another Image-node readback after an unfinished failure");
    require(context.window && item.window() == context.window &&
                context.engine.rootObjects().contains(context.window),
            "Image-node readback requires the engine-owned main test window");
    const auto capture = item.grabToImage();
    require(!capture.isNull(), "Cannot start Image-node readback: " + reason);
    try {
        QSignalSpy ready(capture.data(), &QQuickItemGrabResult::ready);
        require(ready.isValid(), "Cannot observe actual Image-node readback completion");
        context.driver->wait([&] { return ready.count() > 0; }, reason, timeout);
        require(ready.count() == 1 && !capture->image().isNull(),
                "Image-node readback completed without one valid image: " + reason);
    } catch (...) {
        // Existing shared ownership copies without allocating another keeper.
        // Member order releases this only after the engine's windows are gone.
        context.unfinishedItemGrab = capture;
        throw;
    }
    return capture;
}
QImage fixtureReference(Context &context, luma::DetailDiagnostic diagnostic) {
    return fixtureReference(context, context.editor.currentSettings(), context.editor.sourcePath(),
                            diagnostic);
}
QImage fixtureReference(Context &context, const luma::EditDocument &document,
                        const QString &sourcePath, luma::DetailDiagnostic diagnostic) {
    const auto decoded = luma::PhotoDecoder().decode(sourcePath.toStdString());
    require(decoded.image.width <= 512 && decoded.image.height <= 512,
            "Pixel oracle requires a small full-resolution public fixture");
    const auto profile = QGuiApplication::platformName() == "cocoa"
                             ? luma::macos::displayProfile(context.window->winId())
                             : std::vector<std::uint8_t>{};
    const auto pixels =
        luma::renderCPU(decoded.image, document, decoded.metadata.orientation, profile, {},
                        decoded.calibration, diagnostic, 1, decoded.metadata);
    return QImage(pixels.rgba.data(), pixels.width, pixels.height, pixels.width * 4,
                  QImage::Format_RGBA8888)
        .copy();
}
} // namespace ux
