#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../controller_input.inc"
#include "../input_gate.inc"
static SDL_Event key(Uint32 type,SDL_Keycode symbol){
    SDL_Event event;SDL_zero(event);event.type=type;event.key.keysym.sym=symbol;
    event.key.keysym.scancode=SDL_GetScancodeFromKey(symbol);return event;
}
static int spam(void *unused){
    (void)unused;
    for(unsigned i=0;i<100;++i){
        SDL_Event down=key(SDL_KEYDOWN,SDLK_RETURN),up=key(SDL_KEYUP,SDLK_RETURN);
        assert(SDL_PushEvent(&down)==1&&SDL_PushEvent(&up)==1);SDL_Delay(1);
    }
    return 0;
}
int main(void){
    SDL_SetMainReady();SDL_setenv("SDL_VIDEODRIVER","dummy",1);assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS));game_pad_id=42;game_input_reset();
    SDL_Event event=key(SDL_KEYDOWN,SDLK_RETURN);assert(game_input_gate_event(&event));
    game_input_begin();game_input_end();event.key.repeat=1;assert(!game_input_gate_event(&event));
    event=key(SDL_KEYUP,SDLK_RETURN);assert(!game_input_gate_event(&event));
    event=key(SDL_KEYDOWN,SDLK_RETURN);assert(game_input_gate_event(&event));
    event=key(SDL_KEYUP,SDLK_RETURN);assert(game_input_gate_event(&event));
    game_input_reset();SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
    game_input_begin();SDL_Thread *thread=SDL_CreateThread(spam,"battle key spam",NULL);assert(thread);
    Uint32 start=SDL_GetTicks();while(SDL_GetTicks()-start<180){game_input_drain_busy();SDL_Delay(1);}SDL_WaitThread(thread,NULL);
    SDL_Event window;SDL_zero(window);window.type=SDL_WINDOWEVENT;window.window.event=SDL_WINDOWEVENT_EXPOSED;
    assert(SDL_PushEvent(&window)==1);SDL_zero(event);event.type=SDL_USEREVENT;event.user.code=12345;assert(SDL_PushEvent(&event)==1);
    SDL_zero(event);event.type=SDL_QUIT;assert(SDL_PushEvent(&event)==1);game_input_end();
    unsigned keys=0,windows=0,audio=0,quit=0;
    while(SDL_PollEvent(&event)){
        keys+=event.type==SDL_KEYDOWN||event.type==SDL_KEYUP;
        windows+=event.type==SDL_WINDOWEVENT;audio+=event.type==SDL_USEREVENT&&event.user.code==12345;quit+=event.type==SDL_QUIT;
    }
    assert(!keys&&windows==1&&audio==1&&quit==1&&game_input_quit_pending);
    game_input_reset();SDL_zero(event);event.type=SDL_CONTROLLERBUTTONDOWN;event.cbutton.which=42;event.cbutton.button=SDL_CONTROLLER_BUTTON_A;
    assert(game_input_gate_event(&event));game_input_fence();assert(!game_input_gate_event(&event));
    event.type=SDL_CONTROLLERBUTTONUP;assert(!game_input_gate_event(&event));event.type=SDL_CONTROLLERBUTTONDOWN;assert(game_input_gate_event(&event));
    game_input_reset();SDL_zero(event);event.type=SDL_CONTROLLERAXISMOTION;event.caxis.which=42;event.caxis.axis=SDL_CONTROLLER_AXIS_LEFTX;event.caxis.value=30000;
    assert(game_input_gate_event(&event));game_input_fence();assert(!game_input_gate_event(&event));
    event.caxis.value=0;assert(!game_input_gate_event(&event));event.caxis.value=30000;assert(game_input_gate_event(&event));
    game_input_reset();SDL_zero(event);event.type=SDL_USEREVENT;event.user.code=0x4e50454b;event.user.data1=(void *)(uintptr_t)4;event.user.data2=(void *)(uintptr_t)1;
    assert(game_input_gate_event(&event));game_input_fence();assert(!game_input_gate_event(&event));
    event.user.data2=NULL;assert(!game_input_gate_event(&event));event.user.data2=(void *)(uintptr_t)1;assert(game_input_gate_event(&event));
    game_input_reset();game_input_begin();game_input_begin();game_input_end();assert(game_input_busy==1);game_input_end();assert(!game_input_busy);
    SDL_Quit();puts("Input gate: 100 rapid confirms discarded; quit/window/audio preserved; keyboard, controller, stick and touch require release; nested scopes passed");return 0;
}
