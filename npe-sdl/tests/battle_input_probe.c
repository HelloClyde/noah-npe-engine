/* Real production battle rendering and event handling, with diagnostic stats. */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#define main npe_app_main
#include "../main.c"
#undef main
static int battle_spam(void *unused){
    (void)unused;
    for(unsigned i=0;i<100;++i){
        SDL_Event event;SDL_zero(event);event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_RETURN;
        event.key.keysym.scancode=SDL_SCANCODE_RETURN;assert(SDL_PushEvent(&event)==1);
        event.type=SDL_KEYUP;assert(SDL_PushEvent(&event)==1);SDL_Delay(1);
    }
    return 0;
}
int main(int argc,char **argv){
    assert(argc==2||(argc==3&&!strcmp(argv[2],"--ungated-baseline")));int baseline=argc==3;SDL_SetMainReady();SDL_setenv("SDL_VIDEODRIVER","dummy",1);
    assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER));game_input_reset();
    NpeFile file;uint8_t *data;size_t size;char error[256];assert(!read_file(argv[1],&data,&size));
    assert(!npe_parse(&file,data,size,error,sizeof(error)));
    NpeWorld *world=calloc(1,sizeof(*world));assert(world);assert(!npe_world_init(&file,world,9,29,13));
    world->game_mode=3;world->story_event_id=0;npe_world_seed(world,4);
    assert(npe_world_prepare_battle(&file,world)==1&&world->battle_enemy_count);
    world->hp[0]=1;world->attack[0]=1;world->defense[0]=world->battle_party_defenses[0]=0;
    for(unsigned i=0;i<world->battle_enemy_count;++i){
        if(!world->battle_enemy_hps[i])continue;
        world->battle_enemy_hps[i]=world->battle_enemy_max_hps[i]=30000;
        world->battle_enemy_attacks[i]=world->battle_enemy_base_attacks[i]=30000;
        world->battle_enemy_defenses[i]=world->battle_enemy_base_defenses[i]=1000;
        memset(world->battle_enemy_skill_ids[i],0,sizeof(world->battle_enemy_skill_ids[i]));
    }
    SDL_Window *window=SDL_CreateWindow("battle input test",0,0,240,120,SDL_WINDOW_HIDDEN);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,240,120);assert(texture);
    NpeFont font={0};assert(!load_native_fonts(&font));BattleAnimationDisplay display={0};
    display.renderer=renderer;display.texture=texture;display.font=&font;
    world->battle_animation_observer=baseline?play_battle_animation_frame:play_battle_animation;world->battle_animation_context=&display;game_pad_mode=&world->game_mode;
    SDL_Thread *thread=SDL_CreateThread(battle_spam,"queued confirms",NULL);assert(thread);
    unsigned target=0;while(target<3&&!world->battle_enemy_hps[target])++target;assert(target<3);
    int result=npe_world_battle_attack(world,target);SDL_WaitThread(thread,NULL);
    unsigned leaked=0;SDL_Event event;while(game_PollEvent(&event))leaked+=event.type==SDL_KEYDOWN;
    if(baseline){assert(leaked>0);printf("Ungated animation reproduced %u queued confirmations\n",leaked);goto cleanup;}
    assert(!leaked);
    for(unsigned turn=0;result==NPE_BATTLE_CONTINUE&&turn<2;++turn)result=npe_world_battle_attack(world,target);
    assert(result==NPE_BATTLE_LOST&&world->game_mode==7);
    /* More stale input is deliberately queued at the defeat -> title edge. */
    for(unsigned i=0;i<50;++i){SDL_zero(event);event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_RETURN;assert(SDL_PushEvent(&event)==1);event.type=SDL_KEYUP;assert(SDL_PushEvent(&event)==1);}
    assert(advance_world_events(window,renderer,texture,&file,&font,world,3,0)>=0);
    assert(world->game_mode==7&&game_title_input_fence_pending);game_input_fence();game_title_input_fence_pending=0;
    while(game_PollEvent(&event))assert(event.type!=SDL_KEYDOWN);
    SDL_zero(event);event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_RETURN;event.key.keysym.scancode=SDL_SCANCODE_RETURN;
    assert(SDL_PushEvent(&event)==1);assert(game_WaitEventTimeout(&event,100)&&event.type==SDL_KEYDOWN);
    event.key.repeat=1;assert(!game_host_event(&event));
cleanup:
    game_pad_mode=NULL;npe_font_free(&font);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);
    npe_free(&file);free(world);SDL_Quit();if(!baseline)puts("Real battle: queued confirms did not create another action; defeat/title stayed idle; a fresh confirm still works");return 0;
}
