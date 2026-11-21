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
    settings.rows=14;settings.cols=25;settings.sound_muted=1;settings.volume=37;assert(!settings_save(&settings));
    NpeSettings expected=settings;settings_loaded=0;settings_load();assert(!memcmp(&settings,&expected,sizeof(settings)));
    char text[1024];assert(settings_encode(&settings,text,sizeof(text))>0);
    NpeSettings decoded;assert(!settings_decode(text,&decoded));assert(!memcmp(&decoded,&settings,sizeof(settings)));
    assert(settings_decode("NPE_SETTINGS 2",&decoded));
    char legacy[1024];int used=snprintf(legacy,sizeof(legacy),"NPE_SETTINGS 1\n");
    NpeSettings old;settings_defaults(&old);old.keys[0]=SDLK_j;old.keys[1]=SDLK_l;
    old.buttons[0]=SDL_CONTROLLER_BUTTON_LEFTSTICK;old.buttons[1]=SDL_CONTROLLER_BUTTON_RIGHTSTICK;
    for(unsigned i=0;i<10;++i)used+=snprintf(legacy+used,sizeof(legacy)-(size_t)used,"%d ",old.keys[i]);
    for(unsigned i=0;i<11;++i)used+=snprintf(legacy+used,sizeof(legacy)-(size_t)used,"%u ",old.buttons[i]);
    snprintf(legacy+used,sizeof(legacy)-(size_t)used,"\n14 25\n");
    assert(!settings_decode(legacy,&decoded));
    assert(!memcmp(old.keys,decoded.keys,10*sizeof(old.keys[0]))&&!memcmp(old.buttons,decoded.buttons,11));
    assert(decoded.rows==14&&decoded.cols==25&&settings_valid(&decoded));
    assert(!decoded.sound_muted&&decoded.volume==100);
    settings_bind_key(&settings,10,SDLK_q);settings_bind_key(&settings,11,SDLK_e);
    assert(settings_keyboard(SDLK_q,4)==SDLK_j&&settings_keyboard(SDLK_e,4)==SDLK_l);
    assert(settings_keyboard(SDLK_a,1)==SDLK_LEFT);
    settings_bind_button(&settings,11,SDL_CONTROLLER_BUTTON_BACK);
    assert(!settings_save(&settings));NpeSettings upgraded=settings;settings_loaded=0;settings_load();
    assert(!memcmp(&upgraded,&settings,sizeof(settings)));
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
    assert(game_pad_event(&e)&&e.key.keysym.sym==SDLK_RETURN);
    mode=4;SDL_zero(e);e.type=SDL_CONTROLLERBUTTONDOWN;e.cbutton.which=game_pad_id;e.cbutton.button=SDL_CONTROLLER_BUTTON_BACK;
    assert(game_pad_event(&e)&&e.key.keysym.sym==SDLK_j);
    SDL_zero(e);e.type=SDL_CONTROLLERBUTTONDOWN;e.cbutton.which=game_pad_id;e.cbutton.button=SDL_CONTROLLER_BUTTON_RIGHTSTICK;
    assert(game_pad_event(&e)&&e.key.keysym.sym==SDLK_l);
    game_pad_close();assert(!SDL_JoystickDetachVirtual(device));
    puts("Selection bindings: custom Q/E and controller buttons emit J/L; v1 conflicts migrate preserving all old keys/buttons and view, v2 reload passed");
    char *path=settings_path("settings.cfg");assert(path);assert(!remove(path));free(path);assert(!rmdir_test(folder));
    SDL_Quit();puts("Settings: persistence, validation, swaps, keyboard, controller, dialogue and quiz passed");return 0;
}
