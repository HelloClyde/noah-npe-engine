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
        const uint8_t text[]={0xa1,0xb0,0xbf,0xda,0xc4,0xc8,0xa1,0xad,0xa1,0xad,0xa1,0xb1};
        memset(dialogue+84*NPE_WIDTH,0,35*NPE_WIDTH);
        assert(!npe_draw_gbk_text16_color(&font,text,sizeof(text),0,84,15,dialogue));
        world->game_mode=2;draw_pixels(renderer,texture,dialogue);
        SDL_RenderGetLogicalSize(renderer,&width,&height);assert(width==400&&height==232);
        unsigned dialogue_y=232-(118-84+1)-4;
        for(unsigned y=0;y<232;++y)for(unsigned x=0;x<400;++x){
            uint8_t expected=map_before[y*400+x];
            if(y>=dialogue_y&&y<dialogue_y+35){
                expected=0;
                if(x>=4&&x<244)expected=dialogue[(84+y-dialogue_y)*NPE_WIDTH+x-4];
            }
            assert(game_view_pixels[y*400+x]==expected);
        }
        SDL_Surface *overlay_image=SDL_CreateRGBSurfaceWithFormat(0,400,232,32,SDL_PIXELFORMAT_ARGB8888);assert(overlay_image);
        assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,overlay_image->pixels,overlay_image->pitch));
        snprintf(path,sizeof(path),"%sexpanded-dialogue.bmp",test_folder);assert(!SDL_SaveBMP(overlay_image,path));
        world->game_mode=6;world->property_page=0;
        uint8_t shown[NPE_WIDTH*NPE_HEIGHT];memcpy(shown,native,sizeof(shown));
        npe_render_system_menu_overlay(&file,world,0,shown);
        assert(!npe_render_native_menu_money(&file,&font,world,shown));
        game_view_menu_layer(&file,native,0,0,0);draw_pixels(renderer,texture,shown);
        for(unsigned y=0;y<232;++y)for(unsigned x=0;x<400;++x){
            uint8_t expected=map_before[y*400+x];
            if(x>=4&&x<244&&y>=4&&y<124&&game_view_menu_mask[(y-4)*NPE_WIDTH+x-4])
                expected=shown[(y-4)*NPE_WIDTH+x-4];
            assert(game_view_pixels[y*400+x]==expected);
        }
        assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,overlay_image->pixels,overlay_image->pitch));
        snprintf(path,sizeof(path),"%sexpanded-menu.bmp",test_folder);assert(!SDL_SaveBMP(overlay_image,path));
        npe_render_system_submenu_overlay(&file,world,2,shown);
        game_view_menu_layer(&file,native,3,1,0);draw_pixels(renderer,texture,shown);
        for(unsigned y=0;y<NPE_HEIGHT;++y)for(unsigned x=0;x<NPE_WIDTH;++x)
            if(game_view_menu_mask[y*NPE_WIDTH+x])assert(game_view_pixels[(y+4)*400+x+4]==shown[y*NPE_WIDTH+x]);
        const unsigned overlay_modes[]={4,8,13,17,22};
        for(unsigned step=0;step<sizeof(overlay_modes)/sizeof(overlay_modes[0]);++step){
            world->game_mode=(uint16_t)overlay_modes[step];draw_pixels(renderer,texture,dialogue);
            SDL_RenderGetLogicalSize(renderer,&width,&height);assert(width==400&&height==232);
            for(int y=0;y<NPE_HEIGHT;++y)for(int x=0;x<NPE_WIDTH;++x)
                assert(game_view_pixels[(y+56)*400+x+80]==dialogue[y*NPE_WIDTH+x]);
        }
        /* Append diagnostic text only in memory; original NPE bytes stay intact. */
        size_t source_size=file.size;
        uint8_t raw[96];unsigned raw_size=0;
        for(unsigned i=0;i<26;++i){raw[raw_size++]=0xd6;raw[raw_size++]=0xd0;}
        memcpy(raw+raw_size,"ABC123",6);raw_size+=6;raw[raw_size++]='@';raw[raw_size++]=0;
        uint8_t *extended=realloc(file.bytes,source_size+raw_size);assert(extended);
        file.bytes=extended;file.size=source_size+raw_size;memcpy(extended+source_size,raw,raw_size);
        assert(!npe_world_resources_open(&file,world));
        world->game_mode=2;world->text_file_position=(uint32_t)source_size;
        world->text_offset=0;world->text_more=1;
        memcpy(dialogue,native,sizeof(dialogue));
        assert(!npe_render_world_npc_page_layout(&file,&font,world,dialogue,0,NULL,NULL,&game_view_text));
        assert(game_view_text.valid&&game_view_text.count==32&&world->text_more==0);
        assert(npe_text_page_lines(&game_view_text,240)==2);
        assert(npe_text_page_lines(&game_view_text,392)==2);
        assert(npe_text_page_lines(&game_view_text,952)==1);
        unsigned offset_after=world->text_offset;
        draw_pixels(renderer,texture,dialogue);
        unsigned text_top=232-35-4;
        uint8_t glyph_frame[NPE_WIDTH*NPE_HEIGHT]={0};
        const uint8_t chinese[]={0xd6,0xd0};
        assert(!npe_draw_gbk_text16_color(&font,chinese,2,0,0,15,glyph_frame));
        /* The 16th character crossed the old 240px boundary: now stays on row1.
           The 25th moves intact to row2 at a 392px usable width. */
        for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x){
            assert(game_view_pixels[(text_top+y)*400+244+x]==glyph_frame[y*NPE_WIDTH+x]);
            assert(game_view_pixels[(text_top+16+y)*400+4+x]==glyph_frame[y*NPE_WIDTH+x]);
        }
        assert(world->text_offset==offset_after);
        assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,overlay_image->pixels,overlay_image->pitch));
        snprintf(path,sizeof(path),"%sexpanded-reflow.bmp",test_folder);assert(!SDL_SaveBMP(overlay_image,path));
        /* The same capture must not change native pixels or continuation state. */
        uint8_t expected[NPE_WIDTH*NPE_HEIGHT];memcpy(expected,native,sizeof(expected));
        world->text_offset=0;world->text_more=1;
        assert(!npe_render_world_npc_page(&file,&font,world,expected,0,NULL,NULL));
        assert(!memcmp(expected,dialogue,sizeof(expected))&&world->text_offset==offset_after&&world->text_more==0);
        settings.cols=60;draw_pixels(renderer,texture,dialogue);
        assert(game_view_width==960&&world->text_offset==offset_after);
        settings.cols=15;draw_pixels(renderer,texture,dialogue);
        assert(game_view_width==240&&world->text_offset==offset_after);
        const uint8_t controlled[]={0xd6,0xd0,'A','\r','\n',0xd6,0xd0,6,'A',7,'B',7,'@',0};
        memcpy(file.bytes+source_size,controlled,sizeof(controlled));
        world->text_offset=0;world->text_more=1;
        memcpy(dialogue,native,sizeof(dialogue));
        assert(!npe_render_world_npc_page_layout(&file,&font,world,dialogue,0,NULL,NULL,&game_view_text));
        assert(game_view_text.count==6&&game_view_text.glyphs[2].width==0&&npe_text_page_lines(&game_view_text,952)==2);
        uint8_t flowed[NPE_WIDTH*NPE_HEIGHT]={0};npe_text_page_draw(&game_view_text,flowed,240,120,0,84,240);
        assert(!memcmp(flowed+84*240,dialogue+84*240,32*240));
        memcpy(file.bytes+source_size,raw,raw_size);
        world->text_offset=0;world->text_more=1;world->text_style=0;
        const uint16_t text_rect[]={0,84,240,118};memcpy(world->text_rect,text_rect,sizeof(text_rect));
        assert(!npe_render_world_story_page_layout(&file,&font,world,dialogue,&game_view_text));
        assert(game_view_text.valid&&game_view_text.count==32&&npe_text_page_lines(&game_view_text,952)==1);
        settings.cols=25;
        puts("Text reflow: GBK glyphs cross old 240px boundary, wrap intact, resize without advancing page; explicit CRLF and special glyphs preserved; native page/state unchanged");
        game_view_text.valid=0;
        SDL_FreeSurface(overlay_image);free(map_before);npe_font_free(&font);
        settings.rows=7;settings.cols=15;world->game_mode=1;draw_pixels(renderer,texture,native);
        world->game_mode=6;game_view_menu_layer(&file,native,3,1,1);
        const unsigned levels[]={1,2,3};assert(!npe_render_save_slots_overlay(&file,105,46,levels,0,NULL,shown));
        draw_pixels(renderer,texture,shown);
        assert(game_view_pixels[119*240+176]==shown[119*240+176]);
        settings.rows=14;settings.cols=25;
        puts("Expanded UI: menu anchored to viewport, map outside panels retained, dialogue full-width at bottom, independent panels centered");
        world->game_mode=3;draw_pixels(renderer,texture,native);SDL_RenderGetLogicalSize(renderer,&width,&height);assert(width==NPE_WIDTH&&height==NPE_HEIGHT);
        game_view_close();game_view_file=NULL;game_view_world=NULL;game_view_playing=NULL;
        SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);free(before);free(world);npe_free(&file);
        puts("Expanded SDL display: exact native poses preserved, map state unchanged, battle layout restored");
    }
    SDL_Quit();puts("Visual settings: rendered controls, key capture, numeric view entry and restart persistence passed");return 0;
}
