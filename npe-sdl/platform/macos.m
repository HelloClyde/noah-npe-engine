#import <Cocoa/Cocoa.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

@interface NpeSettingsMenu : NSObject
- (void)keys:(id)sender;
- (void)view:(id)sender;
@end
@implementation NpeSettingsMenu
- (void)openPage:(unsigned)page {
    SDL_Event event;SDL_zero(event);event.type=SDL_USEREVENT;
    event.user.code=0x4e504553;event.user.data1=(void *)(uintptr_t)page;
    SDL_PushEvent(&event);
}
- (void)keys:(id)sender {(void)sender;[self openPage:0];}
- (void)view:(id)sender {(void)sender;[self openPage:2];}
@end

void npe_macos_settings_menu(void) {
    @autoreleasepool {
        static NpeSettingsMenu *handler;
        if(handler)return;
        handler=[[NpeSettingsMenu alloc] init];
        NSMenu *bar=[NSApp mainMenu];
        if(!bar){bar=[[NSMenu alloc] initWithTitle:@""];[NSApp setMainMenu:bar];}
        NSMenuItem *root=[[NSMenuItem alloc] initWithTitle:@"设置" action:nil keyEquivalent:@""];
        NSMenu *menu=[[NSMenu alloc] initWithTitle:@"设置"];
        NSMenuItem *keys=[[NSMenuItem alloc] initWithTitle:@"按键映射…" action:@selector(keys:) keyEquivalent:@""];
        NSMenuItem *view=[[NSMenuItem alloc] initWithTitle:@"地图视野…" action:@selector(view:) keyEquivalent:@""];
        [keys setTarget:handler];[view setTarget:handler];
        [menu addItem:keys];[menu addItem:view];[root setSubmenu:menu];
        [bar insertItem:root atIndex:MIN((NSInteger)1,[bar numberOfItems])];
    }
}

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
