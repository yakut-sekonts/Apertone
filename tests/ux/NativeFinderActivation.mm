// Test-target-only native activation and read-only identity observation.
#include "tests/ux/NativeFinderActivation.h"
#include "tests/ux/UxDriver.h"
#import <AppKit/AppKit.h>
#include <QGuiApplication>
#include <QQuickWindow>
#include <atomic>
#include <memory>

namespace ux {
namespace {
void mainThread() {
    require(QGuiApplication::platformName() == "cocoa" && [NSThread isMainThread] && NSApp,
            "Finder deactivation requires the real Cocoa/AppKit main thread");
}
NSWindow *windowFor(QQuickWindow &window) {
    mainThread();
    // Same borrowed Cocoa NSView bridge as NativeRetinaGeometry.mm.
    // NOLINTNEXTLINE(bugprone-casting-through-void,performance-no-int-to-ptr)
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(window.winId());
    require(view && [view isKindOfClass:NSView.class], "Qt window has no real Cocoa NSView");
    NSWindow *native = view.window;
    require(native && native.windowNumber > 0 &&
                [NSApp windowWithWindowNumber:native.windowNumber] == native,
            "Observed native window does not belong to this application");
    return native;
}
struct Counters {
    std::atomic<unsigned> resigned{0}, activated{0}, keyResigned{0};
    std::atomic<unsigned> finderActivated{0}, ownActivated{0};
};
} // namespace
struct NativeFinderActivation::Impl {
    QQuickWindow &window;
    NSWindow *__weak observedWindow;
    const NSInteger number;
    NSRunningApplication *own;
    NSRunningApplication *finder;
    NSMutableArray<id> *appTokens = [NSMutableArray new];
    NSMutableArray<id> *workspaceTokens = [NSMutableArray new];
    std::shared_ptr<Counters> counters = std::make_shared<Counters>();

    explicit Impl(QQuickWindow &target)
        : window(target), observedWindow(windowFor(target)), number(observedWindow.windowNumber),
          own(NSRunningApplication.currentApplication) {
        require(own && !own.terminated && own.finishedLaunching &&
                    own.activationPolicy != NSApplicationActivationPolicyProhibited,
                "Current process cannot participate in real native activation");
        NSArray<NSRunningApplication *> *candidates =
            [NSRunningApplication runningApplicationsWithBundleIdentifier:@"com.apple.finder"];
        require(candidates.count == 1, "Exactly one already-running Finder is required; no launch");
        finder = candidates.firstObject;
        require(finder && ![finder isEqual:own] &&
                    finder.processIdentifier != own.processIdentifier && !finder.terminated &&
                    finder.finishedLaunching &&
                    finder.activationPolicy == NSApplicationActivationPolicyRegular,
                "Pinned Finder must be a live, separate, regular application");
        // Blocks own only weak C++ counters plus pinned NSRunningApplication identities.
        // They never retain this, Qt objects, or the observed NSWindow.
        const std::weak_ptr<Counters> weak = counters;
        NSNotificationCenter *appCenter = NSNotificationCenter.defaultCenter;
        [appTokens addObject:[appCenter addObserverForName:NSApplicationDidResignActiveNotification
                                                    object:NSApp
                                                     queue:nil
                                                usingBlock:^(NSNotification *) {
                                                  if (const auto current = weak.lock())
                                                      ++current->resigned;
                                                }]];
        [appTokens addObject:[appCenter addObserverForName:NSApplicationDidBecomeActiveNotification
                                                    object:NSApp
                                                     queue:nil
                                                usingBlock:^(NSNotification *) {
                                                  if (const auto current = weak.lock())
                                                      ++current->activated;
                                                }]];
        [appTokens addObject:[appCenter addObserverForName:NSWindowDidResignKeyNotification
                                                    object:windowFor(window)
                                                     queue:nil
                                                usingBlock:^(NSNotification *) {
                                                  if (const auto current = weak.lock())
                                                      ++current->keyResigned;
                                                }]];
        NSRunningApplication *pinnedOwn = own, *pinnedFinder = finder;
        NSNotificationCenter *workspace = NSWorkspace.sharedWorkspace.notificationCenter;
        [workspaceTokens
            addObject:[workspace addObserverForName:NSWorkspaceDidActivateApplicationNotification
                                             object:nil
                                              queue:nil
                                         usingBlock:^(NSNotification *notification) {
                                           NSRunningApplication *activated =
                                               notification.userInfo[NSWorkspaceApplicationKey];
                                           if (const auto current = weak.lock()) {
                                               if ([activated isEqual:pinnedFinder])
                                                   ++current->finderActivated;
                                               if ([activated isEqual:pinnedOwn])
                                                   ++current->ownActivated;
                                           }
                                         }]];
    }
    ~Impl() {
        for (id token in appTokens)
            [NSNotificationCenter.defaultCenter removeObserver:token];
        for (id token in workspaceTokens)
            [NSWorkspace.sharedWorkspace.notificationCenter removeObserver:token];
        [appTokens removeAllObjects];
        [workspaceTokens removeAllObjects];
        counters.reset();
    }
    NSWindow *native() const {
        NSWindow *result = windowFor(window);
        require(observedWindow && result == observedWindow && result.windowNumber == number,
                "Native window was recreated during lifecycle observation");
        return result;
    }
};

NativeFinderActivation::NativeFinderActivation(QQuickWindow &window)
    : impl_(std::make_unique<Impl>(window)) {}
NativeFinderActivation::~NativeFinderActivation() = default;
NativeFinderState NativeFinderActivation::state() const {
    @autoreleasepool {
        NSWindow *native = impl_->native();
        NSRunningApplication *front = NSWorkspace.sharedWorkspace.frontmostApplication;
        NativeFinderState result;
        result.applicationActive = NSApp.active;
        result.applicationHidden = NSApp.hidden;
        result.ownRunningActive = impl_->own.active;
        result.finderActive = impl_->finder.active;
        result.finderTerminated = impl_->finder.terminated;
        result.ownFrontmost = [front isEqual:impl_->own];
        result.finderFrontmost = [front isEqual:impl_->finder];
        result.windowKey = native.keyWindow;
        result.windowVisible = native.visible;
        result.windowMiniaturized = native.miniaturized;
        result.ownPID = impl_->own.processIdentifier;
        result.finderPID = impl_->finder.processIdentifier;
        result.windowNumber = static_cast<int>(native.windowNumber);
        result.activationPolicy = static_cast<int>(NSApp.activationPolicy);
        result.appResigned = impl_->counters->resigned.load();
        result.appActivated = impl_->counters->activated.load();
        result.windowResignedKey = impl_->counters->keyResigned.load();
        result.finderActivated = impl_->counters->finderActivated.load();
        result.ownActivated = impl_->counters->ownActivated.load();
        return result;
    }
}
NativeFinderIdentity NativeFinderActivation::identity() const {
    @autoreleasepool {
        NSWindow *native = impl_->native();
        require(!impl_->own.terminated && native.screen,
                "External return target lost its live native process/screen");
        const auto string = [](NSString *value) {
            return value ? QString::fromUtf8(value.UTF8String) : QString();
        };
        const auto rectangle = [](NSRect value) {
            return QRectF(NSMinX(value), NSMinY(value), NSWidth(value), NSHeight(value));
        };
        return {static_cast<int>(impl_->own.processIdentifier),
                static_cast<int>(native.windowNumber),
                string(impl_->own.executableURL.path),
                string(impl_->own.bundleIdentifier),
                string(impl_->own.bundleURL.path),
                string(impl_->own.localizedName),
                string(native.title),
                rectangle(native.frame),
                rectangle(native.screen.frame)};
    }
}
bool NativeFinderActivation::requestFinderActivation() {
    @autoreleasepool {
        (void)impl_->native();
        require(NSApp.active && !NSApp.hidden && !impl_->finder.terminated,
                "Native Finder handoff lost its active/live baseline");
        [NSApp yieldActivationToApplication:impl_->finder];
        return [impl_->finder activateFromApplication:impl_->own options:0];
    }
}
void NativeFinderActivation::requestOwnActivation() {
    @autoreleasepool {
        NSWindow *native = impl_->native();
        require(!NSApp.hidden && !native.miniaturized,
                "Own activation requires the original nonhidden, nonminiaturized window");
        [NSApp activate];
        [native makeKeyAndOrderFront:nil];
    }
}
} // namespace ux
