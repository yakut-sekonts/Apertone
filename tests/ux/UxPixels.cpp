#include "tests/ux/UxPixels.h"
#include "apps/shared/PhotoDecoder.h"
#include "core/render/CpuRenderer.h"
#include "platform/macos/DisplayProfile.h"
#include "tests/ux/UxContext.h"
#include <QGuiApplication>
namespace ux {
QImage fixtureReference(Context &context, luma::DetailDiagnostic diagnostic) {
    const auto decoded = luma::PhotoDecoder().decode(context.editor.sourcePath().toStdString());
    require(decoded.image.width <= 512 && decoded.image.height <= 512,
            "Pixel oracle requires a small full-resolution public fixture");
    const auto profile = QGuiApplication::platformName() == "cocoa"
                             ? luma::macos::displayProfile(context.window->winId())
                             : std::vector<std::uint8_t>{};
    const auto pixels = luma::renderCPU(decoded.image, context.editor.currentSettings(),
                                        decoded.metadata.orientation, profile, {},
                                        decoded.calibration, diagnostic, 1, decoded.metadata);
    return QImage(pixels.rgba.data(), pixels.width, pixels.height, pixels.width * 4,
                  QImage::Format_RGBA8888)
        .copy();
}
} // namespace ux
