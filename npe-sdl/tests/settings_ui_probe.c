#define NPE_SETTINGS_SDL_TEST
#define NPE_MOBILE_INPUT_TEST
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
static int shop_binding_keys(void *unused){
    (void)unused;const SDL_Keycode keys[]={SDLK_q,SDLK_F4};
    for(unsigned i=0;i<2;++i){SDL_Delay(70);SDL_Event event;SDL_zero(event);
        event.type=SDL_KEYDOWN;event.key.keysym.sym=keys[i];SDL_PushEvent(&event);
        event.type=SDL_KEYUP;SDL_PushEvent(&event);}
    return 0;
}
static void push_touch(unsigned action) {
    SDL_Event event={0};
    event.type=SDL_USEREVENT;event.user.code=NPE_MOBILE_EVENT_CODE;
    event.user.data1=(void *)(uintptr_t)action;event.user.data2=(void *)(uintptr_t)1;
    assert(SDL_PushEvent(&event)==1);
    event.user.data2=NULL;
    assert(SDL_PushEvent(&event)==1);
}
static int shop_touch_keys(void *unused) {
    (void)unused;
    SDL_Delay(70);push_touch(10);
    SDL_Delay(70);push_touch(4);
    return 0;
}
static int quiz_touch_key(const NpeFile *file,NpeWorld *world,unsigned action) {
    push_touch(action);
    SDL_Event event;
    while(game_WaitEventTimeout(&event,100))
        if(event.type==SDL_KEYDOWN)return npe_world_quiz_key(file,world,quiz_sdl_key(&event.key));
    return -1;
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
        SDL_Event view_change={0};
        view_change.type=SDL_USEREVENT;view_change.user.code=NPE_MOBILE_VIEW_EVENT;
        view_change.user.data1=(void *)(uintptr_t)17;view_change.user.data2=(void *)(uintptr_t)31;
        assert(!game_host_event(&view_change));
        SDL_RenderGetLogicalSize(renderer,&width,&height);assert(width==31*16&&height==17*16+8);
        view_change.user.data1=(void *)(uintptr_t)41;
        assert(!game_host_event(&view_change)&&settings.rows==17&&settings.cols==31);
        view_change.user.data1=view_change.user.data2=NULL;
        assert(!game_host_event(&view_change));
        SDL_RenderGetLogicalSize(renderer,&width,&height);assert(width==NPE_WIDTH&&height==NPE_HEIGHT);
        view_change.user.data1=(void *)(uintptr_t)14;view_change.user.data2=(void *)(uintptr_t)25;
        assert(!game_host_event(&view_change));
        assert(!memcmp(world,before,sizeof(*world))&&!memcmp(game_view_pixels,map_before,400*232));
        puts("Android viewport event: independent width/height apply immediately, invalid sizes ignored, original mode restores frame");
        NpeFont font={0};assert(!load_native_fonts(&font));
        const unsigned notice_views[][2]={{7,15},{25,25},{25,40}};
        for(unsigned i=0;i<3;++i){
            settings.rows=notice_views[i][0];settings.cols=notice_views[i][1];
            world->game_mode=1;draw_pixels(renderer,texture,native);
            unsigned vw=game_view_width,vh=game_view_height;
            uint8_t *before_notice=malloc((size_t)vw*vh);assert(before_notice);
            memcpy(before_notice,game_view_pixels,(size_t)vw*vh);
            uint8_t notice[NPE_WIDTH*NPE_HEIGHT];memcpy(notice,native,sizeof(notice));
            assert(!npe_render_map_name_notice_layout(&file,&font,world,notice,game_view_notice_rect));
            unsigned sx=game_view_notice_rect[0],sy=game_view_notice_rect[1];
            unsigned rw=game_view_notice_rect[2]-sx+1,rh=game_view_notice_rect[3]-sy+1;
            unsigned cx=(vw-rw)/2,cy=(vh-rh)/2;
            game_view_notice_base=native;draw_pixels(renderer,texture,notice);game_view_notice_base=NULL;
            for(unsigned y=0;y<vh;++y)for(unsigned x=0;x<vw;++x){
                uint8_t expected=x>=cx&&x<cx+rw&&y>=cy&&y<cy+rh?
                    notice[(sy+y-cy)*NPE_WIDTH+sx+x-cx]:before_notice[y*vw+x];
                assert(game_view_pixels[y*vw+x]==expected);
            }
            draw_pixels(renderer,texture,native);assert(!memcmp(game_view_pixels,before_notice,(size_t)vw*vh));
            free(before_notice);
        }
        settings.rows=14;settings.cols=25;draw_pixels(renderer,texture,native);
        puts("Map-name notice: centered at 15x7, 25x25 and 40x25 tiles; all outside pixels and closing map remain unchanged");
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
        /* Real retained yes/no entry and both cursor positions: every pixel
           outside the two resource rectangles must equal the prior map. */
        world->game_mode=1;draw_pixels(renderer,texture,native);
        memcpy(map_before,game_view_pixels,400*232);
        world->game_mode=17;world->map_init_pending=1;
        const unsigned popup_levels[]={0,0,0};
        assert(!draw_world(renderer,texture,&file,world,0,0,0,0,0,0,popup_levels,0,&font));
        NpeIndexedBitmap yes,no;
        assert(!npe_indexed_bitmap(&file,npe_image_group_index(&file,7,7),&yes));
        assert(!npe_indexed_bitmap(&file,npe_image_group_index(&file,7,8),&no));
        for(unsigned selection=0;selection<2;++selection){
            for(unsigned y=0;y<232;++y)for(unsigned x=0;x<400;++x){
                int sx=(int)x-80,sy=(int)y-56;
                int in_yes=sx>=70&&sx<70+(int)yes.width&&sy>=45&&sy<45+(int)yes.height;
                int in_no=sx>=134&&sx<134+(int)no.width&&sy>=45&&sy<45+(int)no.height;
                uint8_t expected=(in_yes||in_no)?last_pixel_frame[sy*NPE_WIDTH+sx]:map_before[y*400+x];
                assert(game_view_pixels[y*400+x]==expected);
            }
            if(!selection)assert(!game_story_choice_input(&file,world,SDLK_RIGHT,renderer,texture));
        }
        assert(!SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,overlay_image->pixels,overlay_image->pitch));
        snprintf(path,sizeof(path),"%sexpanded-choice.bmp",test_folder);assert(!SDL_SaveBMP(overlay_image,path));
        world->game_mode=1;draw_pixels(renderer,texture,native);
        assert(!memcmp(map_before,game_view_pixels,400*232));
        puts("Yes/no popup: actual entry and selection toggle change only NPE button rectangles; surrounding map and closing frame remain identical");
        const unsigned overlay_modes[]={4,8,13,22};
        for(unsigned step=0;step<sizeof(overlay_modes)/sizeof(overlay_modes[0]);++step){
            world->game_mode=(uint16_t)overlay_modes[step];draw_pixels(renderer,texture,dialogue);
            SDL_RenderGetLogicalSize(renderer,&width,&height);assert(width==400&&height==232);
            for(int y=0;y<NPE_HEIGHT;++y)for(int x=0;x<NPE_WIDTH;++x)
                assert(game_view_pixels[(y+56)*400+x+80]==dialogue[y*NPE_WIDTH+x]);
        }
        world->game_mode=1;
        if(!file.rpg_runtime)assert(!npe_world_resources_open(&file,world));
        world->game_mode=4;world->event_context_id=13;world->shop_kind=world->shop_ready=1;
        world->shop_list_ready=1;world->shop_selected_item=1;world->shop_ui=1;
        NpeItem item;assert(!npe_world_item(&file,world,1,&item));
        settings_bind_key(&settings,10,SDLK_q);settings_bind_key(&settings,11,SDLK_e);
        game_pad_mode=&world->game_mode;game_input_reset();SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
        unsigned money=world->money,owned=world->condition_words[1];
        SDL_Thread *shop_keys=SDL_CreateThread(shop_binding_keys,"shop remapped selection",NULL);assert(shop_keys);
        assert(!show_shop_quantity(renderer,texture,&file,&font,world));SDL_WaitThread(shop_keys,NULL);
        assert(world->shop_choice==1&&world->money==money&&world->condition_words[1]==owned);
        world->money=30000;
        shop_keys=SDL_CreateThread(shop_touch_keys,"shop touch selection",NULL);assert(shop_keys);
        assert(!show_shop_quantity(renderer,texture,&file,&font,world));SDL_WaitThread(shop_keys,NULL);
        assert(world->shop_choice==1 && world->condition_words[1]==owned+1 && world->money==30000-item.price);
        puts("Android shop events: option left + A complete a purchase and debit money");
        world->game_mode=8;world->quiz_ready=1;world->quiz_category=0;
        for(unsigned type=0;type<2;++type){
            NpeQuizQuestion question;
            unsigned index=0;
            while(!npe_quiz_question(&file,index,&question) && !!question.input_type!=type)++index;
            assert(!npe_quiz_question(&file,index,&question));
            world->quiz_question_index=(uint16_t)index;world->event_context_id=0;
            if(!type)assert(quiz_touch_key(&file,world,1)==1 && world->event_context_id==1);
            else{
                assert(npe_world_quiz_key(&file,world,'1')==1);
                assert(quiz_touch_key(&file,world,7)==1 && !world->quiz_input_length);
                assert(npe_world_quiz_key(&file,world,'2')==1);
            }
            assert(quiz_touch_key(&file,world,4)==2);
        }
        puts("Android quiz events: direction/confirm submit choice, delete/confirm submit typed answer");
        game_pad_mode=NULL;
        puts("Real shop quantity dialog: remapped Q switches No to Yes; cancel leaves money and inventory unchanged");
        /* Append diagnostic text only in memory; original NPE bytes stay intact. */
        size_t source_size=file.size;
        uint8_t raw[96];unsigned raw_size=0;
        for(unsigned i=0;i<26;++i){raw[raw_size++]=0xd6;raw[raw_size++]=0xd0;}
        memcpy(raw+raw_size,"ABC123",6);raw_size+=6;raw[raw_size++]='@';raw[raw_size++]=0;
        uint8_t *extended=realloc(file.bytes,source_size+raw_size);assert(extended);
        file.bytes=extended;file.size=source_size+raw_size;memcpy(extended+source_size,raw,raw_size);
        if(!file.rpg_runtime)assert(!npe_world_resources_open(&file,world));
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
        NpeTextPage retained=game_view_text;
        file.bytes[source_size]='@';world->text_offset=0;world->text_more=1;
        assert(!npe_render_world_npc_page_layout(&file,&font,world,dialogue,0,NULL,NULL,&game_view_text));
        assert(!memcmp(&retained,&game_view_text,sizeof(retained)));
        file.bytes[source_size]='?';file.bytes[source_size+1]='@';
        assert(!npe_render_world_npc_page_layout(&file,&font,world,dialogue,1,NULL,NULL,&game_view_text));
        assert(!game_view_text.valid);
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
