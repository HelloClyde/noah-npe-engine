#include "../npe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
    assert(argc==2);FILE *fp=fopen(argv[1],"rb");assert(fp);fseek(fp,0,SEEK_END);long size=ftell(fp);rewind(fp);
    uint8_t *bytes=malloc((size_t)size);assert(bytes&&fread(bytes,1,(size_t)size,fp)==(size_t)size);fclose(fp);
    NpeFile file;char error[256];assert(!npe_parse(&file,bytes,(size_t)size,error,sizeof(error)));
    NpeWorld *world=calloc(1,sizeof(*world)),*before=malloc(sizeof(*before));assert(world&&before);
    unsigned maps=file.offset_table_records[8];
    for(unsigned index=0;index<maps;index+=maps>12?maps/12:1){
        NpeMap map;assert(!npe_map(&file,index,&map));if(!map.width||!map.height)continue;
        assert(!npe_world_init(&file,world,index+1,map.width/2,map.height/2));
        memcpy(before,world,sizeof(*before));
        for(unsigned pass=0;pass<3;++pass){
            unsigned cols=pass==0?15:pass==1?25:60,rows=pass==0?7:pass==1?14:40;
            size_t count=(size_t)cols*16*rows*16;uint8_t *pixels=malloc(count+32);assert(pixels);
            memset(pixels,0xcc,count+32);unsigned left,top;
            assert(!npe_render_world_view(&file,world,cols,rows,pixels+16,&left,&top));
            assert(left<map.width&&top<map.height);
            for(unsigned i=0;i<16;++i)assert(pixels[i]==0xcc&&pixels[count+16+i]==0xcc);
            for(size_t i=0;i<count;++i)assert(pixels[16+i]<=15);
            assert(!memcmp(before,world,sizeof(*world)));free(pixels);
        }
    }
    uint8_t small[32];assert(npe_render_world_view(&file,world,61,40,small,NULL,NULL)<0);
    free(before);free(world);npe_free(&file);puts("Expanded views: bounds, clipping, resource colors and unchanged world state passed");return 0;
}
