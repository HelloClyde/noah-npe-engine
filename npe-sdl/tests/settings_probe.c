#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir_test(path) _mkdir(path)
#define rmdir_test(path) _rmdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#define mkdir_test(path) mkdir(path,0700)
#define rmdir_test(path) rmdir(path)
#endif
static char folder[2048];
static char *test_pref_path(const char *org,const char *app){(void)org;(void)app;return SDL_strdup(folder);}
#define SDL_GetPrefPath test_pref_path
#include "../settings.inc"
#undef SDL_GetPrefPath
#define NPE_CONTROLLER_BUTTON_MAP(button) settings_pad_button(button)
#include "../controller_input.inc"
int main(void){
    SDL_SetMainReady();assert(!SDL_Init(SDL_INIT_EVENTS|SDL_INIT_GAMECONTROLLER));
    const char *temp=SDL_getenv("TEMP");if(!temp)temp="/tmp";
    snprintf(folder,sizeof(folder),"%s/npe-settings-%llu/",temp,(unsigned long long)SDL_GetPerformanceCounter());
    assert(!mkdir_test(folder));settings_load();
    assert(settings.keys[0]==SDLK_UP && !settings.rows);
    settings_bind_key(&settings,0,SDLK_w);settings_bind_key(&settings,1,SDLK_s);
    settings_bind_key(&settings,2,SDLK_a);settings_bind_key(&settings,3,SDLK_d);
    assert(settings_keyboard(SDLK_w,1)==SDLK_UP);
    assert(settings_keyboard(SDLK_UP,1)==SDLK_UNKNOWN);
    assert(settings_keyboard(SDLK_a,8)==SDLK_a);
    settings_bind_key(&settings,0,SDLK_s);assert(settings.keys[1]==SDLK_w);
    settings_bind_button(&settings,4,SDL_CONTROLLER_BUTTON_X);
    assert(settings.buttons[6]==SDL_CONTROLLER_BUTTON_A);
    assert(settings_pad_button(SDL_CONTROLLER_BUTTON_X)==SDL_CONTROLLER_BUTTON_A);
    settings.rows=14;settings.cols=25;assert(!settings_save(&settings));
    NpeSettings expected=settings;settings_loaded=0;settings_load();assert(!memcmp(&settings,&expected,sizeof(settings)));
    char text[1024];assert(settings_encode(&settings,text,sizeof(text))>0);
    NpeSettings decoded;assert(!settings_decode(text,&decoded));assert(!memcmp(&decoded,&settings,sizeof(settings)));
    assert(settings_decode("NPE_SETTINGS 2",&decoded));
    decoded=settings;decoded.rows=41;assert(!settings_valid(&decoded));
    decoded=settings;decoded.keys[0]=SDLK_F10;assert(!settings_valid(&decoded));
    decoded=settings;decoded.buttons[0]=decoded.buttons[1];assert(!settings_valid(&decoded));
    int device=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,21,0);assert(device>=0);
    game_pad=SDL_GameControllerOpen(device);assert(game_pad);game_pad_id=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(game_pad));
    uint16_t mode=1;game_pad_mode=&mode;SDL_Event e;SDL_zero(e);e.type=SDL_CONTROLLERBUTTONDOWN;
    e.cbutton.which=game_pad_id;e.cbutton.button=SDL_CONTROLLER_BUTTON_X;
    assert(game_pad_event(&e)&&e.key.keysym.sym==SDLK_RETURN);
    mode=5;SDL_zero(e);e.type=SDL_CONTROLLERBUTTONDOWN;e.cbutton.which=game_pad_id;e.cbutton.button=SDL_CONTROLLER_BUTTON_X;
    assert(game_pad_event(&e)&&e.key.keysym.sym==SDLK_SPACE);
    mode=8;SDL_zero(e);e.type=SDL_CONTROLLERBUTTONDOWN;e.cbutton.which=game_pad_id;e.cbutton.button=SDL_CONTROLLER_BUTTON_X;
    assert(game_pad_event(&e)&&e.key.keysym.sym==SDLK_a);
    game_pad_close();assert(!SDL_JoystickDetachVirtual(device));
    char *path=settings_path("settings.cfg");assert(path);assert(!remove(path));free(path);assert(!rmdir_test(folder));
    SDL_Quit();puts("Settings: persistence, validation, swaps, keyboard, controller, dialogue and quiz passed");return 0;
}
