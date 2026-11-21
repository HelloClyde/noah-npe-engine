#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "../controller_input.inc"
static SDL_Event button(Uint32 type,Uint8 b){
    SDL_Event e;SDL_zero(e);e.type=type;e.cbutton.which=game_pad_id;e.cbutton.button=b;return e;
}
static int help_calls;
static void help(void){++help_calls;}
int main(void){
    assert(!SDL_Init(SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS));
    int device=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,21,0);
    assert(device>=0);game_pad=SDL_GameControllerOpen(device);assert(game_pad);
    game_pad_id=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(game_pad));
    game_pad_show_help=help;
    SDL_Event start=button(SDL_CONTROLLERBUTTONDOWN,SDL_CONTROLLER_BUTTON_START);
    assert(!game_pad_event(&start)&&help_calls==1);
    uint16_t mode=7;game_pad_mode=&mode;
    SDL_Event e=button(SDL_CONTROLLERBUTTONDOWN,SDL_CONTROLLER_BUTTON_A);
    assert(game_pad_event(&e)&&e.key.keysym.sym==SDLK_RETURN);
    mode=5;e=button(SDL_CONTROLLERBUTTONDOWN,SDL_CONTROLLER_BUTTON_A);
    assert(game_pad_event(&e)&&e.key.keysym.sym==SDLK_SPACE);
    mode=8;e=button(SDL_CONTROLLERBUTTONDOWN,SDL_CONTROLLER_BUTTON_B);
    assert(game_pad_event(&e)&&e.key.keysym.sym==SDLK_F4);
    mode=1;e=button(SDL_CONTROLLERBUTTONDOWN,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    assert(game_pad_event(&e)&&e.key.keysym.sym==SDLK_RIGHT);
    game_pad_next=SDL_GetTicks();assert(game_pad_repeat(&e)&&e.key.repeat);
    e=button(SDL_CONTROLLERBUTTONUP,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    assert(!game_pad_event(&e)&&!game_pad_repeat(&e));
    SDL_zero(e);e.type=SDL_CONTROLLERAXISMOTION;e.caxis.which=game_pad_id;
    e.caxis.axis=SDL_CONTROLLER_AXIS_LEFTX;e.caxis.value=4000;assert(!game_pad_event(&e));
    e.caxis.value=-24000;assert(game_pad_event(&e)&&e.key.keysym.sym==SDLK_LEFT);
    SDL_zero(e);e.type=SDL_WINDOWEVENT;e.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
    assert(game_pad_event(&e)&&!game_pad_repeat(&e));
    e=button(SDL_CONTROLLERBUTTONDOWN,SDL_CONTROLLER_BUTTON_A);assert(!game_pad_event(&e));
    e.type=SDL_WINDOWEVENT;e.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;assert(game_pad_event(&e));
    SDL_JoystickID id=game_pad_id;assert(!SDL_JoystickDetachVirtual(device));
    SDL_zero(e);e.type=SDL_CONTROLLERDEVICEREMOVED;e.cdevice.which=id;
    assert(!game_pad_event(&e)&&game_pad_id!=id&&!game_pad_repeat(&e));
    game_pad_close();SDL_Quit();puts("Controller virtual attach, mappings, repeat, deadzone, focus and detach passed");return 0;
}
