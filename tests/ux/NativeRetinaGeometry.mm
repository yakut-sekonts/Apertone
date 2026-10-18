// Test-target-only geometry observations through public AppKit APIs.
#include "tests/ux/NativeRetinaGeometry.h"
#include "tests/ux/UxDriver.h"
#import <AppKit/AppKit.h>
#include <QGuiApplication>
#include <QQuickWindow>
#include <cmath>

namespace ux {
namespace {
NSView *nativeView(QQuickWindow &window) {
    require(QGuiApplication::platformName() == "cocoa" && [NSThread isMainThread] && NSApp,
            "Retina mapping fixture requires the real Cocoa GUI main thread");
    // Cocoa QWindow::winId is the borrowed NSView address; ARC requires the
    // explicit non-owning bridge from its integer WId representation.
    // NOLINTNEXTLINE(bugprone-casting-through-void,performance-no-int-to-ptr)
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(window.winId());
    require(view && [view isKindOfClass:[NSView class]], "Qt Cocoa window has no native NSView");
    NSWindow *native = view.window;
    require(native && native.windowNumber > 0 && native.visible && !native.miniaturized &&
                [NSApp windowWithWindowNumber:native.windowNumber] == native && native.screen,
            "Retina mapping requires a live, visible NSWindow owned by this NSApplication");
    require(window.width() > 0 && window.height() > 0 && NSWidth(view.bounds) > 0 &&
                NSHeight(view.bounds) > 0,
            "Retina native/Qt logical surface is empty");
    return view; // Borrow for this GUI-thread call; never keep it across Qt teardown.
}
} // namespace
NativeRetinaGeometry nativeRetinaGeometry(QQuickWindow &window) {
    @autoreleasepool {
        NSView *view = nativeView(window);
        const NSRect bounds = view.bounds;
        const NSRect backing = [view convertRectToBacking:bounds];
        NSNumber *display = view.window.screen.deviceDescription[@"NSScreenNumber"];
        require(display && view.window.backingScaleFactor > 0 && NSWidth(backing) > 0 &&
                    NSHeight(backing) > 0,
                "Public AppKit backing/display metrics are unavailable");
        return {static_cast<int>(view.window.windowNumber),
                display.unsignedIntValue,
                static_cast<bool>(view.flipped),
                view.window.backingScaleFactor,
                QSizeF(NSWidth(bounds), NSHeight(bounds)),
                QRectF(NSMinX(backing), NSMinY(backing), NSWidth(backing), NSHeight(backing))};
    }
}
NativeRetinaPoint nativeRetinaPoint(QQuickWindow &window, const QPointF &logical) {
    @autoreleasepool {
        require(std::isfinite(logical.x()) && std::isfinite(logical.y()),
                "Logical Retina point is not finite");
        NSView *view = nativeView(window);
        const NSRect bounds = view.bounds;
        // Qt window coordinates use a top-left origin; NSView may be flipped or
        // unflipped. Normalize only the coordinate basis, never multiply by Qt DPR.
        const double x = NSMinX(bounds) + logical.x() * NSWidth(bounds) / window.width();
        const double y = view.flipped
                             ? NSMinY(bounds) + logical.y() * NSHeight(bounds) / window.height()
                             : NSMaxY(bounds) - logical.y() * NSHeight(bounds) / window.height();
        const NSPoint backed = [view convertPointToBacking:NSMakePoint(x, y)];
        // AppKit backing coordinates need not inherit the view's flipped basis.
        // Derive their origin/directions from actual public conversions of the
        // logical corners; never infer them from Qt DPR or NSView.isFlipped.
        const NSPoint origin =
            [view convertPointToBacking:NSMakePoint(NSMinX(bounds), view.flipped ? NSMinY(bounds)
                                                                                 : NSMaxY(bounds))];
        const NSPoint opposite =
            [view convertPointToBacking:NSMakePoint(NSMaxX(bounds), view.flipped ? NSMaxY(bounds)
                                                                                 : NSMinY(bounds))];
        require(std::abs(opposite.x - origin.x) > 0 && std::abs(opposite.y - origin.y) > 0,
                "AppKit backing conversion has an empty logical basis");
        const QPointF fromTopLeft((backed.x - origin.x) * (opposite.x > origin.x ? 1 : -1),
                                  (backed.y - origin.y) * (opposite.y > origin.y ? 1 : -1));
        const NSPoint returned = [view convertPointFromBacking:backed];
        const QPointF roundTrip(
            (returned.x - NSMinX(bounds)) * window.width() / NSWidth(bounds),
            (view.flipped ? returned.y - NSMinY(bounds) : NSMaxY(bounds) - returned.y) *
                window.height() / NSHeight(bounds));
        return {fromTopLeft, roundTrip};
    }
}
} // namespace ux
