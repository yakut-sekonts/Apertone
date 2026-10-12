#include "platform/macos/DisplayProfile.h"
#import <AppKit/AppKit.h>
#import <ColorSync/ColorSync.h>

namespace luma::macos {
std::vector<std::uint8_t> displayProfile(std::uintptr_t nativeView) {
    @autoreleasepool {
        NSView *view = reinterpret_cast<NSView *>(nativeView);
        NSScreen *screen = view.window.screen;
        if (!screen) {
            return {};
        }
        NSNumber *number = screen.deviceDescription[@"NSScreenNumber"];
        if (!number) {
            return {};
        }
        ColorSyncProfileRef profile = ColorSyncProfileCreateWithDisplayID(number.unsignedIntValue);
        if (!profile) {
            return {};
        }
        CFDataRef bytes = ColorSyncProfileCopyData(profile, nullptr);
        CFRelease(profile);
        if (!bytes) {
            return {};
        }
        const auto *first = CFDataGetBytePtr(bytes);
        std::vector<std::uint8_t> result(first, first + CFDataGetLength(bytes));
        CFRelease(bytes);
        return result;
    }
}
} // namespace luma::macos
