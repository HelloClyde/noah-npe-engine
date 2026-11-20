#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
static unsigned scenario,picker_calls;
static char test_pref[2048];
static char *native_test_pref(const char *org,const char *app){(void)org;(void)app;return SDL_strdup(test_pref);}
static BOOL WINAPI native_test_picker(LPOPENFILENAMEW args){(void)args;++picker_calls;return FALSE;}
#define SDL_GetPrefPath native_test_pref
#define GetOpenFileNameW native_test_picker
static void check_controls(void *window);
static BOOL test_show(HWND window,int show){
    wchar_t name[80];GetClassNameW(window,name,80);
    return ShowWindow(window,!wcscmp(name,L"NoahNpeSettings")?SW_HIDE:show);
}
#define ShowWindow test_show
#define NPE_NATIVE_SETTINGS_TEST_HOOK(window) check_controls(window)
#define main npe_app_main
#include "../main.c"
#undef main
#undef ShowWindow
static void require_class(HWND window,const wchar_t *expected){
    wchar_t name[80];assert(GetClassNameW(window,name,80));assert(!wcscmp(name,expected));
}
static void check_controls(void *pointer){
    SettingsWindow *s=pointer;require_class(s->tab,WC_TABCONTROLW);require_class(s->list,WC_LISTVIEWW);
    require_class(s->rows,L"Edit");require_class(s->cols,L"Edit");require_class(s->row_spin,UPDOWN_CLASSW);
    assert(GetClassLongPtrW(s->window,GCLP_HICON));
    require_class(s->original,L"Button");assert(TabCtrl_GetItemCount(s->tab)==4);
    if(scenario!=3)assert(ListView_GetItemCount(s->list)==SETTINGS_KEYS);
    LOGFONTW font;assert(GetObjectW(s->font,sizeof(font),&font));assert(!wcscmp(font.lfFaceName,L"Microsoft YaHei UI"));
    if(scenario==3){
        require_class(s->volume,TRACKBAR_CLASSW);
        SendMessageW(s->volume,TBM_SETPOS,TRUE,37);
        SendMessageW(s->window,WM_HSCROLL,TB_THUMBTRACK,(LPARAM)s->volume);
        assert(s->value.volume==37&&game_sound_player->host_attenuation==63);
        SendMessageW(s->sound,BM_SETCHECK,BST_UNCHECKED,0);
        SendMessageW(s->window,WM_COMMAND,SW_SOUND,0);
        assert(s->value.sound_muted&&game_sound_player->host_attenuation==100);
        SendMessageW(s->sound,BM_SETCHECK,BST_CHECKED,0);
        SendMessageW(s->window,WM_COMMAND,SW_SOUND,0);
        assert(!s->value.sound_muted&&game_sound_player->host_attenuation==63);
        SendMessageW(s->window,WM_COMMAND,IDCANCEL,0);
    }else if(scenario==0){
        HIMC original=ImmCreateContext();assert(original);
        HIMC previous=ImmAssociateContext(s->window,original);
        settings_win_capture(s);assert(s->capturing&&s->ime_detached);
        assert(!ImmGetContext(s->window));
        settings_win_proc(s->window,WM_KEYDOWN,VK_PROCESSKEY,0x11L<<16);
        assert(!s->capturing&&!s->ime_detached&&s->value.keys[0]==SDLK_w);
        HIMC restored=ImmGetContext(s->window);assert(restored==original);ImmReleaseContext(s->window,restored);
        settings_win_capture(s);settings_win_proc(s->window,WM_KEYDOWN,VK_ESCAPE,0);
        assert(!s->capturing&&!s->ime_detached);
        settings_win_capture(s);settings_win_page(s,0);assert(!s->capturing&&!s->ime_detached);
        ImmAssociateContext(s->window,previous);ImmDestroyContext(original);
        ListView_SetItemState(s->list,-1,0,LVIS_SELECTED|LVIS_FOCUSED);
        ListView_SetItemState(s->list,10,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
        settings_win_capture(s);settings_win_proc(s->window,WM_KEYDOWN,'Q',0);
        assert(s->value.keys[10]==SDLK_q);
        wchar_t action[64];LVITEMW label={0};label.iSubItem=0;label.pszText=action;label.cchTextMax=64;
        SendMessageW(s->list,LVM_GETITEMTEXTW,10,(LPARAM)&label);assert(wcsstr(action,L"J"));
        settings_win_proc(s->window,WM_COMMAND,IDCANCEL,0);assert(s->done&&!s->accepted);
    }else if(scenario==1){
        TabCtrl_SetCurSel(s->tab,2);NMHDR change={s->tab,SW_TAB,TCN_SELCHANGE};
        settings_win_proc(s->window,WM_NOTIFY,SW_TAB,(LPARAM)&change);assert(s->page==2);
        SendMessageW(s->original,BM_SETCHECK,BST_UNCHECKED,0);
        settings_win_proc(s->window,WM_COMMAND,SW_ORIGINAL,0);assert(IsWindowEnabled(s->rows));
        SetWindowTextW(s->rows,L"14");SetWindowTextW(s->cols,L"25");
        settings_win_proc(s->window,WM_COMMAND,IDOK,0);assert(s->accepted&&s->value.rows==14&&s->value.cols==25);
    }else{
        settings_win_proc(s->window,WM_COMMAND,SW_RESET,0);
        settings_win_proc(s->window,WM_COMMAND,IDOK,0);assert(s->accepted&&!s->value.rows&&s->value.keys[0]==SDLK_UP);
    }
    assert(settings_win_key(VK_LEFT,0)==SDLK_LEFT);
    assert(settings_win_key(VK_RETURN,1L<<24)==SDLK_KP_ENTER);
    assert(settings_win_key(VK_F10,0)==SDLK_F10);
}
int main(int argc,char **argv){
    assert(argc==2);snprintf(test_pref,sizeof(test_pref),"%s/",argv[1]);
    SDL_SetMainReady();assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER));
    SDL_Window *game=SDL_CreateWindow("native settings lifetime",0,0,320,160,SDL_WINDOW_HIDDEN);assert(game);
    Uint32 game_id=SDL_GetWindowID(game);
    game_scale_window=game;
    SDL_Renderer *scale_renderer=SDL_CreateRenderer(game,-1,SDL_RENDERER_SOFTWARE);assert(scale_renderer);
    SDL_RenderSetLogicalSize(scale_renderer,240,120);
    SDL_Texture *sample=SDL_CreateTexture(scale_renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,1,1);assert(sample);
    Uint32 color=0xff123456;assert(!SDL_UpdateTexture(sample,NULL,&color,4));game_scale_texture=sample;
    assert(!desktop_menu_attach(game));
    assert(desktop_scale_menu&&GetMenuItemCount(desktop_scale_menu)==12);
    const unsigned scales[]={1,2,4,10};
    for(unsigned n=0;n<4;++n){unsigned scale=scales[n];
        SendMessageW(desktop_window,WM_COMMAND,DESKTOP_SCALE_BASE+scale,0);
        SDL_Event input;while(game_PollEvent(&input)){}
        int width,height;SDL_GetWindowSize(game,&width,&height);
        assert(width==240*(int)scale&&height==120*(int)scale);
        SDL_Rect middle={width/2,height/2,1,1};Uint32 pixel=0;
        assert(!SDL_RenderReadPixels(scale_renderer,&middle,SDL_PIXELFORMAT_ARGB8888,&pixel,4)&&pixel==color);
        SendMessageW(desktop_window,WM_INITMENUPOPUP,(WPARAM)desktop_scale_menu,0);
        assert(GetMenuState(desktop_scale_menu,DESKTOP_SCALE_BASE+scale,MF_BYCOMMAND)&MF_CHECKED);
    }
    SDL_SetWindowSize(game,777,333);
    SendMessageW(desktop_window,WM_INITMENUPOPUP,(WPARAM)desktop_scale_menu,0);
    assert(GetMenuState(desktop_scale_menu,DESKTOP_SCALE_CUSTOM,MF_BYCOMMAND)&MF_CHECKED);
    SDL_RenderSetLogicalSize(scale_renderer,400,232);
    SendMessageW(desktop_window,WM_COMMAND,DESKTOP_SCALE_BASE+3,0);
    SDL_Event zoom_input;while(game_PollEvent(&zoom_input)){}
    RECT client;GetClientRect(desktop_window,&client);
    assert(client.right==1200&&client.bottom==696&&game_window_scale_current()==3);
    puts("Window-size submenu: 1x/2x/4x/10x commands, checkmarks, custom drag size and expanded 400x232 canvas at 3x passed");
    assert(SendMessageW(desktop_window,WM_GETICON,ICON_BIG,0));
    assert(SendMessageW(desktop_window,WM_GETICON,ICON_SMALL,0));
    /* Drive the real Win32 -> SDL -> remapping path without desktop input. */
    SendMessageW(desktop_window,WM_SETFOCUS,0,0);
    SDL_StartTextInput();assert(SDL_IsTextInputActive());
    game_keyboard_controls();assert(!SDL_IsTextInputActive());
    HIMC active=ImmGetContext(desktop_window);assert(!active);
    HIMC switched=ImmCreateContext();assert(switched);ImmAssociateContext(desktop_window,switched);
    SendMessageW(desktop_window,WM_INPUTLANGCHANGE,0,(LPARAM)GetKeyboardLayout(0));
    assert(!ImmGetContext(desktop_window));ImmDestroyContext(switched);
    settings_defaults(&settings);settings_loaded=1;
    settings_bind_key(&settings,0,SDLK_w);settings_bind_key(&settings,1,SDLK_s);
    settings_bind_key(&settings,2,SDLK_a);settings_bind_key(&settings,3,SDLK_d);
    uint16_t map_mode=1;game_pad_mode=&map_mode;game_input_reset();
    const unsigned scans[]={0x11,0x1f,0x1e,0x20};
    const SDL_Keycode mapped[]={SDLK_UP,SDLK_DOWN,SDLK_LEFT,SDLK_RIGHT};
    for(unsigned i=0;i<4;++i){
        SDL_Event pending;while(SDL_PollEvent(&pending)){}
        LPARAM flags=1L|((LPARAM)scans[i]<<16);
        SendMessageW(desktop_window,WM_KEYDOWN,VK_PROCESSKEY,flags);
        SendMessageW(desktop_window,WM_KEYUP,VK_PROCESSKEY,flags|((LPARAM)1<<30)|((LPARAM)1<<31));
        unsigned down=0,up=0;
        while(game_PollEvent(&pending)){
            if(pending.type==SDL_KEYDOWN&&pending.key.keysym.sym==mapped[i])++down;
            if(pending.type==SDL_KEYUP&&pending.key.keysym.sym==mapped[i])++up;
        }
        assert(down==1&&up==1);
    }
    SDL_StartTextInput();assert(SDL_IsTextInputActive());
    SDL_Event regained;SDL_zero(regained);regained.type=SDL_WINDOWEVENT;
    regained.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;
    assert(game_host_event(&regained)&&!SDL_IsTextInputActive());
    game_pad_mode=NULL;
    puts("Gameplay IME: text composition disabled on creation/focus; real Win32 PROCESSKEY WASD generates mapped SDL down/up events");
    settings_defaults(&settings);NpeSettings edited=settings;
    scenario=0;assert(!settings_windows_dialog(&edited,0));assert(edited.keys[0]==SDLK_UP);
    scenario=1;assert(settings_windows_dialog(&edited,0));assert(edited.rows==14&&edited.cols==25);
    scenario=2;assert(settings_windows_dialog(&edited,0));assert(!edited.rows&&edited.keys[0]==SDLK_UP);
    NpeSoundPlayer audio={0};
    game_sound_player=&audio;
    scenario=3;assert(!settings_windows_dialog(&edited,3));
    assert(edited.volume==100 && !audio.host_attenuation);
    game_sound_player=NULL;
    assert(SDL_GetWindowFromID(game_id)==game);
    SDL_Event event;while(SDL_PollEvent(&event))assert(event.type!=SDL_QUIT);
    desktop_menu_detach();game_scale_window=NULL;game_scale_texture=NULL;SDL_DestroyTexture(sample);SDL_DestroyRenderer(scale_renderer);SDL_DestroyWindow(game);SDL_Quit();
    scenario=1;picker_calls=0;
    char *arguments[]={"npe-sdl","--settings",NULL};assert(!npe_app_main(2,arguments));
    assert(settings_last_saved&&picker_calls==1);
    scenario=0;picker_calls=0;assert(!npe_app_main(2,arguments));assert(!settings_last_saved&&!picker_calls);
    char path[2200];snprintf(path,sizeof(path),"%ssettings.cfg",test_pref);assert(!remove(path));
    puts("Native settings: game window survived, icons assigned, Save opens game picker, Cancel exits standalone settings");
    puts("Native controls: tabs, system font, list, checkbox, numeric spinboxes, binding, Save, Cancel and defaults passed");return 0;
}
