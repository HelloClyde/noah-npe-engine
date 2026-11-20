#import <Cocoa/Cocoa.h>
#include "settings_types.h"

static NSTextField *native_label(NSString *text,NSRect frame){
    NSTextField *field=[[[NSTextField alloc] initWithFrame:frame] autorelease];
    [field setStringValue:text];[field setEditable:NO];[field setSelectable:NO];
    [field setBezeled:NO];[field setDrawsBackground:NO];return field;
}
static SDL_Keycode native_mac_key(NSEvent *event){
    NSString *text=[event charactersIgnoringModifiers];if(![text length])return SDLK_UNKNOWN;
    unichar ch=[text characterAtIndex:0];
    switch(ch){
        case NSUpArrowFunctionKey:return SDLK_UP;case NSDownArrowFunctionKey:return SDLK_DOWN;
        case NSLeftArrowFunctionKey:return SDLK_LEFT;case NSRightArrowFunctionKey:return SDLK_RIGHT;
        case NSHomeFunctionKey:return SDLK_HOME;case NSEndFunctionKey:return SDLK_END;
        case NSPageUpFunctionKey:return SDLK_PAGEUP;case NSPageDownFunctionKey:return SDLK_PAGEDOWN;
        case NSDeleteFunctionKey:return SDLK_DELETE;case NSInsertFunctionKey:return SDLK_INSERT;
        case '\r':case '\n':return SDLK_RETURN;case 0x7f:return SDLK_BACKSPACE;case '\t':return SDLK_TAB;
        case 0x1b:return SDLK_ESCAPE;
    }
    if(ch>=NSF1FunctionKey&&ch<=NSF12FunctionKey)return SDLK_F1+(ch-NSF1FunctionKey);
    if(ch>=NSF13FunctionKey&&ch<=NSF24FunctionKey)return SDLK_F13+(ch-NSF13FunctionKey);
    return SDL_GetKeyFromName([[text lowercaseString] UTF8String]);
}
@interface NpeNativeSettings : NSObject<NSTableViewDataSource,NSTableViewDelegate,NSTextFieldDelegate> {
@public
    NpeSettings *edited;
    const NpeSettingsHooks *hooks;
    NSTableView *tables[2];NSTabView *tabs;
    NSTextField *hint,*rows,*cols;NSButton *original;
    NSStepper *rowStep,*colStep;
    BOOL capturing;NSInteger capturePage,captureRow;
}
- (void)change:(id)sender;
- (void)restore:(id)sender;
- (void)toggle:(id)sender;
- (void)step:(id)sender;
- (void)tick:(NSTimer *)timer;
- (void)refresh;
@end
@implementation NpeNativeSettings
- (NSInteger)numberOfRowsInTableView:(NSTableView *)table {return [table tag]==1?SETTINGS_BUTTONS:SETTINGS_KEYS;}
- (id)tableView:(NSTableView *)table objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row {
    NSArray *labels=@[@"上",@"下",@"左",@"右",@"确认",@"返回 / 菜单",@"对话 / 翻页",@"卸装",@"说明上一页",@"说明下一页",@"按键说明",@"选项左（J）",@"选项右（L）"];
    if([[column identifier] isEqualToString:@"action"])return [labels objectAtIndex:(NSUInteger)([table tag]==0&&row>=10?row+1:row)];
    const char *name=[table tag]==1?SDL_GameControllerGetStringForButton((SDL_GameControllerButton)edited->buttons[row]):SDL_GetKeyName(edited->keys[row]);
    return name?[NSString stringWithUTF8String:name]:@"";
}
- (void)change:(id)sender {
    capturePage=[sender tag];captureRow=[tables[capturePage] selectedRow];
    if(captureRow<0)return;
    capturing=YES;
    [hint setStringValue:capturePage?@"请按手柄按钮；Esc 取消。重复绑定会交换。":@"请按新按键；Esc 取消。F10 为保留键。"];
}
- (void)refresh {
    [tables[0] reloadData];[tables[1] reloadData];
    [original setState:edited->rows?NSControlStateValueOff:NSControlStateValueOn];
    [rows setIntegerValue:edited->rows?edited->rows:14];[cols setIntegerValue:edited->cols?edited->cols:25];
    [rowStep setIntegerValue:[rows integerValue]];[colStep setIntegerValue:[cols integerValue]];[self toggle:nil];
}
- (void)restore:(id)sender {(void)sender;hooks->defaults(edited);capturing=NO;[self refresh];[hint setStringValue:@"已恢复默认设置，点击“保存”后生效。"];}
- (void)toggle:(id)sender {(void)sender;BOOL enabled=[original state]!=NSControlStateValueOn;
    [rows setEnabled:enabled];[cols setEnabled:enabled];[rowStep setEnabled:enabled];[colStep setEnabled:enabled];}
- (void)step:(id)sender {if(sender==rowStep)[rows setIntegerValue:[rowStep integerValue]];else[cols setIntegerValue:[colStep integerValue]];}
- (void)controlTextDidEndEditing:(NSNotification *)note {
    if([note object]==rows)[rowStep setIntegerValue:[rows integerValue]];
    if([note object]==cols)[colStep setIntegerValue:[cols integerValue]];
}
- (void)tick:(NSTimer *)timer {
    (void)timer;int button=hooks->poll_button();
    if(button==-2){[NSApp abortModal];return;}
    if(capturing && capturePage==1 && button>=0 && [NSApp isActive]){
        hooks->bind_button(edited,(unsigned)captureRow,(Uint8)button);capturing=NO;
        [tables[1] reloadData];[hint setStringValue:@"绑定已更新，点击“保存”后生效。"];}
}
@end

int npe_macos_settings_dialog(NpeSettings *edited,unsigned page,const NpeSettingsHooks *hooks){
    @autoreleasepool {
        NpeNativeSettings *controller=[[NpeNativeSettings alloc] init];controller->edited=edited;controller->hooks=hooks;
        NSAlert *alert=[[NSAlert alloc] init];[alert setMessageText:@"诺亚舟-NPE-引擎设置"];
        [alert setInformativeText:@"使用系统控件修改按键映射和地图视野。"];
        [alert addButtonWithTitle:@"保存"];[alert addButtonWithTitle:@"取消"];
        NSView *root=[[[NSView alloc] initWithFrame:NSMakeRect(0,0,580,470)] autorelease];
        controller->tabs=[[[NSTabView alloc] initWithFrame:NSMakeRect(0,70,580,400)] autorelease];[root addSubview:controller->tabs];
        for(unsigned p=0;p<3;++p){
            NSTabViewItem *item=[[[NSTabViewItem alloc] initWithIdentifier:@(p)] autorelease];
            [item setLabel:[@[@"键盘",@"手柄",@"地图视野"] objectAtIndex:p]];
            NSView *content=[[[NSView alloc] initWithFrame:NSMakeRect(0,0,550,350)] autorelease];[item setView:content];[controller->tabs addTabViewItem:item];
            if(p<2){
                NSScrollView *scroll=[[[NSScrollView alloc] initWithFrame:NSMakeRect(12,52,530,290)] autorelease];[scroll setHasVerticalScroller:YES];[scroll setBorderType:NSBezelBorder];
                NSTableView *table=[[[NSTableView alloc] initWithFrame:[scroll bounds]] autorelease];[table setTag:p];[table setRowHeight:25];
                NSTableColumn *action=[[[NSTableColumn alloc] initWithIdentifier:@"action"] autorelease];[[action headerCell] setStringValue:@"功能"];[action setWidth:245];[action setEditable:NO];[table addTableColumn:action];
                NSTableColumn *binding=[[[NSTableColumn alloc] initWithIdentifier:@"binding"] autorelease];[[binding headerCell] setStringValue:@"当前按键 / 按钮"];[binding setWidth:260];[binding setEditable:NO];[table addTableColumn:binding];
                [table setDataSource:controller];[table setDelegate:controller];[table setTarget:controller];[table setDoubleAction:@selector(change:)];
                [table selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];controller->tables[p]=table;
                [scroll setDocumentView:table];[content addSubview:scroll];
                NSButton *change=[NSButton buttonWithTitle:@"更改绑定…" target:controller action:@selector(change:)];[change setTag:p];[change setFrame:NSMakeRect(392,10,150,32)];[content addSubview:change];
            }else{
                controller->original=[NSButton checkboxWithTitle:@"使用原始地图视野" target:controller action:@selector(toggle:)];[controller->original setFrame:NSMakeRect(20,288,380,28)];[content addSubview:controller->original];
                [content addSubview:native_label(@"显示行数（7–40）",NSMakeRect(20,232,230,25))];
                [content addSubview:native_label(@"显示列数（15–60）",NSMakeRect(20,177,230,25))];
                controller->rows=[[[NSTextField alloc] initWithFrame:NSMakeRect(255,230,120,28)] autorelease];
                controller->cols=[[[NSTextField alloc] initWithFrame:NSMakeRect(255,175,120,28)] autorelease];
                [controller->rows setDelegate:controller];[controller->cols setDelegate:controller];[content addSubview:controller->rows];[content addSubview:controller->cols];
                controller->rowStep=[[[NSStepper alloc] initWithFrame:NSMakeRect(385,230,20,28)] autorelease];
                controller->colStep=[[[NSStepper alloc] initWithFrame:NSMakeRect(385,175,20,28)] autorelease];
                [controller->rowStep setMinValue:7];[controller->rowStep setMaxValue:40];[controller->colStep setMinValue:15];[controller->colStep setMaxValue:60];
                for(NSStepper *step in @[controller->rowStep,controller->colStep]){[step setIncrement:1];[step setValueWraps:NO];[step setTarget:controller];[step setAction:@selector(step:)];[content addSubview:step];}
                [content addSubview:native_label(@"仅扩大地图显示范围，不改变游戏规则。",NSMakeRect(20,100,510,25))];
                [content addSubview:native_label(@"战斗和菜单保持原有布局。",NSMakeRect(20,70,510,25))];
            }
        }
        controller->hint=native_label(@"选中功能后点击“更改绑定”，或双击绑定。",NSMakeRect(5,35,570,28));[root addSubview:controller->hint];
        NSButton *reset=[NSButton buttonWithTitle:@"恢复默认" target:controller action:@selector(restore:)];[reset setFrame:NSMakeRect(0,0,130,30)];[root addSubview:reset];
        [alert setAccessoryView:root];[controller refresh];[controller->tabs selectTabViewItemAtIndex:MIN(page,2u)];
        id monitor=[NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown handler:^NSEvent *(NSEvent *event){
            if(!controller->capturing || [event window]!=[alert window])return event;
            SDL_Keycode key=native_mac_key(event);
            if(key==SDLK_ESCAPE){controller->capturing=NO;[controller->hint setStringValue:@"已取消本次绑定。"];}
            else if(controller->capturePage==0 && key!=SDLK_UNKNOWN && key!=SDLK_F10){
                hooks->bind_key(edited,(unsigned)controller->captureRow,key);controller->capturing=NO;
                [controller->tables[0] reloadData];[controller->hint setStringValue:@"绑定已更新，点击“保存”后生效。"];}
            return nil;
        }];
        NSTimer *timer=[NSTimer timerWithTimeInterval:0.02 target:controller selector:@selector(tick:) userInfo:nil repeats:YES];
        [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSModalPanelRunLoopMode];int accepted=0;
        for(;;){
            NSInteger response=[alert runModal];if(response!=NSAlertFirstButtonReturn)break;
            if([controller->original state]==NSControlStateValueOn){edited->rows=edited->cols=0;accepted=1;break;}
            NSString *a=[controller->rows stringValue],*b=[controller->cols stringValue];
            NSCharacterSet *invalid=[[NSCharacterSet decimalDigitCharacterSet] invertedSet];
            NSInteger rows=[a integerValue],cols=[b integerValue];
            if([a length]&&[b length]&&[a rangeOfCharacterFromSet:invalid].location==NSNotFound&&[b rangeOfCharacterFromSet:invalid].location==NSNotFound&&rows>=7&&rows<=40&&cols>=15&&cols<=60){edited->rows=(unsigned)rows;edited->cols=(unsigned)cols;accepted=1;break;}
            NSAlert *error=[[[NSAlert alloc] init] autorelease];[error setMessageText:@"请输入 7–40 行、15–60 列。"];[error runModal];
        }
        [timer invalidate];[NSEvent removeMonitor:monitor];[alert release];[controller release];return accepted;
    }
}
