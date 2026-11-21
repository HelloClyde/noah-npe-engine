/* UI threads enqueue logical actions; mode translation stays on the game thread. */
#if defined(__ANDROID__) || defined(NPE_MOBILE_INPUT_TEST)
#define NPE_MOBILE_EVENT_CODE 0x4e50454b
static int game_mobile_event(SDL_Event *event) {
    static const SDL_Keycode keys[]={SDLK_UP,SDLK_RIGHT,SDLK_DOWN,SDLK_LEFT,
        SDLK_RETURN,SDLK_F4,SDLK_SPACE,SDLK_u,SDLK_p,SDLK_n,SDLK_j,SDLK_l};
    if(event->type!=SDL_USEREVENT || event->user.code!=NPE_MOBILE_EVENT_CODE)return 1;
    unsigned action=(unsigned)(uintptr_t)event->user.data1;
    if(action>=sizeof(keys)/sizeof(keys[0]))return 0;
    SDL_Keycode key=keys[action];
    unsigned mode=game_pad_mode?*game_pad_mode:0;
    if(mode==8 && action==6)key=SDLK_F2; /* Show the keyboard for a typed question. */
    else if(mode==8 && action==7)key=SDLK_BACKSPACE;
    else if(action==4 && (mode==2 || mode==5))key=SDLK_SPACE;
    Uint32 type=event->user.data2?SDL_KEYDOWN:SDL_KEYUP;
    SDL_zero(*event);event->type=type;
    event->key.state=type==SDL_KEYDOWN?SDL_PRESSED:SDL_RELEASED;
    event->key.keysym.sym=key;event->key.keysym.scancode=SDL_GetScancodeFromKey(key);
    return 1;
}
#ifdef __ANDROID__
#include <jni.h>
JNIEXPORT void JNICALL Java_com_helloclyde_npe_GameActivity_nativeButton(
        JNIEnv *env,jclass cls,jint action,jboolean down) {
    (void)env;(void)cls;
    SDL_Event event;SDL_zero(event);event.type=SDL_USEREVENT;
    event.user.code=NPE_MOBILE_EVENT_CODE;
    event.user.data1=(void *)(uintptr_t)(unsigned)action;
    event.user.data2=(void *)(uintptr_t)(down?1:0);
    SDL_PushEvent(&event);
}
#endif
#else
#define game_mobile_event(event) 1
#endif
