/* Window content size follows the canvas currently presented by SDL. */
static SDL_Window *game_scale_window;
static SDL_Texture *game_scale_texture;
static void game_window_canvas(int *width,int *height){
    *width=NPE_WIDTH;*height=NPE_HEIGHT;
    SDL_Renderer *renderer=game_scale_window?SDL_GetRenderer(game_scale_window):NULL;
    if(renderer){int w,h;SDL_RenderGetLogicalSize(renderer,&w,&h);if(w>0&&h>0){*width=w;*height=h;}}
}
static unsigned game_window_scale_current(void){
    if(!game_scale_window || (SDL_GetWindowFlags(game_scale_window)&(SDL_WINDOW_FULLSCREEN|SDL_WINDOW_MAXIMIZED)))return 0;
    int w,h,bw,bh;SDL_GetWindowSize(game_scale_window,&w,&h);game_window_canvas(&bw,&bh);
    for(unsigned scale=1;scale<=10;++scale)if(w==bw*(int)scale&&h==bh*(int)scale)return scale;
    return 0;
}
static void game_window_scale(unsigned scale){
    if(!game_scale_window||scale<1||scale>10)return;
    int width,height;game_window_canvas(&width,&height);
    if(SDL_GetWindowFlags(game_scale_window)&SDL_WINDOW_FULLSCREEN)SDL_SetWindowFullscreen(game_scale_window,0);
    if(SDL_GetWindowFlags(game_scale_window)&SDL_WINDOW_MAXIMIZED)SDL_RestoreWindow(game_scale_window);
    SDL_SetWindowSize(game_scale_window,width*(int)scale,height*(int)scale);
    SDL_Renderer *renderer=SDL_GetRenderer(game_scale_window);
    if(renderer&&game_scale_texture){
        SDL_RenderClear(renderer);SDL_RenderCopy(renderer,game_scale_texture,NULL,NULL);SDL_RenderPresent(renderer);
    }
}
static void game_window_scale_request(unsigned scale){
    SDL_Event event;SDL_zero(event);event.type=SDL_USEREVENT;
    event.user.code=0x4e50455a;event.user.data1=(void *)(uintptr_t)scale;SDL_PushEvent(&event);
}
#ifdef __APPLE__
unsigned npe_macos_window_scale_current(void){return game_window_scale_current();}
#endif
