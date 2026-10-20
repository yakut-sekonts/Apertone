#include "tests/ux/NativeWindowLifecycle.h"
#include "tests/ux/UxDriver.h"
#import <AppKit/AppKit.h>
#include <QGuiApplication>
#include <QQuickWindow>

namespace ux {
namespace {
NSWindow *ownedWindow(QQuickWindow &window) {
    require(QGuiApplication::platformName() == "cocoa" && [NSThread isMainThread] && NSApp,
            "Window lifecycle requires the actual Cocoa main thread");
    // Borrow the existing Qt Cocoa view. No new window or activation is requested.
    // NOLINTNEXTLINE(bugprone-casting-through-void,performance-no-int-to-ptr)
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(window.winId());
    require(view && [view isKindOfClass:NSView.class], "Lifecycle target has no Cocoa view");
    NSWindow *native = view.window;
    require(native && native.windowNumber > 0 &&
                [NSApp windowWithWindowNumber:native.windowNumber] == native,
            "Lifecycle target is not the owned application window");
    return native;
}
struct Counts {
    unsigned minimized = 0, restored = 0;
};
} // namespace
struct NativeWindowLifecycle::Impl {
    QQuickWindow &window;
    NSWindow *__weak original;
    const NSInteger number;
    std::shared_ptr<Counts> counts = std::make_shared<Counts>();
    NSMutableArray<id> *tokens = [NSMutableArray new];

    explicit Impl(QQuickWindow &target)
        : window(target), original(ownedWindow(target)), number(original.windowNumber) {
        const std::weak_ptr<Counts> weak = counts;
        auto *center = NSNotificationCenter.defaultCenter;
        [tokens addObject:[center addObserverForName:NSWindowDidMiniaturizeNotification
                                              object:original
                                               queue:nil
                                          usingBlock:^(NSNotification *) {
                                            if (const auto value = weak.lock())
                                                ++value->minimized;
                                          }]];
        [tokens addObject:[center addObserverForName:NSWindowDidDeminiaturizeNotification
                                              object:original
                                               queue:nil
                                          usingBlock:^(NSNotification *) {
                                            if (const auto value = weak.lock())
                                                ++value->restored;
                                          }]];
    }
    ~Impl() {
        for (id token in tokens)
            [NSNotificationCenter.defaultCenter removeObserver:token];
        [tokens removeAllObjects];
        counts.reset();
    }
    NSWindow *native() const {
        NSWindow *current = ownedWindow(window);
        require(original && current == original && current.windowNumber == number,
                "Lifecycle action recreated or replaced the owned NSWindow");
        return current;
    }
};
NativeWindowLifecycle::NativeWindowLifecycle(QQuickWindow &window)
    : impl_(std::make_unique<Impl>(window)) {}
NativeWindowLifecycle::~NativeWindowLifecycle() = default;
QJsonObject NativeWindowLifecycle::state() const {
    @autoreleasepool {
        NSWindow *native = impl_->native();
        return {{"window_number", static_cast<int>(native.windowNumber)},
                {"miniaturized", static_cast<bool>(native.miniaturized)},
                {"visible", static_cast<bool>(native.visible)},
                {"key", static_cast<bool>(native.keyWindow)},
                {"application_active", static_cast<bool>(NSApp.active)},
                {"did_miniaturize", static_cast<int>(impl_->counts->minimized)},
                {"did_deminiaturize", static_cast<int>(impl_->counts->restored)},
                {"qt_window_state", static_cast<int>(impl_->window.windowState())}};
    }
}
void NativeWindowLifecycle::minimize() {
    @autoreleasepool {
        NSWindow *native = impl_->native();
        NSButton *button = [native standardWindowButton:NSWindowMiniaturizeButton];
        require(!native.miniaturized && native.visible && native.keyWindow && button &&
                    button.enabled,
                "Actual native minimize control is unavailable");
        // Standard traffic-light buttons can implement their mouse handling
        // outside NSButton's target/action path. Use NSWindow's public action.
        [native performMiniaturize:nil];
    }
}
void NativeWindowLifecycle::restore() {
    @autoreleasepool {
        NSWindow *native = impl_->native();
        require(native.miniaturized, "Restore requires an actually miniaturized NSWindow");
        [native deminiaturize:nil];
    }
}
} // namespace ux
