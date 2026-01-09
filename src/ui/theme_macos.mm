#import <AppKit/AppKit.h>

extern "C" bool diana_is_dark_mode() {
    if (@available(macOS 10.14, *)) {
        NSAppearanceName appearance = [[NSApp effectiveAppearance] bestMatchFromAppearancesWithNames:@[
            NSAppearanceNameAqua,
            NSAppearanceNameDarkAqua
        ]];
        return [appearance isEqualToString:NSAppearanceNameDarkAqua];
    }
    return false;
}
