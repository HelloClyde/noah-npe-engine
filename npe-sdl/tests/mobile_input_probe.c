#include <SDL.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#ifdef main
#undef main
#endif
static uint16_t mode;
static uint16_t *game_pad_mode=&mode;
#define NPE_MOBILE_INPUT_TEST
#include "../mobile_input.inc"
static void check(unsigned action,unsigned down,SDL_Keycode expected){
    SDL_Event event;SDL_zero(event);event.type=SDL_USEREVENT;
    event.user.code=NPE_MOBILE_EVENT_CODE;event.user.data1=(void *)(uintptr_t)action;
    event.user.data2=(void *)(uintptr_t)down;
    assert(game_mobile_event(&event));
    assert(event.type==(down?SDL_KEYDOWN:SDL_KEYUP));
    assert(event.key.keysym.sym==expected);
}
int main(void){
    assert(!SDL_Init(SDL_INIT_EVENTS));
    mode=1;check(0,1,SDLK_UP);check(0,0,SDLK_UP);check(4,1,SDLK_RETURN);
    check(5,1,SDLK_F4);check(6,1,SDLK_SPACE);check(7,1,SDLK_u);
    check(10,1,SDLK_j);check(10,0,SDLK_j);check(11,1,SDLK_l);check(11,0,SDLK_l);
    mode=2;check(4,1,SDLK_SPACE);mode=5;check(4,1,SDLK_SPACE);
    mode=3;check(4,1,SDLK_RETURN);
    mode=4;check(10,1,SDLK_j);check(11,1,SDLK_l);check(4,1,SDLK_RETURN);
    mode=8;check(3,1,SDLK_LEFT);check(1,1,SDLK_RIGHT);check(4,1,SDLK_RETURN);
    check(5,1,SDLK_F4);check(6,1,SDLK_F2);check(7,1,SDLK_BACKSPACE);
    SDL_Event event;SDL_zero(event);event.type=SDL_QUIT;
    assert(game_mobile_event(&event)&&event.type==SDL_QUIT);
    event.type=SDL_USEREVENT;event.user.code=NPE_MOBILE_EVENT_CODE;
    event.user.data1=(void *)(uintptr_t)100;assert(!game_mobile_event(&event));
    SDL_Quit();puts("mobile input: map, dialogue, story, battle and quiz passed");return 0;
}
