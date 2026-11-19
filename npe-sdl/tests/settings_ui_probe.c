#define NPE_SETTINGS_SDL_TEST
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char test_folder[2048];
static unsigned frames;
static int capture_ui=1;
static char *ui_pref(const char *org,const char *app){(void)org;(void)app;return SDL_strdup(test_folder);}
static void ui_present(SDL_Renderer *renderer){
    SDL_RenderPresent(renderer);if(!capture_ui)return;++frames;
    int width,height;assert(!SDL_GetRendererOutputSize(renderer,&width,&height));
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_ARGB8888);assert(surface);
    assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,surface->pixels,surface->pitch));
    unsigned bright=0;for(int y=0;y<height;++y)for(int x=0;x<width;++x)
        bright+=((Uint32 *)((Uint8 *)surface->pixels+y*surface->pitch))[x]==0xffeeeeee;
    assert(bright>100);
    char path[2200];snprintf(path,sizeof(path),"%ssettings-ui.bmp",test_folder);assert(!SDL_SaveBMP(surface,path));if(frames==1){snprintf(path,sizeof(path),"%skeyboard-settings.bmp",test_folder);assert(!SDL_SaveBMP(surface,path));}SDL_FreeSurface(surface);
}
#define SDL_GetPrefPath ui_pref
#define SDL_RenderPresent ui_present
#define main npe_app_main
#include "../main.c"
#undef main
#undef SDL_RenderPresent
#undef SDL_GetPrefPath
static int ui_keys(void *unused){
    (void)unused;
    const SDL_Keycode keys[]={SDLK_RETURN,SDLK_w,SDLK_2,SDLK_3,SDLK_DOWN,SDLK_RETURN,
        SDLK_1,SDLK_4,SDLK_RETURN,SDLK_DOWN,SDLK_RETURN,SDLK_2,SDLK_5,SDLK_RETURN,SDLK_ESCAPE};
    for(unsigned i=0;i<sizeof(keys)/sizeof(keys[0]);++i){
        SDL_Delay(70);SDL_Event event;SDL_zero(event);event.type=SDL_KEYDOWN;
        event.key.keysym.sym=keys[i];assert(SDL_PushEvent(&event)==1);
    }
    return 0;
}
int main(int argc,char **argv){
    assert(argc==2||argc==3);snprintf(test_folder,sizeof(test_folder),"%s/",argv[1]);
    SDL_SetMainReady();SDL_setenv("SDL_VIDEODRIVER","dummy",1);assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER));
    settings_load();SDL_Thread *keys=SDL_CreateThread(ui_keys,"settings UI keys",NULL);assert(keys);
    show_settings_page(0);SDL_WaitThread(keys,NULL);
    assert(settings.keys[0]==SDLK_w && settings.rows==14 && settings.cols==25 && frames>14);
    settings_loaded=0;settings_load();assert(settings.keys[0]==SDLK_w && settings.rows==14 && settings.cols==25);
    if(argc==3){
        capture_ui=0;NpeFile file;uint8_t *data;size_t size;char error[256];
        assert(!read_file(argv[2],&data,&size)&&!npe_parse(&file,data,size,error,sizeof(error)));
        NpeWorld *world=calloc(1,sizeof(*world)),*before=malloc(sizeof(*before));assert(world&&before);
        assert(!npe_world_new_game(&file,world));memcpy(before,world,sizeof(*before));
        int playing=1;game_view_file=&file;game_view_world=world;game_view_playing=&playing;
        SDL_Window *window=SDL_CreateWindow("map test",0,0,400,232,SDL_WINDOW_HIDDEN);assert(window);
        SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);
        SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);assert(texture);
        uint8_t native[NPE_WIDTH*NPE_HEIGHT];assert(!npe_render_world(&file,world,native));
        draw_pixels(renderer,texture,native);int width,height;SDL_RenderGetLogicalSize(renderer,&width,&height);assert(width==400&&height==232);
        assert(!memcmp(world,before,sizeof(*world)));
        SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,400,232,32,SDL_PIXELFORMAT_ARGB8888);assert(surface);
        assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,surface->pixels,surface->pitch));
        unsigned left,top;uint8_t *scratch=malloc(400*232);assert(scratch);
        assert(!npe_render_world_view(&file,world,25,14,scratch,&left,&top));free(scratch);
        int dx=((int)world->view.offset_x-(int)left)*16,dy=((int)world->view.offset_y-(int)top)*16;
        for(int y=0;y<112;++y)for(int x=0;x<NPE_WIDTH;++x){
            int px=dx+x,py=dy+y+4;if(px<0||py<0||px>=400||py>=232)continue;
            Uint32 actual=((Uint32 *)((Uint8 *)surface->pixels+py*surface->pitch))[px];
            assert(actual==game_lcd_color(native[(y+4)*NPE_WIDTH+x]));
        }
        char path[2200];snprintf(path,sizeof(path),"%sexpanded-map.bmp",test_folder);assert(!SDL_SaveBMP(surface,path));SDL_FreeSurface(surface);
        uint8_t *map_before=malloc(400*232);assert(map_before);memcpy(map_before,game_view_pixels,400*232);
        NpeFont font={0};assert(!load_native_fonts(&font));
        uint8_t dialogue[NPE_WIDTH*NPE_HEIGHT];memcpy(dialogue,native,sizeof(dialogue));
        const uint8_t text[]="Dialogue keeps the expanded map.";
        assert(!npe_render_gbk_dialogue(&font,text,sizeof(text)-1,0,dialogue));npe_font_free(&font);
        const unsigned overlay_modes[]={2,4,5,6,8};
        for(unsigned step=0;step<sizeof(overlay_modes)/sizeof(overlay_modes[0]);++step){
            unsigned mode=overlay_modes[step];
                        world->game_mode=(uint16_t)mode;
            uint8_t shown[NPE_WIDTH*NPE_HEIGHT];memcpy(shown,mode==6?native:dialogue,sizeof(shown));
            if(mode==6)npe_render_system_menu_overlay(&file,world,0,shown);
            draw_pixels(renderer,texture,shown);
            SDL_RenderGetLogicalSize(renderer,&width,&height);assert(width==400&&height==232);
            for(int y=0;y<232;++y)for(int x=0;x<400;++x){
                if(x>=dx&&x<dx+NPE_WIDTH&&y>=dy&&y<dy+NPE_HEIGHT)
                    assert(game_view_pixels[y*400+x]==shown[(y-dy)*NPE_WIDTH+x-dx]);
                else assert(game_view_pixels[y*400+x]==map_before[y*400+x]);
            }
        }
        free(map_before);
        puts("Dialogue/menus: expanded dimensions and background retained; complete native UI/bottom border preserved");
        world->game_mode=3;draw_pixels(renderer,texture,native);SDL_RenderGetLogicalSize(renderer,&width,&height);assert(width==NPE_WIDTH&&height==NPE_HEIGHT);
        game_view_close();game_view_file=NULL;game_view_world=NULL;game_view_playing=NULL;
        SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);free(before);free(world);npe_free(&file);
        puts("Expanded SDL display: exact native poses preserved, map state unchanged, battle layout restored");
    }
    SDL_Quit();puts("Visual settings: rendered controls, key capture, numeric view entry and restart persistence passed");return 0;
}
