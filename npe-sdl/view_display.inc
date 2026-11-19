static const NpeFile *game_view_file;
static const NpeWorld *game_view_world;
static const int *game_view_playing;
static SDL_Texture *game_view_texture;
static SDL_Renderer *game_view_renderer;
static uint8_t *game_view_pixels;
static uint8_t *game_view_background;
static uint32_t *game_view_rgba;
static unsigned game_view_width,game_view_height;
static unsigned game_view_map,game_view_left,game_view_top;
static unsigned game_view_offset_x,game_view_offset_y;
static void game_view_close(void){
    if(game_view_texture)SDL_DestroyTexture(game_view_texture);
    free(game_view_pixels);free(game_view_background);free(game_view_rgba);game_view_texture=NULL;
    game_view_pixels=NULL;game_view_rgba=NULL;game_view_renderer=NULL;
    game_view_background=NULL;game_view_map=0;
    game_view_width=game_view_height=0;
}
static uint32_t game_lcd_color(unsigned value){
    return 0xff000000u | (183-value*142/15)<<16 | (200-value*146/15)<<8 | (161-value*119/15);
}
static int game_view_present(SDL_Renderer *renderer,const uint8_t *native){
    if(!settings.rows || !game_view_playing || !*game_view_playing ||
       !game_view_file || !game_view_world)return 0;
    unsigned mode=game_view_world->game_mode;
    /* Map-related popups and scripted UI share the configured canvas.
       Battle/title have independent screens rather than a map backdrop. */
    if(mode==3||mode==7||!game_view_world->map_number)return 0;
    int overlay=mode!=1;
    /* Retain the LCD's four-pixel margins around N complete tile rows.
       This also leaves room for the last line/border of a native dialogue. */
    unsigned width=settings.cols*16,height=settings.rows*16+8;
    if(game_view_renderer!=renderer || width!=game_view_width || height!=game_view_height){
        game_view_close();game_view_pixels=malloc((size_t)width*height);
        game_view_background=malloc((size_t)width*height);
        game_view_rgba=malloc((size_t)width*height*sizeof(uint32_t));
        game_view_texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,(int)width,(int)height);
        if(!game_view_pixels||!game_view_background||!game_view_rgba||!game_view_texture){game_view_close();return 0;}
        game_view_renderer=renderer;game_view_width=width;game_view_height=height;
    }
    unsigned left=game_view_left,top=game_view_top;
    if(!overlay || game_view_map!=game_view_world->map_number ||
       game_view_offset_x!=game_view_world->view.offset_x || game_view_offset_y!=game_view_world->view.offset_y){
        if(npe_render_world_view(game_view_file,game_view_world,settings.cols,settings.rows,
            game_view_pixels,&left,&top))return 0;
        memmove(game_view_pixels+4*width,game_view_pixels,(size_t)width*(height-8));
        memset(game_view_pixels,game_view_world->map_margin_color,4*width);
        memset(game_view_pixels+(size_t)(height-4)*width,game_view_world->map_margin_color,4*width);
        memcpy(game_view_background,game_view_pixels,(size_t)width*height);
        game_view_map=game_view_world->map_number;game_view_left=left;game_view_top=top;
        game_view_offset_x=game_view_world->view.offset_x;game_view_offset_y=game_view_world->view.offset_y;
    }else memcpy(game_view_pixels,game_view_background,(size_t)width*height);
    /* Keep the exact native walking poses, erasures, notices and overlays in
       its existing camera rectangle. Surroundings are a read-only extension. */
    int dx=((int)game_view_world->view.offset_x-(int)left)*16;
    int dy=((int)game_view_world->view.offset_y-(int)top)*16;
    for(int y=overlay?0:4;y<(overlay?NPE_HEIGHT:116);++y)for(int x=0;x<NPE_WIDTH;++x){
        int px=dx+x,py=dy+y;
        if(px>=0&&py>=0&&(unsigned)px<width&&(unsigned)py<height)
            game_view_pixels[(unsigned)py*width+(unsigned)px]=native[y*NPE_WIDTH+x];
    }
    for(size_t i=0;i<(size_t)width*height;++i)game_view_rgba[i]=game_lcd_color(game_view_pixels[i]);
    SDL_UpdateTexture(game_view_texture,NULL,game_view_rgba,(int)width*4);
    SDL_RenderSetLogicalSize(renderer,(int)width,(int)height);
    SDL_RenderClear(renderer);SDL_RenderCopy(renderer,game_view_texture,NULL,NULL);
    SDL_RenderPresent(renderer);return 1;
}
