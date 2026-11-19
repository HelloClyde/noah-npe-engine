/* Read-only host map view. Does not move the native camera or activate NPCs. */
static int view_bitmap(const NpeFile *file,NpeIndexedBitmap *image){
    if(!image->indexed_resource)return 0;
    return npe_indexed_bitmap(file,image->resource_index,image);
}
static int view_blit(const NpeFile *file,NpeIndexedBitmap image,int left,int top,
    unsigned width,unsigned height,uint8_t *pixels,int transparent){
    if(view_bitmap(file,&image))return -1;
    for(unsigned y=0;y<image.height;++y)for(unsigned x=0;x<image.width;++x){
        int px=left+(int)x,py=top+(int)y;
        if(px<0||py<0||(unsigned)px>=width||(unsigned)py>=height)continue;
        uint8_t color=npe_indexed_pixel(&image,x,y);
        if(!transparent||color)pixels[(unsigned)py*width+(unsigned)px]=color;
    }
    return 0;
}
int npe_render_world_view(const NpeFile *file,const NpeWorld *world,unsigned cols,unsigned rows,
    uint8_t *pixels,unsigned *view_left,unsigned *view_top){
    if(!file||!world||!pixels||cols<15||cols>60||rows<7||rows>40||!world->map_number)return -1;
    NpeMap map;if(npe_map(file,world->map_number-1,&map))return -1;
    world_bind_tiles(world,&map);
    int left=(int)world->view.offset_x-((int)cols-15)/2;
    int top=(int)world->view.offset_y-((int)rows-7)/2;
    int max_left=map.width>cols?(int)(map.width-cols):0;
    int max_top=map.height>rows?(int)(map.height-rows):0;
    if(left>max_left)left=max_left;
    if(top>max_top)top=max_top;
    if(left<0)left=0;
    if(top<0)top=0;
    if(view_left)*view_left=(unsigned)left;
    if(view_top)*view_top=(unsigned)top;
    unsigned width=cols*16,height=rows*16;memset(pixels,2,(size_t)width*height);
    for(unsigned y=0;y<rows && top+(int)y<map.height;++y)
        for(unsigned x=0;x<cols && left+(int)x<map.width;++x){
            NpeIndexedBitmap tile;
            if(npe_indexed_bitmap(file,npe_map_tile(&map,(unsigned)left+x,(unsigned)top+y),&tile) ||
               tile.width!=16 || tile.height!=16 ||
               view_blit(file,tile,(int)x*16,(int)y*16,width,height,pixels,0))return -1;
        }
    for(unsigned pass=0;pass<2;++pass){
        NpeIndexedBitmap image;
        if(pass==1 && world->party_ids[0] &&
           (!actor_draw_bitmap(file,world->party_ids[0]-1,world->direction,1,&image)))
            if(view_blit(file,image,((int)world->x-left)*16,((int)world->y-top)*16-16,
                width,height,pixels,1))return -1;
        for(unsigned i=0;i<20;++i){
            const uint8_t *npc;unsigned x,y;
            if(npe_world_npc(world,&map,i,&npc))return -1;
            if(!npe_map_npc_active(npc,world->story_progress))continue;
            if(npe_world_npc_position(world,npc,i,&x,&y))return -1;
            if((y>world->y)!=(pass==1))continue;
            if(npc_draw_bitmap(file,((unsigned)npc[4]*256+npc[5])-1,
                world->npc_override_direction[i]<=3?world->npc_override_direction[i]:npc[6],&image))continue;
            if(view_blit(file,image,((int)x-left)*16,((int)y-top)*16-16,width,height,pixels,1))return -1;
        }
        for(unsigned i=0;i<4;++i){
            NpeMapMonster monster;if(npe_world_monster(world,&map,i,&monster))return -1;
            if(!npe_map_monster_active(&monster,world->story_progress) || (monster.y>world->y)!=(pass==1) ||
               npc_draw_bitmap(file,monster.graphic-1,0,&image))continue;
            if(view_blit(file,image,((int)monster.x-left)*16,((int)monster.y-top)*16-16,width,height,pixels,1))return -1;
        }
    }
    for(unsigned i=0;i<2;++i){
        NpeMapChase chase;NpeIndexedBitmap image;int found=npe_world_chase(world,&map,i,&chase);
        if(found<0)return -1;
        if(!found||!chase.enemy_id)break;
        if(!chase_active(&chase,world->story_progress)||world->chase_cooldown[i])continue;
        unsigned index=npe_image_group_index(file,2,chase.graphic);
        if(npe_indexed_bitmap(file,index,&image))continue;
        if(view_blit(file,image,((int)world->chase_x[i]-left)*16,
            ((int)world->chase_y[i]-top)*16-16,width,height,pixels,1))return -1;
    }
    return 0;
}
