#import <Cocoa/Cocoa.h>
#include <stdlib.h>
#include <string.h>

/* Called on the main thread, before the game starts. No shell/AppleScript. */
char *npe_macos_choose_game(void) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp activateIgnoringOtherApps:YES];
        NSOpenPanel *panel=[NSOpenPanel openPanel];
        [panel setTitle:@"选择 NPE 游戏"];
        [panel setAllowedFileTypes:@[@"npe", @"NPE"]];
        [panel setAllowsMultipleSelection:NO];
        [panel setCanChooseDirectories:NO];
        if([panel runModal]!=NSModalResponseOK)return NULL;
        const char *path=[[[panel URL] path] UTF8String];
        return path?strdup(path):NULL;
    }
}
