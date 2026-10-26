// Test-target-only native activation and read-only identity observation.
#include "tests/ux/NativeApplicationHandoff.h"
#include "tests/ux/UxDriver.h"
#import <AppKit/AppKit.h>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QQuickWindow>
#include <QSaveFile>
#include <QTest>
#include <QUuid>
#include <atomic>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <unistd.h>

namespace ux {
namespace {
void mainThread() {
    require(QGuiApplication::platformName() == "cocoa" && [NSThread isMainThread] && NSApp,
            "Native handoff requires the real Cocoa/AppKit main thread");
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
    std::atomic<unsigned> peerActivated{0}, ownActivated{0};
};
} // namespace
struct NativeApplicationHandoff::Impl {
    QQuickWindow &window;
    NSWindow *__weak observedWindow;
    const NSInteger number;
    NSRunningApplication *own;
    NSRunningApplication *peer;
    NSMutableArray<id> *appTokens = [NSMutableArray new];
    NSMutableArray<id> *workspaceTokens = [NSMutableArray new];
    std::shared_ptr<Counters> counters = std::make_shared<Counters>();
    QProcess child;
    QString requestPath, nonce;

    explicit Impl(QQuickWindow &target, const QString &path)
        : window(target), observedWindow(windowFor(target)), number(observedWindow.windowNumber),
          own(NSRunningApplication.currentApplication) {
        require(own && !own.terminated && own.finishedLaunching &&
                    own.activationPolicy != NSApplicationActivationPolicyProhibited,
                "Current process cannot participate in real native activation");
        if (path.isEmpty()) {
            NSArray<NSRunningApplication *> *candidates =
                [NSRunningApplication runningApplicationsWithBundleIdentifier:@"com.apple.finder"];
            require(candidates.count == 1,
                    "Exactly one already-running Finder is required; no launch");
            peer = candidates.firstObject;
        } else {
            requestPath = path;
            require(QFileInfo(path).isAbsolute() && !QFileInfo::exists(path) &&
                        QFileInfo(path).dir().canonicalPath() == QFileInfo(path).absolutePath(),
                    "Cooperative request must be a fresh canonical owned file");
            nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
            child.start(QCoreApplication::applicationFilePath(),
                        {"--activation-peer", QString::number(QCoreApplication::applicationPid()),
                         nonce, requestPath});
            require(child.waitForStarted(5000), "Cannot launch the owned AppKit peer");
            QByteArray output;
            const bool ready = QTest::qWaitFor(
                [&] {
                    output += child.readAllStandardOutput();
                    return output.contains("LUMA_ACTIVATION_PEER_READY\n");
                },
                5000);
            require(ready, "Owned AppKit peer did not finish launching: state=" +
                               QString::number(child.state()) +
                               " exit=" + QString::number(child.exitCode()) +
                               " stdout=" + QString::fromUtf8(output) +
                               " stderr=" + QString::fromUtf8(child.readAllStandardError()));
            peer = [NSRunningApplication
                runningApplicationWithProcessIdentifier:static_cast<pid_t>(child.processId())];
            require(peer && [peer.executableURL isEqual:own.executableURL],
                    "Cooperative peer must be a separate process of this exact test executable");
            [observedWindow makeKeyAndOrderFront:nil];
            require(QTest::qWaitFor([&] { return own.active && NSApp.active && window.isActive(); },
                                    5000),
                    "Cooperative setup did not return the original active window");
        }
        require(
            peer && ![peer isEqual:own] && peer.processIdentifier != own.processIdentifier &&
                !peer.terminated && peer.finishedLaunching &&
                peer.activationPolicy == (path.isEmpty() ? NSApplicationActivationPolicyRegular
                                                         : NSApplicationActivationPolicyAccessory),
            "Pinned handoff peer must be a live, separate app with the expected activation policy");
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
        NSRunningApplication *pinnedOwn = own, *pinnedPeer = peer;
        NSNotificationCenter *workspace = NSWorkspace.sharedWorkspace.notificationCenter;
        [workspaceTokens
            addObject:[workspace addObserverForName:NSWorkspaceDidActivateApplicationNotification
                                             object:nil
                                              queue:nil
                                         usingBlock:^(NSNotification *notification) {
                                           NSRunningApplication *activated =
                                               notification.userInfo[NSWorkspaceApplicationKey];
                                           if (const auto current = weak.lock()) {
                                               if ([activated isEqual:pinnedPeer])
                                                   ++current->peerActivated;
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
        if (child.state() != QProcess::NotRunning) {
            child.terminate();
            if (!child.waitForFinished(1000)) {
                child.kill();
                child.waitForFinished(1000);
            }
        }
    }
    NSWindow *native() const {
        NSWindow *result = windowFor(window);
        require(observedWindow && result == observedWindow && result.windowNumber == number,
                "Native window was recreated during lifecycle observation");
        return result;
    }
};

NativeApplicationHandoff::NativeApplicationHandoff(QQuickWindow &window,
                                                   const QString &peerRequestPath)
    : impl_(std::make_unique<Impl>(window, peerRequestPath)) {}
NativeApplicationHandoff::~NativeApplicationHandoff() = default;
NativeHandoffState NativeApplicationHandoff::state() const {
    @autoreleasepool {
        NSWindow *native = impl_->native();
        NSRunningApplication *front = NSWorkspace.sharedWorkspace.frontmostApplication;
        NativeHandoffState result;
        result.applicationActive = NSApp.active;
        result.applicationHidden = NSApp.hidden;
        result.ownRunningActive = impl_->own.active;
        result.peerActive = impl_->peer.active;
        result.peerTerminated = impl_->peer.terminated;
        result.ownFrontmost = [front isEqual:impl_->own];
        result.peerFrontmost = [front isEqual:impl_->peer];
        result.windowKey = native.keyWindow;
        result.windowVisible = native.visible;
        result.windowMiniaturized = native.miniaturized;
        result.ownPID = impl_->own.processIdentifier;
        result.peerPID = impl_->peer.processIdentifier;
        result.windowNumber = static_cast<int>(native.windowNumber);
        result.activationPolicy = static_cast<int>(NSApp.activationPolicy);
        result.peerActivationPolicy = static_cast<int>(impl_->peer.activationPolicy);
        result.appResigned = impl_->counters->resigned.load();
        result.appActivated = impl_->counters->activated.load();
        result.windowResignedKey = impl_->counters->keyResigned.load();
        result.peerActivated = impl_->counters->peerActivated.load();
        result.ownActivated = impl_->counters->ownActivated.load();
        return result;
    }
}
NativeHandoffIdentity NativeApplicationHandoff::identity() const {
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
bool NativeApplicationHandoff::requestPeerActivation() {
    @autoreleasepool {
        (void)impl_->native();
        require(NSApp.active && !NSApp.hidden && !impl_->peer.terminated,
                "Native handoff lost its active/live baseline");
        [NSApp yieldActivationToApplication:impl_->peer];
        return [impl_->peer activateFromApplication:impl_->own options:0];
    }
}
void NativeApplicationHandoff::requestOwnActivation() {
    @autoreleasepool {
        NSWindow *native = impl_->native();
        require(!NSApp.hidden && !native.miniaturized,
                "Own activation requires the original nonhidden, nonminiaturized window");
        if (!impl_->requestPath.isEmpty()) {
            // The active peer voluntarily yields activation to this exact parent.
            // This request alone is never accepted as proof of the return.
            if (!QFileInfo::exists(impl_->requestPath)) {
                QSaveFile file(impl_->requestPath);
                file.setDirectWriteFallback(false);
                const auto data =
                    QJsonDocument(QJsonObject{{"parent_pid", QCoreApplication::applicationPid()},
                                              {"nonce", impl_->nonce}})
                        .toJson(QJsonDocument::Compact);
                require(file.open(QIODevice::WriteOnly) && file.write(data) == data.size() &&
                            file.commit(),
                        "Cannot publish cooperative return request");
            }
        } else {
            [NSApp activate];
        }
        [native makeKeyAndOrderFront:nil];
    }
}

int nativeActivationPeerMain(int argc, char **argv) {
    @autoreleasepool {
        if (argc != 5)
            return 64;
        errno = 0;
        char *end = nullptr;
        const auto parentPID = std::strtol(argv[2], &end, 10);
        if (errno || !end || *end || parentPID <= 1 || parentPID > INT_MAX ||
            parentPID != getppid())
            return 64;
        NSString *nonce = [NSString stringWithUTF8String:argv[3]];
        NSString *path = [NSString stringWithUTF8String:argv[4]];
        if (!nonce || !path)
            return 64;
        if (![[NSUUID alloc] initWithUUIDString:nonce] || !path.isAbsolutePath ||
            [NSFileManager.defaultManager fileExistsAtPath:path])
            return 64;
        [NSApplication sharedApplication];
        NSRunningApplication *parent = [NSRunningApplication
            runningApplicationWithProcessIdentifier:static_cast<pid_t>(parentPID)];
        NSRunningApplication *own = NSRunningApplication.currentApplication;
        if (!parent || parent.terminated || ![parent.executableURL isEqual:own.executableURL]) {
            std::fputs("Activation peer rejected parent/executable identity.\n", stderr);
            return 64;
        }
        // The peer must not steal focus again as a regular app completes startup.
        // Accessory apps still support genuine public cross-process activation.
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
        NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(80, 80, 320, 100)
                                                       styleMask:NSWindowStyleMaskTitled
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];
        window.title = NSLocalizedString(@"Luma QA — cooperative activation peer",
                                         @"Native QA helper window title");
        [window orderFront:nil];
        const auto started = NSProcessInfo.processInfo.systemUptime;
        __block bool ready = false;
        __block bool requested = false;
        NSTimer *timer = [NSTimer
            scheduledTimerWithTimeInterval:0.025
                                   repeats:YES
                                     block:^(NSTimer *) {
                                       if (parent.terminated || getppid() != parentPID ||
                                           NSProcessInfo.processInfo.systemUptime - started > 120) {
                                           [NSApp terminate:nil];
                                           return;
                                       }
                                       if (!ready && own.finishedLaunching && NSApp.running) {
                                           ready = true;
                                           if (NSApp.active && own.active) {
                                               [NSApp yieldActivationToApplication:parent];
                                               [parent activateFromApplication:own options:0];
                                           }
                                           std::fputs("LUMA_ACTIVATION_PEER_READY\n", stdout);
                                           std::fflush(stdout);
                                       }
                                       if (!ready || requested || !NSApp.active || !own.active)
                                           return;
                                       NSData *data = [NSData dataWithContentsOfFile:path];
                                       if (!data)
                                           return;
                                       id decoded = [NSJSONSerialization JSONObjectWithData:data
                                                                                    options:0
                                                                                      error:nil];
                                       if (![decoded isKindOfClass:NSDictionary.class] ||
                                           ![decoded[@"nonce"] isEqual:nonce] ||
                                           ![decoded[@"parent_pid"] isEqual:@(parentPID)]) {
                                           [NSApp terminate:nil];
                                           return;
                                       }
                                       requested = true;
                                       [NSApp yieldActivationToApplication:parent];
                                       [parent activateFromApplication:own options:0];
                                     }];
        [NSApp run];
        [timer invalidate];
        return 0;
    }
}
} // namespace ux
