static const NpeFile *game_view_file;
static const NpeWorld *game_view_world;
static const int *game_view_playing;
static SDL_Texture *game_view_texture;
static SDL_Renderer *game_view_renderer;
static uint8_t *game_view_pixels;
static uint32_t *game_view_rgba;
static unsigned game_view_width,game_view_height;
static void game_view_close(void){
    if(game_view_texture)SDL_DestroyTexture(game_view_texture);
    free(game_view_pixels);free(game_view_rgba);game_view_texture=NULL;
    game_view_pixels=NULL;game_view_rgba=NULL;game_view_renderer=NULL;
    game_view_width=game_view_height=0;
}
static uint32_t game_lcd_color(unsigned value){
    return 0xff000000u | (183-value*142/15)<<16 | (200-value*146/15)<<8 | (161-value*119/15);
}
static int game_view_present(SDL_Renderer *renderer,const uint8_t *native){
    if(!settings.rows || !game_view_playing || !*game_view_playing ||
       !game_view_file || !game_view_world || game_view_world->game_mode!=1)return 0;
    unsigned width=settings.cols*16,height=settings.rows*16;
    if(game_view_renderer!=renderer || width!=game_view_width || height!=game_view_height){
        game_view_close();game_view_pixels=malloc((size_t)width*height);
        game_view_rgba=malloc((size_t)width*height*sizeof(uint32_t));
        game_view_texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,(int)width,(int)height);
        if(!game_view_pixels||!game_view_rgba||!game_view_texture){game_view_close();return 0;}
        game_view_renderer=renderer;game_view_width=width;game_view_height=height;
    }
    unsigned left,top;
    if(npe_render_world_view(game_view_file,game_view_world,settings.cols,settings.rows,
        game_view_pixels,&left,&top))return 0;
    /* Keep the exact native walking poses, erasures, notices and overlays in
       its existing camera rectangle. Surroundings are a read-only extension. */
    int dx=((int)game_view_world->view.offset_x-(int)left)*16;
    int dy=((int)game_view_world->view.offset_y-(int)top)*16;
    for(int y=0;y<112;++y)for(int x=0;x<NPE_WIDTH;++x){
        int px=dx+x,py=dy+y;
        if(px>=0&&py>=0&&(unsigned)px<width&&(unsigned)py<height)
            game_view_pixels[(unsigned)py*width+(unsigned)px]=native[(y+4)*NPE_WIDTH+x];
    }
    for(size_t i=0;i<(size_t)width*height;++i)game_view_rgba[i]=game_lcd_color(game_view_pixels[i]);
    SDL_UpdateTexture(game_view_texture,NULL,game_view_rgba,(int)width*4);
    SDL_RenderSetLogicalSize(renderer,(int)width,(int)height);
    SDL_RenderClear(renderer);SDL_RenderCopy(renderer,game_view_texture,NULL,NULL);
    SDL_RenderPresent(renderer);return 1;
}
