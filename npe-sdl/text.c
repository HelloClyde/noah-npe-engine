#include "text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quiz_keyboard.inc"
#include "native_npc_fonts.inc"
#include "native12_control_glyphs.inc"
#ifdef _WIN32
#include <windows.h>
#endif

int npe_render_battle_entry_frame(const NpeFile *file,const NpeFont *font,
                                  const NpeWorld *world,unsigned stage,
                                  uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    static const uint8_t body[]={0xcc,0xe5},qi[]={0xc6,0xf8};
    if(!file || !world || !pixels || stage>3 || world->battle_actor>=3)return -1;
    if(!stage)return npe_render_battle_actors_scene(file,world,pixels);
    if(stage==1)return npe_render_battle_cursor(file,world,pixels);
    /* Ordinary entry4E:7FAA..7FB0 draws controls before the HUD;
       scripted entry4E:8BF0..8BFE draws them in the opposite order. */
    if(stage==(world->story_event_id?3u:2u))
        return npe_render_battle_controls(file,1,pixels);
    if(!font || !font->data ||
       npe_draw_gbk_text(font,body,sizeof(body),70,88,pixels) ||
       npe_draw_gbk_text(font,qi,sizeof(qi),70,104,pixels))return -1;
    unsigned actor=world->battle_actor;
    npe_render_battle_hud_values(world->hp[actor],world->max_hp[actor],
        world->qi[actor],world->max_qi[actor],pixels);
    return 0;
}

enum { GLYPH_BYTES = 24, ASCII_COUNT = 95,
       GBK_LEADS = 126, GBK_TRAILS = 191,
       FONT_BYTES = 8 + (ASCII_COUNT + GBK_LEADS * GBK_TRAILS) * GLYPH_BYTES,
       FONT16_EXTRA = ASCII_COUNT + 94 * 94,
       FONT16_EXTENDED = FONT16_EXTRA + 33,
       FONT16_FIRST_COUNT = 32 * 191,
       FONT16_EXTENDED_COUNT = FONT16_FIRST_COUNT + 87 * 97,
       FONT16_BYTES = 8 + (FONT16_EXTENDED + FONT16_EXTENDED_COUNT) * 32,
       DIALOGUE_COLUMNS = 19, DIALOGUE_ROWS = 5 };

/* SDL_GetBasePath supplies UTF-8, independently of the Windows ANSI code page.
   Keep the font library usable without a dependency on SDL. */
static FILE *font_file_open(const char *path) {
#ifdef _WIN32
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    wchar_t *wide;
    FILE *input;
    if (!count || (size_t)count > SIZE_MAX / sizeof(*wide)) return NULL;
    wide = malloc((size_t)count * sizeof(*wide));
    if (!wide) return NULL;
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, count)) {
        free(wide);
        return NULL;
    }
    input = _wfopen(wide, L"rb");
    free(wide);
    return input;
#else
    return fopen(path, "rb");
#endif
}

int npe_font_load(NpeFont *font, const char *path) {
    FILE *input;
    uint8_t *data;
    size_t got;
    int extra;
    if (!font || !path) return -1;
    input = font_file_open(path);
    if (!input) return -1;
    data = malloc(FONT_BYTES);
    if (!data) { fclose(input); return -1; }
    got = fread(data, 1, FONT_BYTES, input);
    extra = fgetc(input);
    fclose(input);
    if (got != FONT_BYTES || extra != EOF ||
        memcmp(data, "NPEFONT1", 8)) {
        free(data);
        return -1;
    }
    free(font->data);
    font->data = data;
    return 0;
}

int npe_font_load16(NpeFont *font, const char *path) {
    FILE *input;
    uint8_t *data;
    size_t got;
    int extra;
    if (!font || !path) return -1;
    input = font_file_open(path);
    if (!input) return -1;
    data = malloc(FONT16_BYTES);
    if (!data) { fclose(input); return -1; }
    got = fread(data, 1, FONT16_BYTES, input);
    extra = fgetc(input);
    fclose(input);
    if (got != FONT16_BYTES || extra != EOF ||
        memcmp(data, "NPEF16V3", 8)) {
        free(data);
        return -1;
    }
    free(font->data16);
    font->data16 = data;
    return 0;
}

void npe_font_free(NpeFont *font) {
    if (font) {
        free(font->data);
        free(font->data16);
        font->data = font->data16 = NULL;
    }
}

size_t npe_gbk_page_end(const uint8_t *text, size_t size, size_t start) {
    size_t pos;
    if (!text || start > size) return SIZE_MAX;
    for (pos = start; pos < size;) {
        if (text[pos] == '{') return pos;
        if (text[pos] >= 0x81 && text[pos] <= 0xfe) {
            if (pos + 1 >= size || text[pos + 1] < 0x40 ||
                text[pos + 1] > 0xfe ||
                (text[pos + 1] == 0x7f && text[pos] >= 0xa1 && text[pos] <= 0xa7))
                return SIZE_MAX;
            pos += 2;
        } else pos++;
    }
    return size;
}

static const uint8_t *glyph(const NpeFont *font, const uint8_t *text,
                            size_t size, size_t *advance) {
    unsigned index;
    if (!size) return NULL;
    unsigned lead = text[0], trail = size > 1 ? text[1] : 0;
    if (lead < 0x80) {
        *advance = 1;
        /* Original28:FD38 copies control/DEL slots from the same ASCII table. */
        if (lead < 0x20 || lead == 0x7f)
            return native12_controls[lead == 0x7f ? 32 : lead];
        index = lead - 0x20;
    } else {
        *advance = 2;
        /* Original28:1F08 consumes both bytes even when the trail is zero.
           Original17:9D64 uses the resident A1A1 glyph for every other pair. */
        if ((lead >= 0xa1 && lead <= 0xfd && trail >= 0xa1 && trail <= 0xfe) ||
            (lead >= 0x81 && lead <= 0xa0 && trail >= 0x40 && trail <= 0xfe) ||
            (lead >= 0xa8 && lead <= 0xfe && trail >= 0x40 && trail <= 0xa0))
            index = ASCII_COUNT + (lead - 0x81) * GBK_TRAILS + trail - 0x40;
        else index = ASCII_COUNT + (0xa1 - 0x81) * GBK_TRAILS + 0xa1 - 0x40;
    }
    return font->data + 8 + (size_t)index * GLYPH_BYTES;
}

static int native_text_coordinate(unsigned value) {
    value &= 0xffffu;
    return value < 0x8000u ? (int)value : (int)value - 0x10000;
}

static void draw_native_glyph_rows(const uint8_t *bits, int left, int top,
                                    unsigned width, int ascii,
                                    unsigned first_row, unsigned end_row,
                                    uint8_t color, uint8_t *pixels) {
    /* Original17:9DA2/18:660A reject negative Chinese coordinates and clip
       its cell. Original28:FD38 leaves ASCII width unclipped. In the original
       bottom-up packed buffer, a right-edge spill reaches the previous row. */
    if (!ascii) {
        if (left < 0 || top < 0 || left >= NPE_WIDTH || top >= NPE_HEIGHT)
            return;
        if (width > (unsigned)(NPE_WIDTH - left)) width = NPE_WIDTH - left;
    }
    for (unsigned y = first_row; y < end_row; ++y) {
        int row = top + (int)y;
        if (row >= NPE_HEIGHT) break;
        unsigned mask = (unsigned)bits[y * 2] << 8 | bits[y * 2 + 1];
        for (unsigned x = 0; x < width; ++x) {
            if (!(mask & (0x8000u >> x))) continue;
            if (ascii) {
                int packed_pixel = (NPE_HEIGHT - 1 - row) * NPE_WIDTH + left + (int)x;
                if (packed_pixel >= 0 && packed_pixel < NPE_WIDTH * NPE_HEIGHT) {
                    unsigned visible_row = NPE_HEIGHT - 1 - packed_pixel / NPE_WIDTH;
                    pixels[visible_row * NPE_WIDTH + packed_pixel % NPE_WIDTH] = color;
                }
            } else pixels[row * NPE_WIDTH + left + x] = color;
        }
    }
}

static int draw_native12_text_color(const NpeFont *font, const uint8_t *text,
                                    size_t size, unsigned left, unsigned top,
                                    unsigned ascii_pitch, uint8_t color,
                                    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    size_t pos;
    unsigned cursor = left & 0xffffu;
    int row = native_text_coordinate(top);
    if (!font || !font->data || (!text && size) || !pixels) return -1;
    for (pos = 0; pos < size && text[pos];) {
        const uint8_t *bits;
        size_t advance;
        unsigned pitch;
        int column = native_text_coordinate(cursor);
        /* Original28:1F08 checks signed word coordinates before each byte. */
        if (column >= NPE_WIDTH - 1 || row >= NPE_HEIGHT - 1) break;
        bits = glyph(font, text + pos, size - pos, &advance);
        if (!bits) return -1;
        pitch = advance == 1 ? ascii_pitch : 12;
        draw_native_glyph_rows(bits, column, row, pitch, advance == 1,
                               0, 12, color, pixels);
        cursor = (cursor + pitch) & 0xffffu;
        pos += advance;
    }
    return 0;
}

int npe_draw_gbk_text_color(const NpeFont *font, const uint8_t *text,
                            size_t size, unsigned left, unsigned top,
                            uint8_t color,
                            uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    return draw_native12_text_color(font, text, size, left, top, 8, color, pixels);
}

int npe_draw_gbk_text(const NpeFont *font, const uint8_t *text, size_t size,
                      unsigned left, unsigned top,
                      uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    return npe_draw_gbk_text_color(font, text, size, left, top, 15, pixels);
}

static const uint8_t *glyph16(const NpeFont *font,unsigned lead,unsigned trail,
                               unsigned *width){
    size_t index;
    if(lead<0x80){
        /* Original28:1F08 uses resident control glyphs as well as ASCII. */
        index=lead<0x20?FONT16_EXTRA+lead:
              lead==0x7f?FONT16_EXTRA+32:lead-0x20;
        *width=8;
    }else{
        if(lead>=0xa1&&lead<=0xfd&&trail>=0xa1&&trail<=0xfe)
            index=ASCII_COUNT+(size_t)(lead-0xa1)*94+trail-0xa1;
        else if(lead>=0x81&&lead<=0xa0&&trail>=0x40&&trail<=0xfe)
            index=FONT16_EXTENDED+(size_t)(lead-0x81)*191+trail-0x40;
        else if(lead>=0xa8&&lead<=0xfe&&trail>=0x40&&trail<=0xa0)
            index=FONT16_EXTENDED+FONT16_FIRST_COUNT+
                  (size_t)(lead-0xa8)*97+trail-0x40;
        else index=ASCII_COUNT; /* Original17:95E8 resident A1A1 fallback. */
        *width=16;
    }
    return font->data16+8+index*32;
}

static int draw_gbk_text16_rows(const NpeFont *font, const uint8_t *text,
                                size_t size, unsigned left, unsigned top,
                                unsigned first_row, unsigned end_row,
                                uint8_t color,
                                uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    size_t pos;
    unsigned cursor = left & 0xffffu;
    int row = native_text_coordinate(top);
    if (!font || !font->data16 || (!text && size) || !pixels ||
        first_row > end_row ||
        end_row > 16) return -1;
    for (pos = 0; pos < size && text[pos];) {
        size_t advance;
        unsigned width;
        const uint8_t *bits;
        int column = native_text_coordinate(cursor);
        if (column >= NPE_WIDTH - 1 || row >= NPE_HEIGHT - 1) break;
        bits=glyph16(font,text[pos],pos+1<size?text[pos+1]:0,&width);
        advance=width/8;
        draw_native_glyph_rows(bits, column, row, width, advance == 1,
                               first_row, end_row, color, pixels);
        cursor = (cursor + width) & 0xffffu;
        pos += advance;
    }
    return 0;
}

int npe_draw_gbk_text16_color(const NpeFont *font, const uint8_t *text,
                              size_t size, unsigned left, unsigned top,
                              uint8_t color,
                              uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    return draw_gbk_text16_rows(font, text, size, left, top,
                                0, 16, color, pixels);
}

int npe_render_native_title_screen(const NpeFile *file, const NpeFont *font,
                                    unsigned selection,
                                    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    static const uint8_t title_prefix[] = {
        'R','P','G',0xcf,0xb5,0xc1,0xd0,0xd6,0xae,0xa3,0xad
    };
    static const uint8_t choices[3][8] = {
        {0xd0,0xc2,0xb5,0xc4,0xd5,0xf7,0xb3,0xcc},
        {0xd4,0xd9,0xd0,0xf8,0xc7,0xb0,0xd4,0xb5},
        {0xcd,0xcb,0xb3,0xf6,0xd3,0xce,0xcf,0xb7}
    };
    uint16_t backdrop=npe_image_group_index(file,7,15);
    uint8_t title[sizeof(title_prefix) + 12];
    unsigned x, y, row;
    size_t title_size = 0;
    if (!file || !font || !font->data16 || !pixels || selection >= 3)
        return -1;
    if(npe_render_indexed_resource(file,backdrop,0,0,0,pixels))return -1;
    while (title_size < 12 && file->title[title_size]) ++title_size;
    memcpy(title, title_prefix, sizeof(title_prefix));
    memcpy(title + sizeof(title_prefix), file->title, title_size);
    if (npe_draw_gbk_text16_color(font, title,
                                  sizeof(title_prefix) + title_size,
                                  43, 30, 15, pixels)) return -1;
    for (row = 0; row < 3; ++row)
        if (npe_draw_gbk_text16_color(font, choices[row],
                                      sizeof(choices[row]),
                                      88, 57 + row * 16, 15, pixels))
            return -1;
    for (y = 57 + selection * 16; y < 74 + selection * 16; ++y) {
        pixels[y * NPE_WIDTH + 86] = 7;
        for (x = 87; x < 154; ++x)
            pixels[y * NPE_WIDTH + x] = 15;
    }
    if (selection == 2)
        for (x = 87; x < 154; ++x)
            pixels[105 * NPE_WIDTH + x] = 7;
    if (npe_draw_gbk_text16_color(font, choices[selection],
                                  sizeof(choices[selection]),
                                  88, 57 + selection * 16, 0, pixels))
        return -1;
    if (selection < 2 && draw_gbk_text16_rows(
            font, choices[selection + 1], sizeof(choices[selection + 1]),
            88, 57 + (selection + 1) * 16, 0, 1, 0, pixels))
        return -1;
    return 0;
}

static int draw_native12_compact(const NpeFont *font, const uint8_t *text,
                                  size_t size, unsigned left, unsigned top,
                                  uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    return draw_native12_text_color(font, text, size, left, top, 6, 15, pixels);
}

/* Mode8 uses the firmware's six-pixel ASCII cells. At the right edge
   a final ASCII cell still fits even though its source glyph is12 wide. */
static int quiz_text(const NpeFont *font,const uint8_t *text,size_t size,
                      unsigned left,unsigned top,unsigned limit,int wrap,
                      uint8_t *pixels) {
    unsigned x=left,y=top;
    for(size_t pos=0;pos<size&&pos<limit;) {
        size_t step=text[pos]<0x80?1:2;
        unsigned width=step==1?6:12;
        if(pos+step>size || pos+step>limit) break;
        if(wrap && x+width>NPE_WIDTH){x=0;y+=12;}
        if(y+12>(wrap?82u:NPE_HEIGHT)) break;
        if(text[pos]>=0x20) {
            size_t advance;
            const uint8_t *bits=glyph(font,text+pos,size-pos,&advance);
            if(!bits || advance!=step)return -1;
            for(unsigned row=0;row<12;++row) {
                unsigned mask=(unsigned)bits[2*row]<<8|bits[2*row+1];
                for(unsigned col=0;col<12&&x+col<NPE_WIDTH;++col)
                    if(mask&(0x8000u>>col))pixels[(y+row)*NPE_WIDTH+x+col]=15;
            }
        }
        pos+=step;x+=width;
    }
    return 0;
}

int npe_render_quiz_page(const NpeFile *file,const NpeFont *font,
                         const NpeWorld *world,
                         uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    NpeQuizQuestion question;uint16_t backdrop=npe_image_group_index(file,7,19);
    if(!file||!font||!font->data||!world||!world->quiz_ready||!pixels ||
       npe_quiz_question(file,world->quiz_question_index,&question))return -1;
    if(npe_render_indexed_resource(file,backdrop,0,0,0,pixels))return -1;
    if(quiz_text(font,question.text[0],question.text_size[0],0,10,
                  UINT32_MAX,1,pixels))return -1;
    if(!question.input_type) {
        for(unsigned i=0;i<4;++i)
            if(quiz_text(font,question.text[i+1],question.text_size[i+1],
                          (i&1)*120,84+(i/2)*12,20,0,pixels))return -1;
        unsigned i=world->event_context_id;
        if(i<4) {
            unsigned left=(i&1)*120,top=84+(i/2)*12;
            unsigned right=left+6*(uint8_t)question.text_size[i+1];
            for(unsigned y=top;y<=top+12&&y<NPE_HEIGHT;++y)
                for(unsigned x=left;x<=right&&x<NPE_WIDTH;++x)
                    pixels[y*NPE_WIDTH+x]=15-pixels[y*NPE_WIDTH+x];
        }
    } else {
        static const uint8_t answer[]={0xb4,0xf0,0xb0,0xb8,0xa3,0xba};
        /*46:91FA initializes the resident English keyboard footer. */
        memset(pixels+104*NPE_WIDTH,0,16*NPE_WIDTH);
        for(unsigned y=0;y<16;++y)for(unsigned x=0;x<24;++x) {
            unsigned value=quiz_english_keyboard[y*12+x/2];
            pixels[(104+y)*NPE_WIDTH+x]=(x&1)?value&15:value>>4;
        }
        for(unsigned x=0;x<NPE_WIDTH;++x)pixels[84*NPE_WIDTH+x]=15;
        if(quiz_text(font,answer,sizeof(answer),0,86,40,0,pixels))return -1;
        for(unsigned y=87;y<=99;++y)memset(pixels+y*NPE_WIDTH+36,0,193);
        if(quiz_text(font,(const uint8_t *)world->quiz_input,
                      world->quiz_input_length,36,87,33,0,pixels))return -1;
    }
    return 0;
}

static void shop_money(uint32_t money,char number[32]) {
    /*49:FA58..FC2E formats a signed word quotient plus a four-digit tail. */
    int16_t high=(int16_t)((int32_t)money/10000);
    int16_t low=(int16_t)(money-(uint32_t)((int32_t)high*10000));
    if(!high)snprintf(number,32,"%d",low);
    else snprintf(number,32,"%d%s%d",high,low>=100&&low<=999?"0":low>=10&&low<=99?"00":low<=9?"000":"",low);
}
static int shop_counter(const NpeFont *font,const uint8_t *text,size_t size,
                         unsigned limit,unsigned x,unsigned y,uint8_t *pixels) {
    /*2F:A0B6 renders a fixed number of compact cells, padding with blanks. */
    for(unsigned row=y;row<y+12;++row)memset(pixels+row*NPE_WIDTH+x,0,limit*6);
    return draw_native12_compact(font,text,size<limit?size:limit,x,y,pixels);
}
int npe_render_shop_totals(const NpeFont *font,const NpeWorld *world,unsigned item_id,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    char number[32];shop_money(world->money,number);
    if(shop_counter(font,(const uint8_t *)number,strlen(number),6,25,93,pixels))return -1;
    if(item_id){snprintf(number,sizeof(number),"%d",(int16_t)world->condition_words[item_id]);}
    else number[0]=0;
    return shop_counter(font,(const uint8_t *)number,strlen(number),4,35,68,pixels);
}
static int shop_cursor_star(const NpeFont *font,unsigned row,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    static const uint8_t star[]={0xa1,0xef};
    unsigned top=14+row*13;
    if(top>=NPE_HEIGHT)return 0;
    if(top+12<=NPE_HEIGHT)return draw_native12_compact(font,star,sizeof(star),200,top,pixels);
    /* Native LCD drawing clips a byte-row cursor against the screen.
       Render the glyph at a valid origin before copying its visible rows. */
    uint8_t glyph[NPE_WIDTH*NPE_HEIGHT]={0};
    if(draw_native12_compact(font,star,sizeof(star),200,0,glyph))return -1;
    for(unsigned y=top;y<NPE_HEIGHT;++y)for(unsigned x=200;x<212;++x)
        if(glyph[(y-top)*NPE_WIDTH+x])pixels[y*NPE_WIDTH+x]=15;
    return 0;
}

static int render_shop_list(const NpeFile *file,const NpeFont *font,const NpeWorld *world,
                         unsigned context,unsigned first,unsigned selection,
                         uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    static const uint8_t buy[]={0xc2,0xf2},sell[]={0xca,0xdb},owned[]={0xcf,0xd6,0xd3,0xd0},money[]={0xc7,0xae};
    uint8_t ids[254];unsigned count;uint16_t icon;char number[32];
    int selling=world && world->shop_transaction_sell!=0;
    unsigned selected_id=0xffff;
    if(!world || !pixels || ((selling||world->shop_catalog_ready)?npe_world_shop_items(file,world,ids,&count):npe_shop_items(file,context,ids,&count)) ||
       (!world->shop_list_ready && count && (selection>=count || first>selection || selection>=first+7)))return -1;
    if(world->shop_list_ready)selected_id=world->shop_selected_item;
    else if(count)selected_id=ids[selection];
    int selected=selected_id!=0xffff;
    icon=npe_image_group_index(file,5,(uint16_t)selected_id);
    unsigned last=first+6,row=selection-first;
    if(world->shop_list_ready){
        first=0;
        while(first<count && ids[first]<world->shop_first_item)++first;
        last=first;
        while(last+1<count && ids[last+1]<=world->shop_last_item)++last;
        row=world->shop_row;
    }
    /*3F:DC18..DCB4: outer(0,0)..(238,118), two horizontal lines and
      the left panel divider. The final screen row/column stay clear. */
    memset(pixels,0,NPE_WIDTH*NPE_HEIGHT);
    for(unsigned x=0;x<=238;++x)pixels[x]=pixels[118*NPE_WIDTH+x]=15;
    for(unsigned y=0;y<=118;++y)pixels[y*NPE_WIDTH]=pixels[y*NPE_WIDTH+238]=15;
    for(unsigned x=10;x<=66;++x)pixels[60*NPE_WIDTH+x]=pixels[85*NPE_WIDTH+x]=15;
    for(unsigned y=10;y<=110;++y)pixels[y*NPE_WIDTH+66]=15;
    if(draw_native12_compact(font,selling?sell:buy,sizeof(buy),10,10,pixels) ||
       draw_native12_compact(font,owned,sizeof(owned),10,68,pixels) ||
       draw_native12_compact(font,money,sizeof(money),10,93,pixels))return -1;
    if(npe_render_shop_totals(font,world,selected?selected_id:0,pixels))return -1;
    if(selected && npe_render_indexed_resource(file,icon,19,25,0,pixels))return -1;
    for(unsigned i=first;selected && i<count && i<=last &&
        (!world->shop_list_ready || ids[i]<=world->shop_last_item);++i){NpeItem item;
        if(npe_native_item(file,ids[i],&item))return -1;
        unsigned y=14+(i-first)*13;
        if(draw_native12_compact(font,item.name,item.name_size,71,y,pixels))return -1;
        snprintf(number,sizeof(number),"%d",selling?(int16_t)(item.price>>1):(int16_t)item.price);
        if(draw_native12_compact(font,(const uint8_t *)number,strlen(number),155,y,pixels))return -1;
    }
    /*3D:F056..F06A returns before drawing item rows and the new cursor
      when the selected item is FFFF, even if retained bounds are valid. */
    return selected?shop_cursor_star(font,row,pixels):0;
}

int npe_render_shop_buy(const NpeFile *file,const NpeFont *font,const NpeWorld *world,
                         unsigned context,unsigned first,unsigned selection,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    return render_shop_list(file,font,world,context,first,selection,pixels);
}
int npe_render_shop_list(const NpeFile *file,const NpeFont *font,const NpeWorld *world,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    return render_shop_list(file,font,world,world->event_context_id,world->shop_first,world->shop_selection,pixels);
}
int npe_render_shop_cursor(const NpeFile *file,const NpeFont *font,const NpeWorld *world,
                           unsigned old_row,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    uint16_t icon;char number[8];
    if(!world || !pixels)return -1;
    icon=npe_image_group_index(file,5,world->shop_selected_item);
    /*40:C420..C508/42:5CB6..5DDE clears only the icon and old cursor;
      names, prices, money and other retained pixels are not redrawn. */
    for(unsigned y=25;y<=57;++y)memset(pixels+y*NPE_WIDTH+19,0,34);
    if(npe_render_indexed_resource(file,icon,19,25,0,pixels))return -1;
    if(world->shop_selected_item>=255 && world->shop_selected_item!=0xffff)return -1;
    snprintf(number,sizeof(number),"%d",(int16_t)(world->shop_selected_item==0xffff?
             world->inventory_prefix_word:world->condition_words[world->shop_selected_item]));
    if(shop_counter(font,(const uint8_t *)number,strlen(number),4,35,68,pixels))return -1;
    unsigned top=14+old_row*13;
    for(unsigned y=top;y<=top+16 && y<NPE_HEIGHT;++y)memset(pixels+y*NPE_WIDTH+200,0,21);
    return shop_cursor_star(font,world->shop_row,pixels);
}
int npe_render_shop_page(const NpeFile *file,const NpeFont *font,const NpeWorld *world,
                         uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    uint8_t fresh[NPE_WIDTH*NPE_HEIGHT];
    if(npe_render_shop_list(file,font,world,fresh))return -1;
    /*3F:DD3A..DD50/3D:F00C..F022 with clear=0 replaces70..230,10..115,
      updates money and item fields, and preserves every other retained pixel. */
    for(unsigned y=10;y<=115;++y)memcpy(pixels+y*NPE_WIDTH+70,fresh+y*NPE_WIDTH+70,161);
    for(unsigned y=93;y<105;++y)memcpy(pixels+y*NPE_WIDTH+25,fresh+y*NPE_WIDTH+25,36);
    if(world->shop_selected_item!=0xffff || world->shop_transaction_sell){
        for(unsigned y=25;y<=57;++y)memcpy(pixels+y*NPE_WIDTH+19,fresh+y*NPE_WIDTH+19,34);
        for(unsigned y=68;y<80;++y)memcpy(pixels+y*NPE_WIDTH+35,fresh+y*NPE_WIDTH+35,24);
        if(world->shop_selected_item!=0xffff)
            return npe_render_shop_cursor(file,font,world,world->shop_row,pixels);
    }
    return 0;
}
/* Original compact formatter: controls occupy one blank cell. Callers pass
   their terminated substring, not a host source span or its return count. */
static int description_compact(const NpeFont *font,const uint8_t *text,unsigned capacity,
                                unsigned left,unsigned top,unsigned right,unsigned bottom,uint8_t *pixels){
    unsigned x=left,y=top,edge=left+((right-left)/6u)*6u;
    for(unsigned pos=0;pos<capacity&&text[pos];){
        unsigned step=text[pos]>=0x80u?2u:1u,width=step*6u;
        if(x+width>edge){x=left;y+=12;}
        if(y+12>bottom)break;
        if(pos+step>capacity)return -1;
        /*29:E552 calls the opaque text service03:41A8 per formatted
          line. Clear occupied cells, including controls, not its whole
          rectangle: a short last line retains the underlying image. */
        for(unsigned row=y;row<y+12;++row)
            memset(pixels+row*NPE_WIDTH+x,0,width);
        if(text[pos]>=0x20u&&draw_native12_compact(font,text+pos,step,x,y,pixels))return -1;
        x+=width;pos+=step;
    }
    return 0;
}
static int description_runtime(const NpeFile *file,NpeWorld *world){
    if(!file->rpg_runtime&&npe_world_resources_open((NpeFile *)file,world))return -1;
    return npe_rpg_runtime_owner(file)==world?0:-1;
}
static int description_slice(const NpeFile *file,NpeWorld *world,uint32_t source,
                              uint16_t limit,int cursor_kind,int direction,uint8_t *buffer,unsigned capacity){
    uint16_t start,result,count=limit;
    if(!direction){
        world->item_description_length=world->shop_description_end=world->shop_description_start=0;
        if(npe_rpg_runtime_text_length(file,source,cursor_kind,&world->item_description_length))return -1;
        start=0;
    }else if(direction>0)start=world->shop_description_start=(uint16_t)(world->shop_description_end+1u);
    else{
        start=world->shop_description_start;count=(uint16_t)(0u-limit);
        world->shop_description_end=(uint16_t)(start-1u);
    }
    if(npe_rpg_runtime_text_slice(file,source,start,count,buffer,capacity,&result))return -1;
    if(!direction)world->shop_description_end=(uint16_t)(result-1u);
    else if(direction>0)world->shop_description_end=(uint16_t)(world->shop_description_end+result);
    else world->shop_description_start=(uint16_t)(start-result);
    return 0;
}
static int description_boundary(const NpeWorld *world,int direction){
    return direction>0?world->shop_description_end==(uint16_t)(world->item_description_length-1u):
        direction<0&&(!world->shop_description_start||world->shop_description_start>=0x8000u);
}
int npe_render_shop_quantity_counter(const NpeFont *font,const NpeWorld *world,uint8_t *pixels){
    char number[8];snprintf(number,sizeof(number),"%u",world->shop_quantity);
    return shop_counter(font,(const uint8_t *)number,strlen(number),4,115,65,pixels);
}
static void shop_invert(uint8_t *pixels,unsigned left,unsigned right){
    for(unsigned y=87;y<=99;++y)for(unsigned x=left;x<=right;++x)
        pixels[y*NPE_WIDTH+x]=15-pixels[y*NPE_WIDTH+x];
}
void npe_render_shop_choice_move(uint8_t *pixels){
    shop_invert(pixels,67,100);shop_invert(pixels,141,175);
}
int npe_render_shop_description(const NpeFile *file,const NpeFont *font,NpeWorld *world,
                                 int direction,uint8_t *pixels){
    if(!file||!font||!world||!pixels)return -1;
    /*30:6B9E/90E8 test live words before clearing or loading the item. */
    if(direction>0){
        if(world->shop_description_end==(uint16_t)(world->item_description_length-1u))return 0;
    }else if(direction<0&&world->shop_description_start>=0x8000u)return 0;
    else if(direction<0&&!world->shop_description_start)return 0;
    if(!file->rpg_runtime&&npe_world_resources_open((NpeFile *)file,world))return -1;
    if(npe_rpg_runtime_owner(file)!=world)return -1;
    for(unsigned y=9;y<=58;++y)memset(pixels+y*NPE_WIDTH+35,0,171);
    NpeItem item;if(npe_world_item(file,world,world->shop_selected_item,&item))return -1;
    uint16_t start,result,count=82;
    if(!direction){
        world->item_description_length=world->shop_description_end=world->shop_description_start=0;
        if(npe_rpg_runtime_text_length(file,UINT32_C(0x5273ab),1,&world->item_description_length))return -1;
        start=0;
    }else if(direction>0){
        start=world->shop_description_start=(uint16_t)(world->shop_description_end+1u);
    }else{
        start=world->shop_description_start;count=(uint16_t)-82;
        world->shop_description_end=(uint16_t)(start-1u);
    }
    uint8_t buffer[83]={0};
    if(npe_rpg_runtime_text_slice(file,UINT32_C(0x5273ab),start,count,buffer,sizeof(buffer),&result))return -1;
    if(!direction)world->shop_description_end=(uint16_t)(result-1u);
    else if(direction>0)world->shop_description_end=(uint16_t)(world->shop_description_end+result);
    else world->shop_description_start=(uint16_t)(start-result);
    /* Consume the actual terminated local, which can differ from the return
       count after signed word wrap. Compact formatter uses28 ASCII cells. */
    unsigned x=35,y=9;
    for(unsigned pos=0;pos<sizeof(buffer)&&buffer[pos];){
        unsigned step=buffer[pos]>=0x80u?2u:1u,width=step*6u;
        if(x+width>203){x=35;y+=12;}
        if(y+12>58)break;
        if(pos+step>sizeof(buffer))return -1;
        if(buffer[pos]>=0x20u&&draw_native12_compact(font,buffer+pos,step,x,y,pixels))return -1;
        x+=width;pos+=step;
    }
    return 1;
}
int npe_render_shop_quantity(const NpeFile *file,const NpeFont *font,NpeWorld *world,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    if(!file||!world||!pixels||world->shop_selected_item==0xffff)return -1;
    /*49:567C..5872 uses the current NPE UI base, never a game-specific image. */
    uint16_t popup=npe_image_group_index(file,7,9);
    if(npe_render_indexed_resource(file,popup,30,5,0,pixels)||
       npe_render_shop_quantity_counter(font,world,pixels)||
       npe_render_shop_description(file,font,world,0,pixels)!=1)return -1;
    shop_invert(pixels,world->shop_choice?67:141,world->shop_choice?100:175);
    return 0;
}

int npe_render_native_menu_money(const NpeFile *file,const NpeFont *font,
                                  const NpeWorld *world,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    uint16_t frame=npe_image_group_index(file,7,1);char number[16];
    if(!file || !world || !pixels)return -1;
    /* Restore the current NPE money frame before drawing the original
       compact12 font; this replaces the old synthetic 5x7 digits. */
    if(npe_render_indexed_resource(file,frame,0,7,0,pixels))return -1;
    snprintf(number,sizeof(number),"%u",world->money);
    return draw_native12_compact(font,(const uint8_t *)number,strlen(number),36,16,pixels);
}

static void invert_exit_row(unsigned top,uint8_t *pixels) {
    for(unsigned y=top;y<=top+16;++y)for(unsigned x=70;x<=166;++x)
        pixels[y*NPE_WIDTH+x]=15-pixels[y*NPE_WIDTH+x];
}

int npe_render_exit_choice(const NpeFile *file,const NpeFont *font,
                            const NpeWorld *world,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    /* Resident4C:A9EA: 请选择支线：; names come from the current NPE.
      36:8836 draws the title,36:88BA/893A request one16-pixel character. */
    static const uint8_t title[]={0xc7,0xeb,0xd1,0xa1,0xd4,0xf1,0xd6,0xa7,0xcf,0xdf,0xa3,0xba};
    NpeMap map,first,second;NpeMapExit exit;
    if(!world || !pixels || !world->map_number || npe_map(file,world->map_number-1,&map) ||
       npe_world_exit(world,&map,world->exit_slot,&exit) || !exit.destination_map || !exit.second_map ||
       npe_map(file,exit.destination_map-1,&first) || npe_map(file,exit.second_map-1,&second))return -1;
    memset(pixels,0,NPE_WIDTH*NPE_HEIGHT);
    if(npe_draw_gbk_text16_color(font,title,sizeof(title),38,15,15,pixels) ||
       npe_draw_gbk_text16_color(font,first.name,first.name[0]>=0x80?2:1,70,45,15,pixels) ||
       npe_draw_gbk_text16_color(font,second.name,second.name[0]>=0x80?2:1,70,68,15,pixels))return -1;
    invert_exit_row(45,pixels);return 0;
}

void npe_render_exit_choice_move(uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    /*37:1978..19B4 toggles both rows even on repeated up/down input. */
    if(pixels){invert_exit_row(45,pixels);invert_exit_row(68,pixels);}
}

static size_t member_status_text(const NpeWorld *world,unsigned member,
                                 int positive,uint8_t text[8]) {
    static const uint8_t names[4][2]={
        {0xb6,0xbe},{0xc3,0xdf},{0xc2,0xd2},{0xb7,0xe2}
    };
    size_t size=0;
    /*47:5C16..5E88 reads signed map words6A04..6A0A, in this order.
      Battle status words are separate and do not select these labels. */
    for(unsigned s=0;s<4;++s){
        int16_t value=(int16_t)world->status_guard[member][s];
        if(positive?value>0:value<0){memcpy(text+size,names[s],2);size+=2;}
    }
    return size;
}

static int render_native_member_page(const NpeFile *file, const NpeFont *font,
                                    const NpeWorld *world, unsigned member,int initial,
                                    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    static const uint8_t normal[] = {0xd5,0xfd,0xb3,0xa3};
    uint16_t backdrop=npe_image_group_index(file,7,18);
    uint16_t portrait=npe_world_portrait_index(file,world,member);
    const uint8_t *name_end;
    char number[32];
    uint8_t negative[8],positive[8];
    if (!file || !font || !font->data || !world || !pixels || member >= 3)
        return -1;
    if (!(name_end = memchr(world->party_names[member], 0, 12))) return -1;
    if((initial && npe_render_indexed_resource(file,backdrop,0,0,0,pixels)) ||
       npe_render_indexed_resource(file,portrait,14,9,0,pixels))return -1;
    size_t negative_size=member_status_text(world,member,0,negative);
    size_t positive_size=member_status_text(world,member,1,positive);
    if (shop_counter(font, world->party_names[member],
                              (size_t)(name_end - world->party_names[member]),
                              12,147,10,pixels) ||
        shop_counter(font,negative_size?negative:normal,
                     negative_size?negative_size:sizeof(normal),8,177,46,pixels) ||
        shop_counter(font,positive,positive_size,8,177,60,pixels))return -1;
#define STATUS_NUMBER(value, limit, left, top) do { \
    snprintf(number, sizeof(number), "%u", (unsigned)(value)); \
    if (shop_counter(font, (const uint8_t *)number, strlen(number), \
                              (limit),(left),(top),pixels)) return -1; \
} while (0)
#define STATUS_RATIO(a, b, limit, left, top) do { \
    snprintf(number, sizeof(number), "%u/%u", (unsigned)(a), (unsigned)(b)); \
    if (shop_counter(font, (const uint8_t *)number, strlen(number), \
                              (limit),(left),(top),pixels)) return -1; \
} while (0)
    STATUS_NUMBER(world->level[member],4,71,12);
    STATUS_RATIO(world->experience[member],
                 world->experience_to_next[member],11,147,24);
    STATUS_NUMBER(world->other_stats[member][2],4,116,46);
    STATUS_NUMBER(world->other_stats[member][1],4,116,60);
    STATUS_NUMBER(world->money,6,40,82);
    STATUS_NUMBER(world->other_stats[member][0],4,40,99);
    STATUS_NUMBER(world->attack[member],4,116,82);
    STATUS_NUMBER(world->defense[member],4,116,99);
    STATUS_RATIO(world->hp[member],world->max_hp[member],7,177,82);
    STATUS_RATIO(world->qi[member],world->max_qi[member],7,177,99);
#undef STATUS_NUMBER
#undef STATUS_RATIO
    return 0;
}
int npe_render_native_member_screen(const NpeFile *file,const NpeFont *font,
                                    const NpeWorld *world,unsigned member,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    return render_native_member_page(file,font,world,member,1,pixels);
}
int npe_render_native_member_view(const NpeFile *file,const NpeFont *font,
                                  const NpeWorld *world,const NpeWorld *before,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    int initial=!before || before->game_mode!=6 || before->property_page!=9;
    return world ? render_native_member_page(file,font,world,world->property_member,initial,pixels) : -1;
}

int npe_render_native_battle_member_view(const NpeFile *file,const NpeFont *font,
                                         const NpeWorld *world,unsigned member,int initial,uint8_t *pixels){
    uint8_t record[148];
    if(!world||member>=3||npe_world_battle_record_image(world,member,record))return -1;
    NpeWorld *view=malloc(sizeof(*view));if(!view)return -1;
    *view=*world;
    memcpy(view->party_names[member],record,12);
    view->level[member]=record[24];
#define RECORD_WORD(o) ((uint16_t)((unsigned)record[o]*256u+record[(o)+1]))
    view->party_characters[member].pose_ids[7]=RECORD_WORD(60);
    for(unsigned i=0;i<4;++i)view->status_guard[member][i]=RECORD_WORD(62+2*i);
    view->experience_to_next[member]=RECORD_WORD(74);view->experience[member]=RECORD_WORD(76);
    view->max_hp[member]=RECORD_WORD(78);view->hp[member]=RECORD_WORD(80);
    view->max_qi[member]=RECORD_WORD(82);view->qi[member]=RECORD_WORD(84);
    for(unsigned i=0;i<3;++i)view->other_stats[member][i]=RECORD_WORD(86+2*i);
    view->attack[member]=RECORD_WORD(96);view->defense[member]=RECORD_WORD(98);
#undef RECORD_WORD
    int result=render_native_member_page(file,font,view,member,initial,pixels);
    free(view);return result;
}

int npe_render_native_equipment_screen(const NpeFile *file,
                                       const NpeWorld *world, unsigned member,
                                       uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    uint16_t backdrop=npe_image_group_index(file,7,10);
    uint16_t portrait=npe_world_portrait_index(file,world,member);
    unsigned x, y;
    if (!file || !world || !pixels || member >= 3)
        return -1;
    if(npe_render_indexed_resource(file,backdrop,0,0,0,pixels) ||
       npe_render_indexed_resource(file,portrait,18,9,0,pixels))return -1;
    /* The native window service clears these edges and draws the empty
       lower-left selection frame after the NPE backdrop and portrait. */
    for (x = 85; x <= 226; ++x) pixels[73 * NPE_WIDTH + x] = 0;
    for (x = 6; x <= 234; ++x) pixels[77 * NPE_WIDTH + x] = 0;
    /*38:170E..173C uses UIbase+11, the current NPE selector bitmap. */
    uint16_t selector=npe_image_group_index(file,7,11);
    unsigned slot=world->equipment_view_slot;
    if(slot>=6)return -1;
    if(npe_render_indexed_resource(file,selector,(int16_t)(7+38*slot),78,2,pixels))return -1;
    for (y = 114; y <= 115; ++y)
        for (x = 6; x <= 234; ++x)
            pixels[y * NPE_WIDTH + x] = 0;
    return 0;
}

static void equipment_clear(const NpeWorld *world,uint8_t *pixels,
                             unsigned left,unsigned top,unsigned right,unsigned bottom) {
    for(unsigned y=top;y<=bottom;++y)
        memset(pixels+y*NPE_WIDTH+left,world->lcd_background,right-left+1);
}
int npe_render_equipment_unequip(const NpeWorld *world,uint8_t *pixels) {
    if(!world || !pixels || world->equipment_view_slot>=6)return -1;
    /*3F:87B6..8818: inclusive icon erase, twelve blank name cells,
      then the description rectangle. Keep the selector and other icons;
      the native33x33 erase can overlap the selector's inside edge. */
    unsigned x=9+38*world->equipment_view_slot;
    equipment_clear(world,pixels,x,80,x+32,112);
    equipment_clear(world,pixels,130,10,201,21);
    equipment_clear(world,pixels,85,28,226,73);
    return 0;
}
int npe_render_equipment_view(const NpeFile *file,const NpeFont *font,NpeWorld *world,
                              const NpeWorld *before,uint8_t *pixels) {
    static const unsigned slots[6]={0,2,3,4,1,5};
    if(!file || !font || !world || !pixels)return -1;
    unsigned member=world->property_member,slot=world->equipment_view_slot;
    if(member>=3 || slot>=6)return -1;
    int initial=!before || before->game_mode!=6 || before->property_page!=10;
    int member_change=initial || before->property_member!=member;
    int description_only=!initial && !member_change && before->equipment_view_slot==slot &&
        before->equipment_description_revision!=world->equipment_description_revision;
    if(initial){
        if(npe_render_native_equipment_screen(file,world,member,pixels))return -1;
    }else if(!member_change && !description_only){
        /*46:619A/7CAA clear four inclusive border strips, retaining icons. */
        unsigned x=7+38*before->equipment_view_slot;
        equipment_clear(world,pixels,x,78,x+36,80);
        equipment_clear(world,pixels,x,78,x+2,114);
        equipment_clear(world,pixels,x,112,x+36,114);
        equipment_clear(world,pixels,x+34,78,x+36,114);
    }
    if(member_change){
        uint16_t portrait=npe_world_portrait_index(file,world,member);
        if(npe_render_indexed_resource(file,portrait,18,9,0,pixels))return -1;
        equipment_clear(world,pixels,6,77,234,115);
        for(unsigned i=0;i<6;++i){
            unsigned item_id=world->equipped_items[member][slots[i]];
            if(!item_id)continue;
            uint16_t icon=npe_image_group_index(file,5,(uint16_t)item_id);
            if(npe_render_indexed_resource(file,icon,(int16_t)(9+38*i),80,0,pixels))return -1;
        }
    }
    if(!description_only){
        uint16_t selector=npe_image_group_index(file,7,11);
        if(npe_render_indexed_resource(file,selector,(int16_t)(7+38*slot),78,2,pixels))return -1;
        world->equipment_description_item=world->equipped_items[member][slots[slot]];
        equipment_clear(world,pixels,130,10,201,21);
    }
    equipment_clear(world,pixels,85,28,226,73);
    if(description_only){
        if(!world->equipment_description_local_ready)return -1;
        return description_compact(font,world->equipment_description_local,
            sizeof(world->equipment_description_local),85,28,226,73,pixels);
    }
    world->equipment_description_local_ready=0;
    if(world->equipment_description_item){
        NpeItem item;
        if(npe_world_item(file,world,world->equipment_description_item,&item))return -1;
        if(draw_native12_compact(font,item.name,item.name_size,130,10,pixels))return -1;
        if(description_runtime(file,world) || description_slice(file,world,UINT32_C(0x5273ab),
            66,1,0,world->equipment_description_local,sizeof(world->equipment_description_local)))return -1;
        world->equipment_description_local_ready=1;
        return description_compact(font,world->equipment_description_local,
            sizeof(world->equipment_description_local),85,28,226,73,pixels);
    }
    /*38:19B4 skips the loader and index initialization for an empty slot. */
    return 0;
}

static int draw_target_field(const NpeFont *font, const uint8_t label[4],
                             const uint8_t *value, size_t value_size,
                             unsigned left, unsigned top,
                             uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    static const uint8_t colon = ':';
    if (draw_native12_compact(font, label, 4, left, top, pixels) ||
        draw_native12_compact(font, &colon, 1, left + 24, top, pixels))
        return -1;
    return value_size ? draw_native12_compact(font, value, value_size,
                                              left + 30, top, pixels) : 0;
}

static int draw_magic_field(const NpeFont *font, const uint8_t label[4],
                            const uint8_t *value, size_t value_size,
                            unsigned left, unsigned top,
                            uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    static const uint8_t colon = ':';
    if (draw_native12_compact(font, label, 4, left, top, pixels) ||
        draw_native12_compact(font, &colon, 1, left + 24, top, pixels))
        return -1;
    return value_size ? draw_native12_compact(font, value, value_size,
                                              left + 31, top, pixels) : 0;
}

static int target_item_name(const NpeFile *file,const NpeWorld *world,
                            unsigned item_id,NpeItem *item) {
    if(world->game_mode==4 && world->item_record_id==(item_id&255u)){
        memset(item,0,sizeof(*item));
        item->name=world->item_record_bytes;
        const uint8_t *end=memchr(item->name,0,12);
        item->name_size=end?(size_t)(end-item->name):12;
        return 0;
    }
    return npe_native_item(file,item_id,item);
}

static int render_item_target(const NpeFile *file,
                                         const NpeFont *font,
                                         const NpeWorld *world,
                                         unsigned member, unsigned item_id,
                                         unsigned quantity,
                                         uint8_t pixels[NPE_WIDTH * NPE_HEIGHT],int full) {
    static const uint8_t left_labels[6][4] = {
        {0xbc,0xb6,0xb1,0xf0}, {0xbe,0xad,0xd1,0xe9},
        {0xc9,0xfa,0xc3,0xfc}, {0xc4,0xda,0xc1,0xa6},
        {0xb9,0xa5,0xbb,0xf7}, {0xb7,0xc0,0xd3,0xf9}
    };
    static const uint8_t right_labels[5][4] = {
        {0xd7,0xb4,0xcc,0xac}, {0xc3,0xe2,0xd2,0xdf},
        {0xc1,0xe9,0xc1,0xa6}, {0xd0,0xd2,0xd4,0xcb},
        {0xc9,0xed,0xb7,0xa8}
    };
    static const uint8_t normal[] = {0xd5,0xfd,0xb3,0xa3};
    static const uint8_t sleep[] = {0xc3,0xdf};
    uint16_t portrait=npe_world_portrait_index(file,world,member);
    uint16_t icon=npe_image_group_index(file,5,(uint16_t)item_id);
    NpeItem item;
    unsigned x, y;
    char number[32];
    if (!file || !font || !font->data || !world || !pixels || member >= 3 ||
        !world->party_active[member] || !item_id || quantity > 99) return -1;
    /*2F:59C8..5A18: group base plus the selected record's eighth image. */
    if (full && target_item_name(file, world, item_id, &item)) return -1;
    if(full)memset(pixels, 0, NPE_WIDTH * NPE_HEIGHT);
    if(full)for (x = 0; x < NPE_WIDTH - 1; ++x)
        pixels[x] = pixels[118 * NPE_WIDTH + x] = 15;
    if(full)for (y = 0; y < NPE_HEIGHT - 1; ++y)
        pixels[y * NPE_WIDTH] = pixels[y * NPE_WIDTH + 238] = 15;
    if(npe_render_indexed_resource(file,portrait,1,1,0,pixels))return -1;
    if(full)for (x = 8; x <= 54; ++x) pixels[62 * NPE_WIDTH + x] = 15;
    if(full)for (y = 8; y <= 104; ++y) pixels[y * NPE_WIDTH + 54] = 15;
    if(full && npe_render_indexed_resource(file,icon,11,63,0,pixels))return -1;
    if (full && draw_native12_compact(font, item.name, item.name_size,
                              11, 105, pixels)) return -1;
    snprintf(number, sizeof(number), "%u", quantity);
    if (draw_native12_compact(font, (const uint8_t *)number,
                              strlen(number), 38,
                              93, pixels)) return -1;
#define TARGET_NUMBER(label, value, x, y) do { \
    snprintf(number, sizeof(number), "%u", (unsigned)(value)); \
    if (draw_target_field(font, (label), (const uint8_t *)number, \
                          strlen(number), (x), (y), pixels)) return -1; \
} while (0)
#define TARGET_RATIO(label, a, b, x, y) do { \
    snprintf(number, sizeof(number), "%u/%u", (unsigned)(a), (unsigned)(b)); \
    if (draw_target_field(font, (label), (const uint8_t *)number, \
                          strlen(number), (x), (y), pixels)) return -1; \
} while (0)
    TARGET_NUMBER(left_labels[0], world->level[member], 55, 11);
    TARGET_RATIO(left_labels[1], world->experience[member],
                 world->experience_to_next[member], 55, 25);
    TARGET_RATIO(left_labels[2], world->hp[member],
                 world->max_hp[member], 55, 39);
    TARGET_RATIO(left_labels[3], world->qi[member],
                 world->max_qi[member], 55, 53);
    TARGET_NUMBER(left_labels[4], world->attack[member], 55, 67);
    TARGET_NUMBER(left_labels[5], world->defense[member], 55, 81);
    if (draw_target_field(font, right_labels[0], normal, sizeof(normal),
                          158, 25, pixels) ||
        draw_target_field(font, right_labels[1],
                          (world->game_mode == 3 ? world->battle_party_status[member][1] : world->status_guard[member][1]) ? sleep : NULL,
                          (world->game_mode == 3 ? world->battle_party_status[member][1] : world->status_guard[member][1]) ? sizeof(sleep) : 0,
                          158, 39, pixels)) return -1;
    TARGET_NUMBER(right_labels[2], world->other_stats[member][0], 158, 53);
    TARGET_NUMBER(right_labels[3], world->other_stats[member][2], 158, 67);
    TARGET_NUMBER(right_labels[4], world->other_stats[member][1], 158, 81);
#undef TARGET_NUMBER
#undef TARGET_RATIO
    return 0;
}

int npe_render_native_item_target_screen(const NpeFile *file,const NpeFont *font,
                                         const NpeWorld *world,unsigned member,
                                         unsigned item_id,unsigned quantity,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    return render_item_target(file,font,world,member,item_id,quantity,pixels,1);
}

int npe_render_map_item_target_screen(const NpeFile *file, const NpeFont *font,
                                      const NpeWorld *world, unsigned member,
                                      unsigned item_id, unsigned quantity,
                                      uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    return npe_render_map_item_target_view(file,font,world,member,item_id,quantity,1,pixels);
}

/*2F:5434..58CE: shared use/equip backdrop is drawn only on entry. */
static int render_item_target_backdrop(const NpeFile *file,const NpeFont *font,
                                      unsigned item_id,const NpeItem *item,
                                      const uint8_t left[6][4],const uint8_t right[][4],
                                      unsigned right_count,unsigned right_y,uint8_t *pixels) {
    uint16_t icon=npe_image_group_index(file,5,(uint16_t)item_id);
    memset(pixels,0,NPE_WIDTH*NPE_HEIGHT);
    for(unsigned x=0;x<NPE_WIDTH-1;++x)pixels[x]=pixels[118*NPE_WIDTH+x]=15;
    for(unsigned y=0;y<NPE_HEIGHT-1;++y)pixels[y*NPE_WIDTH]=pixels[y*NPE_WIDTH+238]=15;
    for(unsigned x=8;x<=54;++x)pixels[62*NPE_WIDTH+x]=15;
    for(unsigned y=8;y<=104;++y)pixels[y*NPE_WIDTH+54]=15;
    if(npe_render_indexed_resource(file,icon,11,63,0,pixels))return -1;
    if(shop_counter(font,item->name,item->name_size,12,11,105,pixels))return -1;
    uint8_t label[5];label[4]=':';
    for(unsigned row=0;row<6;++row){memcpy(label,left[row],4);
        if(shop_counter(font,label,5,5,55,11+14*row,pixels))return -1;
    }
    for(unsigned row=0;row<right_count;++row){memcpy(label,right[row],4);
        if(shop_counter(font,label,5,5,158,right_y+14*row,pixels))return -1;
    }
    return 0;
}

int npe_render_map_item_target_view(const NpeFile *file,const NpeFont *font,
                                    const NpeWorld *world,unsigned member,
                                    unsigned item_id,unsigned quantity,int full,uint8_t *pixels) {
    static const uint8_t left[6][4] = {
        {0xbc,0xb6,0xb1,0xf0}, {0xbe,0xad,0xd1,0xe9},
        {0xc9,0xfa,0xc3,0xfc}, {0xc4,0xda,0xc1,0xa6},
        {0xb9,0xa5,0xbb,0xf7}, {0xb7,0xc0,0xd3,0xf9}
    };
    static const uint8_t right[5][4] = {
        {0xd7,0xb4,0xcc,0xac}, {0xc3,0xe2,0xd2,0xdf},
        {0xc1,0xe9,0xc1,0xa6}, {0xd0,0xd2,0xd4,0xcb},
        {0xc9,0xed,0xb7,0xa8}
    };
    static const uint8_t names[4][2] = {{0xb6,0xbe},{0xc3,0xdf},{0xc2,0xd2},{0xb7,0xe2}};
    static const uint8_t normal[] = {0xd5,0xfd,0xb3,0xa3};
    uint8_t negative[8], positive[8]; unsigned ns=0, ps=0;
    char number[32]; NpeItem item;uint16_t portrait=npe_world_portrait_index(file,world,member);
    if(!file || !font || !font->data || !world || !pixels || member>=3 ||
       !world->party_active[member] || !item_id)return -1;
    if(full){
        if(target_item_name(file,world,item_id,&item) ||
           render_item_target_backdrop(file,font,item_id,&item,left,right,5,25,pixels))return -1;
    }
    /*2F:5928..5A1E draws quantity and portrait after all static fields. */
    snprintf(number,sizeof(number),"%u",quantity);
    if(shop_counter(font,(const uint8_t *)number,strlen(number),2,38,93,pixels))return -1;
    if(npe_render_indexed_resource(file,portrait,1,1,0,pixels))return -1;
    /*2F:5ABA..5F40 clears only the fixed value fields; experience
      uses eleven cells (5BC4), the other left fields use nine. */
    for(unsigned row=0;row<6;++row){
        if(row==0)snprintf(number,sizeof(number),"%u",world->level[member]);
        else if(row==1)snprintf(number,sizeof(number),"%u/%u",world->experience[member],world->experience_to_next[member]);
        else if(row==2)snprintf(number,sizeof(number),"%d/%d",(int16_t)world->hp[member],(int16_t)world->max_hp[member]);
        else if(row==3)snprintf(number,sizeof(number),"%d/%d",(int16_t)world->qi[member],(int16_t)world->max_qi[member]);
        else snprintf(number,sizeof(number),"%d",(int16_t)(row==4?world->attack[member]:world->defense[member]));
        if(shop_counter(font,(const uint8_t *)number,strlen(number),row==1?11:9,85,11+14*row,pixels))return -1;
    }
    for(unsigned s=0;s<4;++s){
        int16_t value=(int16_t)world->status_guard[member][s];
        if(value<0){memcpy(negative+ns,names[s],2);ns+=2;}
        if(value>0){memcpy(positive+ps,names[s],2);ps+=2;}
    }
    if(shop_counter(font,ns?negative:normal,ns?ns:sizeof(normal),8,188,25,pixels) ||
       shop_counter(font,positive,ps,8,188,39,pixels))return -1;
    const unsigned order[3]={0,2,1};
    for(unsigned row=0;row<3;++row){
        snprintf(number,sizeof(number),"%d",(int16_t)world->other_stats[member][order[row]]);
        if(shop_counter(font,(const uint8_t *)number,strlen(number),8,188,53+14*row,pixels))return -1;
    }
    return 0;
}

static int render_equip_target(const NpeFile *file, const NpeFont *font,
                                          const NpeWorld *world, unsigned member,
                                          unsigned item_id, unsigned quantity,
                                          uint8_t pixels[NPE_WIDTH * NPE_HEIGHT],NpeWorld *cache_world,int full) {
    static const uint8_t wear_labels[6][4] = {
        {0xcd,0xb7,0xb4,0xf7}, {0xc9,0xed,0xb4,0xa9},
        {0xce,0xe4,0xc6,0xf7}, {0xd1,0xa5,0xd7,0xd3},
        {0xc5,0xfb,0xb9,0xd2}, {0xc5,0xe5,0xb4,0xf7}
    };
    /*2F:623A..647E: head, body, weapon, shoes, pendant, accessory. */
    static const unsigned slots[6] = {0,2,3,4,1,5};
    static const uint8_t labels[7][4] = {
        {0xd7,0xb4,0xcc,0xac}, {0xc3,0xe2,0xd2,0xdf},
        {0xc1,0xe9,0xc1,0xa6}, {0xd0,0xd2,0xd4,0xcb},
        {0xc9,0xed,0xb7,0xa8}, {0xb9,0xa5,0xbb,0xf7},
        {0xb7,0xc0,0xd3,0xf9}
    };
    static const uint8_t state_names[4][2] = {
        {0xb6,0xbe}, {0xc3,0xdf}, {0xc2,0xd2}, {0xb7,0xe2}
    };
    static const uint8_t normal[] = {0xd5,0xfd,0xb3,0xa3};
    NpeItem selected;uint16_t portrait=npe_world_portrait_index(file,world,member);
    uint8_t negative[8], positive[8]; unsigned negative_size=0, positive_size=0;
    if(!file || !font || !font->data || !world || !pixels || member>=3 ||
       !world->party_active[member] || !item_id)return -1;
    if(full){
        if(cache_world ? npe_world_item(file,cache_world,item_id,&selected) :
                         npe_native_item(file,item_id,&selected))return -1;
        if(render_item_target_backdrop(file,font,item_id,&selected,wear_labels,labels,7,11,pixels))return -1;
    }
    /*2F:5928..5A1E: quantity precedes the opaque portrait. Its two
      compact cells truncate 100 to 10; a wide portrait may cover them. */
    char quantity_text[8];snprintf(quantity_text,sizeof(quantity_text),"%u",quantity);
    if(shop_counter(font,(const uint8_t *)quantity_text,strlen(quantity_text),2,38,93,pixels))return -1;
    if(npe_render_indexed_resource(file,portrait,1,1,0,pixels))return -1;
    for (unsigned row=0;row<6;++row) {
        NpeItem equipped; unsigned id=world->equipped_items[member][slots[row]];
        if(cache_world){
            /*2F:61C6..64BC loads every row, including empty ID0. */
            if(npe_world_item(file,cache_world,id,&equipped))return -1;
        }else if(id && npe_native_item(file,id,&equipped))return -1;
        if(shop_counter(font,id?equipped.name:NULL,id?equipped.name_size:0,12,85,11+14*row,pixels))return -1;
    }
    for (unsigned s=0;s<4;++s) {
        int16_t value=(int16_t)world->status_guard[member][s];
        if (value<0) {memcpy(negative+negative_size,state_names[s],2);negative_size+=2;}
        if (value>0) {memcpy(positive+positive_size,state_names[s],2);positive_size+=2;}
    }
    if(shop_counter(font,negative_size?negative:normal,negative_size?negative_size:sizeof(normal),8,188,11,pixels) ||
       shop_counter(font,positive,positive_size,8,188,25,pixels))return -1;
    unsigned values[5]={world->other_stats[member][0],world->other_stats[member][2],
                        world->other_stats[member][1],world->attack[member],world->defense[member]};
    for (unsigned row=0;row<5;++row) {
        /*2F:65BC..6888 uses signed %d and eight opaque compact cells. */
        char number[16];snprintf(number,sizeof(number),"%d",(int16_t)values[row]);
        if(shop_counter(font,(const uint8_t *)number,strlen(number),8,188,39+14*row,pixels))return -1;
    }
    return 0;
}

int npe_render_native_equip_target_screen(const NpeFile *file,const NpeFont *font,
                                          const NpeWorld *world,unsigned member,
                                          unsigned item_id,unsigned quantity,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    return render_equip_target(file,font,world,member,item_id,quantity,pixels,NULL,1);
}
int npe_render_equip_target_screen(const NpeFile *file,const NpeFont *font,
                                    NpeWorld *world,unsigned member,
                                    unsigned item_id,unsigned quantity,int full,uint8_t *pixels) {
    return render_equip_target(file,font,world,member,item_id,quantity,pixels,world,full);
}

static void draw_inventory_frame(uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    unsigned x, y;
    memset(pixels, 0, NPE_WIDTH * NPE_HEIGHT);
    for (x = 0; x < NPE_WIDTH - 1; ++x)
        pixels[x] = pixels[(NPE_HEIGHT - 2) * NPE_WIDTH + x] = 15;
    for (y = 0; y < NPE_HEIGHT - 1; ++y)
        pixels[y * NPE_WIDTH] =
            pixels[y * NPE_WIDTH + NPE_WIDTH - 2] = 15;
}

int npe_render_empty_inventory_screen(const NpeFont *font,
                                      uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    static const uint8_t empty[] = {
        0xd4,0xdd,0xce,0xde,0xb5,0xc0,0xbe,0xdf,'!'
    };
    if (!font || !font->data || !pixels) return -1;
    draw_inventory_frame(pixels);
    return npe_draw_gbk_text(font, empty, sizeof(empty), 90, 44, pixels);
}

int npe_render_empty_magic_screen(const NpeFont *font,
                                  uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    static const uint8_t empty[] = {
        0xd4,0xdd,0xce,0xde,0xd0,0xde,0xc1,0xb6,
        0xb3,0xc9,0xcf,0xc9,0xca,0xf5,'!'
    };
    if (!font || !font->data || !pixels) return -1;
    draw_inventory_frame(pixels);
    return npe_draw_gbk_text(font, empty, sizeof(empty), 78, 44, pixels);
}

static int render_magic_list(const NpeFile *file, const NpeFont *font,
                                        const NpeWorld *world, unsigned member,
                                        const unsigned ids[18], unsigned count,
                                        unsigned first, unsigned selection,
                                        uint8_t pixels[NPE_WIDTH * NPE_HEIGHT],NpeWorld *cache_world,
                                        int draw_names) {
    uint16_t portrait=npe_world_portrait_index(file,world,member);
    NpeSkill skill;
    char qi[16];
    unsigned x, y, line = 0, cursor = 53;
    size_t pos;
    unsigned skill_id, column, row;
    if (!ids || !count || count > 18 || first % 3 || selection < first ||
        selection >= count || selection >= first + 9) return -1;
    skill_id = ids[selection];
    column = (selection - first) % 3;
    row = (selection - first) / 3;
    if (!file || !font || !font->data || !world || !pixels ||
        member >= 3 || !world->party_active[member] || !skill_id ||
        skill_id > file->offset_table_records[3]) return -1;
    /*3B:60E8..6136 uses the caster's resident eighth image. */
    if (cache_world?npe_world_skill(file,cache_world,skill_id,&skill):npe_skill(file, skill_id - 1, &skill)) return -1;
    /*3B:5DDA skips the frame, names and portrait on a retained update.
      The selection caller clears the description before that update. */
    if(draw_names){
        draw_inventory_frame(pixels);
        for (x = 9; x <= 230; ++x) pixels[51 * NPE_WIDTH + x] = 15;
        for (y = 51; y <= 110; ++y) pixels[y * NPE_WIDTH + 52] = 15;
        if(npe_render_indexed_resource(file,portrait,1,52,0,pixels))return -1;
    }else{
        for(y=60;y<=110;++y)memset(pixels+y*NPE_WIDTH+53,0,178);
    }
    /* Source3B:5F6C..60BA: nine entries, three columns 73 pixels
       apart and three rows 16 pixels apart. Description/fee follow the
       selected original slot, including duplicate IDs. */
    for (unsigned index = first; draw_names && index < count && index < first + 9; ++index) {
        NpeSkill entry;
        unsigned local = index - first;
        if (!ids[index] || npe_skill(file, ids[index] - 1, &entry) ||
            npe_draw_gbk_text_color(font, entry.name, entry.name_size,
                                    10 + 73 * (local % 3),
                                    1 + 16 * (local / 3), 15, pixels)) return -1;
    }
    if(npe_render_indexed_resource(file,npe_image_group_index(file,7,17),
        (int16_t)(25+column*73),(int16_t)(13+row*16),0,pixels))return -1;
    if(cache_world){
        if(npe_render_map_magic_description(file,font,cache_world,skill_id,0,pixels)!=1)return -1;
    }else{
    for (pos = 0; pos < skill.description_size;) {
        size_t advance;
        unsigned width;
        if (skill.description[pos] == '\r' || skill.description[pos] == '\n') {
            if (skill.description[pos] == '\r' &&
                pos + 1 < skill.description_size &&
                skill.description[pos + 1] == '\n') ++pos;
            ++line;
            cursor = 53;
            ++pos;
            continue;
        }
        if (!glyph(font, skill.description + pos,
                   skill.description_size - pos, &advance)) return -1;
        width = advance == 1 ? 6 : 12;
        /* The original description allows a trailing ASCII cell after
           fourteen Chinese cells (e.g. the '+' in original spell42). */
        if (cursor + width > 230) { ++line; cursor = 53; }
        if (line >= 4 || npe_draw_gbk_text_color(font,
            skill.description + pos, advance, cursor,
            60 + line * 12, 15, pixels)) return -1;
        cursor += width;
        pos += advance;
    }
    }
    if (snprintf(qi, sizeof(qi), "%d/%d", (int16_t)skill.qi_cost,
                 (int16_t)world->qi[member]) <= 0) return -1;
    /*3B:6326..6340 uses seven compact cells at (9,106), including
      the two blank bottom rows of the original twelve-pixel field. */
    return shop_counter(font,(const uint8_t *)qi,strlen(qi),7,9,106,pixels);
}

int npe_render_native_magic_list_screen(const NpeFile *file,const NpeFont *font,
                                         const NpeWorld *world,unsigned member,const unsigned ids[18],
                                         unsigned count,unsigned first,unsigned selection,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    return render_magic_list(file,font,world,member,ids,count,first,selection,pixels,NULL,1);
}

int npe_render_map_magic_list(const NpeFile *file,const NpeFont *font,NpeWorld *world,
                               unsigned member,const unsigned ids[18],unsigned count,unsigned first,
                               unsigned selection,unsigned previous_first,unsigned previous_selection,
                               int full,uint8_t *pixels){
    uint8_t top[51*NPE_WIDTH];
    if(!world || !pixels || (!full && (previous_selection<previous_first ||
       previous_selection>=previous_first+9)))return -1;
    if(!full)memcpy(top,pixels,sizeof(top));
    if(render_magic_list(file,font,world,member,ids,count,first,selection,pixels,world,
                         full || first!=previous_first))return -1;
    if(!full && first==previous_first){
        unsigned old=previous_selection-first,local=selection-first;
        memcpy(pixels,top,sizeof(top));
        for(unsigned y=13+old/3*16;y<=17+old/3*16;++y)
            for(unsigned x=(23+old%3*73)&~3u;x<=38+old%3*73;++x)pixels[y*NPE_WIDTH+x]=0;
        for(unsigned y=0;y<4;++y){
            for(unsigned x=0;x<=y*2;++x)pixels[(13+local/3*16+y)*NPE_WIDTH+local%3*73+28-y+x]=15;
            pixels[(13+local/3*16+y)*NPE_WIDTH+local%3*73+29+y]=6;
        }
    }
    return 0;
}

int npe_render_native_magic_screen(const NpeFile *file, const NpeFont *font,
                                   const NpeWorld *world, unsigned member,
                                   unsigned skill_id,
                                   uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    unsigned ids[18] = {skill_id};
    return npe_render_native_magic_list_screen(file, font, world, member,
                                               ids, 1, 0, 0, pixels);
}

int npe_update_native_magic_list_screen(const NpeFile *file, const NpeFont *font,
                                        const NpeWorld *world, unsigned member,
                                        const unsigned ids[18], unsigned count,
                                        unsigned first, unsigned selection,
                                        unsigned previous_first, unsigned previous_selection,
                                        uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    uint8_t top[51 * NPE_WIDTH];
    if (!pixels || previous_selection < previous_first ||
        previous_selection >= previous_first + 9) return -1;
    memcpy(top, pixels, sizeof(top));
    if (npe_render_native_magic_list_screen(file, font, world, member,
                                            ids, count, first, selection, pixels)) return -1;
    if (first == previous_first) {
        /* Source3B:5DDA ->613E skips name redraw when moving within
           the same window. Erasing the old cursor also clears the first
           scanline of the following name row; preserve that native result. */
        unsigned old = previous_selection - first, local = selection - first;
        memcpy(pixels, top, sizeof(top));
        if (old == local) return 0;
        for (unsigned y = 13 + old / 3 * 16; y <= 17 + old / 3 * 16; ++y)
            for (unsigned x = (23 + old % 3 * 73) & ~3u;
                 x <= 38 + old % 3 * 73; ++x)
                pixels[y * NPE_WIDTH + x] = 0;
        for (unsigned y = 0; y < 4; ++y) {
            for (unsigned x = 0; x <= y * 2; ++x)
                pixels[(13 + local / 3 * 16 + y) * NPE_WIDTH + local % 3 * 73 + 28 - y + x] = 15;
            pixels[(13 + local / 3 * 16 + y) * NPE_WIDTH + local % 3 * 73 + 29 + y] = 6;
        }
    }
    return 0;
}

static void timed_notice_box(size_t size,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    /* Source3E:FE26..FE62 measures eight pixels per GBK byte and uses
       X=120-width/2-5 ..120+width/2+5. Text itself uses16-pixel glyphs. */
    unsigned left=120-(unsigned)size*4-5,right=120+(unsigned)size*4+5;
    for (unsigned y = 48; y <= 72; ++y)
        for (unsigned x = left; x <= right; ++x) pixels[y * NPE_WIDTH + x] = 0;
    for (unsigned y = 49; y <= 71; ++y)
        for (unsigned x = left+1; x < right; ++x)
            pixels[y * NPE_WIDTH + x] = x == left+1 || x == right-1 || y == 49 || y == 71 ? 15 : 5;
}

int npe_render_timed_notice(const NpeFont *font,const uint8_t *message,
                            size_t size,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    if (!font || !font->data16 || !pixels || (!message && size) || size>28) return -1;
    timed_notice_box(size,pixels);
    return npe_draw_gbk_text16_color(font,message,size,120-(unsigned)size*4,52,15,pixels);
}

int npe_render_money_notice(const NpeFont *font,uint16_t amount,int adding,
                             uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    /* Original4C:A9A4/A9AA/A9B0 strings andAA02 signed%d format. */
    static const uint8_t gain[]={0xb5,0xc3,0xb5,0xbd},loss[]={0xca,0xa7,0xc8,0xa5};
    static const uint8_t money[]={0xbf,0xe9,0xc7,0xae};
    uint8_t message[24];char number[16];
    int length=snprintf(number,sizeof(number),"%d",(int)(int16_t)amount);
    if(length<0||(size_t)length>=sizeof(number))return -1;
    memcpy(message,adding?gain:loss,4);memcpy(message+4,number,(size_t)length);
    memcpy(message+4+length,money,4);
    return npe_render_timed_notice(font,message,(size_t)length+8,pixels);
}

int npe_render_item_notice(const NpeFile *file,const NpeFont *font,unsigned item_id,int adding,
                            uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    /* Original4C:A998/A99E/A9B6: quoted current-NPE name, no quantity. */
    static const uint8_t gain[]={0xb5,0xc3,0xb5,0xbd,'"'},loss[]={0xca,0xa7,0xc8,0xa5,'"'};
    NpeItem item;uint8_t message[24];size_t length=0;item_id&=255u;
    if(!file)return -1;
    if(item_id){if(npe_native_item(file,item_id,&item)||item.name_size>12)return -1;length=item.name_size;}
    memcpy(message,adding?gain:loss,5);
    if(length)memcpy(message+5,item.name,length);
    message[5+length]='"';
    return npe_render_timed_notice(font,message,length+6,pixels);
}

int npe_render_object_notice(const NpeFile *file,const NpeFont *font,unsigned item_id,
                              uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    /*37:15CC copies runtime33:6E0A "得到 ", then appends739E's
      current-NPE name. The description starts separately at73AB. */
    static const uint8_t prefix[]={0xb5,0xc3,0xb5,0xbd,' '};
    uint8_t message[17];NpeItem item;size_t size=0;item_id&=255u;
    if(!file)return -1;
    if(item_id){
        if(npe_native_item(file,item_id,&item)||item.name_size>12)return -1;
        size=item.name_size;
    }
    memcpy(message,prefix,sizeof(prefix));
    if(size)memcpy(message+sizeof(prefix),item.name,size);
    return npe_render_timed_notice(font,message,sizeof(prefix)+size,pixels);
}

int npe_render_cached_item_notice(const NpeFont *font,const NpeItem *item,int adding,
                                  int object,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    static const uint8_t gain[]={0xb5,0xc3,0xb5,0xbd},loss[]={0xca,0xa7,0xc8,0xa5};
    uint8_t message[18];size_t length;
    if(!item||item->name_size>12)return -1;
    memcpy(message,adding?gain:loss,4);message[4]=object?' ':'"';
    memcpy(message+5,item->name,item->name_size);length=5+item->name_size;
    if(!object)message[length++]='"';
    return npe_render_timed_notice(font,message,length,pixels);
}

int npe_render_skill_notice(const NpeFile *file,const NpeFont *font,unsigned skill_id,
                             uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    /*40:A24E copies4C:A9B8 over the earlier character-name copy;
      40:A2B2/A2D0 then append the NPE skill name and closing quote. */
    static const uint8_t prefix[]={0xd0,0xde,0xc1,0xb7,0xb3,0xc9,'"'};
    NpeSkill skill;uint8_t message[20];size_t length=sizeof(prefix);
    if(!file)return -1;
    memcpy(message,prefix,sizeof(prefix));
    skill_id&=255u;
    if(skill_id){if(npe_skill(file,skill_id-1,&skill)||skill.name_size>12)return -1;
        memcpy(message+length,skill.name,skill.name_size);length+=skill.name_size;}
    message[length++]='"';return npe_render_timed_notice(font,message,length,pixels);
}

int npe_render_world_skill_notice(const NpeFile *file,const NpeFont *font,NpeWorld *world,
                                  unsigned id,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    static const uint8_t prefix[]={0xd0,0xde,0xc1,0xb7,0xb3,0xc9,'"'};
    NpeSkill skill;uint8_t message[20];size_t length=sizeof(prefix);
    if(npe_world_skill(file,world,id,&skill))return -1;
    memcpy(message,prefix,length);memcpy(message+length,skill.name,skill.name_size);
    length+=skill.name_size;message[length++]='"';
    return npe_render_timed_notice(font,message,length,pixels);
}

int npe_render_party_notice(const NpeFile *file, const NpeFont *font,
                            const NpeWorld *world, unsigned member, int adding,
                            uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    /* Firmware4C:A9C0/A9CA reads the current map record's name. */
    static const uint8_t leave[] = {0xc0,0xeb,0xbf,0xaa,0xb6,0xd3,0xce,0xe9};
    static const uint8_t join[] = {0xbc,0xd3,0xc8,0xeb,0xb6,0xd3,0xce,0xe9};
    uint8_t message[20];
    size_t length = 0;
    (void)file;
    if (!world || member >= 3 || !world->party_ids[member]) return -1;
    while (length < 12 && world->party_names[member][length]) ++length;
    memcpy(message, world->party_names[member], length);
    memcpy(message + length, adding ? join : leave, sizeof(join));
    return npe_render_timed_notice(font, message, length + sizeof(join), pixels);
}

int npe_render_map_name_notice(const NpeFile *file,const NpeFont *font,
                               const NpeWorld *world,uint8_t *pixels) {
    NpeMap map;size_t size=0;
    if(!world || npe_map(file,world->map_number-1,&map))return -1;
    if(npe_rpg_runtime_owner(file)==world && npe_rpg_runtime_map_current(file,world->map_number)){
        /*21:FE26 measures byte strlen independently of28:1F08's glyph loop.
           A consumed NUL trail does not terminate the latter. Read the live
           resident name and following state rather than a bounded NPE copy. */
        uint8_t lead,trail=0;
        while(size<=28){
            if(npe_rpg_runtime_read(file,UINT32_C(0x5278e4)+(uint32_t)size,&lead,1))return -1;
            if(!lead)break;
            ++size;
        }
        if(size>28 || !font || !font->data16 || !pixels)return -1;
        timed_notice_box(size,pixels);
        unsigned cursor=120-(unsigned)size*4;
        uint32_t address=UINT32_C(0x5278e4);
        /* Original28:1F12 stops at x>=239 before reading the next lead.
           A glyph starting before that boundary may extend beyond the LCD. */
        while(cursor<NPE_WIDTH-1){
            if(npe_rpg_runtime_read(file,address,&lead,1))return -1;
            if(!lead)break;
            if(lead>=0x80 && npe_rpg_runtime_read(file,address+1,&trail,1))return -1;
            unsigned width;const uint8_t *bits=glyph16(font,lead,trail,&width);
            for(unsigned y=0;y<16;++y){
                unsigned mask=(unsigned)bits[y*2]<<8|bits[y*2+1];
                for(unsigned x=0;x<width&&cursor+x<NPE_WIDTH;++x)
                    if(mask&(0x8000u>>x))pixels[(52+y)*NPE_WIDTH+cursor+x]=15;
            }
            cursor+=width;address+=width/8;
        }
        return 0;
    }
    while(size<sizeof(map.name) && map.name[size])++size;
    return npe_render_timed_notice(font,map.name,size,pixels);
}

static int battle_message_box(const NpeFont *font,const uint8_t *message,
                              size_t size,uint8_t *pixels) {
    return npe_render_timed_notice(font,message,size,pixels);
}

int npe_render_battle_insufficient_qi(const NpeFont *font,
                                      uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    static const uint8_t message[] = {0xc4,0xda,0xc1,0xa6,0xb2,0xbb,0xd7,0xe3,'!'};
    return battle_message_box(font, message, sizeof(message), pixels);
}

int npe_render_battle_item_rejection(const NpeFont *font, int throwing,
                                      uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    /* Resident firmware strings 4E:DBDC / DBE6, independent of game data. */
    static const uint8_t use[] = {0xb2,0xbb,0xc4,0xdc,0xca,0xb9,0xd3,0xc3,'!'};
    static const uint8_t toss[] = {0xb2,0xbb,0xc4,0xdc,0xcd,0xb6,0xd6,0xc0,'!'};
    return battle_message_box(font, throwing ? toss : use, sizeof(use), pixels);
}

int npe_render_save_failure(const NpeFont *font, int saving,
                            uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    /* Resident 4E:45E0 /45EE: six Chinese characters followed by ASCII '!'. */
    static const uint8_t save[] = {0xb1,0xa3,0xb4,0xe6,0xbd,0xf8,0xb6,0xc8,0xca,0xa7,0xb0,0xdc,'!'};
    static const uint8_t load[] = {0xb6,0xc1,0xc8,0xa1,0xbd,0xf8,0xb6,0xc8,0xca,0xa7,0xb0,0xdc,'!'};
    return npe_render_timed_notice(font,saving ? save : load,sizeof(save),pixels);
}

int npe_render_shop_no_money(const NpeFont *font,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    /* Resident4D:5862: 余额不足! */
    static const uint8_t message[]={0xd3,0xe0,0xb6,0xee,0xb2,0xbb,0xd7,0xe3,'!'};
    return npe_render_timed_notice(font,message,sizeof(message),pixels);
}

int npe_render_map_magic_message(const NpeFont *font, int unavailable,
                                  uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    static const uint8_t insufficient[] = {0xc4,0xa7,0xb7,0xa8,0xd6,0xb5,0xb2,0xbb,0xd7,0xe3,'!'};
    static const uint8_t rejected[] = {0xb8,0xc3,0xc4,0xa7,0xb7,0xa8,0xd6,0xbb,0xc4,0xdc,0xb6,0xd4,0xb5,0xd0,0xc8,0xcb,0xca,0xb9,0xd3,0xc3,'!'};
    if (!font || !font->data16 || !pixels) return -1;
    unsigned width = unavailable ? 177 : 97, left = (241 - width) / 2;
    for (unsigned y = 48; y <= 72; ++y)
        for (unsigned x = left - 1; x <= left + width; ++x) pixels[y * NPE_WIDTH + x] = 0;
    for (unsigned y = 49; y <= 71; ++y)
        for (unsigned x = left; x < left + width; ++x)
            pixels[y * NPE_WIDTH + x] = x == left || x == left + width - 1 || y == 49 || y == 71 ? 15 : 5;
    return npe_draw_gbk_text16_color(font, unavailable ? rejected : insufficient,
        unavailable ? sizeof(rejected) : sizeof(insufficient), left + 4, 52, 15, pixels);
}

int npe_render_magic_member_overlay(const NpeFile *file, const NpeFont *font,
                                     const NpeWorld *world, unsigned selection,
                                     uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    unsigned slots[3], count = 0;
    if (!file || !font || !world || !pixels || world->game_mode != 6) return -1;
    for (unsigned i = 0; i < 3; ++i)
        if (world->party_active[i]) slots[count++] = i;
    if (!count || selection >= count) return -1;
    uint16_t ordinal = count == 1 ? 13 : count == 2 ? 14 : 16;
    uint16_t menu=npe_image_group_index(file,7,ordinal);
    if(npe_render_indexed_resource(file,menu,42,37,0,pixels))return -1;
    for (unsigned i = 0; i < count; ++i) {
        const uint8_t *name=world->party_names[slots[i]];
        const uint8_t *end = memchr(name, 0, 12);
        if (!end || npe_draw_gbk_text(font, name, (size_t)(end - name),
                                      52, 49 + i * 15, pixels)) return -1;
    }
    /* Native one/two/three-name captures have unequal top/bottom padding
       around the 15-pixel name rows. Preserve these cursor rectangles. */
    unsigned bottom = count == 1 ? 64 : (selection + 1 == count ? 65 : 63) + selection * 15;
    for (unsigned y = selection ? 48 + selection * 15 : 45; y <= bottom; ++y)
        for (unsigned x = 50; x <= (count == 1 ? 104u : 105u); ++x)
            pixels[y * NPE_WIDTH + x] = 15 - pixels[y * NPE_WIDTH + x];
    return 0;
}

static int map_magic_value(const NpeFont *font,const uint8_t *value,size_t length,
                           unsigned cells,unsigned x,unsigned y,uint8_t *pixels) {
    uint8_t field[8];
    if(!pixels || cells>sizeof(field) || x+cells*6>NPE_WIDTH || y+12>NPE_HEIGHT)return -1;
    /*33:F302/F394/F9C0: seven left cells, eight right cells. The
      runtime31:465E format is %d; only these value fields are cleared. */
    for(unsigned row=y;row<y+12;++row)memset(pixels+row*NPE_WIDTH+x,0,cells*6);
    memset(field,' ',cells);
    if(length>cells)length=cells;
    if(length)memcpy(field,value,length);
    return draw_native12_compact(font,field,cells,x,y,pixels);
}

static int render_magic_target(const NpeFile *file,
                                          const NpeFont *font,
                                          const NpeWorld *world,
                                          unsigned caster, unsigned target,
                                          unsigned skill_id,
                                          int insufficient,
                                          uint8_t pixels[NPE_WIDTH * NPE_HEIGHT],const NpeSkill *cached,int full) {
    static const uint8_t left_labels[6][4] = {
        {0xb5,0xc8,0xbc,0xb6}, {0xbe,0xad,0xd1,0xe9},
        {0xc9,0xfa,0xc3,0xfc}, {0xc4,0xda,0xc1,0xa6},
        {0xb9,0xa5,0xbb,0xf7}, {0xb7,0xc0,0xd3,0xf9}
    };
    static const uint8_t right_labels[5][4] = {
        {0xd7,0xb4,0xcc,0xac}, {0xc3,0xe2,0xd2,0xdf},
        {0xc1,0xe9,0xc1,0xa6}, {0xd0,0xd2,0xd4,0xcb},
        {0xc9,0xed,0xb7,0xa8}
    };
    static const uint8_t normal[] = {0xd5,0xfd,0xb3,0xa3};
    static const uint8_t sleep[] = {0xc3,0xdf};
    static const uint8_t insufficient_text[] = {
        0xc4,0xa7,0xb7,0xa8,0xd6,0xb5,0xb2,0xbb,0xd7,0xe3,'!'
    };
    uint16_t portrait=npe_world_portrait_index(file,world,target);
    uint16_t icon=npe_image_group_index(file,8,(uint16_t)skill_id);
    NpeSkill skill;
    unsigned x, y;
    char number[32];
    if (!file || !font || !font->data || !world || !pixels ||
        caster >= 3 || target >= 3 || !world->party_active[caster] ||
        !world->party_active[target] || !skill_id ||
        (!cached && npe_skill(file, skill_id - 1, &skill))) return -1;
    if(cached)skill=*cached;
    /*33:F1CA..F21C uses the target's resident eighth image. */
    if(full)draw_inventory_frame(pixels);
    if(full)for (x = 8; x <= 75; ++x) pixels[62 * NPE_WIDTH + x] = 15;
    if(full)for (y = 8; y <= 112; ++y) pixels[y * NPE_WIDTH + 75] = 15;
    if(full && npe_render_indexed_resource(file,icon,22,63,0,pixels))return -1;
    if (full && draw_native12_compact(font, skill.name, skill.name_size,
                              2, 105, pixels)) return -1;
    if(full){
        for(unsigned row=0;row<6;++row)if(draw_magic_field(font,left_labels[row],NULL,0,76,11+14*row,pixels))return -1;
        for(unsigned row=0;row<5;++row)if(draw_magic_field(font,right_labels[row],NULL,0,152,25+14*row,pixels))return -1;
    }
    /*33:EF16..F1BA paints the static page before F1CA..F21C paints
      the portrait. Large resident images can overwrite those labels. */
    if(npe_render_indexed_resource(file,portrait,12,1,0,pixels))return -1;
    snprintf(number, sizeof(number), "%d/%d", (int16_t)skill.qi_cost,
             (int16_t)world->qi[caster]);
    if (map_magic_value(font, (const uint8_t *)number,
                         strlen(number), 7, 15, 93, pixels)) return -1;
#define MAGIC_FIELD(label, value, length, x, y) \
    map_magic_value(font,(value),(length),(x)==76?7:8,(x)+31,(y),pixels)
#define MAGIC_NUMBER(label, value, x, y) do { \
    snprintf(number, sizeof(number), "%d", (int16_t)(value)); \
    if (MAGIC_FIELD((label), (const uint8_t *)number, \
                          strlen(number), (x), (y))) return -1; \
} while (0)
#define MAGIC_RATIO(label, a, b, x, y) do { \
    snprintf(number, sizeof(number), "%d/%d", (int16_t)(a), (int16_t)(b)); \
    if (MAGIC_FIELD((label), (const uint8_t *)number, \
                          strlen(number), (x), (y))) return -1; \
} while (0)
    MAGIC_NUMBER(left_labels[0], world->level[target], 76, 11);
    MAGIC_RATIO(left_labels[1], world->experience[target],
                world->experience_to_next[target], 76, 25);
    MAGIC_RATIO(left_labels[2], world->hp[target],
                world->max_hp[target], 76, 39);
    MAGIC_RATIO(left_labels[3], world->qi[target],
                world->max_qi[target], 76, 53);
    MAGIC_NUMBER(left_labels[4], world->attack[target], 76, 67);
    MAGIC_NUMBER(left_labels[5], world->defense[target], 76, 81);
    if (MAGIC_FIELD(right_labels[0], normal, sizeof(normal), 152, 25) ||
        MAGIC_FIELD(right_labels[1],
                          (world->game_mode == 3 ? world->battle_party_status[target][1] : world->status_guard[target][1]) ? sleep : NULL,
                          (world->game_mode == 3 ? world->battle_party_status[target][1] : world->status_guard[target][1]) ? sizeof(sleep) : 0,
                          152, 39)) return -1;
    MAGIC_NUMBER(right_labels[2], world->other_stats[target][0], 152, 53);
    MAGIC_NUMBER(right_labels[3], world->other_stats[target][2], 152, 67);
    MAGIC_NUMBER(right_labels[4], world->other_stats[target][1], 152, 81);
#undef MAGIC_NUMBER
#undef MAGIC_RATIO
#undef MAGIC_FIELD
    if (insufficient) {
        if (!font->data16) return -1;
        for (y = 48; y <= 72; ++y)
            for (x = 71; x <= 169; ++x)
                pixels[y * NPE_WIDTH + x] = 0;
        for (y = 49; y <= 71; ++y)
            for (x = 72; x <= 168; ++x)
                pixels[y * NPE_WIDTH + x] = x == 72 || x == 168 ||
                                             y == 49 || y == 71 ? 15 : 5;
        if (npe_draw_gbk_text16_color(font, insufficient_text,
                                      sizeof(insufficient_text), 76, 52,
                                      15, pixels)) return -1;
    }
    return 0;
}

int npe_render_native_magic_target_screen(const NpeFile *file,const NpeFont *font,const NpeWorld *world,
                                           unsigned caster,unsigned target,unsigned id,int insufficient,
                                           uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    return render_magic_target(file,font,world,caster,target,id,insufficient,pixels,NULL,1);
}
int npe_render_map_magic_target(const NpeFile *file,const NpeFont *font,NpeWorld *world,
                                  unsigned caster,unsigned target,unsigned id,int full,uint8_t *pixels){
    NpeSkill skill;if(npe_world_skill(file,world,id,&skill))return -1;
    return render_magic_target(file,font,world,caster,target,id,0,pixels,&skill,full);
}

static int render_map_item_description(const NpeFile *file,const NpeFont *font,
                                    NpeWorld *world,unsigned id,int direction,uint8_t *pixels){
    if(!file||!font||!world||!pixels||direction<-1||direction>1)return -1;
    if(description_boundary(world,direction))return 0;
    if(description_runtime(file,world))return -1;
    for(unsigned y=72;y<=110;++y)memset(pixels+y*NPE_WIDTH+45,0,186);
    NpeItem item;if(npe_world_item(file,world,id,&item))return -1;
    uint8_t buffer[91]={0};
    if(description_slice(file,world,UINT32_C(0x5273ab),90,1,direction,buffer,sizeof(buffer))||
       description_compact(font,buffer,sizeof(buffer),45,72,230,110,pixels))return -1;
    return 1;
}
int npe_render_map_item_description(const NpeFile *file,const NpeFont *font,NpeWorld *world,int direction,uint8_t *pixels){
    if(!world)return -1;
    return render_map_item_description(file,font,world,world->shop_selected_item,direction,pixels);
}
int npe_render_map_magic_description(const NpeFile *file,const NpeFont *font,NpeWorld *world,
                                     unsigned id,int direction,uint8_t *pixels){
    if(!file||!font||!world||!pixels||direction<-1||direction>1)return -1;
    if(description_boundary(world,direction))return 0;
    if(description_runtime(file,world))return -1;
    /*3B:6366..6426 formats the initial substring over the portrait.
      Paging47:589C/5B14 clears from59; its text still starts at60. */
    if(direction)for(unsigned y=59;y<=110;++y)
        memset(pixels+y*NPE_WIDTH+53,0,178);
    NpeSkill skill;if(npe_world_skill(file,world,id,&skill))return -1;
    uint8_t buffer[113]={0};
    if(description_slice(file,world,UINT32_C(0x52731b),112,2,direction,buffer,sizeof(buffer))||
       description_compact(font,buffer,sizeof(buffer),53,60,230,110,pixels))return -1;
    return 1;
}

static int render_item_list(const NpeFile *file, const NpeFont *font,
                                const unsigned *item_ids,
                                const unsigned *quantities, unsigned count,
                                unsigned selection,
                                uint8_t pixels[NPE_WIDTH * NPE_HEIGHT], int empty_return, int preserve_detail,
                                NpeWorld *world,int full,unsigned previous) {
    NpeItem item;
    uint16_t icon;
    char number[3];
    unsigned x, y, i, entry;
    unsigned page_start, page_end, column, row;
    size_t pos, line = 0, digits;
    unsigned description_x = 45;
    if (!file || !font || !font->data || !pixels || count > NPE_NATIVE_ITEM_MAX ||
        (!empty_return && (!item_ids || !quantities || !count || selection >= count || !item_ids[selection])) ||
        (!world && npe_native_item(file,empty_return?255:item_ids[selection],&item)))
        return -1;
    icon=npe_image_group_index(file,5,empty_return?255:item_ids[selection]);
    uint8_t detail[(NPE_HEIGHT-72)*(NPE_WIDTH-10)];
    if(preserve_detail)for(y=72;y<NPE_HEIGHT;++y)
        memcpy(detail+(y-72)*(NPE_WIDTH-10),pixels+y*NPE_WIDTH+10,NPE_WIDTH-10);
    if(!world || full)draw_inventory_frame(pixels);
    if(preserve_detail)for(y=72;y<NPE_HEIGHT;++y)
        memcpy(pixels+y*NPE_WIDTH+10,detail+(y-72)*(NPE_WIDTH-10),NPE_WIDTH-10);
    if(!world || full)for (x = 9; x <= 230; ++x)
        pixels[71 * NPE_WIDTH + x] = 15;
    if((!world || full) && !preserve_detail)for (y = 71; y <= 110; ++y)
        pixels[y * NPE_WIDTH + 43] = 15;
    for(y=72;y<=104;++y)for(x=10;x<=42;++x)pixels[y*NPE_WIDTH+x]=0;
    page_start = (selection / NPE_NATIVE_ITEM_PAGE_SIZE) *
                 NPE_NATIVE_ITEM_PAGE_SIZE;
    page_end = page_start + NPE_NATIVE_ITEM_PAGE_SIZE;
    if (page_end > count) page_end = count;
    if(world && !full){
        unsigned old_column=previous%NPE_NATIVE_ITEM_COLUMNS;
        unsigned old_row=(previous%NPE_NATIVE_ITEM_PAGE_SIZE)/NPE_NATIVE_ITEM_COLUMNS;
        /*49:F198..F1E4 erases x20..38, y18..22, offset from the
          x25 cursor. It also erases the name pixels inside that rectangle. */
        for(y=18+old_row*16;y<=22+old_row*16;++y)
            memset(pixels+y*NPE_WIDTH+20+old_column*73,0,19);
    }
    if(!world || full)for (entry = page_start; entry < page_end; ++entry) {
        NpeItem listed;
        column = (entry - page_start) % NPE_NATIVE_ITEM_COLUMNS;
        row = (entry - page_start) / NPE_NATIVE_ITEM_COLUMNS;
        if (!item_ids[entry] || !quantities[entry] ||
            (world?npe_world_item(file,world,item_ids[entry],&listed):npe_native_item(file,item_ids[entry],&listed)) ||
            snprintf(number, sizeof(number), "%u", quantities[entry]) <= 0 ||
            draw_native12_compact(font, listed.name, listed.name_size,
                                   10 + column * 73, 6 + row * 16, pixels)) return -1;
        digits = strlen(number);
        for (i = 0; i < digits; ++i) {
            const uint8_t *glyph_bits = npe_native_counter_glyph(number[i]);
            unsigned left = 70 + column * 73 + i * 6;
            if (!glyph_bits) return -1;
            /* The native counter is opaque, including its spacer and
               two bottom rows. Long raw names are erased beneath it. */
            for (y = 0; y < 12; ++y)
                for (x = 0; x < 6; ++x)
                    pixels[(6 + row * 16 + y) * NPE_WIDTH + left + x] =
                        y < 10 && x < 5 && (glyph_bits[y] & (0x10u >> x)) ? 15 : 0;
        }
    }
    column = (selection - page_start) % NPE_NATIVE_ITEM_COLUMNS;
    row = (selection - page_start) / NPE_NATIVE_ITEM_COLUMNS;
    if(npe_render_indexed_resource(file,npe_image_group_index(file,7,17),
        (int16_t)(25+column*73),(int16_t)(18+row*16),0,pixels) ||
        npe_render_indexed_resource(file,icon,10,72,0,pixels))return -1;
    if(world){
        return render_map_item_description(file,font,world,empty_return?255:item_ids[selection],0,pixels)==1?0:-1;
    }
    /* Detail redraw clears the description rectangle after the image.
       Oversized native ordinal images can overlap this rectangle. */
    for (y = 72; y <= 110; ++y)
        for (x = 45; x <= 230; ++x) pixels[y * NPE_WIDTH + x] = 0;
    for (pos = 0; pos < item.description_size;) {
        size_t advance;
        if (item.description[pos] == '\r' ||
            item.description[pos] == '\n') {
            if (item.description[pos] == '\r' &&
                pos + 1 < item.description_size &&
                item.description[pos + 1] == '\n') ++pos;
            ++line;
            description_x = 45;
            ++pos;
            continue;
        }
        if (!glyph(font, item.description + pos,
                   item.description_size - pos, &advance)) return -1;
        if (description_x + (advance == 1 ? 6u : 12u) > 230) {
            ++line;
            description_x = 45;
        }
        /* Original 山海经 capture clips the remaining description after
           three rows; a long NPE description must not close the list. */
        if (line >= 3) break;
        if (npe_draw_gbk_text(font,
            item.description + pos, advance,
            description_x, 72 + (unsigned)line * 12,
            pixels)) return -1;
        description_x += advance == 1 ? 6 : 12;
        pos += advance;
    }
    return 0;
}

int npe_render_item_list_screen(const NpeFile *file, const NpeFont *font,
                                  const unsigned *item_ids, const unsigned *quantities,
                                  unsigned count, unsigned selection,
                                  uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    return render_item_list(file,font,item_ids,quantities,count,selection,pixels,0,0,NULL,1,selection);
}
int npe_update_item_list_screen(const NpeFile *file,const NpeFont *font,
                                  const unsigned *item_ids,const unsigned *quantities,
                                  unsigned count,unsigned selection,
                                  uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    return render_item_list(file,font,item_ids,quantities,count,selection,pixels,0,1,NULL,1,selection);
}
int npe_render_battle_empty_item_return(const NpeFile *file, const NpeFont *font,
                                         uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    /* F4 from target37 calls the detail redraw without re-entering the
       inventory scan. The list is empty but cursor and icon255 remain. */
    return render_item_list(file,font,NULL,NULL,0,0,pixels,1,0,NULL,1,0);
}

int npe_render_map_item_list(const NpeFile *file,const NpeFont *font,NpeWorld *world,
                              const unsigned *ids,const unsigned *quantities,
                              unsigned count,unsigned selection,unsigned previous,
                              int full,uint8_t *pixels) {
    if(!world || !pixels || count>NPE_NATIVE_ITEM_MAX ||
       (count && (!ids || !quantities || selection>=count)))return -1;
    world->shop_selected_item=count?(uint16_t)ids[selection]:0xffff;
    world->shop_row=(uint8_t)(selection%NPE_NATIVE_ITEM_PAGE_SIZE/NPE_NATIVE_ITEM_COLUMNS+1);
    world->inventory_column=(uint8_t)(selection%NPE_NATIVE_ITEM_COLUMNS+1);
    if(!count)return npe_render_empty_inventory_screen(font,pixels);
    return render_item_list(file,font,ids,quantities,count,selection,pixels,0,!full,world,full,previous);
}

int npe_render_battle_item_list(const NpeFile *file,const NpeFont *font,NpeWorld *world,
                                 const unsigned *ids,const unsigned *quantities,unsigned count,
                                 unsigned selection,unsigned previous,int full,int empty_return,
                                 uint8_t *pixels) {
    if(!world)return -1;
    if(!count && empty_return){
        world->shop_selected_item=0xffff;world->shop_row=world->inventory_column=1;
        return render_item_list(file,font,NULL,NULL,0,0,pixels,1,0,world,1,0);
    }
    return npe_render_map_item_list(file,font,world,ids,quantities,count,selection,previous,full,pixels);
}

int npe_render_single_item_screen(const NpeFile *file, const NpeFont *font,
                                   unsigned item_id, unsigned quantity,
                                   uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    return npe_render_item_list_screen(file, font, &item_id, &quantity,
                                       1, 0, pixels);
}

int npe_render_equipment_rejected_screen(const NpeFile *file,
                                          const NpeFont *font,
                                          unsigned item_id,
                                          unsigned quantity,
                                          uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    if (npe_render_single_item_screen(file, font, item_id, quantity,
                                      pixels)) return -1;
    return npe_render_equipment_rejected_overlay(font, pixels);
}

int npe_render_map_item_rejection(const NpeFont *font, unsigned rejection,
                                  uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    static const uint8_t equipment[] = {0xb8,0xc3,0xb5,0xc0,0xbe,0xdf,0xd6,0xbb,0xc4,0xdc,0xd7,0xb0,0xb1,0xb8,'!'};
    static const uint8_t enemy[] = {0xb8,0xc3,0xb5,0xc0,0xbe,0xdf,0xd6,0xbb,0xc4,0xdc,0xb6,0xd4,0xb5,0xd0,0xc8,0xcb,0xca,0xb9,0xd3,0xc3,'!'};
    static const uint8_t quest[] = {0xb2,0xbb,0xc4,0xdc,0xca,0xb9,0xd3,0xc3,'!'};
    if(!font || !font->data16 || !pixels || rejection>2)return -1;
    unsigned left=rejection==2?80:rejection?32:56, right=rejection==2?160:rejection?208:184;
    for(unsigned y=48;y<=72;++y)for(unsigned x=left-1;x<=right+1;++x)pixels[y*NPE_WIDTH+x]=0;
    for(unsigned y=49;y<=71;++y)for(unsigned x=left;x<=right;++x)
        pixels[y*NPE_WIDTH+x]=(x==left||x==right||y==49||y==71)?15:5;
    return npe_draw_gbk_text16_color(font,rejection==2?quest:rejection?enemy:equipment,
        rejection==2?sizeof(quest):rejection?sizeof(enemy):sizeof(equipment),left+4,52,15,pixels);
}

int npe_render_equipment_notice(const NpeFont *font,unsigned reason,uint8_t *pixels) {
    /* Original runtime30:587C and586C, displayed by03:4050. */
    static const uint8_t sex[]={0xb8,0xc3,0xd0,0xd4,0xb1,0xf0,0xb5,0xc4,0xc8,0xcb,0xb2,0xbb,0xc4,0xdc,0xca,0xb9,0xd3,0xc3,'!'};
    static const uint8_t item[]={0xb8,0xc3,0xb5,0xc0,0xbe,0xdf,0xb2,0xbb,0xc4,0xdc,0xd7,0xb0,0xb1,0xb8,'!'};
    if(!font || !font->data16 || !pixels || reason>1)return -1;
    unsigned left=reason?56:40,right=reason?184:200;
    for(unsigned y=48;y<=72;++y)for(unsigned x=left-1;x<=right+1;++x)pixels[y*NPE_WIDTH+x]=0;
    for(unsigned y=49;y<=71;++y)for(unsigned x=left;x<=right;++x)
        pixels[y*NPE_WIDTH+x]=(x==left || x==right || y==49 || y==71)?15:5;
    return npe_draw_gbk_text16_color(font,reason?item:sex,reason?sizeof(item):sizeof(sex),left+4,52,15,pixels);
}

int npe_render_equipment_rejected_overlay(const NpeFont *font,
                                           uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    static const uint8_t message[] = {
        0xb8,0xc3,0xb5,0xc0,0xbe,0xdf,0xd6,0xbb,
        0xc4,0xdc,0xca,0xb9,0xd3,0xc3,'!'
    };
    unsigned x, y;
    if (!font || !font->data16 || !pixels) return -1;
    for (y = 48; y <= 72; ++y)
        for (x = 55; x <= 185; ++x)
            pixels[y * NPE_WIDTH + x] = 0;
    for (y = 49; y <= 71; ++y)
        for (x = 56; x <= 184; ++x)
            pixels[y * NPE_WIDTH + x] =
                x == 56 || x == 184 || y == 49 || y == 71 ? 15 : 5;
    return npe_draw_gbk_text16_color(font, message, sizeof(message),
                                      60, 52, 15, pixels);
}

int npe_render_gbk_dialogue(const NpeFont *font, const uint8_t *text,
                            size_t size, int has_more,
                            uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    unsigned column = 0, row = 0, lines, top, x, y;
    size_t pos;
    if (!font || !font->data || (!text && size) || !pixels) return -1;
    for (pos = 0; pos < size;) {
        size_t advance;
        if (text[pos] == '\r' || text[pos] == '\n') {
            if (text[pos] == '\r' && pos + 1 < size && text[pos + 1] == '\n')
                ++pos;
            row++;
            column = 0;
            pos++;
            continue;
        }
        if (!glyph(font, text + pos, size - pos, &advance)) return -1;
        if (column == DIALOGUE_COLUMNS) { row++; column = 0; }
        if (row >= DIALOGUE_ROWS) return -1;
        column++;
        pos += advance;
    }
    lines = row + (column != 0);
    if (lines < 2) lines = 2;
    if (lines > DIALOGUE_ROWS) return -1;
    top = NPE_HEIGHT - (lines * 13 + 9);
    for (y = top; y < NPE_HEIGHT; ++y)
        for (x = 0; x < NPE_WIDTH; ++x)
            pixels[y * NPE_WIDTH + x] =
                y == top || y == NPE_HEIGHT - 1 ? 15 : 0;
    row = column = 0;
    for (pos = 0; pos < size;) {
        const uint8_t *bits;
        size_t advance;
        if (text[pos] == '\r' || text[pos] == '\n') {
            if (text[pos] == '\r' && pos + 1 < size && text[pos + 1] == '\n')
                ++pos;
            row++;
            column = 0;
            pos++;
            continue;
        }
        bits = glyph(font, text + pos, size - pos, &advance);
        if (!bits) return -1;
        if (column == DIALOGUE_COLUMNS) { row++; column = 0; }
        for (y = 0; y < 12; ++y) {
            unsigned mask = (unsigned)bits[y * 2] << 8 | bits[y * 2 + 1];
            for (x = 0; x < 12; ++x)
                if (mask & (0x8000u >> x))
                    pixels[(top + 5 + row * 13 + y) * NPE_WIDTH +
                           6 + column * 12 + x] = 15;
        }
        column++;
        pos += advance;
    }
    if (has_more)
        for (y = 0; y < 4; ++y)
            for (x = 0; x <= y * 2; ++x)
                pixels[(NPE_HEIGHT - 7 + y) * NPE_WIDTH + 226 + x] = 15;
    return 0;
}

typedef struct {const NpeFile *file;const NpeFont *font;uint8_t *pixels;int error,npc;NpeTextPage *page;} StoryTextDraw;
static int story_text_glyph(const NpeFont *font,const uint8_t *text,unsigned count,
                            unsigned left,unsigned top,uint8_t *pixels){
    unsigned width;
    if(left>=NPE_WIDTH||top>=NPE_HEIGHT)return 0;
    const uint8_t *bits=glyph16(font,text[0],count==2?text[1]:0,&width);
    for(unsigned y=0;y<16&&top+y<NPE_HEIGHT;++y){
        unsigned mask=(unsigned)bits[2*y]<<8|bits[2*y+1];
        for(unsigned x=0;x<width&&left+x<NPE_WIDTH;++x)
            if(mask&(0x8000u>>x))pixels[(top+y)*NPE_WIDTH+left+x]=15;
    }
    return 0;
}
static void npc_text_special_glyph(StoryTextDraw *draw,unsigned kind,uint8_t code,
                                    unsigned left,unsigned top){
    const uint8_t *bits=native_npc_fonts[kind][code];
    for(unsigned y=0;y<16&&top+y<NPE_HEIGHT;++y)
        for(unsigned x=0;x<8&&left+x<NPE_WIDTH;++x)
            if(bits[y]&(0x80u>>x))draw->pixels[(top+y)*NPE_WIDTH+left+x]=15;
}
static void npc_text_line_draw(StoryTextDraw *draw,const uint8_t *row,
                                unsigned left,unsigned top){
    /* Original26:459C selects fonts8/23 for controls6/8, and font9
       for a byte run after any other nonzero control below9 until7/NUL.
       Run contents are single-byte glyphs, including high/control bytes. */
    unsigned x=left,pos=0;
    while(x<NPE_WIDTH&&top<NPE_HEIGHT&&row[pos]&&row[pos]!='\r'){
        uint8_t ch=row[pos];
        if(ch==6||ch==8){
            npc_text_special_glyph(draw,ch==6?0:2,row[pos+1],x,top);
            x+=8;pos+=2;
        }else if(ch<9){
            ++pos;
            while(x<NPE_WIDTH&&row[pos]&&row[pos]!=7){
                npc_text_special_glyph(draw,1,row[pos++],x,top);x+=8;
            }
            if(row[pos]==7)++pos;
        }else{
            unsigned advance=ch<0x80?1:2;
            if(story_text_glyph(draw->font,row+pos,advance,x,top,draw->pixels)){
                draw->error=1;return;
            }
            x+=advance*8;pos+=advance;
        }
    }
}
static void story_text_line_draw(StoryTextDraw *draw,const uint8_t *row,
                                  unsigned left,unsigned top){
    if(draw->npc){npc_text_line_draw(draw,row,left,top);return;}
    /* Original28:1F08 checks the LCD boundary before each lead byte, and
       consumes a zero trail without treating it as the next lead terminator. */
    unsigned x=left,pos=0;
    while(x<239u&&top<119u&&row[pos]){
        unsigned advance=row[pos]<0x80?1:2;
        if(story_text_glyph(draw->font,row+pos,advance,x,top,draw->pixels)){
            draw->error=1;return;
        }
        x+=advance*8;pos+=advance;
    }
}
static uint8_t text_runtime_byte(StoryTextDraw *draw,uint32_t address){
    uint8_t ch=0;
    if(npe_rpg_runtime_read(draw->file,address,&ch,1))draw->error=1;
    return ch;
}
static void npc_text_layout_draw(StoryTextDraw *draw,uint32_t address,const uint16_t rect[4]){
    /* NPC3B:7440 ->03:42B4 ->1F:2B72 is distinct from story29:E552.
       It keeps >80 pairs together, pads the final cell, and consumes CR/LF. */
    unsigned columns=(uint16_t)(rect[2]-rect[0]+1u)/8u;
    unsigned lines=(uint16_t)(rect[3]-rect[1]+1u)/16u;
    uint16_t pos=0;unsigned marked=0;uint8_t row[258]={0};
    for(unsigned line=0;line<lines&&!draw->error;++line){
        unsigned row_pos=0,used=0;
        while(used<columns&&!draw->error){
            if(row_pos>=sizeof(row)-3u){draw->error=1;return;}
            if(!row_pos&&marked)row[row_pos++]=7;
            uint8_t ch=text_runtime_byte(draw,address+pos);
            if(ch>0x80&&used<columns-1u){
                row[row_pos++]=ch;++pos;
                row[row_pos++]=text_runtime_byte(draw,address+pos);++pos;used+=2;
            }else if(ch>0x80&&used==columns-1u){
                row[row_pos++]=' ';++used;
            }else if(ch=='\n'||ch=='\r'){
                ++pos;break;
            }else if(!ch){
                break;
            }else if(ch==6||ch==8){
                row[row_pos++]=ch;++pos;
                row[row_pos++]=text_runtime_byte(draw,address+pos);++pos;++used;
            }else if(ch==7){
                row[row_pos++]=ch;++pos;marked=!marked;
            }else{
                row[row_pos++]=ch;++pos;++used;
            }
        }
        if(marked){
            row[row_pos++]=7;
            if(text_runtime_byte(draw,address+pos)==7)++pos;
        }else{
            uint8_t next=text_runtime_byte(draw,address+pos);
            if(next=='\n'||next=='\r')++pos;
        }
        row[row_pos]=0;
        story_text_line_draw(draw,row,rect[0],(uint16_t)(rect[1]+line*16u));
        if(!text_runtime_byte(draw,address+pos))return;
    }
}
static void text_page_capture(StoryTextDraw *draw,uint32_t address,int clear){
    NpeTextPage *page=draw->page;if(!page)return;
    if(!clear&&!text_runtime_byte(draw,address))return;
    if(page->count && page->count<NPE_TEXT_PAGE_GLYPHS)page->glyphs[page->count++].width=0;
    unsigned pos=0,marked=0;
    while(pos<256 && page->count<NPE_TEXT_PAGE_GLYPHS){
        uint8_t ch=text_runtime_byte(draw,address+pos++);if(draw->error||!ch)break;
        NpeTextGlyph *g=page->glyphs+page->count;memset(g,0,sizeof(*g));
        if(ch=='\r'||ch=='\n'){
            uint8_t next=text_runtime_byte(draw,address+pos);
            if(ch=='\r'&&next=='\n')++pos;
            ++page->count;continue;
        }
        const uint8_t *bits;unsigned width;
        if(draw->npc && marked && ch!=7){
            bits=native_npc_fonts[1][ch];
            for(unsigned y=0;y<16;++y)g->bits[y*2]=bits[y];
            g->width=8;
        }else if(draw->npc && (ch==6||ch==8)){
            bits=native_npc_fonts[ch==6?0:2][text_runtime_byte(draw,address+pos++)];
            for(unsigned y=0;y<16;++y)g->bits[y*2]=bits[y];
            g->width=8;
        }else if(draw->npc && ch<9){
            marked=ch==7?!marked:1;continue;
        }else{
            uint8_t trail=ch>=0x80?text_runtime_byte(draw,address+pos++):0;
            bits=glyph16(draw->font,ch,trail,&width);memcpy(g->bits,bits,32);g->width=(uint8_t)width;
        }
        ++page->count;
    }
    page->valid=!draw->error;
}
unsigned npe_text_page_lines(const NpeTextPage *page,unsigned width){
    unsigned x=0,lines=1;if(!page||!width)return lines;
    for(unsigned i=0;i<page->count;++i){unsigned w=page->glyphs[i].width;
        if(!w){x=0;++lines;continue;}
        if(x&&x+w>width){x=0;++lines;}x+=w;
    }
    return lines;
}
void npe_text_page_draw(const NpeTextPage *page,uint8_t *pixels,unsigned width,unsigned height,
                        unsigned left,unsigned top,unsigned available){
    unsigned x=0,y=top;if(!page||!pixels||!available)return;
    for(unsigned i=0;i<page->count;++i){const NpeTextGlyph *g=page->glyphs+i;
        if(!g->width){x=0;y+=16;continue;}
        if(x&&x+g->width>available){x=0;y+=16;}
        for(unsigned row=0;row<16&&y+row<height;++row){
            unsigned mask=(unsigned)g->bits[row*2]<<8|g->bits[row*2+1];
            for(unsigned col=0;col<g->width&&left+x+col<width;++col)
                if(mask&(0x8000u>>col))pixels[(y+row)*width+left+x+col]=15;
        }
        x+=g->width;
    }
}
static void world_story_text_draw(void *context,uint32_t address,const uint16_t rect[4],int clear){
    StoryTextDraw *draw=context;
    text_page_capture(draw,address,clear);
    unsigned left=rect[0],top=rect[1],right=rect[2],bottom=rect[3];
    if(clear&&left<=right&&top<=bottom){
        for(unsigned y=top;y<=bottom&&y<NPE_HEIGHT;++y)
            for(unsigned x=left;x<=right&&x<NPE_WIDTH;++x)draw->pixels[y*NPE_WIDTH+x]=0;
    }
    if(draw->npc){npc_text_layout_draw(draw,address,rect);return;}
    /* Original29:E552 formats bytes, counting every signed-negative byte.
       At a full line an odd count moves its final byte to the next line.
       Low trails therefore differ from host GBK character-width wrapping. */
    uint8_t length=0,ch;
    for(;;){
        if(npe_rpg_runtime_read(draw->file,address+length,&ch,1)){draw->error=1;return;}
        if(!ch)break;
        if(++length==0){draw->error=1;return;}
    }
    unsigned columns=(uint8_t)((uint16_t)(right-left)/8u);
    uint8_t row[258]={0},y=(uint8_t)top,high=0,used=0,row_pos=0,pos=0;
    /* Retain the row bytes between lines: a consumed NUL trail can expose
       the prior line's tail. Caller-stack bytes across invocations remain a
       separate lifecycle question; this local backing starts at zero. */
    while(pos<length&&(unsigned)y+16u<=bottom){
        if(npe_rpg_runtime_read(draw->file,address+pos,&ch,1)){draw->error=1;return;}
        row[row_pos]=ch;
        if(ch>=0x80)++high;
        ++used;
        if(pos==(unsigned)length-1u){
            row[row_pos+1u]=0;story_text_line_draw(draw,row,left,y);return;
        }
        if(used==columns){
            if(high&1u){--pos;--high;row[row_pos]=0;}
            else row[row_pos+1u]=0;
            story_text_line_draw(draw,row,left,y);
            if(draw->error)return;
            used=row_pos=0;y=(uint8_t)(y+16u);
        }else{
            ++row_pos;
        }
        ++pos;
    }
}
int npe_render_world_story_page_layout(const NpeFile *file,const NpeFont *font,NpeWorld *world,uint8_t *pixels,NpeTextPage *page){
    if(!file||!font||!font->data16||!world||!pixels)return -1;
    /* Standalone render callers acquire the same World-owned RAM as main.
       A different live owner must be closed by its caller, while still alive. */
    if(!file->rpg_runtime&&npe_world_resources_open((NpeFile *)file,world))return -1;
    if(npe_rpg_runtime_owner(file)!=world)return -1;
    NpeTextPage captured={0};
    StoryTextDraw draw={file,font,pixels,0,0,page?&captured:NULL};
    int result=npe_rpg_runtime_story_page(file,world_story_text_draw,&draw,pixels);
    if(page&&captured.valid&&!result&&!draw.error)*page=captured;
    return result||draw.error?-1:0;
}

int npe_render_world_npc_page_layout(const NpeFile *file,const NpeFont *font,NpeWorld *world,uint8_t *pixels,
                              int entry,NpeTextPageLoad load,void *load_context,NpeTextPage *page){
    if(!file||!font||!font->data16||!world||!pixels)return -1;
    if(!file->rpg_runtime&&npe_world_resources_open((NpeFile *)file,world))return -1;
    if(npe_rpg_runtime_owner(file)!=world)return -1;
    NpeTextPage captured={0};
    StoryTextDraw draw={file,font,pixels,0,1,page?&captured:NULL};
    if(page&&entry)page->valid=0;
    int result=npe_rpg_runtime_npc_page(file,world_story_text_draw,&draw,pixels,entry,load,load_context);
    if(page&&captured.valid&&!result&&!draw.error)*page=captured;
    return result||draw.error?-1:0;
}

int npe_render_world_story_page(const NpeFile *file,const NpeFont *font,NpeWorld *world,uint8_t *pixels){
    return npe_render_world_story_page_layout(file,font,world,pixels,NULL);
}
int npe_render_world_npc_page(const NpeFile *file,const NpeFont *font,NpeWorld *world,uint8_t *pixels,
                             int entry,NpeTextPageLoad load,void *context){
    return npe_render_world_npc_page_layout(file,font,world,pixels,entry,load,context,NULL);
}
int npe_render_native_window_story_page(const NpeFont *font,
                                        const NpeStoryRecord *story,size_t start,
                                        size_t *next,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    if(!font||!font->data16||!story||!next||!pixels||
       (story->header_value&255u)!=0||start>=story->text_size)return -1;
    size_t pos=start;
    if(!start&&story->text[pos]=='{')++pos;
    size_t end=npe_gbk_page_end(story->text,story->text_size,pos);
    if(end==SIZE_MAX)return -1;
    /* Original43:6854..688E fixes (0,84)..(240,118). Inclusive
       clear clips x240 and leaves the final LCD scanline untouched. */
    if(end>pos)memset(pixels+84*NPE_WIDTH,0,35*NPE_WIDTH);
    unsigned x=0,y=84;
    while(pos<end){
        uint8_t ch=story->text[pos];unsigned width=ch<0x80?8:16;
        size_t advance=width==8?1:2;
        if(x+width>NPE_WIDTH){x=0;y+=16;}
        if(pos+advance>end)return -1;
        /* Original46:E7B6..E7CA stops before a third font row whose
           bottom exceeds118. The story reader already consumed the block. */
        if(y+16>118)break;
        /* Same firmware formatter as full-screen text: LF consumes
           an ASCII blank cell, rather than starting another line. */
        if(ch>=0x20&&npe_draw_gbk_text16_color(font,story->text+pos,advance,x,y,15,pixels))return -1;
        x+=width;pos+=advance;
    }
    *next=end<story->text_size?end+1:end;
    return 0;
}

int npe_render_native_story_page(const NpeFile *file,const NpeFont *font,
                                 const NpeStoryRecord *story,size_t start,
                                 size_t *next,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    uint16_t backdrop=npe_image_group_index(file,7,19);
    if(!file||!font||!font->data16||!story||!next||!pixels||
       (story->header_value&255u)!=1||start>=story->text_size)return -1;
    /* Source43:6770 reads raw3 into7ACE. Full text style1 starts at
       (0,12); Source3F:8D70..8D90 draws UI base7AEC+19. Braces in
       this style split 32-pixel text blocks, at most three per page
       (Source3F:8F90..8F9E, 9118). They do not use NPC page breaks. */
    if(npe_render_indexed_resource(file,backdrop,0,0,0,pixels))return -1;
    size_t pos=start;
    if(!start&&story->text[pos]=='{')++pos;
    for(unsigned block=0;block<3&&pos<story->text_size;++block){
        size_t end=npe_gbk_page_end(story->text,story->text_size,pos);
        unsigned x=0,y=12+32*block;
        if(end==SIZE_MAX)return -1;
        while(pos<end){
            uint8_t ch=story->text[pos];
            unsigned width=ch<0x80?8:16;
            size_t advance=width==8?1:2;
            if(x+width>NPE_WIDTH){x=0;y+=16;}
            if(y+16>NPE_HEIGHT||pos+advance>end)return -1;
            /* Firmware text consumes LF as a blank ASCII cell. It is
               not a line break here (original NPE story37 comparison). */
            if(ch>=0x20&&npe_draw_gbk_text16_color(font,story->text+pos,advance,x,y,15,pixels))return -1;
            x+=width;pos+=advance;
        }
        if(pos<story->text_size)++pos;
    }
    *next=pos;return 0;
}

static int battle_postwin_panel(const NpeFile *file,unsigned offset,
                                unsigned left,unsigned top,uint8_t *pixels){
    uint16_t bitmap=npe_image_group_index(file,6,(uint16_t)offset);
    if(npe_render_indexed_resource(file,bitmap,(int16_t)left,(int16_t)top,0,pixels))return -1;
    return 0;
}
static int battle_postwin_number(const char *text,unsigned left,unsigned top,uint8_t *pixels){
    for(unsigned i=0;text[i];++i){
        const uint8_t *glyph=npe_native_counter_glyph(text[i]);if(!glyph)return -1;
        for(unsigned y=0;y<10&&top+y<NPE_HEIGHT;++y)
            for(unsigned x=0;x<6&&left+6*i+x<NPE_WIDTH;++x)
                if(x<5&&(glyph[y]&(0x10u>>x)))
                    pixels[(top+y)*NPE_WIDTH+left+6*i+x]=15;
    }
    return 0;
}
int npe_render_battle_level_panel(const NpeFile *file,const NpeFont *font,
                                  const NpeWorld *world,unsigned member,
                                  unsigned ordinal,unsigned phase,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    unsigned left=10+25*ordinal,top=14;
    char number[20];
    if(!file||!font||!world||!pixels||member>=3||ordinal>=3||
       !world->party_active[member]||
       (phase!=NPE_BATTLE_ANIMATION_BEGIN&&phase!=NPE_BATTLE_ANIMATION_DAMAGE))return -1;
    if(phase==NPE_BATTLE_ANIMATION_BEGIN){
        if(battle_postwin_panel(file,33,left,top,pixels))return -1;
        const uint8_t *name=npe_world_battle_name(world,member);
        const uint8_t *end=memchr(name,0,12);if(!end)return -1;
        if(npe_draw_gbk_text(font,name,(size_t)(end-name),left+20,top+8,pixels))return -1;
    }
    unsigned column=phase==NPE_BATTLE_ANIMATION_BEGIN?45:106;
    snprintf(number,sizeof(number),"%u",world->level[member]);
    if(battle_postwin_number(number,left+column,top+23,pixels))return -1;
    for(unsigned stat=0;stat<4;++stat){
        unsigned x=column,y=36+13*stat;
        if(stat<2){
            x=phase==NPE_BATTLE_ANIMATION_BEGIN?32:90;
            snprintf(number,sizeof(number),"%d/%d",(int)(int16_t)(stat?world->qi[member]:world->hp[member]),
                     (int)(int16_t)(stat?world->max_qi[member]:world->max_hp[member]));
        }else snprintf(number,sizeof(number),"%d",(int)(int16_t)(stat==2?world->attack[member]:world->defense[member]));
        if(battle_postwin_number(number,left+x,top+y,pixels))return -1;
    }
    return 0;
}
int npe_render_battle_drop_panel(const NpeFile *file,const NpeFont *font,
                                 const NpeWorld *world,unsigned target,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    unsigned enemy=target/2,slot=target%2,left=40+20*enemy,top=30+20*enemy;
    if(!file||!font||!world||!pixels||enemy>=world->battle_enemy_count||
       !world->battle_enemy_drop_ids[enemy][slot]||
       battle_postwin_panel(file,31,left,top,pixels))return -1;
    const uint8_t *name=world->battle_drop_names[enemy][slot];
    size_t size=0;while(size<12 && name[size])++size;
    return npe_draw_gbk_text(font,name,size,left+48,top+8,pixels);
}
int npe_render_battle_learn_panel(const NpeFile *file,const NpeFont *font,
                                  const NpeWorld *world,unsigned target,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    unsigned id=target&255u,member=(target>>8)&255u,ordinal=target>>16,compact=0;
    NpeSkill skill;uint8_t raw[42];
    if(!file||!font||!font->data||!world||!pixels||member>=3||!id||
       !world->party_active[member]||world->skill_record_id!=id||npe_world_skill_view(world,raw,&skill))return -1;
    for(unsigned i=0;i<member;++i)compact+=world->party_active[i]!=0;
    unsigned left=36+8*compact,top=(uint16_t)(40+14*ordinal);
    if(battle_postwin_panel(file,32,left,top,pixels))return -1;
    /* The original loop can place more than eighteen panels. Clip the
       transparent glyphs as the LCD does, including partial bottom rows. */
    size_t pos=0;unsigned cursor=left+54;
    while(pos<skill.name_size){
        size_t advance=0;const uint8_t *bits=glyph(font,skill.name+pos,skill.name_size-pos,&advance);
        if(!bits)return -1;
        for(unsigned y=0;y<12&&top+7+y<NPE_HEIGHT;++y){
            unsigned mask=(unsigned)bits[y*2]<<8|bits[y*2+1];
            for(unsigned x=0;x<12&&cursor+x<NPE_WIDTH;++x)
                if(mask&(0x8000u>>x))pixels[(top+7+y)*NPE_WIDTH+cursor+x]=15;
        }
        cursor+=advance==1?8:12;pos+=advance;
    }
    return 0;
}

int npe_render_native_dialogue(const NpeFont *font, const uint8_t *text,
                               size_t size,
                               uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    unsigned lines = 0, width = 0, top, row;
    size_t pos;
    if (!font || !font->data16 || (!text && size) || !pixels) return -1;
    for (pos = 0; pos < size;) {
        unsigned glyph_width;
        size_t advance;
        if (text[pos] == '\r' || text[pos] == '\n') {
            if (text[pos] == '\r' && pos + 1 < size && text[pos + 1] == '\n')
                ++pos;
            ++lines;
            width = 0;
            ++pos;
            continue;
        }
        if (text[pos] >= 0x20 && text[pos] <= 0x7e) {
            glyph_width = 8;
            advance = 1;
        } else if (pos + 1 < size && text[pos] >= 0xa1 &&
                   text[pos] <= 0xfe && text[pos + 1] >= 0xa1 &&
                   text[pos + 1] <= 0xfe) {
            glyph_width = 16;
            advance = 2;
        } else return -1;
        if (width + glyph_width > NPE_WIDTH) {
            ++lines;
            width = 0;
        }
        width += glyph_width;
        pos += advance;
    }
    lines += width != 0;
    if (lines < 2) lines = 2;
    if (lines > 7) return -1;
    top = NPE_HEIGHT - (lines * 16 + 4);
    /* Native dialogue leaves the last LCD row in the map background color. */
    memset(pixels + top * NPE_WIDTH, 0,
           (NPE_HEIGHT - 1 - top) * NPE_WIDTH);
    row = width = 0;
    for (pos = 0; pos < size;) {
        unsigned glyph_width;
        size_t advance;
        if (text[pos] == '\r' || text[pos] == '\n') {
            if (text[pos] == '\r' && pos + 1 < size && text[pos + 1] == '\n')
                ++pos;
            ++row;
            width = 0;
            ++pos;
            continue;
        }
        glyph_width = text[pos] >= 0x20 && text[pos] <= 0x7e ? 8 : 16;
        advance = glyph_width == 8 ? 1 : 2;
        if (width + glyph_width > NPE_WIDTH) {
            ++row;
            width = 0;
        }
        if (npe_draw_gbk_text16_color(font, text + pos, advance,
                                      width, top + row * 16, 15, pixels))
            return -1;
        width += glyph_width;
        pos += advance;
    }
    return 0;
}

int npe_render_gbk_quiz_answers(const NpeFont *font,
                                const NpeQuizQuestion *question,
                                uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    uint8_t *lines;
    size_t size = 0, used = 0;
    unsigned answer;
    int result;
    if (!font || !question || !pixels) return -1;
    for (answer = 0; answer < 4; ++answer) {
        if (size > SIZE_MAX - 3 ||
            question->text_size[answer + 1] > SIZE_MAX - size - 3 ||
            (!question->text[answer + 1] &&
             question->text_size[answer + 1]))
            return -1;
        size += question->text_size[answer + 1] + 3;
    }
    lines = malloc(size ? size : 1);
    if (!lines) return -1;
    for (answer = 0; answer < 4; ++answer) {
        lines[used++] = (uint8_t)('A' + answer);
        lines[used++] = ' ';
        if (question->text_size[answer + 1])
            memcpy(lines + used, question->text[answer + 1],
                   question->text_size[answer + 1]);
        used += question->text_size[answer + 1];
        if (answer != 3) lines[used++] = '\n';
    }
    result = npe_render_gbk_dialogue(font, lines, used, 0, pixels);
    free(lines);
    return result;
}
