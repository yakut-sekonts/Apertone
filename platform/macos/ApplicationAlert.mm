#include "platform/macos/ApplicationAlert.h"
#import <AppKit/AppKit.h>

namespace luma::macos {
void showApplicationError(const std::string &title, const std::string &message) {
    @autoreleasepool {
        NSAlert *alert = [[NSAlert alloc] init];
        alert.alertStyle = NSAlertStyleCritical;
        alert.messageText = [NSString stringWithUTF8String:title.c_str()] ?: @"Luma";
        alert.informativeText = [NSString stringWithUTF8String:message.c_str()] ?: @"";
        [NSApp activate];
        [alert runModal];
    }
}
}
