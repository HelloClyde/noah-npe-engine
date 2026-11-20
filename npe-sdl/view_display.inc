static NpeTextPage game_view_text;
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
/* UI is a separate host layer; never move a rectangle of map pixels with it. */
static uint8_t game_view_menu_mask[NPE_WIDTH*NPE_HEIGHT];
static uint8_t game_view_menu_base[NPE_WIDTH*NPE_HEIGHT];
static int game_view_menu_valid;
static void game_view_mask_rect(int x,int y,int w,int h){
    for(int row=y;row<y+h&&row<NPE_HEIGHT;++row)
        for(int col=x;col<x+w&&col<NPE_WIDTH;++col)
            if(row>=0&&col>=0)game_view_menu_mask[row*NPE_WIDTH+col]=1;
}
static void game_view_mask_resource(const NpeFile *file,unsigned image,int x,int y){
    NpeIndexedBitmap bitmap;
    if(!npe_indexed_bitmap(file,npe_image_group_index(file,7,(uint16_t)image),&bitmap))
        game_view_mask_rect(x,y,bitmap.width,bitmap.height);
}
static void game_view_menu_layer(const NpeFile *file,const uint8_t *base,
                                 unsigned selection,int submenu,int saves){
    memcpy(game_view_menu_base,base,sizeof(game_view_menu_base));
    memset(game_view_menu_mask,0,sizeof(game_view_menu_mask));
    game_view_mask_resource(file,1,0,7);game_view_mask_resource(file,2,0,37);
    if(submenu){
        unsigned image=selection==0?3:selection==2?4:5;
        game_view_mask_resource(file,image,42,selection==2?70:37);
    }
    /* Save-slot and later native panels may overlap the menu. Their changed
       scanline spans are added at presentation, including opaque interiors. */
    if(saves)game_view_mask_resource(file,12,105,46);
    game_view_menu_valid=1;
}
static void game_view_blit(const uint8_t *src,int sx,int sy,int w,int h,int x,int y){
    for(int row=0;row<h;++row)for(int col=0;col<w;++col){
        int px=x+col,py=y+row;
        if(px>=0&&py>=0&&(unsigned)px<game_view_width&&(unsigned)py<game_view_height)
            game_view_pixels[py*game_view_width+px]=src[(sy+row)*NPE_WIDTH+sx+col];
    }
}
static void game_view_popup_resource(const uint8_t *native,unsigned image,int x,int y,int dx,int dy){
    NpeIndexedBitmap bitmap;
    if(npe_indexed_bitmap(game_view_file,npe_image_group_index(game_view_file,7,(uint16_t)image),&bitmap))return;
    int w=bitmap.width,h=bitmap.height;
    if(w>NPE_WIDTH-x)w=NPE_WIDTH-x;
    if(h>NPE_HEIGHT-y)h=NPE_HEIGHT-y;
    game_view_blit(native,x,y,w,h,dx+x,dy+y);
}
static void game_view_close(void){
    if(game_scale_texture==game_view_texture)game_scale_texture=NULL;
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
    int dx=((int)game_view_world->view.offset_x-(int)left)*16;
    int dy=((int)game_view_world->view.offset_y-(int)top)*16;
    int dialogue=mode==2 || (mode==5 && game_view_world->text_style==0);
    int menu=mode==6 && game_view_world->property_page!=9 &&
        game_view_world->property_page!=10 && game_view_menu_valid;
    if(!overlay){
        game_view_blit(native,0,4,NPE_WIDTH,112,dx,dy+4);
        /* Freeze the actual displayed map, including the last native walking
           pose, rather than reconstructing it when a retained popup opens. */
        memcpy(game_view_background,game_view_pixels,(size_t)width*height);
        game_view_menu_valid=0;game_view_text.valid=0;
    }else if(dialogue){
        /* Preserve the native page and glyphs, but anchor its two-line
           window to the bottom of the whole viewport and extend its fill. */
        unsigned top=84,bottom=118;
        if(mode==5 && game_view_world->text_rect[1]<NPE_HEIGHT &&
           game_view_world->text_rect[3]<NPE_HEIGHT &&
           game_view_world->text_rect[3]>=game_view_world->text_rect[1]){
            top=game_view_world->text_rect[1];bottom=game_view_world->text_rect[3];
        }
        unsigned inset=width>=NPE_WIDTH+8?4:0;
        unsigned available=width-inset*2;
        unsigned h=bottom-top+1;
        if(game_view_text.valid){
            unsigned needed=npe_text_page_lines(&game_view_text,available)*16+3;
            if(needed>h)h=needed;
            if(h>height-4)h=height-4;
        }
        unsigned y=height>=h+4?height-h-4:0;
        memset(game_view_pixels+(size_t)y*width,0,(size_t)width*h);
        if(game_view_text.valid)npe_text_page_draw(&game_view_text,game_view_pixels,width,y+h,inset,y,available);
        else game_view_blit(native,0,(int)top,NPE_WIDTH,(int)(bottom-top+1),inset,(int)y);
    }else if(mode==17){
        /* Yes/no is painted over the retained LCD. Center only its two NPE
           resources, never the map rectangle that happens to surround them. */
        int mx=((int)width-NPE_WIDTH)/2,my=((int)height-NPE_HEIGHT)/2;
        game_view_popup_resource(native,7,70,45,mx,my);
        game_view_popup_resource(native,8,134,45,mx,my);
    }else if(menu){
        uint8_t mask[sizeof(game_view_menu_mask)];memcpy(mask,game_view_menu_mask,sizeof(mask));
        for(unsigned y=0;y<NPE_HEIGHT;++y){
            int first=NPE_WIDTH,last=-1;
            for(unsigned x=0;x<NPE_WIDTH;++x)
                if(!mask[y*NPE_WIDTH+x] && native[y*NPE_WIDTH+x]!=game_view_menu_base[y*NPE_WIDTH+x]){
                    if((int)x<first)first=(int)x;
                    last=(int)x;
                }
            if(last>=first)memset(mask+y*NPE_WIDTH+first,1,(size_t)(last-first+1));
        }
        unsigned mx=width>=NPE_WIDTH+8?4:0,my=height>=NPE_HEIGHT+8?4:0;
        for(unsigned y=0;y<NPE_HEIGHT;++y)for(unsigned x=0;x<NPE_WIDTH;++x)
            if(mask[y*NPE_WIDTH+x] && y+my<height && x+mx<width)
                game_view_pixels[(y+my)*width+x+mx]=native[y*NPE_WIDTH+x];
    }else{
        /* Independent member/shop/quiz panels are centered in the viewport. */
        game_view_blit(native,0,0,NPE_WIDTH,NPE_HEIGHT,
            ((int)width-NPE_WIDTH)/2,((int)height-NPE_HEIGHT)/2);
    }
    for(size_t i=0;i<(size_t)width*height;++i)game_view_rgba[i]=game_lcd_color(game_view_pixels[i]);
    SDL_UpdateTexture(game_view_texture,NULL,game_view_rgba,(int)width*4);
    SDL_RenderSetLogicalSize(renderer,(int)width,(int)height);
    SDL_RenderClear(renderer);SDL_RenderCopy(renderer,game_view_texture,NULL,NULL);
    SDL_RenderPresent(renderer);return 1;
}
