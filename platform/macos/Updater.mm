#include "apps/desktop/UpdateController.h"
#import <Sparkle/Sparkle.h>

@interface ApertoneUpdateDelegate : NSObject <SPUUpdaterDelegate, SPUStandardUserDriverDelegate>
@property(nonatomic) UpdateCallbacks callbacks;
@end
@implementation ApertoneUpdateDelegate
- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id> *)change
                       context:(void *)context {
    if ([keyPath isEqualToString:@"canCheckForUpdates"]) {
        self.callbacks.availability();
    } else {
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
    }
}
- (BOOL)supportsGentleScheduledUpdateReminders {
    return YES;
}
- (BOOL)standardUserDriverShouldHandleShowingScheduledUpdate:(SUAppcastItem *)item
                                         andInImmediateFocus:(BOOL)focus {
    (void)item;
    (void)focus;
    return NO; // Scheduled reminders live in Apertone's bell, including at launch.
}
- (void)standardUserDriverWillHandleShowingUpdate:(BOOL)show
                                        forUpdate:(SUAppcastItem *)item
                                            state:(SPUUserUpdateState *)state {
    (void)show;
    (void)state;
    self.callbacks.found(QString::fromUtf8(item.displayVersionString.UTF8String));
}
- (BOOL)updater:(SPUUpdater *)updater
    mayPerformUpdateCheck:(SPUUpdateCheck)check
                    error:(NSError *__autoreleasing *)error {
    (void)updater;
    (void)check;
    (void)error;
    self.callbacks.status(true, QStringLiteral("Checking for updates…"));
    return YES;
}
- (void)updater:(SPUUpdater *)updater didFindValidUpdate:(SUAppcastItem *)item {
    (void)updater;
    self.callbacks.found(QString::fromUtf8(item.displayVersionString.UTF8String));
    self.callbacks.status(false, QStringLiteral("A new version of Apertone is available"));
}
- (void)updater:(SPUUpdater *)updater
    didFinishUpdateCycleForUpdateCheck:(SPUUpdateCheck)check
                                 error:(NSError *)error {
    (void)updater;
    (void)check;
    if (error && error.code == SUNoUpdateError) {
        self.callbacks.failed();
        self.callbacks.finished();
        self.callbacks.status(false, QStringLiteral("No updates available"));
    } else if (error && error.code != SUInstallationCanceledError) {
        self.callbacks.failed();
        self.callbacks.status(false, QStringLiteral("Could not update Apertone: ") +
                                         QString::fromUtf8(error.localizedDescription.UTF8String));
    } else {
        if (error)
            self.callbacks.failed();
        self.callbacks.status(false, QStringLiteral("Update check completed"));
    }
}
- (void)standardUserDriverWillFinishUpdateSession {
    self.callbacks.finished();
}
- (BOOL)updater:(SPUUpdater *)updater
    shouldPostponeRelaunchForUpdate:(SUAppcastItem *)item
                 untilInvokingBlock:(void (^)(void))handler {
    (void)updater;
    (void)item;
    void (^ownedHandler)(void) = [handler copy];
    self.callbacks.install([ownedHandler] { ownedHandler(); });
    return YES;
}
@end

class SparkleUpdater final : public NativeUpdater {
    ApertoneUpdateDelegate *delegate_;
    SPUStandardUpdaterController *controller_;

  public:
    explicit SparkleUpdater(UpdateCallbacks callbacks) {
        delegate_ = [ApertoneUpdateDelegate new];
        delegate_.callbacks = std::move(callbacks);
        controller_ = [[SPUStandardUpdaterController alloc] initWithStartingUpdater:NO
                                                                    updaterDelegate:delegate_
                                                                 userDriverDelegate:delegate_];
        [controller_.updater addObserver:delegate_
                              forKeyPath:@"canCheckForUpdates"
                                 options:NSKeyValueObservingOptionNew
                                 context:nullptr];
    }
    ~SparkleUpdater() override {
        [controller_.updater removeObserver:delegate_ forKeyPath:@"canCheckForUpdates"];
        // The controller and its weak delegates remain valid for the same lifetime.
        controller_ = nil;
        delegate_ = nil;
    }
    bool start(QString &error) override {
        NSError *failure = nil;
        const bool started = [controller_.updater startUpdater:&failure];
        if (!started)
            error = QString::fromUtf8(failure.localizedDescription.UTF8String);
        return started;
    }
    void check(bool foreground) override {
        if (foreground)
            [controller_ checkForUpdates:nil];
        else
            [controller_.updater checkForUpdatesInBackground];
    }
    bool automatic() const override {
        return controller_.updater.automaticallyChecksForUpdates;
    }
    void setAutomatic(bool enabled) override {
        controller_.updater.automaticallyChecksForUpdates = enabled;
    }
    bool canCheck() const override {
        return controller_.updater.canCheckForUpdates;
    }
};
std::unique_ptr<NativeUpdater> makeNativeUpdater(UpdateCallbacks callbacks) {
    return std::make_unique<SparkleUpdater>(std::move(callbacks));
}
