static void (*desktop_settings_callback)(unsigned);
/* Native desktop menus; the game still consumes SDL input events. */
#ifdef _WIN32
#include <SDL_syswm.h>
#include <imm.h>
#include <wchar.h>
static HWND desktop_window;
static int desktop_console_open, desktop_console_parent, desktop_console_cli;
static int desktop_stdout_redirected, desktop_stderr_redirected;

static int desktop_redirected(DWORD stream) {
    DWORD type = GetFileType(GetStdHandle(stream));
    return type == FILE_TYPE_PIPE || type == FILE_TYPE_DISK;
}
static void desktop_console_streams(const char *device) {
    if (!desktop_stdout_redirected) (void)freopen(device, "w", stdout);
    if (!desktop_stderr_redirected) (void)freopen(device, "w", stderr);
}
static void desktop_console_restore_parent(void) {
    desktop_console_parent = desktop_console_cli &&
        (!desktop_stdout_redirected || !desktop_stderr_redirected) &&
        AttachConsole(ATTACH_PARENT_PROCESS);
    desktop_console_streams(desktop_console_parent ? "CONOUT$" : "NUL");
}
static void desktop_console_start(int cli) {
    desktop_console_cli = cli;
    desktop_stdout_redirected = desktop_redirected(STD_OUTPUT_HANDLE);
    desktop_stderr_redirected = desktop_redirected(STD_ERROR_HANDLE);
    desktop_console_restore_parent();
}
static int desktop_console_toggle(void) {
    fflush(stdout);
    fflush(stderr);
    if (desktop_console_open || desktop_console_parent) {
        if (!FreeConsole()) return -1;
        desktop_console_parent = 0;
    }
    if (desktop_console_open) {
        desktop_console_open = 0;
        desktop_console_restore_parent();
        return 0;
    }
    if (!AllocConsole()) {
        desktop_console_restore_parent();
        return -1;
    }
    desktop_console_open = 1;
    desktop_console_streams("CONOUT$");
    SetConsoleTitleW(L"诺亚舟-NPE-引擎 调试终端");
    HWND console = GetConsoleWindow();
    HMENU system_menu = console ? GetSystemMenu(console, FALSE) : NULL;
    if (system_menu) EnableMenuItem(system_menu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
    puts("NPE debug console enabled.");
    fflush(stdout);
    return 0;
}
static WNDPROC desktop_original_proc;
static HIMC desktop_ime_context;
static int desktop_ime_detached;
static void desktop_keyboard_controls(void){
    if(!desktop_window)return;
    HIMC prior=ImmAssociateContext(desktop_window,NULL);
    if(!desktop_ime_detached){desktop_ime_context=prior;desktop_ime_detached=1;}
}
static HMENU desktop_menu,desktop_scale_menu;
static char *desktop_next_game;
static int desktop_drag_hit;
static POINT desktop_drag_origin;
static RECT desktop_drag_rect;
static int desktop_drag_left(int hit){return hit==HTLEFT||hit==HTTOPLEFT||hit==HTBOTTOMLEFT;}
static int desktop_drag_right(int hit){return hit==HTRIGHT||hit==HTTOPRIGHT||hit==HTBOTTOMRIGHT;}
static int desktop_drag_top(int hit){return hit==HTTOP||hit==HTTOPLEFT||hit==HTTOPRIGHT;}
static int desktop_drag_bottom(int hit){return hit==HTBOTTOM||hit==HTBOTTOMLEFT||hit==HTBOTTOMRIGHT;}
static void desktop_drag_end(HWND hwnd){
    desktop_drag_hit=0;
    if(GetCapture()==hwnd)ReleaseCapture();
}
/* Keep SDL's main loop running instead of entering DefWindowProc's modal
   move/size loop. Mouse capture supplies updates until release or cancellation. */
static int desktop_drag_event(HWND hwnd,UINT message,WPARAM wparam){
    if(message==WM_NCLBUTTONDOWN){
        int hit=(int)wparam;
        if(hit!=HTCAPTION && !desktop_drag_left(hit) && !desktop_drag_right(hit) &&
           !desktop_drag_top(hit) && !desktop_drag_bottom(hit))return 0;
        if(IsZoomed(hwnd)){
            if(hit!=HTCAPTION)return 0;
            POINT cursor;RECT before;GetCursorPos(&cursor);GetWindowRect(hwnd,&before);
            ShowWindow(hwnd,SW_RESTORE);
            RECT after;GetWindowRect(hwnd,&after);
            LONG width=after.right-after.left,old_width=before.right-before.left;
            LONG offset=old_width>0?MulDiv(cursor.x-before.left,width,old_width):width/2;
            SetWindowPos(hwnd,NULL,cursor.x-offset,cursor.y-GetSystemMetrics(SM_CYCAPTION)/2,
                0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
        }
        GetCursorPos(&desktop_drag_origin);GetWindowRect(hwnd,&desktop_drag_rect);
        desktop_drag_hit=hit;SetCapture(hwnd);return 1;
    }
    if(!desktop_drag_hit)return 0;
    if(message==WM_MOUSEMOVE){
        POINT cursor;GetCursorPos(&cursor);
        LONG dx=cursor.x-desktop_drag_origin.x,dy=cursor.y-desktop_drag_origin.y;
        RECT rect=desktop_drag_rect;
        if(desktop_drag_hit==HTCAPTION){OffsetRect(&rect,dx,dy);}
        else{
            if(desktop_drag_left(desktop_drag_hit))rect.left+=dx;
            if(desktop_drag_right(desktop_drag_hit))rect.right+=dx;
            if(desktop_drag_top(desktop_drag_hit))rect.top+=dy;
            if(desktop_drag_bottom(desktop_drag_hit))rect.bottom+=dy;
            MINMAXINFO limits;memset(&limits,0,sizeof(limits));
            SendMessageW(hwnd,WM_GETMINMAXINFO,0,(LPARAM)&limits);
            LONG min_width=limits.ptMinTrackSize.x,min_height=limits.ptMinTrackSize.y;
            if(rect.right-rect.left<min_width){
                if(desktop_drag_left(desktop_drag_hit))rect.left=rect.right-min_width;
                else rect.right=rect.left+min_width;
            }
            if(rect.bottom-rect.top<min_height){
                if(desktop_drag_top(desktop_drag_hit))rect.top=rect.bottom-min_height;
                else rect.bottom=rect.top+min_height;
            }
        }
        SetWindowPos(hwnd,NULL,rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,
            SWP_NOZORDER|SWP_NOACTIVATE);return 1;
    }
    if(message==WM_LBUTTONUP || message==WM_CANCELMODE || message==WM_CAPTURECHANGED){
        desktop_drag_end(hwnd);return message==WM_LBUTTONUP;
    }
    if(message==WM_KEYDOWN && wparam==VK_ESCAPE){
        RECT r=desktop_drag_rect;
        SetWindowPos(hwnd,NULL,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOZORDER|SWP_NOACTIVATE);
        desktop_drag_end(hwnd);return 1;
    }
    return 0;
}
enum { DESKTOP_OPEN=41001,DESKTOP_EXIT,DESKTOP_ABOUT,DESKTOP_SETTINGS,DESKTOP_VIEW,DESKTOP_AUDIO,DESKTOP_CONSOLE,DESKTOP_SCALE_BASE=41100,DESKTOP_SCALE_CUSTOM=41120 };
static char *desktop_choose_game(HWND owner){
    WCHAR path[32768]={0};OPENFILENAMEW dialog;
    memset(&dialog,0,sizeof(dialog));dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=owner;
    dialog.lpstrFilter=L"NPE games (*.npe)\0*.npe\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile=path;dialog.nMaxFile=32768;
    dialog.lpstrTitle=L"\u9009\u62e9 NPE \u6e38\u620f";
    dialog.Flags=OFN_EXPLORER|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_HIDEREADONLY;
    if(!GetOpenFileNameW(&dialog)){
        if(CommDlgExtendedError())MessageBoxW(owner,L"\u65e0\u6cd5\u6253\u5f00\u6587\u4ef6\u9009\u62e9\u7a97\u53e3\u3002",L"NPE",MB_OK|MB_ICONERROR);
        return NULL;
    }
    int length=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,path,-1,NULL,0,NULL,NULL);
    char *selected=length>0?malloc((size_t)length):NULL;
    if(!selected || !WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,path,-1,selected,length,NULL,NULL)){
        free(selected);return NULL;
    }
    return selected;
}
static void desktop_quit(void){SDL_Event event;SDL_zero(event);event.type=SDL_QUIT;SDL_PushEvent(&event);}
static LRESULT CALLBACK desktop_window_proc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam){
    if(desktop_drag_event(hwnd,message,wparam))return 0;
    if(message==WM_INITMENUPOPUP && (HMENU)wparam==desktop_scale_menu){
        unsigned selected=game_window_scale_current();
        for(unsigned i=1;i<=10;++i)CheckMenuItem(desktop_scale_menu,DESKTOP_SCALE_BASE+i,MF_BYCOMMAND|(selected==i?MF_CHECKED:MF_UNCHECKED));
        CheckMenuItem(desktop_scale_menu,DESKTOP_SCALE_CUSTOM,MF_BYCOMMAND|(!selected?MF_CHECKED:MF_UNCHECKED));
    }
    if(message==WM_COMMAND && HIWORD(wparam)==0){
        unsigned command=LOWORD(wparam);
        if(command>DESKTOP_SCALE_BASE&&command<=DESKTOP_SCALE_BASE+10){game_window_scale_request(command-DESKTOP_SCALE_BASE);return 0;}
        switch(LOWORD(wparam)){
        case DESKTOP_OPEN:{
            char *selected=desktop_choose_game(hwnd);
            if(selected){free(desktop_next_game);desktop_next_game=selected;game_pad_reset();desktop_quit();}
            return 0;
        }
        case DESKTOP_EXIT:desktop_quit();return 0;
        case DESKTOP_SETTINGS:if(desktop_settings_callback)desktop_settings_callback(0);return 0;
        case DESKTOP_VIEW:if(desktop_settings_callback)desktop_settings_callback(2);return 0;
        case DESKTOP_AUDIO:if(desktop_settings_callback)desktop_settings_callback(3);return 0;
        case DESKTOP_CONSOLE:
            if(desktop_console_toggle())
                MessageBoxW(hwnd,L"无法切换调试终端。",L"调试终端",MB_OK|MB_ICONERROR);
            CheckMenuItem(desktop_menu,DESKTOP_CONSOLE,MF_BYCOMMAND|(desktop_console_open?MF_CHECKED:MF_UNCHECKED));
            return 0;
        case DESKTOP_ABOUT:
            MessageBoxW(hwnd,L"\u8bfa\u4e9a\u821f-NPE-\u5f15\u64ce\r\n\r\n\u4f5c\u8005\uff1aHelloClyde\r\n\r\n\u7eaf C11 + SDL2 \u5b9e\u73b0\u7684 NPE \u6e38\u620f\u5f15\u64ce\u3002",
                L"\u5173\u4e8e\u8bfa\u4e9a\u821f-NPE-\u5f15\u64ce",MB_OK|MB_ICONINFORMATION);
            game_pad_reset();return 0;
        }
    }
    LRESULT result=CallWindowProcW(desktop_original_proc,hwnd,message,wparam,lparam);
    if(message==WM_SETFOCUS || message==WM_INPUTLANGCHANGE)desktop_keyboard_controls();
    return result;
}
static int desktop_menu_attach(SDL_Window *window){
    SDL_SysWMinfo info;SDL_VERSION(&info.version);
    if(!SDL_GetWindowWMInfo(window,&info) || info.subsystem!=SDL_SYSWM_WINDOWS)return 0;
    HWND hwnd=info.info.win.window;
    HICON large=(HICON)LoadImageW(GetModuleHandleW(NULL),MAKEINTRESOURCEW(101),IMAGE_ICON,
        GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON),LR_SHARED);
    HICON small=(HICON)LoadImageW(GetModuleHandleW(NULL),MAKEINTRESOURCEW(101),IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED);
    if(large)SendMessageW(hwnd,WM_SETICON,ICON_BIG,(LPARAM)large);
    if(small)SendMessageW(hwnd,WM_SETICON,ICON_SMALL,(LPARAM)small);
    HMENU bar=CreateMenu(),file=CreatePopupMenu(),about=CreatePopupMenu(),options=CreatePopupMenu();
    if(!bar||!file||!about||!options){if(options)DestroyMenu(options);if(bar)DestroyMenu(bar);if(file)DestroyMenu(file);if(about)DestroyMenu(about);return -1;}
    AppendMenuW(file,MF_STRING,DESKTOP_OPEN,L"\u6253\u5f00 NPE...(&O)");
    AppendMenuW(file,MF_SEPARATOR,0,NULL);AppendMenuW(file,MF_STRING,DESKTOP_EXIT,L"\u9000\u51fa(&X)");
    AppendMenuW(options,MF_STRING,DESKTOP_SETTINGS,L"\u6309\u952e\u6620\u5c04...\tF10");
    AppendMenuW(options,MF_STRING,DESKTOP_VIEW,L"\u5730\u56fe\u89c6\u91ce...");
    HMENU zoom=CreatePopupMenu();
    if(!zoom){DestroyMenu(bar);DestroyMenu(file);DestroyMenu(about);DestroyMenu(options);return -1;}
    for(unsigned i=1;i<=10;++i){wchar_t text[16];swprintf(text,16,L"%u×",i);AppendMenuW(zoom,MF_STRING,DESKTOP_SCALE_BASE+i,text);}
    AppendMenuW(zoom,MF_SEPARATOR,0,NULL);AppendMenuW(zoom,MF_STRING|MF_GRAYED,DESKTOP_SCALE_CUSTOM,L"自定义大小");
    AppendMenuW(options,MF_POPUP,(UINT_PTR)zoom,L"窗口大小");
    AppendMenuW(options,MF_STRING,DESKTOP_AUDIO,L"声音…");
    AppendMenuW(options,MF_STRING|(desktop_console_open?MF_CHECKED:MF_UNCHECKED),
                DESKTOP_CONSOLE,L"显示调试终端");
    AppendMenuW(about,MF_STRING,DESKTOP_ABOUT,L"\u5173\u4e8e\u8bfa\u4e9a\u821f-NPE-\u5f15\u64ce...");
    AppendMenuW(bar,MF_POPUP,(UINT_PTR)file,L"\u6587\u4ef6(&F)");
    AppendMenuW(bar,MF_POPUP,(UINT_PTR)options,L"\u8bbe\u7f6e(&S)");
    AppendMenuW(bar,MF_POPUP,(UINT_PTR)about,L"\u5173\u4e8e(&A)");
    if(!SetMenu(hwnd,bar)){DestroyMenu(bar);return -1;}
    desktop_window=hwnd;desktop_menu=bar;desktop_scale_menu=zoom;
    desktop_original_proc=(WNDPROC)SetWindowLongPtrW(hwnd,GWLP_WNDPROC,(LONG_PTR)desktop_window_proc);
    desktop_keyboard_controls();
    DrawMenuBar(hwnd);SDL_SetWindowSize(window,960,480);return 0;
}
static void desktop_menu_detach(void){
    if(desktop_window){
        desktop_drag_end(desktop_window);
        if(desktop_ime_detached)ImmAssociateContext(desktop_window,desktop_ime_context);
        desktop_ime_detached=0;desktop_ime_context=NULL;
        if(desktop_original_proc)SetWindowLongPtrW(desktop_window,GWLP_WNDPROC,(LONG_PTR)desktop_original_proc);
        SetMenu(desktop_window,NULL);
    }
    if(desktop_menu)DestroyMenu(desktop_menu);
    desktop_window=NULL;desktop_original_proc=NULL;desktop_menu=NULL;desktop_scale_menu=NULL;
}
#else
static void desktop_keyboard_controls(void){}
static int desktop_menu_attach(SDL_Window *window){(void)window;return 0;}
static void desktop_menu_detach(void){}
#endif
