#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <commdlg.h>
static void game_pad_reset(void){}
static POINT test_cursor;
static BOOL test_get_cursor(LPPOINT point){*point=test_cursor;return TRUE;}
#define GetCursorPos test_get_cursor
#include "../windows_menu.inc"
#undef GetCursorPos
static unsigned selected_settings_page=99;
static void test_settings_page(unsigned page){selected_settings_page=page;}
static SDL_atomic_t audio_calls;
static void audio(void *context,Uint8 *data,int length){(void)context;memset(data,0,(size_t)length);SDL_AtomicAdd(&audio_calls,1);}
int main(void){
    SDL_setenv("SDL_AUDIODRIVER","dummy",1);
    assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO));
    SDL_Window *window=SDL_CreateWindow("Drag regression",100,100,320,160,SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE);assert(window);
    assert(!desktop_menu_attach(window));
    assert(GetMenuItemCount(GetMenu(desktop_window))==3);
    HMENU options=GetSubMenu(GetMenu(desktop_window),1);assert(options);
    assert(GetMenuItemID(options,0)==DESKTOP_SETTINGS && GetMenuItemID(options,1)==DESKTOP_VIEW);
    desktop_settings_callback=test_settings_page;
    SendMessageW(desktop_window,WM_COMMAND,DESKTOP_SETTINGS,0);assert(selected_settings_page==0);
    SendMessageW(desktop_window,WM_COMMAND,DESKTOP_VIEW,0);assert(selected_settings_page==2);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_AudioSpec spec;SDL_zero(spec);spec.freq=22050;spec.format=AUDIO_S16SYS;spec.channels=1;spec.samples=256;spec.callback=audio;
    SDL_AudioDeviceID device=SDL_OpenAudioDevice(NULL,0,&spec,NULL,0);assert(device);SDL_PauseAudioDevice(device,0);
    RECT original;GetWindowRect(desktop_window,&original);test_cursor.x=original.left+100;test_cursor.y=original.top+10;
    Uint32 start=SDL_GetTicks();SendMessageW(desktop_window,WM_NCLBUTTONDOWN,HTCAPTION,0);
    assert(SDL_GetTicks()-start<1000 && desktop_drag_hit==HTCAPTION);
    unsigned frames=0;
    for(unsigned i=0;i<30;++i){
        ++test_cursor.x;++test_cursor.y;SendMessageW(desktop_window,WM_MOUSEMOVE,MK_LBUTTON,0);
        SDL_Event e;while(SDL_PollEvent(&e)){}
        SDL_SetRenderDrawColor(renderer,(Uint8)i,0,0,255);SDL_RenderClear(renderer);SDL_RenderPresent(renderer);++frames;SDL_Delay(5);
    }
    RECT moved;GetWindowRect(desktop_window,&moved);assert(moved.left==original.left+30&&moved.top==original.top+30);
    assert(frames==30&&SDL_AtomicGet(&audio_calls)>0);
    SendMessageW(desktop_window,WM_LBUTTONUP,0,0);assert(!desktop_drag_hit);
    SendMessageW(desktop_window,WM_NCLBUTTONDOWN,HTBOTTOMRIGHT,0);test_cursor.x+=40;test_cursor.y+=20;
    SendMessageW(desktop_window,WM_MOUSEMOVE,MK_LBUTTON,0);RECT sized;GetWindowRect(desktop_window,&sized);
    assert(sized.right-sized.left==moved.right-moved.left+40 && sized.bottom-sized.top==moved.bottom-moved.top+20);
    SendMessageW(desktop_window,WM_KEYDOWN,VK_ESCAPE,0);GetWindowRect(desktop_window,&sized);
    assert(!desktop_drag_hit && !memcmp(&sized,&moved,sizeof(sized)));
    SendMessageW(desktop_window,WM_NCLBUTTONDOWN,HTCAPTION,0);SendMessageW(desktop_window,WM_CANCELMODE,0,0);assert(!desktop_drag_hit);
    SDL_CloseAudioDevice(device);desktop_menu_detach();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
    puts("Native move/resize remained nonblocking: 30 rendered frames plus live audio, mouse release, Escape and cancellation passed");return 0;
}
