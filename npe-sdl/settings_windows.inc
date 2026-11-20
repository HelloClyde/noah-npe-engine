#include <commctrl.h>
#include <imm.h>
#include <SDL_syswm.h>
enum { SW_TAB=5100,SW_LIST,SW_CHANGE,SW_ORIGINAL,SW_ROWS,SW_COLS,SW_ROW_SPIN,
    SW_COL_SPIN,SW_RESET,SW_HINT,SW_ROW_LABEL,SW_COL_LABEL,SW_NOTE,SW_SOUND,SW_VOLUME,SW_VOLUME_LABEL };
typedef struct {
    HWND window,owner,tab,list,change,original,rows,cols,hint,row_label,col_label,note;
    HWND row_spin,col_spin,sound,volume,volume_label;
    HIMC capture_ime;int ime_detached;
    HFONT font;NpeSettings value;unsigned page;int capturing,done,accepted,scale;
} SettingsWindow;
static const wchar_t *settings_win_labels[]={L"上",L"下",L"左",L"右",L"确认",L"返回 / 菜单",
    L"对话 / 翻页",L"卸装",L"说明上一页",L"说明下一页",L"按键说明",L"选项左（J）",L"选项右（L）"};
static int settings_win_px(SettingsWindow *s,int value){return MulDiv(value,s->scale,96);}
static HWND settings_win_control(SettingsWindow *s,DWORD ex,const wchar_t *class_name,
    const wchar_t *text,DWORD style,int x,int y,int width,int height,int id){
    HWND control=CreateWindowExW(ex,class_name,text,WS_CHILD|WS_VISIBLE|style,
        settings_win_px(s,x),settings_win_px(s,y),settings_win_px(s,width),settings_win_px(s,height),
        s->window,(HMENU)(INT_PTR)id,GetModuleHandleW(NULL),NULL);
    SendMessageW(control,WM_SETFONT,(WPARAM)s->font,TRUE);return control;
}
static void settings_win_utf8(const char *text,wchar_t *out,int size){
    if(!text || !MultiByteToWideChar(CP_UTF8,0,text,-1,out,size))out[0]=0;
}
static void settings_win_list(SettingsWindow *s){
    ListView_DeleteAllItems(s->list);
    unsigned count=s->page==1?SETTINGS_BUTTONS:SETTINGS_KEYS;
    for(unsigned i=0;i<count;++i){
        LVITEMW item;memset(&item,0,sizeof(item));item.mask=LVIF_TEXT;item.iItem=(int)i;
        item.pszText=(wchar_t *)settings_win_labels[s->page==0&&i>=10?i+1:i];
        SendMessageW(s->list,LVM_INSERTITEMW,0,(LPARAM)&item);
        const char *name=s->page==1?SDL_GameControllerGetStringForButton((SDL_GameControllerButton)s->value.buttons[i]):SDL_GetKeyName(s->value.keys[i]);
        wchar_t binding[128];settings_win_utf8(name,binding,128);item.iSubItem=1;item.pszText=binding;
        SendMessageW(s->list,LVM_SETITEMTEXTW,i,(LPARAM)&item);
    }
    ListView_SetItemState(s->list,0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
}
static void settings_win_map_enable(SettingsWindow *s){
    int enabled=SendMessageW(s->original,BM_GETCHECK,0,0)!=BST_CHECKED;
    HWND controls[]={s->rows,s->cols,s->row_spin,s->col_spin,s->row_label,s->col_label};
    for(unsigned i=0;i<sizeof(controls)/sizeof(controls[0]);++i)EnableWindow(controls[i],enabled);
}
static void settings_win_capture_end(SettingsWindow *s){
    if(s->ime_detached){ImmAssociateContext(s->window,s->capture_ime);s->ime_detached=0;}
    s->capturing=0;
}
static void settings_win_audio_preview(SettingsWindow *s){
    wchar_t label[32];
    swprintf(label,32,L"音量 %u%%",s->value.volume);
    SetWindowTextW(s->volume_label,label);
    EnableWindow(s->volume,!s->value.sound_muted);
    npe_sound_set_volume(game_sound_player,s->value.sound_muted?0:s->value.volume);
}
static void settings_win_page(SettingsWindow *s,unsigned page){
    settings_win_capture_end(s);s->page=page;int map=page==2,sound=page==3;
    ShowWindow(s->list,(map||sound)?SW_HIDE:SW_SHOW);ShowWindow(s->change,(map||sound)?SW_HIDE:SW_SHOW);
    HWND controls[]={s->original,s->rows,s->cols,s->row_spin,s->col_spin,s->row_label,s->col_label,s->note};
    for(unsigned i=0;i<sizeof(controls)/sizeof(controls[0]);++i)ShowWindow(controls[i],map?SW_SHOW:SW_HIDE);
    ShowWindow(s->sound,sound?SW_SHOW:SW_HIDE);ShowWindow(s->volume,sound?SW_SHOW:SW_HIDE);
    ShowWindow(s->volume_label,sound?SW_SHOW:SW_HIDE);
    SetWindowTextW(s->hint,sound?L"调整会立即生效，取消可恢复。":map?L"设置地图显示的 tile 行数和列数。":L"选中功能后点击“更改绑定”，或双击当前绑定。");
    EnableWindow(s->change,TRUE);if(!map&&!sound)settings_win_list(s);settings_win_map_enable(s);
}
static void settings_win_capture(SettingsWindow *s){
    if(ListView_GetNextItem(s->list,-1,LVNI_SELECTED)<0)return;
    if(!s->ime_detached){s->capture_ime=ImmAssociateContext(s->window,NULL);s->ime_detached=1;}
    s->capturing=1;SetFocus(s->window);EnableWindow(s->change,FALSE);
    SetWindowTextW(s->hint,s->page==1?L"请按手柄按钮；Esc 取消。重复绑定会交换。":L"请按新按键；Esc 取消。F10 为保留键。");
}
static SDL_Keycode settings_win_key(WPARAM vk,LPARAM flags){
    if(vk==VK_PROCESSKEY){UINT scan=(UINT)((flags>>16)&255);if(flags&(1L<<24))scan|=0xe000;vk=MapVirtualKeyW(scan,MAPVK_VSC_TO_VK_EX);}
    if(vk>='A'&&vk<='Z')return SDLK_a+(SDL_Keycode)(vk-'A');
    if(vk>='0'&&vk<='9')return SDLK_0+(SDL_Keycode)(vk-'0');
    if(vk>=VK_F1&&vk<=VK_F12)return SDLK_F1+(SDL_Keycode)(vk-VK_F1);
    if(vk>=VK_F13&&vk<=VK_F24)return SDLK_F13+(SDL_Keycode)(vk-VK_F13);
    if(vk>=VK_NUMPAD1&&vk<=VK_NUMPAD9)return SDLK_KP_1+(SDL_Keycode)(vk-VK_NUMPAD1);
    switch(vk){
        case VK_UP:return SDLK_UP;case VK_DOWN:return SDLK_DOWN;case VK_LEFT:return SDLK_LEFT;case VK_RIGHT:return SDLK_RIGHT;
        case VK_RETURN:return (flags&(1L<<24))?SDLK_KP_ENTER:SDLK_RETURN;
        case VK_SPACE:return SDLK_SPACE;case VK_TAB:return SDLK_TAB;case VK_BACK:return SDLK_BACKSPACE;
        case VK_DELETE:return SDLK_DELETE;case VK_INSERT:return SDLK_INSERT;case VK_HOME:return SDLK_HOME;case VK_END:return SDLK_END;
        case VK_PRIOR:return SDLK_PAGEUP;case VK_NEXT:return SDLK_PAGEDOWN;
        case VK_SHIFT:return ((flags>>16)&255)==0x36?SDLK_RSHIFT:SDLK_LSHIFT;
        case VK_CONTROL:return (flags&(1L<<24))?SDLK_RCTRL:SDLK_LCTRL;
        case VK_MENU:return (flags&(1L<<24))?SDLK_RALT:SDLK_LALT;
        case VK_NUMPAD0:return SDLK_KP_0;case VK_DECIMAL:return SDLK_KP_PERIOD;
        case VK_ADD:return SDLK_KP_PLUS;case VK_SUBTRACT:return SDLK_KP_MINUS;case VK_MULTIPLY:return SDLK_KP_MULTIPLY;
        case VK_DIVIDE:return SDLK_KP_DIVIDE;case VK_CAPITAL:return SDLK_CAPSLOCK;
        default:{UINT character=MapVirtualKeyW((UINT)vk,MAPVK_VK_TO_CHAR)&0x7fffffff;
            return character>0&&character<128?(SDL_Keycode)character:SDLK_UNKNOWN;}
    }
}
static void settings_win_bound(SettingsWindow *s,int number){
    int selected=ListView_GetNextItem(s->list,-1,LVNI_SELECTED);if(selected<0)return;
    if(s->page==1)settings_bind_button(&s->value,(unsigned)selected,(Uint8)number);
    else settings_bind_key(&s->value,(unsigned)selected,(SDL_Keycode)number);
    settings_win_list(s);ListView_SetItemState(s->list,selected,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
    settings_win_capture_end(s);EnableWindow(s->change,TRUE);SetFocus(s->list);
    SetWindowTextW(s->hint,L"绑定已更新；点击“保存”后生效。");
}
static LRESULT CALLBACK settings_win_proc(HWND window,UINT message,WPARAM wp,LPARAM lp){
    SettingsWindow *s=(SettingsWindow *)GetWindowLongPtrW(window,GWLP_USERDATA);
    if(message==WM_NCCREATE){s=((CREATESTRUCTW *)lp)->lpCreateParams;s->window=window;SetWindowLongPtrW(window,GWLP_USERDATA,(LONG_PTR)s);}
    if(!s)return DefWindowProcW(window,message,wp,lp);
    if(message==WM_KEYDOWN||message==WM_SYSKEYDOWN){
        if(s->capturing){
            if(lp&(1L<<30))return 0;
            if(wp==VK_ESCAPE){settings_win_capture_end(s);EnableWindow(s->change,TRUE);SetFocus(s->list);SetWindowTextW(s->hint,L"已取消本次绑定。");}
            else if(s->page==0){SDL_Keycode key=settings_win_key(wp,lp);if(key && key!=SDLK_F10)settings_win_bound(s,key);}
            return 0;
        }
    }
    if(message==WM_NOTIFY){
        NMHDR *note=(NMHDR *)lp;
        if(note->idFrom==SW_TAB && note->code==TCN_SELCHANGE){settings_win_page(s,(unsigned)TabCtrl_GetCurSel(s->tab));return 0;}
        if(note->idFrom==SW_LIST && (note->code==NM_DBLCLK||note->code==NM_RETURN)){settings_win_capture(s);return 0;}
    }
    if(message==WM_HSCROLL&&(HWND)lp==s->volume&&s->volume){
        s->value.volume=(unsigned)SendMessageW(s->volume,TBM_GETPOS,0,0);
        settings_win_audio_preview(s);
        return 0;
    }
    if(message==WM_COMMAND){
        switch(LOWORD(wp)){
            case SW_CHANGE:settings_win_capture(s);return 0;
            case SW_ORIGINAL:settings_win_map_enable(s);return 0;
            case SW_SOUND:
                s->value.sound_muted=SendMessageW(s->sound,BM_GETCHECK,0,0)!=BST_CHECKED;
                settings_win_audio_preview(s);
                return 0;
            case SW_RESET:settings_defaults(&s->value);SendMessageW(s->original,BM_SETCHECK,BST_CHECKED,0);
                SendMessageW(s->sound,BM_SETCHECK,BST_CHECKED,0);
                SendMessageW(s->volume,TBM_SETPOS,TRUE,s->value.volume);
                settings_win_audio_preview(s);
                SetWindowTextW(s->rows,L"14");SetWindowTextW(s->cols,L"25");settings_win_page(s,s->page);return 0;
            case IDOK:{
                if(SendMessageW(s->original,BM_GETCHECK,0,0)==BST_CHECKED)s->value.rows=s->value.cols=0;
                else{BOOL a,b;s->value.rows=GetDlgItemInt(window,SW_ROWS,&a,FALSE);s->value.cols=GetDlgItemInt(window,SW_COLS,&b,FALSE);
                    if(!a||!b||s->value.rows<7||s->value.rows>40||s->value.cols<15||s->value.cols>60){
                        MessageBoxW(window,L"请输入 7–40 行、15–60 列。",L"地图视野",MB_OK|MB_ICONINFORMATION);return 0;}}
                s->accepted=1;s->done=1;return 0;
            }
            case IDCANCEL:s->done=1;return 0;
        }
    }
    if(message==WM_CLOSE){s->done=1;return 0;}
    return DefWindowProcW(window,message,wp,lp);
}
static int settings_windows_dialog(NpeSettings *edited,unsigned page){
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_LISTVIEW_CLASSES|ICC_TAB_CLASSES|ICC_UPDOWN_CLASS|ICC_BAR_CLASSES};InitCommonControlsEx(&common);
    WNDCLASSW cls;memset(&cls,0,sizeof(cls));cls.lpfnWndProc=settings_win_proc;cls.hInstance=GetModuleHandleW(NULL);
    cls.hIcon=LoadIconW(GetModuleHandleW(NULL),MAKEINTRESOURCEW(101));
    cls.hCursor=LoadCursorW(NULL,MAKEINTRESOURCEW(32512));cls.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);cls.lpszClassName=L"NoahNpeSettings";
    RegisterClassW(&cls);
    SettingsWindow s;memset(&s,0,sizeof(s));s.value=*edited;s.page=page;s.scale=96;
    SDL_Window *parent=SDL_GetKeyboardFocus();SDL_SysWMinfo info;SDL_VERSION(&info.version);
    if(parent&&SDL_GetWindowWMInfo(parent,&info)&&info.subsystem==SDL_SYSWM_WINDOWS)s.owner=info.info.win.window;
    HDC dc=GetDC(s.owner);if(dc){s.scale=GetDeviceCaps(dc,LOGPIXELSY);ReleaseDC(s.owner,dc);}
    s.font=CreateFontW(-MulDiv(10,s.scale,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
    RECT size={0,0,settings_win_px(&s,620),settings_win_px(&s,490)};AdjustWindowRectEx(&size,WS_CAPTION|WS_SYSMENU,FALSE,WS_EX_DLGMODALFRAME);
    s.window=CreateWindowExW(WS_EX_DLGMODALFRAME,L"NoahNpeSettings",L"设置 — 诺亚舟-NPE-引擎",
        WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,size.right-size.left,size.bottom-size.top,s.owner,NULL,GetModuleHandleW(NULL),&s);
    if(!s.window){DeleteObject(s.font);return 0;}
    s.tab=settings_win_control(&s,0,WC_TABCONTROLW,L"",WS_TABSTOP,16,14,588,34,SW_TAB);
    const wchar_t *tabs[]={L"键盘",L"手柄",L"地图视野",L"声音"};
    for(unsigned i=0;i<sizeof(tabs)/sizeof(*tabs);++i){TCITEMW item;memset(&item,0,sizeof(item));item.mask=TCIF_TEXT;item.pszText=(wchar_t *)tabs[i];SendMessageW(s.tab,TCM_INSERTITEMW,i,(LPARAM)&item);}
    s.hint=settings_win_control(&s,0,L"STATIC",L"",0,20,60,575,34,SW_HINT);
    s.list=settings_win_control(&s,WS_EX_CLIENTEDGE,WC_LISTVIEWW,L"",WS_TABSTOP|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,20,100,575,280,SW_LIST);
    ListView_SetExtendedListViewStyle(s.list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_GRIDLINES);
    LVCOLUMNW column;memset(&column,0,sizeof(column));column.mask=LVCF_TEXT|LVCF_WIDTH;
    column.pszText=L"功能";column.cx=settings_win_px(&s,265);SendMessageW(s.list,LVM_INSERTCOLUMNW,0,(LPARAM)&column);
    column.pszText=L"当前按键 / 按钮";column.cx=settings_win_px(&s,285);SendMessageW(s.list,LVM_INSERTCOLUMNW,1,(LPARAM)&column);
    s.change=settings_win_control(&s,0,L"BUTTON",L"更改绑定…",WS_TABSTOP|BS_PUSHBUTTON,440,392,155,32,SW_CHANGE);
    s.original=settings_win_control(&s,0,L"BUTTON",L"使用原始地图视野",WS_TABSTOP|BS_AUTOCHECKBOX,26,106,300,28,SW_ORIGINAL);
    s.row_label=settings_win_control(&s,0,L"STATIC",L"显示行数（7–40）",0,26,160,210,26,SW_ROW_LABEL);
    s.rows=settings_win_control(&s,WS_EX_CLIENTEDGE,L"EDIT",L"",WS_TABSTOP|ES_NUMBER|ES_AUTOHSCROLL,250,156,120,30,SW_ROWS);
    s.row_spin=settings_win_control(&s,0,UPDOWN_CLASSW,L"",UDS_ALIGNRIGHT|UDS_SETBUDDYINT|UDS_ARROWKEYS,350,156,20,30,SW_ROW_SPIN);
    SendMessageW(s.row_spin,UDM_SETBUDDY,(WPARAM)s.rows,0);SendMessageW(s.row_spin,UDM_SETRANGE32,7,40);
    s.col_label=settings_win_control(&s,0,L"STATIC",L"显示列数（15–60）",0,26,210,210,26,SW_COL_LABEL);
    s.cols=settings_win_control(&s,WS_EX_CLIENTEDGE,L"EDIT",L"",WS_TABSTOP|ES_NUMBER|ES_AUTOHSCROLL,250,206,120,30,SW_COLS);
    s.col_spin=settings_win_control(&s,0,UPDOWN_CLASSW,L"",UDS_ALIGNRIGHT|UDS_SETBUDDYINT|UDS_ARROWKEYS,350,206,20,30,SW_COL_SPIN);
    SendMessageW(s.col_spin,UDM_SETBUDDY,(WPARAM)s.cols,0);SendMessageW(s.col_spin,UDM_SETRANGE32,15,60);
    s.note=settings_win_control(&s,0,L"STATIC",L"仅扩大地图显示范围，不改变游戏规则。\r\n菜单随视野定位，对话区贴底显示。",0,26,270,540,70,SW_NOTE);
    SendMessageW(s.original,BM_SETCHECK,s.value.rows?BST_UNCHECKED:BST_CHECKED,0);
    SetDlgItemInt(s.window,SW_ROWS,s.value.rows?s.value.rows:14,FALSE);SetDlgItemInt(s.window,SW_COLS,s.value.cols?s.value.cols:25,FALSE);
    settings_win_control(&s,0,L"BUTTON",L"恢复默认",WS_TABSTOP|BS_PUSHBUTTON,20,444,120,32,SW_RESET);
    settings_win_control(&s,0,L"BUTTON",L"保存",WS_TABSTOP|BS_DEFPUSHBUTTON,365,444,110,32,IDOK);
    settings_win_control(&s,0,L"BUTTON",L"取消",WS_TABSTOP|BS_PUSHBUTTON,485,444,110,32,IDCANCEL);
    s.sound=settings_win_control(&s,0,L"BUTTON",L"开启声音",WS_TABSTOP|BS_AUTOCHECKBOX,26,106,300,28,SW_SOUND);
    s.volume_label=settings_win_control(&s,0,L"STATIC",L"",0,26,156,300,26,SW_VOLUME_LABEL);
    s.volume=settings_win_control(&s,0,TRACKBAR_CLASSW,L"",WS_TABSTOP|TBS_AUTOTICKS,26,189,510,40,SW_VOLUME);
    SendMessageW(s.sound,BM_SETCHECK,s.value.sound_muted?BST_UNCHECKED:BST_CHECKED,0);
    SendMessageW(s.volume,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));
    SendMessageW(s.volume,TBM_SETTICFREQ,10,0);
    SendMessageW(s.volume,TBM_SETPOS,TRUE,s.value.volume);
    settings_win_audio_preview(&s);
    TabCtrl_SetCurSel(s.tab,(int)page);settings_win_page(&s,page);
#ifdef NPE_NATIVE_SETTINGS_TEST_HOOK
    NPE_NATIVE_SETTINGS_TEST_HOOK(&s);
#endif
    if(s.owner)EnableWindow(s.owner,FALSE);
    ShowWindow(s.window,SW_SHOW);SetForegroundWindow(s.window);SetFocus(page==3?s.sound:page==2?s.original:s.list);
    while(!s.done){
        MSG msg;while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){
            if(msg.message==WM_QUIT){SDL_Event quit;SDL_zero(quit);quit.type=SDL_QUIT;SDL_PushEvent(&quit);s.done=1;break;}
            if(s.capturing && (msg.message==WM_KEYDOWN||msg.message==WM_SYSKEYDOWN) &&
               (msg.hwnd==s.window||IsChild(s.window,msg.hwnd))){settings_win_proc(s.window,msg.message,msg.wParam,msg.lParam);continue;}
            if(msg.message==WM_KEYDOWN && (msg.hwnd==s.window||IsChild(s.window,msg.hwnd))){
                if(msg.wParam==VK_ESCAPE){s.done=1;continue;}
                if(msg.wParam==VK_RETURN){
                    if(msg.lParam&(1L<<30))continue;
                    HWND focus=GetFocus();
                    if(focus==s.list)settings_win_capture(&s);
                    else{
                        wchar_t name[32]={0};GetClassNameW(focus,name,32);
                        LONG_PTR type=GetWindowLongPtrW(focus,GWL_STYLE)&BS_TYPEMASK;
                        if(!wcscmp(name,L"Button") && (type==BS_PUSHBUTTON||type==BS_DEFPUSHBUTTON))SendMessageW(focus,BM_CLICK,0,0);
                        else SendMessageW(s.window,WM_COMMAND,IDOK,0);
                    }
                    continue;
                }
            }
            if(!IsDialogMessageW(s.window,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
        }
        if(s.done)break;
        int button=settings_native_poll_button();if(button==-2){s.done=1;break;}
        if(s.capturing && s.page==1 && button>=0 && GetForegroundWindow()==s.window)settings_win_bound(&s,button);
        SDL_Delay(10);
    }
    if(s.owner)EnableWindow(s.owner,TRUE);
    settings_win_capture_end(&s);DestroyWindow(s.window);DeleteObject(s.font);
    if(s.owner)SetForegroundWindow(s.owner);
    if(s.accepted)*edited=s.value;
    else npe_sound_set_volume(game_sound_player,edited->sound_muted?0:edited->volume);
    return s.accepted;
}
