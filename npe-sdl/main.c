#include "npe.h"
#include "sound_sdl.h"
#include "text.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif
#include "settings.inc"
#define NPE_CONTROLLER_BUTTON_MAP(button) settings_pad_button(button)
#include "controller_input.inc"
#include "mobile_input.inc"
#include "view_display.inc"
#ifdef __APPLE__
char *npe_macos_choose_game(void);
void npe_macos_settings_menu(void);
#endif
#include "windows_menu.inc"

/* 47:536A..5494 routes I/J/K/L/M (both cases) to the same services as
   up/left/confirm/right/down. The shared item/magic list dispatchers
   4B:B62C..B6EC and4C:317E..325A accept the same aliases. Apply this only
   at those consumers and the map dispatcher37:1624..19E6; quiz text
   input must retain literal letters. System/member menus use4B:B5A0.
   SDL2 alphabetic key symbols remain lowercase with Shift/Caps Lock. */
static SDL_Keycode native_menu_navigation_key(SDL_Keycode key){
    switch(key){
        case SDLK_i:return SDLK_UP;
        case SDLK_j:return SDLK_LEFT;
        case SDLK_k:return SDLK_RETURN;
        case SDLK_l:return SDLK_RIGHT;
        case SDLK_m:return SDLK_DOWN;
        default:return key;
    }
}

/* Engine continuations run on the game thread while every UI waits.
   Poll in bounded slices as well as accepting SDL wake notifications. */
static NpeSoundPlayer *game_sound_player;
static int game_audio_event(const SDL_Event *event){
    return npe_sound_handle_event(game_sound_player,event);
}
static void show_controller_help(void);
static void show_settings(void);
static void show_settings_page(unsigned);
static void request_settings_page(unsigned page){
    SDL_Event event;SDL_zero(event);event.type=SDL_USEREVENT;
    event.user.code=0x4e504553;event.user.data1=(void *)(uintptr_t)page;
    SDL_PushEvent(&event);
}
static int game_host_event(SDL_Event *event){
    if(event->type==SDL_USEREVENT && event->user.code==0x4e504553){
        show_settings_page((unsigned)(uintptr_t)event->user.data1);return 0;
    }
    if(event->type==SDL_KEYDOWN || event->type==SDL_KEYUP){
        if(event->key.keysym.sym==SDLK_F10){
            if(event->type==SDL_KEYDOWN && !event->key.repeat)show_settings();
            return 0;
        }
        event->key.keysym.sym=settings_keyboard(event->key.keysym.sym,game_pad_mode?*game_pad_mode:0);
    }
    return 1;
}
static int game_PollEvent(SDL_Event *event){
    (void)npe_sound_pump(game_sound_player);
    while(SDL_PollEvent(event))if(game_host_event(event)&&game_mobile_event(event)&&!game_audio_event(event)&&game_pad_event(event))return 1;
    return game_pad_repeat(event);
}
static int game_wait_event_timeout(SDL_Event *event,int timeout,const char *caller){
    /* Preserve the logical wait site for input-backend instrumentation. */
    (void)caller;
    if(!game_pad && (!game_sound_player || !game_sound_player->device)){
        int got=SDL_WaitEventTimeout(event,timeout);
        if(!got || (game_host_event(event) && game_mobile_event(event) && !game_audio_event(event) && game_pad_event(event)))return got;
    }
    Uint32 start=SDL_GetTicks();
    for(;;){
        (void)npe_sound_pump(game_sound_player);
        int remaining=timeout;
        if(timeout>=0){
            Uint32 elapsed=SDL_GetTicks()-start;
            if(elapsed>(Uint32)timeout)return 0;
            remaining=timeout-(int)elapsed;
        }
        int slice=remaining<0 || remaining>10?10:remaining;
        int got=SDL_WaitEventTimeout(event,slice);
        if(got && game_host_event(event) && game_mobile_event(event) && !game_audio_event(event) && game_pad_event(event))return 1;
        if(!got && game_pad_repeat(event))return 1;
        if(timeout>=0 && (SDL_GetTicks()-start>=(Uint32)timeout || (!got && remaining==slice)))return 0;
    }
}
#define game_WaitEventTimeout(event,timeout) game_wait_event_timeout(event,timeout,__func__)
static int game_WaitEvent(SDL_Event *event){
    return game_wait_event_timeout(event,-1,"game_WaitEvent");
}
static void game_Delay(Uint32 milliseconds){
    if(!game_sound_player || !game_sound_player->device){SDL_Delay(milliseconds);return;}
    (void)npe_sound_pump(game_sound_player);
    if(!milliseconds){SDL_Delay(0);return;}
    while(milliseconds){
        Uint32 slice=milliseconds>10?10:milliseconds;
        SDL_Delay(slice);milliseconds-=slice;
        (void)npe_sound_pump(game_sound_player);
    }
}

static char *gbk_to_utf8(const uint8_t *bytes, size_t size) {
    return npe_gbk_to_utf8(bytes, size);
}

static int load_native_fonts(NpeFont *font) {
    #ifdef __ANDROID__
    const char *internal=SDL_AndroidGetInternalStoragePath();
    char *base=internal?SDL_strdup(internal):NULL;
#else
    char *base=SDL_GetBasePath();
#endif
    if(!base){fprintf(stderr,"cannot locate native font assets: %s\n",SDL_GetError());return -1;}
    size_t length=strlen(base)+sizeof("/assets/gbk12-native.bin");
    char *path=malloc(length);
    if(!path){SDL_free(base);fprintf(stderr,"cannot allocate native font path\n");return -1;}
    snprintf(path,length,"%s/assets/gbk12-native.bin",base);
    int result=npe_font_load(font,path);
    if(!result){
        snprintf(path,length,"%s/assets/gbk16-native.bin",base);
        result=npe_font_load16(font,path);
    }
    if(result)fprintf(stderr,"required native bitmap font unavailable: %s\n",path);
    free(path);SDL_free(base);
    return result;
}


#include "settings_ui.inc"

static void draw_pixels(SDL_Renderer *renderer, SDL_Texture *texture,
                        const uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
static SDL_Renderer *last_pixel_renderer;
static uint8_t last_pixel_frame[NPE_WIDTH*NPE_HEIGHT];
/*03:4050 ->4F:D530 ->4E:F282 exits on the timer message or the
  keyboard scan flag (44:E528 ->4F:4C34). Consume the dismissing key
  here so it is not also interpreted by the following menu/battle. */
static int wait_timed_notice(Uint32 duration) {
    Uint32 start = SDL_GetTicks();
    while ((Uint32)(SDL_GetTicks() - start) < duration) {
        SDL_Event input;
        while (game_PollEvent(&input)) {
            if (input.type == SDL_QUIT) {
                SDL_PushEvent(&input);
                return -2;
            }
            if (input.type == SDL_KEYDOWN && input.key.keysym.sym != SDLK_UNKNOWN)
                return 0;
        }
        game_Delay(1);
    }
    return 0;
}
static int shop_frame_valid;
static NpeWorld shop_frame_state;
static uint8_t shop_draw_frame[NPE_WIDTH*NPE_HEIGHT];
static const NpeFile *shop_frame_file;
static SDL_Renderer *shop_frame_renderer;
static void retain_shop_frame(const NpeFile *file,const NpeWorld *world,SDL_Renderer *renderer,const uint8_t *pixels){
    if(pixels)memcpy(shop_draw_frame,pixels,sizeof(shop_draw_frame));
    shop_frame_state=*world;shop_frame_file=file;shop_frame_renderer=renderer;shop_frame_valid=1;
}
/* Mode17 paints on plane0; confirmation selects the retained plane1
   without committing it (45:D39A..D3A2). Keep that source for followups. */
static uint8_t story_choice_background[NPE_WIDTH*NPE_HEIGHT];
static int story_choice_background_valid;
static const NpeFile *story_choice_background_file;
static SDL_Renderer *story_choice_background_renderer;
static int world_map_frame_valid;
static int world_map_frame_uncommitted;
static int world_map_frame_submit_pending;
/* Retained LCD edges for the existing transition adapters.03:4214 itself
  clears using the resident graphics background; system menu/periodic
  rendering now carries that color in NpeWorld.lcd_background. */
static uint8_t world_map_saved_edges[8*NPE_WIDTH];
static int world_map_saved_edges_valid;
static const NpeFile *world_map_saved_edges_file;
static SDL_Renderer *world_map_saved_edges_renderer;

static void save_map_edges(const NpeFile *file,SDL_Renderer *renderer,const uint8_t *pixels){
    memcpy(world_map_saved_edges,pixels,4*NPE_WIDTH);
    memcpy(world_map_saved_edges+4*NPE_WIDTH,pixels+(NPE_HEIGHT-4)*NPE_WIDTH,4*NPE_WIDTH);
    world_map_saved_edges_file=file;world_map_saved_edges_renderer=renderer;world_map_saved_edges_valid=1;
}

static void restore_map_edges(const NpeFile *file,SDL_Renderer *renderer,uint8_t *pixels){
    if(!world_map_saved_edges_valid || world_map_saved_edges_file!=file ||
       world_map_saved_edges_renderer!=renderer)return;
    memcpy(pixels,world_map_saved_edges,4*NPE_WIDTH);
    memcpy(pixels+(NPE_HEIGHT-4)*NPE_WIDTH,world_map_saved_edges+4*NPE_WIDTH,4*NPE_WIDTH);
}
static NpeWorld world_map_frame_state;
static const NpeFile *world_map_frame_file;
static SDL_Renderer *world_map_frame_renderer;
static uint8_t world_map_frame[NPE_WIDTH*NPE_HEIGHT];
/*50:15A8 restores the scene saved at system-menu entry. Nested panels
  may replace the visible LCD without replacing this backing scene. */
static uint8_t system_menu_background[NPE_WIDTH*NPE_HEIGHT];
static int system_menu_background_valid, system_menu_frame_valid;
static const NpeFile *system_menu_background_file;
static SDL_Renderer *system_menu_background_renderer;
static NpeWorld system_menu_frame_state;
static unsigned system_menu_frame_controls[10];
static void play_battle_animation(void *,const NpeWorld *,unsigned,unsigned,unsigned);
static void retain_battle_entry(const NpeWorld *,SDL_Renderer *,unsigned,const uint8_t *);
static const uint8_t *retained_battle_actors(const NpeWorld *,SDL_Renderer *);
static void retain_world_frame(const NpeFile *,const NpeWorld *,SDL_Renderer *,const uint8_t *,int);
static int same_display_world(const NpeWorld *a,const NpeWorld *b) {
    /* Host dispatch policy is not an original RAM field or a reason to
       submit another LCD frame when the input scope ends. */
    size_t offset=offsetof(NpeWorld,defer_story_events);
    return !memcmp(a,b,offset) &&
        !memcmp((const uint8_t *)a+offset+1,(const uint8_t *)b+offset+1,
                sizeof(*a)-offset-1);
}

static void retain_battle_enemy_pointer_clear(const NpeWorld *,unsigned);
static void apply_battle_enemy_pointer_clears(const NpeWorld *,uint8_t *);

static int read_file(const char *path, uint8_t **out, size_t *size) {
    SDL_RWops *rw = SDL_RWFromFile(path, "rb");
    Sint64 length;
    uint8_t *bytes;
    if (!rw) { fprintf(stderr, "open: %s\n", SDL_GetError()); return -1; }
    length = SDL_RWsize(rw);
    if (length < 0 || (uint64_t)length > SIZE_MAX ||
        (uint64_t)length > 128u * 1024u * 1024u) {
        fprintf(stderr, "unsupported file size\n"); SDL_RWclose(rw); return -1;
    }
    bytes = malloc((size_t)length ? (size_t)length : 1);
    if (!bytes || SDL_RWread(rw, bytes, 1, (size_t)length) != (size_t)length) {
        fprintf(stderr, "read failed\n"); free(bytes); SDL_RWclose(rw); return -1;
    }
    SDL_RWclose(rw);
    *out = bytes; *size = (size_t)length;
    return 0;
}

static unsigned first_bitmap(const NpeFile *file) {
    unsigned i;
    for (i = 0; i < file->section_count; ++i)
        if (file->sections[i].bitmap_count) return i;
    return file->section_count;
}

static void print_info(const NpeFile *file) {
    unsigned i;
    printf("NPE title bytes: ");
    for (i = 0; i < 12; ++i) printf("%02X", file->title[i]);
    printf("\nsize: %zu; sections: %u; indexed bitmaps: %u; battle bitmaps: %u; animations: %u groups / %u frames\n",
           file->size, file->section_count, file->indexed_bitmap_count,
           file->battle_bitmap_count,
           file->animation_group_count, file->animation_frame_count);
    for (i = 0; i < file->section_count; ++i) {
        const NpeSection *s = &file->sections[i];
        NpeAudioResource sound;
        printf("%02u  0x%08X  %9u bytes  bitmaps: %u  records: %u",
               i, s->offset, s->size, s->bitmap_count,
               file->offset_table_records[i]);
        if (!npe_audio_resource(file, i, &sound))
            printf("  audio13 count: %u", sound.header_count);
        putchar('\n');
    }
}

static int print_map_info(const NpeFile *file) {
    unsigned i, j, x, y;
    unsigned parsed = 0, malformed = 0, exits = 0, bad_exits = 0;
    unsigned long long tiles = 0, bad_tiles = 0;
    for (i = 0; i < file->offset_table_records[8]; ++i) {
        NpeMap map;
        if (npe_map(file, i, &map) || map.tail_size < 592) {
            malformed++;
            continue;
        }
        parsed++;
        for (y = 0; y < map.height; ++y)
            for (x = 0; x < map.width; ++x) {
                NpeIndexedBitmap bitmap;
                tiles++;
                if (npe_indexed_bitmap(file, npe_map_tile(&map, x, y), &bitmap) ||
                    bitmap.width != 16 || bitmap.height != 16) bad_tiles++;
            }
        for (j = 0; j < 12; ++j) {
            NpeMapExit exit;
            if (npe_map_exit(&map, j, &exit)) return 1;
            if (!exit.destination_map) continue;
            exits++;
            if (exit.destination_map > file->offset_table_records[8]) bad_exits++;
        }
    }
    printf("maps: %u; parsed: %u; malformed: %u; tile refs: %llu; invalid tiles: %llu; exits: %u; invalid targets: %u\n",
           file->offset_table_records[8], parsed, malformed,
           tiles, bad_tiles, exits, bad_exits);
    return malformed || bad_tiles || bad_exits ? 1 : 0;
}

static char *dialogue_utf8(const NpeFile *file, uint32_t offset) {
    NpeDialoguePage page;
    char *utf8;
    size_t i;
    if (npe_dialogue_page(file, offset, &page)) return NULL;
    utf8 = gbk_to_utf8(page.bytes, page.size);
    if (!utf8) return NULL;
    for (i = 0; utf8[i]; ++i) if (utf8[i] == '{') utf8[i] = '\n';
    return utf8;
}

typedef struct {const NpeFile *file;NpeWorld *world;} NpcTextLoad;
static int load_npc_text(void *context){
    NpcTextLoad *load=context;
    /*3B:7212 always reloads on a fresh native entry, independently of the
      host-only prepared cache used by numeric diagnostic callers. */
    load->world->mode2_ready=0;
    return npe_world_prepare_mode2(load->file,load->world);
}
static void npc_text_window(NpeWorld *world){
    /*3B:7178..71BA runs before both the fresh-entry and input branches. */
    static const uint16_t rect[4]={0,84,240,118};
    memcpy(world->text_rect,rect,sizeof(rect));
}
static int show_dialogue_pages(SDL_Window *window, SDL_Renderer *renderer,
                               SDL_Texture *texture, const NpeFile *file,
                               NpeWorld *world, const NpeFont *font,
                               uint32_t offset) {
    uint8_t base[NPE_WIDTH*NPE_HEIGHT],pixels[sizeof(base)];
    (void)window;(void)offset; /* The original reads the live797A/797C pointer. */
    if(last_pixel_renderer==renderer)memcpy(base,last_pixel_frame,sizeof(base));
    else if(npe_render_world_frame(file,world,0,base))return -1;
    save_map_edges(file,renderer,base);
    memcpy(pixels,base,sizeof(pixels));
    NpcTextLoad load={file,world};
    if(world->map_init_pending){
        if(npe_render_world_npc_page(file,font,world,pixels,1,load_npc_text,&load))return -1;
        draw_pixels(renderer,texture,pixels);
    }
    for(;;){
        SDL_Event event;
        if(!game_WaitEvent(&event))return -1;
        if(event.type==SDL_QUIT){SDL_PushEvent(&event);return -1;}
        if(event.type!=SDL_KEYDOWN)continue;
        npc_text_window(world);
        SDL_Keycode key=event.key.keysym.sym;
        if(key==SDLK_SPACE){
            if(world->text_more==1){
                if(npe_render_world_npc_page(file,font,world,pixels,0,NULL,NULL))return -1;
                draw_pixels(renderer,texture,pixels);continue;
            }
            /*3B:76DA..76E8 restores and submits before dispatching followup. */
            world->map_init_pending=1;memcpy(pixels,base,sizeof(pixels));
            draw_pixels(renderer,texture,pixels);return 0;
        }
        if(world->mode2_context>=10&&(key==SDLK_RETURN||key==SDLK_KP_ENTER||key==SDLK_k)){
            /*3B:7792..779A clears continuation and restores without submitting. */
            world->text_more=0;world->map_init_pending=1;
            memcpy(pixels,base,sizeof(pixels));return 0;
        }
    }
}

static char *npc_dialogue_utf8(const NpeFile *file, unsigned npc_id,
                               unsigned case_index,
                               unsigned *case_count) {
    NpeNpcAction action;
    if (npe_npc_action(file, npc_id, case_index, &action)) return NULL;
    *case_count = action.case_count;
    return dialogue_utf8(file, action.dialogue_offset[0]);
}

static int print_npc_info(const NpeFile *file, unsigned npc_id) {
    unsigned case_count;
    char *utf8 = npc_dialogue_utf8(file, npc_id, 0, &case_count);
    if (!utf8) {
        fprintf(stderr, "NPC %u has no supported dialogue record\n", npc_id);
        return 1;
    }
    printf("NPC %u: %u conditional cases; first dialogue:\n", npc_id,
           case_count);
    puts(utf8);
    free(utf8);
    return 0;
}

static int print_story_info(const NpeFile *file, unsigned index) {
    NpeStoryRecord record;
    char *utf8;
    size_t i;
    if (npe_story_record(file, index, &record) ||
        !(utf8 = gbk_to_utf8(record.text, record.text_size))) {
        fprintf(stderr, "story record %u is unsupported\n", index);
        return 1;
    }
    for (i = 0; utf8[i]; ++i) if (utf8[i] == '{') utf8[i] = '\n';
    printf("story record %u: header=%u, control=%u,%u\n%s\n",
           index, record.header_value, record.control_byte,
           record.control_word, utf8);
    free(utf8);
    return 0;
}

static int print_character_info(const NpeFile *file, unsigned index) {
    NpeCharacter character;
    const uint8_t *end;
    char *name;
    unsigned slot;
    if (npe_character(file, index, &character)) {
        fprintf(stderr, "character record %u is unsupported\n", index);
        return 1;
    }
    end = memchr(character.name, 0, 12);
    name = gbk_to_utf8(character.name,
                       end ? (size_t)(end - character.name) : 12);
    if (!name) return 1;
    printf("character %u: %s; HP %u/%u; QI %u/%u; attack %u; defense %u; initial equipment:",
           index, name, character.hp, character.max_hp,
           character.qi, character.max_qi,
           character.attack, character.defense);
    for (slot = 0; slot < 6; ++slot)
        printf(" %u", character.equipped_items[slot]);
    putchar('\n');
    free(name);
    return 0;
}
static int print_skill_info(const NpeFile *file, unsigned skill_id) {
    NpeSkill skill;
    char *name, *description;
    if (!skill_id || npe_skill(file, skill_id - 1, &skill) ||
        !(name = gbk_to_utf8(skill.name, skill.name_size))) {
        fprintf(stderr, "skill %u is unsupported\n", skill_id);
        return 1;
    }
    description = gbk_to_utf8(skill.description, skill.description_size);
    printf("skill %u: %s; type %u; level %u; QI cost %u; HP effect %u; parameters %u,%u\n%s\n",
           skill_id, name, skill.effect_type, skill.required_level,
           skill.qi_cost,
           skill.hp_effect, skill.parameter_a, skill.parameter_b,
           description ? description : "");
    free(description);
    free(name);
    return 0;
}
static int print_enemy_info(const NpeFile *file, unsigned index) {
    NpeEnemy enemy;
    char *name;
    if (npe_enemy(file, index, &enemy) ||
        !(name = gbk_to_utf8(enemy.name, enemy.name_size))) {
        fprintf(stderr, "enemy record %u is unsupported\n", index);
        return 1;
    }
    printf("enemy %u: %s; graphic %u; max HP %u; max QI %u; attack %u; defense %u\n",
           index + 1, name, enemy.graphic, enemy.max_hp, enemy.max_qi,
           enemy.attack, enemy.defense);
    free(name);
    return 0;
}
static int print_encounter_info(const NpeFile *file, unsigned map_number) {
    NpeMap map;
    unsigned slot, total = 0;
    if (!map_number || npe_map(file, map_number - 1, &map)) return 1;
    printf("map %u encounter enemies:\n", map_number);
    for (slot = 0; slot < 8; ++slot) {
        unsigned enemy_id;
        NpeEnemy enemy;
        char *name;
        int found = npe_map_encounter(&map, slot, &enemy_id);
        if (found < 0) return 1;
        if (!found) continue;
        if (npe_enemy(file, enemy_id - 1, &enemy) ||
            !(name = gbk_to_utf8(enemy.name, enemy.name_size))) return 1;
        printf("  slot %u: enemy %u %s, graphic %u, max HP %u\n",
               slot, enemy_id, name, enemy.graphic, enemy.max_hp);
        free(name);
        total++;
    }
    if (!total) puts("  none");
    return 0;
}

typedef struct {
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
    int initialized;
    int resume;
    unsigned previous;
    NpeWorld *world;
} NpeItemListView;
static int select_native_item(SDL_Renderer *renderer, SDL_Texture *texture,
                              const NpeFile *file, const NpeFont *font,
                              const unsigned *item_ids,
                              const unsigned *quantities, unsigned count,
                              unsigned *saved_selection, int battle_empty,
                              NpeItemListView *view) {
    uint8_t local_pixels[NPE_WIDTH * NPE_HEIGHT];
    SDL_Event event;
    uint8_t *pixels=view?view->pixels:local_pixels;
    unsigned selection = saved_selection ? *saved_selection : 0;
    if (!renderer || !texture || !font || !font->data ||
        count > NPE_NATIVE_ITEM_MAX) return -1;
    if (selection >= count) selection = 0;
    int redraw=!(view && view->resume);
    if(view)view->resume=0;
    for (;;) {
        if (saved_selection) *saved_selection = selection;
        if(redraw){
            int rendered = view&&view->world ?
                (battle_empty?npe_render_battle_item_list(file,font,view->world,item_ids,quantities,count,
                                         selection,view->previous,!view->initialized,battle_empty==2,pixels):
                npe_render_map_item_list(file,font,view->world,item_ids,quantities,count,
                                         selection,view->previous,!view->initialized,pixels)) : count ?
                (view&&view->initialized?npe_update_item_list_screen:npe_render_item_list_screen)
                    (file,font,item_ids,quantities,count,selection,pixels) :
                battle_empty == 2 ? npe_render_battle_empty_item_return(file,font,pixels) :
                                    npe_render_empty_inventory_screen(font, pixels);
            if (rendered) return -1;
            if(view){view->initialized=1;view->previous=selection;}
            draw_pixels(renderer, texture, pixels);
        }
        redraw=!(view&&view->world);
        if (!game_WaitEvent(&event)) return 0;
        if (event.type == SDL_QUIT) {
            SDL_PushEvent(&event);
            return 0;
        }
        if (event.type != SDL_KEYDOWN) continue;
        SDL_Keycode key=native_menu_navigation_key(event.key.keysym.sym);
        unsigned old_selection=selection;
        if(view&&view->world && count && (key==SDLK_n || key==SDLK_p)){
            int result=npe_render_map_item_description(file,font,view->world,key==SDLK_n?1:-1,pixels);
            if(result<0)return -1;
            if(result)draw_pixels(renderer,texture,pixels);
            continue;
        }
        if (key == SDLK_RIGHT &&
            selection % NPE_NATIVE_ITEM_COLUMNS <
                NPE_NATIVE_ITEM_COLUMNS - 1 &&
            selection + 1 < count)
            ++selection;
        else if (key == SDLK_LEFT &&
                 selection % NPE_NATIVE_ITEM_COLUMNS)
            --selection;
        else if (key == SDLK_DOWN &&
                 selection + NPE_NATIVE_ITEM_COLUMNS < count)
            selection += NPE_NATIVE_ITEM_COLUMNS;
        else if (key == SDLK_UP &&
                 selection >= NPE_NATIVE_ITEM_COLUMNS)
            selection -= NPE_NATIVE_ITEM_COLUMNS;
        else if ((key==SDLK_RETURN || key==SDLK_KP_ENTER) && count)
            return (int)item_ids[selection];
        else if ((key==SDLK_RETURN || key==SDLK_KP_ENTER) && battle_empty)
            return 255; /* Native 7A4C=-1 is truncated to the selected byte. */
        else if (event.key.keysym.sym == SDLK_ESCAPE ||
                 event.key.keysym.sym == SDLK_BACKSPACE ||
                 event.key.keysym.sym == SDLK_F4) return 0;
        if(selection!=old_selection){
            redraw=1;
            if(view && selection/NPE_NATIVE_ITEM_PAGE_SIZE!=old_selection/NPE_NATIVE_ITEM_PAGE_SIZE)
                view->initialized=0;
        }
    }
}

/* Native ordinary attacks and thrown items bypass the picker when only
   one enemy is alive. Multiple targets retain the arrow erasure buffer. */
static int show_battle_enemy_target(SDL_Renderer *renderer, SDL_Texture *texture,
                                     const NpeFile *file, const NpeFont *font,
                                     NpeWorld *world, unsigned icon) {
    static const uint8_t body[] = {0xcc, 0xe5}, qi[] = {0xc6, 0xf8};
    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT];
    unsigned living = 0, actor, previous;
    int redraw=1;
    SDL_Event event;
    if (!world || !world->battle_enemy_count || world->battle_actor >= 3)
        return -1;
    for (unsigned i = 0; i < world->battle_enemy_count && i < 3; ++i)
        living += (int16_t)world->battle_enemy_hps[i] > 0;
    if (!living) return -1;
    world->battle_target = world->battle_enemy_count - 1;
    if (npe_world_battle_cycle_target(world, 1) < 0) return -1;
    if(npe_world_battle_select_attack(world,world->battle_target)<0)return -1;
    if (living == 1) return 1;
    world->battle_menu_phase=17; /*40:1B8C ordinary target selector. */
    actor = world->battle_actor;
    /* The native selector adds its arrow to the current LCD. Live record
       coordinates may have changed since the actors were last drawn. */
    if(last_pixel_renderer==renderer){
        memcpy(pixels,last_pixel_frame,sizeof(pixels));
        if(npe_draw_battle_enemy_target_pointer(file,world,world->battle_target,pixels))return -1;
    }else{
        if (npe_render_battle_enemy_target(file, world, pixels) ||
            npe_draw_gbk_text(font, body, sizeof(body), 70, 88, pixels) ||
            npe_draw_gbk_text(font, qi, sizeof(qi), 70, 104, pixels)) return -1;
        npe_render_battle_hud_values(world->hp[actor], world->max_hp[actor],
                                   world->qi[actor], world->max_qi[actor], pixels);
        if (npe_render_battle_icon_selection(icon, pixels)) return -1;
    }
    for (;;) {
        if(redraw){draw_pixels(renderer,texture,pixels);redraw=0;}
        if (!game_WaitEvent(&event)) return -1;
        if (event.type == SDL_QUIT) { SDL_PushEvent(&event); return -1; }
        if (event.type != SDL_KEYDOWN) continue;
        SDL_Keycode key = native_menu_navigation_key(event.key.keysym.sym);
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
        { retain_battle_enemy_pointer_clear(world,world->battle_target); return 1; }
        if (key == SDLK_F4 || key == SDLK_BACKSPACE) {
            world->battle_menu_phase=10;
            retain_battle_enemy_pointer_clear(world,world->battle_target);return 0;
        }
        previous = world->battle_target;
        if (key == SDLK_UP || key == SDLK_DOWN) {
            redraw=1;
            retain_battle_enemy_pointer_clear(world,previous);
            if (npe_world_battle_cycle_target(world, key == SDLK_UP ? -1 : 1) < 0)return -1;
            if(previous!=world->battle_target){
                if(npe_clear_battle_enemy_target(world,previous,pixels))return -1;
                draw_pixels(renderer,texture,pixels);
                if(npe_update_battle_enemy_target(file,world,previous,pixels))return -1;
            }
        }
    }
}

static int draw_native_battle_target(SDL_Renderer *,SDL_Texture *,const NpeFile *,
    const NpeFont *,NpeWorld *,unsigned,unsigned,int,int,unsigned,uint8_t *);

static int battle_resources_set(const NpeFile *file,NpeWorld *world,int active) {
    return npe_world_battle_resources(file,world,active);
}

static int show_battle_item_target(SDL_Renderer *renderer,SDL_Texture *texture,
                                    const NpeFile *file,const NpeFont *font,
                                    NpeWorld *world,const NpeItem *item,unsigned *target) {
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];SDL_Event event;
    unsigned party=item->target_side==1,active=0,previous=0;
    *target=party?world->battle_actor:0;
    if(party){for(unsigned i=0;i<3;++i)active+=world->party_active[i]!=0;}
    else for(unsigned i=0;i<world->battle_enemy_count;++i)if((int16_t)world->battle_enemy_hps[i]>0){
        if(!active)*target=i;
        ++active;
    }
    (void)npe_world_battle_select_target(world,*target,(int)party);
    if(item->group || active==1)return 1;
    world->battle_menu_phase=37; /*38:4F16 item target selector. */
    if(draw_native_battle_target(renderer,texture,file,font,world,*target,0,(int)party,1,2,pixels))return -1;
    for(;;){
        if(!game_WaitEvent(&event))return -1;
        if(event.type==SDL_QUIT){SDL_PushEvent(&event);return -1;}
        if(event.type!=SDL_KEYDOWN)continue;
        SDL_Keycode key=native_menu_navigation_key(event.key.keysym.sym);
        if(key==SDLK_RETURN || key==SDLK_KP_ENTER){
            if(!party)retain_battle_enemy_pointer_clear(world,*target);
            world->battle_target=(uint8_t)*target;return 1;
        }
        if(key==SDLK_F4 || key==SDLK_BACKSPACE)return 0;
        if(key!=SDLK_UP && key!=SDLK_DOWN)continue;
        previous=*target;
        for(unsigned i=0;i<(party?3:world->battle_enemy_count);++i){
            unsigned step=party?(key==SDLK_UP?1u:2u):
                                (key==SDLK_UP?world->battle_enemy_count-1u:1u);
            *target=(*target+step)%(party?3u:world->battle_enemy_count);
            if(party?world->party_active[*target]:(int16_t)world->battle_enemy_hps[*target]>0)break;
        }
        (void)npe_world_battle_select_target(world,*target,(int)party);
        if(!party)world->battle_display_target=world->battle_native_targets[world->battle_actor];
        if(draw_native_battle_target(renderer,texture,file,font,world,*target,previous,(int)party,0,2,pixels))return -1;
    }
}

static int show_battle_items(SDL_Renderer *renderer,SDL_Texture *texture,
                               const NpeFile *file,const NpeFont *font,
                               NpeWorld *world,int mode) {
    unsigned ids[NPE_NATIVE_ITEM_MAX],quantities[NPE_NATIVE_ITEM_MAX],selection=0;
    int throwing=mode==2,empty_return=0;NpeItemListView view={.world=world};
    /*42:8EDE..8F06 releases the packed battle images before the list. */
    if(battle_resources_set(file,world,0))return 0;
    world->battle_menu_phase=33; /*42:8F14 inventory selector. */
    for(;;){
        unsigned count=0,target=world->battle_actor;
        for(unsigned id=1;id<255;++id){
            if((int16_t)world->condition_words[id]<=0)continue;
            ids[count]=id;quantities[count++]=world->condition_words[id];
        }
        int chosen=select_native_item(renderer,texture,file,font,ids,quantities,count,&selection,empty_return?2:1,&view);
        if(chosen<=0){
            /*36:E2D8..E318 restores the packed images before use/throw. */
            if(battle_resources_set(file,world,1))return 0;
            world->battle_menu_phase=32; /*36:E3C6 returns to the use/throw list. */
            if(world->battle_commands[world->battle_actor].resource_id)
                (void)npe_world_battle_cancel_target(world);
            return 0;
        }
        NpeItem item;
        if(npe_world_item(file,world,(unsigned)chosen,&item))return 0;
        int status=throwing?npe_world_battle_reserve_throw_item(file,world,(unsigned)chosen):
                            npe_world_battle_reserve_use_item(file,world,(unsigned)chosen);
        if(status==-2){
            uint8_t restored[NPE_WIDTH*NPE_HEIGHT];memcpy(restored,view.pixels,sizeof(restored));
            if(npe_render_battle_item_rejection(font,throwing,view.pixels))return 0;
            draw_pixels(renderer,texture,view.pixels);if(wait_timed_notice(2000))return 0;
            memcpy(view.pixels,restored,sizeof(restored));draw_pixels(renderer,texture,view.pixels);view.resume=1;
            continue;
        }
        if(status)return 0;
        /*38:4CEA..4D2A loads images even for a group or one-target command. */
        if(battle_resources_set(file,world,1))return 0;
        int confirmation=show_battle_item_target(renderer,texture,file,font,world,&item,&target);
        if(confirmation!=1){
            /*36:E1B0..E270 releases images, refunds the item, then sets33. */
            if(confirmation==0 && battle_resources_set(file,world,0))return 0;
            (void)npe_world_battle_cancel_item_selection(world);
            if(confirmation<0)return 0;
            world->battle_menu_phase=33;
            if(chosen==255)empty_return=1;
            view.initialized=0;continue;
        }
        status=throwing?npe_world_battle_throw_item(file,world,(unsigned)chosen,target):
                         npe_world_battle_use_item(file,world,(unsigned)chosen,target);
        if(status<0){(void)npe_world_battle_cancel_item_selection(world);return 0;}
        return 1;
    }
}

static int show_use_item(SDL_Window *window, SDL_Renderer *renderer,
                          SDL_Texture *texture, const NpeFile *file,
                          const NpeFont *font, NpeWorld *world,
                          int battle_item_mode) {
    unsigned ids[NPE_NATIVE_ITEM_MAX], quantities[NPE_NATIVE_ITEM_MAX];
    unsigned selection=0, members[3], member_count=0;
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]; SDL_Event event;
    NpeItemListView view={.world=world};
    (void)window;
    if (!file || !world || file->section_count<=2) return 0;
    if (world->game_mode==3 && world->battle_enemy_count)
        return show_battle_items(renderer,texture,file,font,world,battle_item_mode);
    if (!font || !font->data || !renderer || !texture) return 0;
    for (unsigned i=0;i<3;++i) if (world->party_active[i]) members[member_count++]=i;
    if (!member_count) return 0;
    world->game_mode=4;world->story_event_id=2;
    for (;;) {
        unsigned count=0;
        for (unsigned id=1;id<255;++id) {
            if ((int16_t)world->condition_words[id]<=0) continue;
            ids[count]=id;quantities[count++]=world->condition_words[id];
        }
        int chosen=select_native_item(renderer,texture,file,font,ids,quantities,count,&selection,0,&view);
        NpeItem item;
        if (chosen<=0) {npe_world_close_menu(world);return 0;}
        if (npe_world_item(file,world,(unsigned)chosen,&item)) return 0;
        int quest_rejected=0;
        if(item.effect_type==24){
            int result=npe_world_use_item(file,world,(unsigned)chosen,members[0]);
            if(result==1)return 1; /* Outer event loop resumes the stored mode/id. */
            if(result!=-3)return 0;
            quest_rejected=1;
        }
        if (quest_rejected || item.effect_type>=25 || item.target_side!=1) {
            memcpy(pixels,view.pixels,sizeof(pixels));
            if (npe_render_map_item_list(file,font,world,ids,quantities,count,selection,selection,0,pixels) ||
                npe_render_map_item_rejection(font,quest_rejected?2:item.effect_type>=25?0:1,pixels)) return 0;
            draw_pixels(renderer,texture,pixels);
            if (wait_timed_notice(2000)) return 0;
            continue;
        }
        if (item.group) {
            if (npe_world_use_item(file,world,(unsigned)chosen,members[0])<0) return 0;
            view.initialized=0;
            continue;
        }
        unsigned member_selection=0;
        int target_initial=1, redraw=1;
        for (;;) {
            unsigned member=members[member_selection];
            if(redraw){
                if (npe_world_item(file,world,(unsigned)chosen,&item) ||
                    npe_render_map_item_target_view(file,font,world,member,
                    (unsigned)chosen,world->condition_words[chosen],target_initial,pixels)) return 0;
                draw_pixels(renderer,texture,pixels);
                target_initial=0;redraw=0;
            }
            if (!game_WaitEvent(&event)) return 0;
            if (event.type==SDL_QUIT) {SDL_PushEvent(&event);return 0;}
            if (event.type!=SDL_KEYDOWN) continue;
            SDL_Keycode key=native_menu_navigation_key(event.key.keysym.sym);
            if (key==SDLK_F4 || key==SDLK_BACKSPACE || key==SDLK_ESCAPE) {selection=0;view.initialized=0;break;}
            if ((int16_t)world->condition_words[chosen]<=0) continue;
            if (key==SDLK_UP) {member_selection=(member_selection+member_count-1)%member_count;redraw=1;}
            else if (key==SDLK_DOWN) {member_selection=(member_selection+1)%member_count;redraw=1;}
            else if (key==SDLK_RETURN || key==SDLK_KP_ENTER) {
                if (npe_world_use_item(file,world,(unsigned)chosen,member)<0) return 0;
                redraw=1;
            }
        }
    }
}

static void show_native_equip_choice(SDL_Window *window,
                                     SDL_Renderer *renderer,
                                     SDL_Texture *texture,
                                     const NpeFile *file,
                                     const NpeFont *font,
                                     NpeWorld *world) {
    unsigned ids[NPE_NATIVE_ITEM_MAX], quantities[NPE_NATIVE_ITEM_MAX];
    unsigned selection=0, members[3], member_count=0;
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]; SDL_Event event;
    NpeItemListView view={.world=world};
    (void)window;
    if (!file || !world || !font || !font->data || !renderer || !texture ||
        file->section_count<=2) return;
    for (unsigned i=0;i<3;++i) if (world->party_active[i]) members[member_count++]=i;
    if (!member_count) return;
    world->game_mode=4;
    for (;;) {
        unsigned count=0;
        /* Re-scan after returning from the target page: refunds and the
           newly empty item slot must be reflected in the native list. */
        for (unsigned id=1;id<255;++id) {
            if ((int16_t)world->condition_words[id]<=0) continue;
            ids[count]=id;quantities[count++]=world->condition_words[id];
        }
        int chosen=select_native_item(renderer,texture,file,font,ids,quantities,
                                      count,&selection,0,&view);
        NpeItem item;
        if (chosen<=0) {npe_world_close_menu(world);return;}
        if (npe_world_item(file,world,(unsigned)chosen,&item)) return;
        /*2F:1630 admits24;2F:1654 dispatches its quest service before
          the ordinary equipment target. Both inventory commands share it. */
        int quest_rejected=0;
        if(item.effect_type==24){
            int result=npe_world_use_item(file,world,(unsigned)chosen,members[0]);
            if(result==1)return;
            if(result!=-3)return;
            quest_rejected=1;
        }
        if (quest_rejected || item.effect_type<24) {
            memcpy(pixels,view.pixels,sizeof(pixels));
            if (quest_rejected ? npe_render_map_item_rejection(font,2,pixels) :
                npe_render_equipment_rejected_overlay(font,pixels)) return;
            draw_pixels(renderer,texture,pixels);
            if(wait_timed_notice(2000))return;
            draw_pixels(renderer,texture,view.pixels);view.resume=1;
            continue;
        }
        unsigned member_selection=0;
        int redraw=1;
        for (;;) {
            unsigned member=members[member_selection];
            if(redraw){
                if (npe_render_equip_target_screen(file,font,world,member,
                    (unsigned)chosen,world->condition_words[chosen],redraw==1,pixels)) return;
                draw_pixels(renderer,texture,pixels);
            }
            redraw=0;
            if (!game_WaitEvent(&event)) return;
            if (event.type==SDL_QUIT) {SDL_PushEvent(&event);return;}
            if (event.type!=SDL_KEYDOWN) continue;
            SDL_Keycode key=native_menu_navigation_key(event.key.keysym.sym);
            if (key==SDLK_F4 || key==SDLK_BACKSPACE || key==SDLK_ESCAPE){
                selection=0;view.initialized=0;break;
            }
            /* Original depleted-item page keeps its current member;
               only the return key remains active until the list scan. */
            if ((int16_t)world->condition_words[chosen]<=0) continue;
            if (key==SDLK_UP && member_count>1){member_selection=(member_selection+member_count-1)%member_count;redraw=2;}
            else if (key==SDLK_DOWN && member_count>1){member_selection=(member_selection+1)%member_count;redraw=2;}
            else if (key==SDLK_RETURN || key==SDLK_KP_ENTER) {
                int result=npe_world_equip_item(file,world,(unsigned)chosen,member);
                if(result==-2 || result==-3){
                    uint8_t restored[NPE_WIDTH*NPE_HEIGHT];memcpy(restored,pixels,sizeof(restored));
                    if(npe_render_equipment_notice(font,result==-3,pixels))return;
                    draw_pixels(renderer,texture,pixels);
                    if(wait_timed_notice(2000))return;
                    memcpy(pixels,restored,sizeof(restored));draw_pixels(renderer,texture,pixels);
                }else if(result<0)return;
                else redraw=result?2:0;
            }
        }
    }
}

static void show_magic_list(SDL_Window *window, SDL_Renderer *renderer,
                            SDL_Texture *texture, const NpeFile *file,
                            const NpeFont *font, NpeWorld *world) {
    unsigned members[3], member_count = 0, member_selection = 0;
    unsigned ids[18], count = 0, caster = 0, target = 0, selection = 0, first = 0;
    unsigned last_selection = 0, last_first = 0;
    int choosing = 1, targeting = 0, list_visible = 0, redraw=1;
    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT];
    SDL_Event event;
    (void)window;
    if (!file || !font || !font->data || !renderer || !texture || !world ||
        world->game_mode != 6) return;
    for (unsigned i = 0; i < 3; ++i)
        if (world->party_active[i]) members[member_count++] = i;
    if (!member_count) return;
    for (;;) {
        NpeSkill skill;
        if(redraw){
        if (choosing) {
            /* The caster picker overlays the retained system-menu map. */
            if (system_menu_background_valid && system_menu_background_file == file &&
                system_menu_background_renderer == renderer)
                memcpy(pixels, system_menu_background, sizeof(pixels));
            else if (npe_render_world_load(file, world, pixels)) return;
            npe_render_system_menu_overlay(file, world, 1, pixels);
            if (npe_render_native_menu_money(file, font, world, pixels)) return;
            if (npe_render_magic_member_overlay(file, font, world, member_selection, pixels)) return;
        } else if (!count) {
            if (npe_render_empty_magic_screen(font, pixels)) return;
        } else {
            if (targeting) {
                if (npe_render_map_magic_target(file,font,world,caster,target,
                                                ids[selection],redraw==1,pixels))return;
                list_visible = 0;
            } else {
                int result = 0;
                result=npe_render_map_magic_list(file,font,world,caster,ids,count,first,selection,
                        last_first,last_selection,!list_visible,pixels);
                if (result) return;
                list_visible = 1; last_selection = selection; last_first = first;
            }
        }
        draw_pixels(renderer, texture, pixels);
        redraw=0;
        }
        if (!game_WaitEvent(&event)) return;
        if (event.type == SDL_QUIT) { SDL_PushEvent(&event); return; }
        if (event.type != SDL_KEYDOWN) continue;
        SDL_Keycode key = native_menu_navigation_key(event.key.keysym.sym);
        if(key==SDLK_F4){npe_world_close_system_menu(world);return;}
        if (key == SDLK_BACKSPACE || key == SDLK_ESCAPE) {
            if (choosing) return;
            if (targeting) { targeting = 0; list_visible = 0; }
            else { choosing = 1; list_visible = 0; }
            redraw=1;
            continue;
        }
        if (choosing) {
            if (key == SDLK_UP && member_selection){--member_selection;redraw=1;}
            else if (key == SDLK_DOWN && member_selection + 1 < member_count){++member_selection;redraw=1;}
            else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
                caster = members[member_selection]; target = members[0];
                count = npe_world_available_cached_skills(file, world, caster, ids);
                selection = first = 0; choosing = 0; list_visible = 0;
                redraw=1;
            }
            continue;
        }
        if (!count) continue;
        if(!targeting&&(key==SDLK_n||key==SDLK_p)){
            int result=npe_render_map_magic_description(file,font,world,ids[selection],key==SDLK_n?1:-1,pixels);
            if(result<0)return;
            if(result)draw_pixels(renderer,texture,pixels);
            continue;
        }
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            int result;
            if(npe_world_skill(file,world,ids[selection],&skill))return;
            if (!skill.raw[38] || (int16_t)world->qi[caster] < (int16_t)skill.qi_cost) {
                uint8_t restored[sizeof(pixels)];memcpy(restored,pixels,sizeof(restored));
                if (npe_render_map_magic_message(font, !skill.raw[38], pixels)) return;
                draw_pixels(renderer, texture, pixels);
                if (wait_timed_notice(2000)) return;
                memcpy(pixels,restored,sizeof(pixels));draw_pixels(renderer,texture,pixels);
                continue;
            }
            if (!targeting && skill.raw[37] != 1) { targeting = 1;redraw=1;continue; }
            result = npe_world_menu_cast_skill(file, world, caster, target, ids[selection]);
            if (result < 0) fprintf(stderr, "map magic service pending/refused: %d\n", result);
            redraw=targeting?2:1;
        } else if (targeting) {
            if (key == SDLK_UP && target > members[0]) {
                for (unsigned i = target; i-- > 0;)
                    if (world->party_active[i]) { target = i;redraw=2;break; }
            } else if (key == SDLK_DOWN) {
                for (unsigned i = target + 1; i < 3; ++i)
                    if (world->party_active[i]) { target = i;redraw=2;break; }
            }
        } else {
            if (key == SDLK_UP && selection >= 3) selection -= 3;
            else if (key == SDLK_DOWN && selection + 3 < count) selection += 3;
            else if (key == SDLK_LEFT && selection % 3) --selection;
            else if (key == SDLK_RIGHT && selection % 3 < 2 && selection + 1 < count) ++selection;
            while (selection < first) first -= 3;
            while (selection >= first + 9) first += 3;
            if(selection!=last_selection || first!=last_first)redraw=1;
        }
    }
}

static int draw_battle_animation_commands(SDL_Renderer *renderer, SDL_Texture *texture,
                                          const NpeFile *file, const NpeWorld *world,
                                          const NpeFont *font, unsigned icon);
static int draw_battle_cached_commands(SDL_Renderer *, SDL_Texture *,
    const NpeFile *, const NpeWorld *, unsigned, unsigned, unsigned, unsigned);
static int draw_battle_actor_advance(SDL_Renderer *,SDL_Texture *,
    const NpeFile *,const NpeWorld *,const NpeFont *,unsigned,unsigned);
static int draw_battle_scene(SDL_Renderer *renderer, SDL_Texture *texture,
                             const NpeFile *file, unsigned enemy_id,
                             const NpeWorld *world, const NpeFont *font,
                             unsigned icon, unsigned menu_level,
                             unsigned menu_selection, unsigned item_selection);

static int draw_native_battle_target(SDL_Renderer *renderer, SDL_Texture *texture,
                                     const NpeFile *file, const NpeFont *font,
                                     NpeWorld *world, unsigned target, unsigned previous,
                                     int party, int entering, unsigned icon, uint8_t *pixels) {
    static const uint8_t body[]={0xcc,0xe5},qi[]={0xc6,0xf8};
    /*3C:2644 reads7AC0 to distinguish the physical target's side.
      Friendly movement changes64CE while retaining7AC0. */
    party=world->battle_display_target<=world->battle_record_party_count;
    unsigned actor=world->battle_actor,member=party?target:actor;
    /*3A:0EAA..0ECE submits scene/cursor/controls/HUD/target/status.
      38:4F1A..4F46 draws item HUD before the cursor and controls. */
    if(entering){
        if(npe_render_battle_actors_scene(file,world,pixels))return -1;
        draw_pixels(renderer,texture,pixels);
        if(icon==2){
            if(npe_draw_gbk_text(font,body,sizeof(body),70,88,pixels) ||
               npe_draw_gbk_text(font,qi,sizeof(qi),70,104,pixels))return -1;
            npe_render_battle_hud_values(world->hp[member],world->max_hp[member],
                                        world->qi[member],world->max_qi[member],pixels);
            draw_pixels(renderer,texture,pixels);
        }
        if(npe_render_battle_cursor(file,world,pixels))return -1;
        draw_pixels(renderer,texture,pixels);
        if(npe_render_battle_controls(file,icon,pixels))return -1;
        draw_pixels(renderer,texture,pixels);
        if(npe_draw_gbk_text(font,body,sizeof(body),70,88,pixels) ||
           npe_draw_gbk_text(font,qi,sizeof(qi),70,104,pixels))return -1;
    }else{
        if(party){
            /*3C:25AE..25C6 clears inclusive HUD rectangle (70,88)..(134,119).
               The LCD service aligns the left edge to a four-pixel cell. */
            for(unsigned y=88;y<120;++y)memset(pixels+y*NPE_WIDTH+68,0,67);
            draw_pixels(renderer,texture,pixels);
        }
        if(party?npe_clear_battle_party_target(world,previous,pixels):
                 npe_clear_battle_enemy_target(world,previous,pixels))return -1;
        draw_pixels(renderer,texture,pixels);
        if(party && (npe_draw_gbk_text(font,body,sizeof(body),70,88,pixels) ||
                     npe_draw_gbk_text(font,qi,sizeof(qi),70,104,pixels)))return -1;
    }
    if(party || entering){
        npe_render_battle_hud_values(world->hp[member],world->max_hp[member],
                                    world->qi[member],world->max_qi[member],pixels);
        if(entering && icon!=2)draw_pixels(renderer,texture,pixels);
    }
    if(party?npe_draw_battle_party_target_pointer(file,world,target,pixels):
             npe_draw_battle_enemy_target_pointer(file,world,target,pixels))return -1;
    draw_pixels(renderer,texture,pixels);
    if(entering){
        /*3A:0ECE calls38:8296 on target-page entry even before the
          first round refresh. Use the live status words here. */
        npe_render_battle_status_refresh(world,pixels);
        draw_pixels(renderer,texture,pixels);
    }
    return 0;
}

static void show_battle_skill(SDL_Window *window, SDL_Renderer *renderer,
                              SDL_Texture *texture, const NpeFile *file,
                              const NpeFont *font, NpeWorld *world) {
    unsigned ids[18],count,actor,selection=0,first=0,target,previous=0;
    unsigned last_selection=0,last_first=0;
    int targeting=0,list_visible=0,redraw=1;
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];SDL_Event event;
    (void)window;
    if(!file || !font || !font->data || !renderer || !texture || !world ||
       world->game_mode!=3 || !world->battle_enemy_count || world->battle_actor>=3)return;
    actor=world->battle_actor;
    if(world->battle_party_status[actor][3]<0)return;
    target=actor;
    /*4E:06F4..071C releases the packed images before the magic list. */
    if(battle_resources_set(file,world,0))return;
    world->battle_menu_phase=21;
    count=npe_world_available_cached_skills(file,world,actor,ids);
    for(;;){
        if(redraw){
            if(targeting){
                unsigned shown=targeting==2?target:world->battle_target;
                if(draw_native_battle_target(renderer,texture,file,font,world,shown,previous,
                                            targeting==2,redraw==1,0,pixels))return;
                previous=shown;list_visible=0;
            }else{
                if(count){
                    if(npe_render_map_magic_list(file,font,world,actor,ids,count,first,selection,
                                                last_first,last_selection,!list_visible,pixels))return;
                    list_visible=1;last_selection=selection;last_first=first;
                }else if(npe_render_empty_magic_screen(font,pixels))return;
                draw_pixels(renderer,texture,pixels);
            }
            redraw=0;
        }
        if(!game_WaitEvent(&event))return;
        if(event.type==SDL_QUIT){SDL_PushEvent(&event);return;}
        if(event.type!=SDL_KEYDOWN)continue;
        SDL_Keycode key=native_menu_navigation_key(event.key.keysym.sym);
        if(key==SDLK_F4 || key==SDLK_BACKSPACE || key==SDLK_ESCAPE){
            if(targeting && key==SDLK_ESCAPE)continue;
            if(!targeting){
                /*36:E07A..E0BA restores them before the action buttons. */
                if(battle_resources_set(file,world,1))return;
                world->battle_menu_phase=20;
                if(world->battle_commands[actor].kind==8)(void)npe_world_battle_cancel_target(world);
                return;
            }
            /*36:E01C..E044 frees them before returning to the magic list. */
            if(battle_resources_set(file,world,0))return;
            world->battle_menu_phase=21;
            targeting=0;target=actor;list_visible=0;redraw=1;continue;
        }
        if(key==SDLK_RETURN || key==SDLK_KP_ENTER){
            NpeSkill skill;
            /*3A:07B4 also runs when the list getter returns skill0. */
            if(npe_world_skill(file,world,count?ids[selection]:0,&skill))return;
            if(!count || !skill.required_level || skill.required_level>world->level[actor])continue;
            if(!targeting && (int16_t)world->qi[actor]<(int16_t)skill.qi_cost){
                uint8_t saved[sizeof(pixels)];memcpy(saved,pixels,sizeof(saved));
                if(npe_render_battle_insufficient_qi(font,pixels))return;
                draw_pixels(renderer,texture,pixels);
                if(wait_timed_notice(2000))return;
                memcpy(pixels,saved,sizeof(saved));draw_pixels(renderer,texture,pixels);
                continue;
            }
            if(!npe_skill_battle_effect(&skill)){
                fprintf(stderr,"native magic effect not implemented: skill %u type %u\n",ids[selection],skill.raw[14]);continue;
            }
            if(!targeting){
                if(npe_world_battle_select_magic(file,world,ids[selection])<0)continue;
                /*3A:0CFE..0D3E runs once after a valid list confirmation. */
                if(battle_resources_set(file,world,1))return;
                if(!skill.raw[37]){
                    if(!skill.raw[38]){
                        world->battle_target=world->battle_enemy_count-1;
                        npe_world_battle_cycle_target(world,1);targeting=1;redraw=1;world->battle_menu_phase=27;
                        (void)npe_world_battle_select_target(world,world->battle_target,0);continue;
                    }
                    unsigned active=0;for(unsigned i=0;i<3;++i)active+=world->party_active[i]!=0;
                    if(active>1){targeting=2;redraw=1;world->battle_menu_phase=27;continue;}
                }
            }
            int result=npe_world_battle_cast_magic(file,world,ids[selection],
                         skill.raw[38]?target:world->battle_target);
            if(result<0){fprintf(stderr,"native magic refused: %d\n",result);continue;}
            return;
        }
        if(!count)continue;
        if(targeting==1){
            unsigned old=world->battle_target;
            if(key==SDLK_UP || key==SDLK_DOWN)npe_world_battle_cycle_target(world,key==SDLK_UP?-1:1);
            if(world->battle_target!=old){(void)npe_world_battle_select_target(world,world->battle_target,0);redraw=2;}
        }else if(targeting==2){
            unsigned old=target;int direction=key==SDLK_UP?1:key==SDLK_DOWN?-1:0;
            if(direction)for(unsigned step=0;step<3;++step){
                target=(unsigned)((int)target+3+direction)%3;if(world->party_active[target])break;
            }
            if(target!=old){(void)npe_world_battle_select_target(world,target,1);redraw=2;}
        }else{
            if(key==SDLK_UP && selection>=3)selection-=3;
            else if(key==SDLK_DOWN && selection+3<count)selection+=3;
            else if(key==SDLK_LEFT && selection%3)--selection;
            else if(key==SDLK_RIGHT && selection%3<2 && selection+1<count)++selection;
            while(selection<first)first-=3;
            while(selection>=first+9)first+=3;
            if(selection!=last_selection || first!=last_first)redraw=1;
        }
    }
}

static int draw_battle_entry(SDL_Renderer *renderer,SDL_Texture *texture,
                             const NpeFile *file,const NpeWorld *world,
                             const NpeFont *font) {
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
    /* Original ordinary4E:7F8C..7FB0 and scripted4E:8BDC..8BFE
       commit four stages; the renderer preserves their separate order. */
    for(unsigned stage=0;stage<4;++stage){
        if(npe_render_battle_entry_frame(file,font,world,stage,pixels))return -1;
        draw_pixels(renderer,texture,pixels);
        retain_battle_entry(world,renderer,stage,pixels);
    }
    return 0;
}

typedef struct {
    const NpeFile *file;NpeWorld *world;SDL_Renderer *renderer;SDL_Texture *texture;
} ObjectDisplay;
static int commit_object_pickup(void *opaque,const uint8_t *pixels,unsigned wait_parameter) {
    ObjectDisplay *display=opaque;(void)wait_parameter;
    draw_pixels(display->renderer,display->texture,pixels);
    retain_world_frame(display->file,display->world,display->renderer,pixels,1);
    return 0;
}

static int show_facing_action(SDL_Window *window, SDL_Renderer *renderer,
                               SDL_Texture *texture, const NpeFile *file,
                               const NpeFont *font, NpeWorld *world,
                               int talk) {
    unsigned npc_id;
    NpeMapObject object;
    int status;
    int found = talk ? npe_world_facing_npc(file, world, &npc_id) : 0;
    if (found < 0) return found;
    if (found == 1) {
        if(npe_world_begin_talk(file,world)!=1)return -1;
        status=show_dialogue_pages(window,renderer,texture,file,world,font,0);
        if(status)return status;
        status=npe_world_finish_mode2(world);
        return status<0?status:1;
    }
    if (talk) return 0;
    ObjectDisplay display={file,world,renderer,texture};
    status = npe_world_object_action_with_display(file,world,&object,commit_object_pickup,&display);
    if (status <= 0) {
        if (status < 0) fprintf(stderr, "object action unsupported (%d)\n", status);
        return status;
    }
    {
        uint8_t base[NPE_WIDTH*NPE_HEIGHT],pixels[sizeof(base)];
        if(last_pixel_renderer!=renderer)return -1;
        memcpy(base,last_pixel_frame,sizeof(base));memcpy(pixels,base,sizeof(pixels));
        /*37:1506 submits the changed map before inventory increments.
          Preserve that plane after the new inventory state is committed. */
        NpeItem item;
        if(npe_world_item(file,world,object.item_index,&item)||
           npe_render_cached_item_notice(font,&item,1,1,pixels))return -1;
        retain_world_frame(file,world,renderer,base,1);
        draw_pixels(renderer,texture,pixels);
        if(wait_timed_notice(5000))return -2;
        draw_pixels(renderer,texture,base);
        retain_world_frame(file,world,renderer,base,1);
    }
    return 1;
}

static void show_walk_story(SDL_Window *window, SDL_Renderer *renderer,
                            SDL_Texture *texture, const NpeFile *file,
                            const NpeFont *font, NpeWorld *world) {
    NpeStoryRecord story;
    (void)window;
    while (world->game_mode == 5) {
        unsigned event_id = world->story_event_id;
        int status;
        if (!event_id || npe_story_record(file, event_id - 1, &story)) {
            fprintf(stderr, "story %u is unsupported\n", event_id);
            return;
        }
        if(npe_world_prepare_story(file,world)!=1)return;
        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
        if((story.header_value&255u)!=1){
            if(world->event_blank_background)memset(pixels,0,sizeof(pixels));
            else if(last_pixel_renderer==renderer)memcpy(pixels,last_pixel_frame,sizeof(pixels));
            else if(npe_render_world_frame(file,world,0,pixels))return;
        }
        if(!file->rpg_runtime&&npe_world_resources_open((NpeFile *)file,world))return;
        if(npe_rpg_runtime_owner(file)!=world||npe_rpg_runtime_text_prefix(file))return;
        for(;;){
            if(npe_render_world_story_page(file,font,world,pixels)){
                fprintf(stderr,"native story text service failed\n");return;
            }
            draw_pixels(renderer,texture,pixels);
            /*43:6914 submits the page, then6920 consumes7AB4 before
              waiting for its next key. The final return requests it again. */
            world->map_init_pending=0;
            SDL_Event event;int advanced=0;
            while(game_WaitEvent(&event)){
                if(event.type==SDL_QUIT){SDL_PushEvent(&event);return;}
                /*43:6928..693C accepts only ASCII/native Space (20/90). */
                if(event.type==SDL_KEYDOWN&&event.key.keysym.sym==SDLK_SPACE){
                    advanced=1;break;
                }
            }
            if(!advanced)return;
            if(world->text_more!=1)break;
        }
        /*43:6956..695E sets7AB4 before restoring the saved followup.
          Full-screen and window stories share this return service. */
        world->map_init_pending=1;
        status=npe_world_finish_story(file,world);
        if(status<=0){
            if(status<0)fprintf(stderr,"story %u followup unsupported (%d)\n",event_id,status);
            return;
        }
    }
}

static unsigned quiz_sdl_key(const SDL_KeyboardEvent *event) {
    SDL_Keycode key=event->keysym.sym;
    switch(key) {
    case SDLK_LEFT:return 0x80;case SDLK_RIGHT:return 0x81;
    case SDLK_UP:return 0x82;case SDLK_DOWN:return 0x83;
    case SDLK_RETURN:case SDLK_KP_ENTER:return 0x86;
    case SDLK_BACKSPACE:case SDLK_DELETE:return 0x89;
    case SDLK_F4:return 0x8e;
    default:break;
    }
    if(key>='a'&&key<='z') {
        int upper=!!(event->keysym.mod&KMOD_SHIFT)^!!(event->keysym.mod&KMOD_CAPS);
        return (unsigned)(upper?key-'a'+'A':key);
    }
    if(event->keysym.mod&KMOD_SHIFT) {
        if(key==SDLK_9)return '(';
        if(key==SDLK_0)return ')';
        if(key==SDLK_8)return '*';
        if(key==SDLK_EQUALS)return '+';
        /* Other shifted punctuation is outside the original input set. */
        return 0;
    }
    return key>=0x20&&key<=0x7e?(unsigned)key:0;
}
static int quiz_notice(SDL_Renderer *renderer,SDL_Texture *texture,
                        const NpeFont *font,const uint8_t *message,size_t length,
                        const uint8_t *parent) {
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];memcpy(pixels,parent,sizeof(pixels));
    if(npe_render_timed_notice(font,message,length,pixels))return -1;
    draw_pixels(renderer,texture,pixels);
    if(wait_timed_notice(800)==-2)return -2;
    draw_pixels(renderer,texture,parent);return 0;
}
static int quiz_reward_notice(SDL_Renderer *renderer,SDL_Texture *texture,
                               const NpeFont *font,uint16_t award,const uint8_t *parent) {
    static const uint8_t reward[]={0xbd,0xb1,0xc0,0xf8,0xbd,0xf0,0xc7,0xae,0x20};
    uint8_t message[24];char number[8];
    int digits=snprintf(number,sizeof(number),"%u",award);
    memcpy(message,reward,sizeof(reward));memcpy(message+sizeof(reward),number,(size_t)digits);
    return quiz_notice(renderer,texture,font,message,sizeof(reward)+(size_t)digits,parent);
}
static void show_quiz_question(SDL_Window *window, SDL_Renderer *renderer,
                               SDL_Texture *texture, const NpeFile *file,
                               const NpeFont *font, NpeWorld *world) {
    static const uint8_t correct[]={0xb9,0xa7,0xcf,0xb2,0xc4,0xfa,0xa3,0xac,0xbb,0xd8,0xb4,0xf0,0xd5,0xfd,0xc8,0xb7,0xa3,0xa1};
    static const uint8_t wrong[]={0xbb,0xd8,0xb4,0xf0,0xb4,0xed,0xce,0xf3,0xa3,0xac,0xd2,0xaa,0xc5,0xac,0xc1,0xa6,0xc5,0xb6,0xa3,0xa1};
    static const uint8_t exhausted[]={0xc4,0xfa,0xb4,0xf0,0xb4,0xed,0xcc,0xab,0xb6,0xe0,0xa3,0xac,0xc7,0xeb,0xcc,0xe1,0xb8,0xdf,0x49,0x51,0xd4,0xd9,0xc0,0xb4,0xa3,0xa1};
    static const uint8_t timeout[]={0xb4,0xf0,0xcc,0xe2,0xca,0xb1,0xbc,0xe4,0xb5,0xbd,0xa3,0xac,0xb2,0xbb,0xbb,0xd8,0xb4,0xf0,0xcb,0xe3,0xb4,0xed,0xce,0xf3};
    (void)window;
    while(world->game_mode==8) {
        NpeQuizQuestion question;uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
        if(npe_world_prepare_question(file,world,&question) ||
           npe_render_quiz_page(file,font,world,pixels)) {
            fprintf(stderr,"cannot read or render NPE quiz service\n");return;
        }
        draw_pixels(renderer,texture,pixels);
        unsigned periods=0;Uint32 deadline=SDL_GetTicks()+4000;int action=0;
        while(action<2) {
            Uint32 now=SDL_GetTicks();
            if(SDL_TICKS_PASSED(now,deadline)) {
                deadline+=4000;
                if(++periods>7) {
                    world->event_context_id=5;world->quiz_input[0]=0;
                    if(quiz_notice(renderer,texture,font,timeout,sizeof(timeout),pixels))return;
                    action=2;break;
                }
            }
            SDL_Event input;
            now=SDL_GetTicks();
            int remaining=SDL_TICKS_PASSED(now,deadline)?0:(int)(deadline-now);
            if(!game_WaitEventTimeout(&input,remaining))continue;
            if(input.type==SDL_QUIT){SDL_PushEvent(&input);return;}
            if(input.type!=SDL_KEYDOWN)continue;
            if(input.key.keysym.sym==SDLK_ESCAPE){SDL_Event quit={0};quit.type=SDL_QUIT;SDL_PushEvent(&quit);return;}
            action=npe_world_quiz_key(file,world,quiz_sdl_key(&input.key));
            if(action<0)return;
            if(action==1) {
                if(npe_render_quiz_page(file,font,world,pixels))return;
                draw_pixels(renderer,texture,pixels);
            }
        }
        if(action==3) {
            NpeWorld after=*world;uint16_t award;
            if(npe_world_finish_quiz(&after,&award))return;
            world->money=after.money;
            if(quiz_reward_notice(renderer,texture,font,award,pixels))return;
            *world=after;
        } else {
            NpeWorld after=*world;
            /* Compute the candidate before displaying feedback. Playback
               belongs to the map controller after that feedback returns. */
            after.sound_observer=NULL;
            int result=question.input_type?npe_world_answer_text(file,&after):
                npe_world_answer_question(file,&after,world->event_context_id);
            after.sound_observer=world->sound_observer;
            if(result<0)return;
            if(world->quiz_category<10) {
                unsigned category=world->quiz_category;
                world->quiz_category_correct[category]=after.quiz_category_correct[category];
                world->quiz_category_wrong[category]=after.quiz_category_wrong[category];
                if(quiz_notice(renderer,texture,font,result?correct:wrong,
                               result?sizeof(correct):sizeof(wrong),pixels))return;
                ++world->quiz_answer_count;if(!result)++world->quiz_wrong_count;
                if(!result&&world->quiz_wrong_count>=3) {
                    if(quiz_notice(renderer,texture,font,exhausted,sizeof(exhausted),pixels))return;
                    uint16_t score=(uint16_t)(world->quiz_answer_count-2u*world->quiz_wrong_count);
                    uint16_t award=(uint16_t)(score*(category+3u));world->money=after.money;
                    if(quiz_reward_notice(renderer,texture,font,award,pixels))return;
                }
            }
            *world=after;
        }
        if(world->game_mode==1) {
            /*37:1340 /4E:A3F4 reissues the retained map slot, including
              when the same track was already playing during the quiz. */
            npe_world_sound_map_resume(world);
            if(npe_render_world_load(file,world,pixels))return;
            /* The controller supplies the final map commit. */
            retain_world_frame(file,world,renderer,pixels,0);
        }
    }
}

static void draw_pixels(SDL_Renderer *renderer, SDL_Texture *texture,
                        const uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    shop_frame_valid=0;
    world_map_frame_valid=0;
    world_map_frame_uncommitted=0;
    world_map_frame_submit_pending=0;
    system_menu_frame_valid=0;
    uint32_t rgba[NPE_WIDTH * NPE_HEIGHT];
    unsigned i;
    /* Approximate green LCD ramp; exact LCD PWM is not emulated. */
    for (i = 0; i < NPE_WIDTH * NPE_HEIGHT; ++i) {
        uint32_t value = pixels[i];
        uint32_t red = 183 - value * 142 / 15;
        uint32_t green = 200 - value * 146 / 15;
        uint32_t blue = 161 - value * 119 / 15;
        rgba[i] = 0xff000000u | red << 16 | green << 8 | blue;
    }
    if(pixels!=last_pixel_frame)memcpy(last_pixel_frame,pixels,sizeof(last_pixel_frame));
    last_pixel_renderer=renderer;
    if(game_view_present(renderer,pixels))return;
    SDL_RenderSetLogicalSize(renderer,NPE_WIDTH,NPE_HEIGHT);
    SDL_UpdateTexture(texture, NULL, rgba, NPE_WIDTH * (int)sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    if(pixels!=last_pixel_frame)memcpy(last_pixel_frame,pixels,sizeof(last_pixel_frame));
    last_pixel_renderer=renderer;
    SDL_RenderPresent(renderer);
}
static void draw_bitmap(SDL_Renderer *renderer, SDL_Texture *texture,
                        const NpeFile *file, unsigned section, unsigned index) {
    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT];
    if (!npe_bitmap(file, section, index, pixels))
        draw_pixels(renderer, texture, pixels);
}
static int draw_animation(SDL_Renderer *renderer, SDL_Texture *texture,
                          const NpeFile *file, unsigned group, unsigned frame) {
    NpeIndexedBitmap bitmap;
    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT] = {0};
    unsigned x, y;
    if (npe_animation_frame(file, group, frame, &bitmap)) return -1;
    for (y = 0; y < bitmap.height; ++y)
        for (x = 0; x < bitmap.width; ++x)
            pixels[(bitmap.y + y) * NPE_WIDTH + bitmap.x + x] =
                npe_indexed_pixel(&bitmap, x, y);
    draw_pixels(renderer, texture, pixels);
    return 0;
}

static void move_battle_list_row(NpeWorld *world,int item,int down) {
    /*47:8B74..8BF0 /49:A690..A70C use byte increment/decrement,
      with wrap only at the named endpoint; item rows toggle zero/nonzero. */
    uint8_t *row=item?&world->battle_item_row:&world->battle_menu_row;
    if(item)*row=(uint8_t)(!*row);
    else if(down)*row=*row==4?0:(uint8_t)(*row+1);
    else *row=*row==0?4:(uint8_t)(*row-1);
}

static int battle_menu_icon(const NpeWorld *world) {
    switch(world->battle_menu_phase/10u){
    case 1:return 1;case 2:return 0;case 3:return 2;case 4:return 3;
    default:return -1;
    }
}
static unsigned battle_menu_level(const NpeWorld *world) {
    return world->battle_menu_phase==31?1u:world->battle_menu_phase==32?2u:0u;
}
static uint8_t battle_phase_for_icon(unsigned icon) {
    static const uint8_t phases[]={20,10,30,40};return phases[icon];
}
/* Native key masks. Return1 for navigation,2 for the two-stage flag
   return,3 for a consumed key without display,0 for an action selector. */
static int battle_menu_navigation(NpeWorld *world,unsigned key) {
    unsigned phase=world->battle_menu_phase;
    if(phase==31 || phase==32){
        if(key==1 || key==2){move_battle_list_row(world,phase==32,key==2);return 1;}
        if(key==64){world->battle_menu_phase=(uint8_t)(phase==32?31:30);return phase==32?1:2;}
        if(key==16){
            if(phase==31 && world->battle_menu_row==1){world->battle_menu_phase=32;return 1;}
            return 0;
        }
        return key?3:0;
    }
    if(phase!=10 && phase!=20 && phase!=30 && phase!=40)return 3;
    if(key==1 || key==2 || key==4 || key==8){
        unsigned direction=key==1?0:key==8?1:key==2?2:3;
        int icon=npe_world_battle_move_icon(world,(unsigned)battle_menu_icon(world),direction);
        if(icon<0 || battle_phase_for_icon((unsigned)icon)==phase)return 3;
        world->battle_menu_phase=battle_phase_for_icon((unsigned)icon);return 1;
    }
    if(key==16 && phase==30){world->battle_menu_phase=31;return 1;}
    return 0;
}

static int draw_battle_command_return_plane(SDL_Renderer *renderer,SDL_Texture *texture,
    const NpeFile *file,const NpeFont *font,const NpeWorld *world,unsigned icon,unsigned stage,
    const uint8_t *button_plane) {
    static const uint8_t body[]={0xcc,0xe5},qi[]={0xc6,0xf8};
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
    if(!world||stage>1||last_pixel_renderer!=renderer)return -1;
    memcpy(pixels,last_pixel_frame,sizeof(pixels));
    if(!stage){
        uint8_t actors[NPE_WIDTH*NPE_HEIGHT];
        const uint8_t *retained=retained_battle_actors(world,renderer);
        if(retained)memcpy(actors,retained,sizeof(actors));
        else if(npe_render_battle_actors_scene(file,world,actors))return -1;
        /*36:E47C..E498 restores the inclusive (38,10)..(86,103)
          menu rectangle from plane1; the LCD aligns its left edge to36.
          Controls are displayed before3C:2548 restores the overlapping HUD. */
        for(unsigned y=10;y<=103;++y)memcpy(pixels+y*NPE_WIDTH+36,actors+y*NPE_WIDTH+36,51);
        if(button_plane){
            for(unsigned y=88;y<NPE_HEIGHT;++y)
                memcpy(pixels+y*NPE_WIDTH,button_plane+y*NPE_WIDTH,68);
        }else if(npe_render_battle_controls(file,icon,pixels))return -1;
    }else{
        /*3C:25AE..25C6 clears the inclusive HUD rectangle before
          writing counters of the newly selected actor. */
        for(unsigned y=88;y<NPE_HEIGHT;++y)memset(pixels+y*NPE_WIDTH+68,0,67);
        if(!font||npe_draw_gbk_text(font,body,sizeof(body),70,88,pixels)||
           npe_draw_gbk_text(font,qi,sizeof(qi),70,104,pixels))return -1;
        unsigned actor=world->battle_actor;
        npe_render_battle_hud_values(world->hp[actor],world->max_hp[actor],
            world->qi[actor],world->max_qi[actor],pixels);
    }
    draw_pixels(renderer,texture,pixels);return 0;
}
static int draw_battle_command_return_stage(SDL_Renderer *renderer,SDL_Texture *texture,
    const NpeFile *file,const NpeFont *font,const NpeWorld *world,unsigned icon,unsigned stage) {
    return draw_battle_command_return_plane(renderer,texture,file,font,world,icon,stage,NULL);
}
static int draw_battle_asset(SDL_Renderer *renderer, SDL_Texture *texture,
                             const NpeFile *file, unsigned index) {
    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT] = {0};
    if (npe_rpg_runtime_draw(file,(uint16_t)(index+1),0,0,0,pixels)) return -1;
    draw_pixels(renderer, texture, pixels);
    return 0;
}
static int draw_map(SDL_Renderer *renderer, SDL_Texture *texture,
                    const NpeFile *file, const NpeMap *map,
                    unsigned left, unsigned top) {
    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT];
    if (npe_render_map(file, map, left, top, pixels)) return -1;
    draw_pixels(renderer, texture, pixels);
    return 0;
}
static void submit_pending_world_frame(const NpeFile *file,SDL_Renderer *renderer,SDL_Texture *texture){
    if(!world_map_frame_submit_pending||!world_map_frame_valid||world_map_frame_file!=file||world_map_frame_renderer!=renderer)return;
    NpeWorld retained=world_map_frame_state;
    draw_pixels(renderer,texture,world_map_frame);
    retain_world_frame(file,&retained,renderer,world_map_frame,0);
}
static int draw_world(SDL_Renderer *renderer, SDL_Texture *texture,
                      const NpeFile *file, NpeWorld *world,
                      unsigned system_menu_selection,
                      unsigned status_member, int system_submenu_open,
                      unsigned system_submenu_selection,
                      int equipment_page_open,
                      int save_slots_open, const unsigned save_slot_levels[3],
                      unsigned save_slot_selection,
                      const NpeFont *font) {
    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT];
    uint8_t menu_background[NPE_WIDTH * NPE_HEIGHT];
    int initialize_menu=world->game_mode==6 && world->map_init_pending;
    if(initialize_menu){
        /*4B:B5A8..B5C6 calls49:0000 with the entry flag before clearing7AB4.
          Resource-triggered menus need the same setup as the F4 entry. */
        world->lcd_background=0;
        world->property_page=world->property_member=world->equipment_view_slot=0;
        world->menu_party_count=(uint8_t)(1+(world->party_active[1]==1)+(world->party_active[2]==1));
        world->system_menu_stage=1;
        const uint8_t row=1,member=0;
        if(file->rpg_runtime && (npe_rpg_runtime_write(file,0x527ae2,&row,1) ||
            npe_rpg_runtime_write(file,0x527a78,&member,1)))return -1;
        system_menu_selection=status_member=system_submenu_selection=save_slot_selection=0;
        system_submenu_open=equipment_page_open=save_slots_open=0;
        system_menu_frame_valid=system_menu_background_valid=0;
    }
    if(world->game_mode==4 && world->story_event_id!=2){
        if(world->map_init_pending){
            shop_frame_valid=0;
            int result=npe_world_shop_mode_begin(file,world);
            if(result<0)return -1;
            if(!result && last_pixel_renderer==renderer)
                retain_shop_frame(file,world,renderer,last_pixel_frame);
        }
        /*22:B740 returns without an LCD service. Do not replace the retained
          display with a member screen or an invented empty shop. */
        if((int16_t)world->event_context_id<10)return 0;
    }
    NpeWorld frame_state=*world;
    if(world->game_mode==6)submit_pending_world_frame(file,renderer,texture);
    unsigned menu_controls[10]={system_menu_selection,status_member,
        (unsigned)system_submenu_open,system_submenu_selection,
        (unsigned)equipment_page_open,(unsigned)save_slots_open,
        save_slot_selection,save_slot_levels[0],save_slot_levels[1],save_slot_levels[2]};
    if(world->game_mode==17){
        if(!world->map_init_pending)return 0;
        if(last_pixel_renderer!=renderer)return -1;
        memcpy(pixels,last_pixel_frame,sizeof(pixels));
        memcpy(story_choice_background,pixels,sizeof(pixels));
        story_choice_background_file=file;story_choice_background_renderer=renderer;
        story_choice_background_valid=1;
        if(npe_render_choice_event(file,pixels))return -1;
        world->event_context_id=0;world->map_init_pending=0;
        draw_pixels(renderer,texture,pixels);return 0;
    }
    /* The synchronous quiz service owns its LCD and input loop. */
    if(world->game_mode==8 && world->quiz_ready)return 0;
    if(world->game_mode==6 && system_menu_frame_valid &&
       system_menu_background_file==file && system_menu_background_renderer==renderer &&
       same_display_world(&system_menu_frame_state,world) &&
       !memcmp(system_menu_frame_controls,menu_controls,sizeof(menu_controls)))return 0;
    if(world->game_mode!=6)system_menu_background_valid=0;
    if(world->game_mode==6 && (world->property_page==9 || world->property_page==10)){
        const NpeWorld *before=NULL;
        if(system_menu_frame_valid &&
           system_menu_background_file==file && system_menu_background_renderer==renderer &&
           system_menu_frame_state.game_mode==6 && system_menu_frame_state.property_page==world->property_page){
            before=&system_menu_frame_state;
            memcpy(pixels,last_pixel_frame,sizeof(pixels));
        }
        int result=world->property_page==9 ?
            npe_render_native_member_view(file,font,world,before,pixels) :
            npe_render_equipment_view(file,font,world,before,pixels);
        if(result)return -1;
        draw_pixels(renderer,texture,pixels);
        system_menu_frame_state=*world;
        memcpy(system_menu_frame_controls,menu_controls,sizeof(menu_controls));
        system_menu_frame_valid=1;
        return 0;
    }
    int resume_exit_choice=world->game_mode==1 && world->exit_choice &&
                           world_map_frame_state.game_mode==6;
    /* The system menu restores its retained background. With7AC4 still
       positive that background is the exit prompt (or its later map LCD),
       not a newly rendered map. Only the menu mode may have changed. */
    if(resume_exit_choice)frame_state.game_mode=6;
    if ((world->game_mode == 1 || world->game_mode == 6) && world_map_frame_valid &&
        world_map_frame_renderer == renderer && world_map_frame_file == file &&
        same_display_world(&world_map_frame_state,&frame_state)) {
        /* Source37:18CE draws chase into retained LCD without committing.
           Keep the last walking commit visible until the next native draw. */
        if(world->game_mode==1 && world_map_frame_uncommitted && !resume_exit_choice &&
           !system_submenu_open && !save_slots_open)
            return 0;
        memcpy(pixels,world_map_frame,sizeof(pixels));
        if(resume_exit_choice){world_map_frame_state=*world;world_map_frame_uncommitted=1;}
    }
    else if(world->game_mode==6 && system_menu_background_valid &&
            system_menu_background_file==file && system_menu_background_renderer==renderer)
        memcpy(pixels,system_menu_background,sizeof(pixels));
    else if(world->game_mode==6 && last_pixel_renderer==renderer)
        memcpy(pixels,last_pixel_frame,sizeof(pixels));
    else if (npe_render_world(file, world, pixels)) return -1;
    if(world->game_mode==6){
        if(!system_menu_background_valid || system_menu_background_file!=file ||
           system_menu_background_renderer!=renderer){
            memcpy(system_menu_background,pixels,sizeof(pixels));
            system_menu_background_file=file;system_menu_background_renderer=renderer;
            system_menu_background_valid=1;
        }
        memcpy(menu_background,pixels,sizeof(pixels));
        if(!world_map_frame_valid || world_map_frame_file!=file ||
           world_map_frame_renderer!=renderer || world_map_frame_state.game_mode!=6)
            save_map_edges(file,renderer,pixels);
    }
    if(world->game_mode==4 && world->shop_ready){
        if(shop_frame_valid && shop_frame_file==file && shop_frame_renderer==renderer &&
           same_display_world(&shop_frame_state,world))return 0;
        if(shop_frame_valid && shop_frame_file==file && shop_frame_renderer==renderer &&
           shop_frame_state.shop_ui==1 && world->shop_ui==1 &&
           shop_frame_state.event_context_id==world->event_context_id &&
           shop_frame_state.shop_ready==world->shop_ready &&
           shop_frame_state.shop_transaction_sell==world->shop_transaction_sell &&
           shop_frame_state.shop_selected_item!=world->shop_selected_item){
            memcpy(pixels,shop_draw_frame,sizeof(pixels));
            if(shop_frame_state.shop_first_item==world->shop_first_item &&
               shop_frame_state.shop_last_item==world->shop_last_item){
                if(npe_render_shop_cursor(file,font,world,shop_frame_state.shop_row,pixels))return -1;
            }else if(npe_render_shop_page(file,font,world,pixels))return -1;
        }else if(npe_render_shop_list(file,font,world,pixels))return -1;
        /*3F:DC18/3D:EEAA clears the retained plane before drawing the
          list border. Map initialization restores its cleared edge rows,
          including beneath the visible border, rather than the list LCD. */
        memset(world_map_saved_edges,0,sizeof(world_map_saved_edges));
        world_map_saved_edges_file=file;world_map_saved_edges_renderer=renderer;
        world_map_saved_edges_valid=1;
        draw_pixels(renderer,texture,pixels);retain_shop_frame(file,world,renderer,pixels);return 0;
    }
    if (world->game_mode == 4 && equipment_page_open) {
        if (npe_render_native_equipment_screen(file, world, status_member,
                                               pixels)) return -1;
    } else if (world->game_mode == 4) {
        if (!font || npe_render_native_member_screen(file,font,world,status_member,pixels))return -1;
    }
    npe_render_system_menu_overlay(file, world, system_menu_selection, pixels);
    if(world->game_mode==6 && npe_render_native_menu_money(file,font,world,pixels))return -1;
    if (system_submenu_open) {
        if (system_menu_selection == 0)
            npe_render_property_submenu_overlay(file, world,
                                                system_submenu_selection,
                                                pixels);
        else if (system_menu_selection == 2)
            npe_render_item_submenu_overlay(file, world,
                                            system_submenu_selection,
                                            pixels);
        else
            npe_render_system_submenu_overlay(file, world,
                                              system_submenu_selection,
                                              pixels);
    }
    if (save_slots_open &&
        npe_render_save_slots_overlay(file, 105, 46, save_slot_levels,
                                      save_slot_selection, NULL, pixels)) return -1;
    draw_pixels(renderer, texture, pixels);
    if(initialize_menu)world->map_init_pending=0;
    if(world->game_mode==6){
        system_menu_frame_state=*world;
        memcpy(system_menu_frame_controls,menu_controls,sizeof(menu_controls));
        system_menu_frame_valid=1;
    }
    if(world->game_mode==1 || world->game_mode==6){
        /* Direction input in49:0000/36:337A/36:47A2 only changes menu
           rectangles; preserve the entry background across those commits. */
        memcpy(world_map_frame,world->game_mode==6 ? menu_background : pixels,sizeof(pixels));
        world_map_frame_state=*world;
        world_map_frame_file=file;world_map_frame_renderer=renderer;world_map_frame_valid=1;
    }
    return 0;
}
static int game_shop_return_display(SDL_Renderer *renderer,SDL_Texture *texture,
                                    const NpeFile *file,NpeWorld *world){
    int result=npe_world_shop_return(world);
    if(result<0)return -1;
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
    if(!result){
        if(last_pixel_renderer!=renderer)return -1;
        memcpy(pixels,last_pixel_frame,sizeof(pixels));
        if(shop_frame_valid && shop_frame_file==file && shop_frame_renderer==renderer)
            for(unsigned y=4;y<=111;++y)
                memcpy(pixels+y*NPE_WIDTH+29,shop_draw_frame+y*NPE_WIDTH+29,183);
        draw_pixels(renderer,texture,pixels);retain_shop_frame(file,world,renderer,pixels);
        return 0;
    }
    /*37:1340..1366 restores plane1 and redraws player/NPC slots. The
      graphics background left by the common initializer clears its edges. */
    if(npe_render_world_load(file,world,pixels))return -1;
    save_map_edges(file,renderer,pixels);npe_world_sound_map_resume(world);
    world->map_init_pending=0;
    draw_pixels(renderer,texture,pixels);retain_world_frame(file,world,renderer,pixels,1);
    return 1;
}
static int game_shop_move_display(SDL_Renderer *renderer,SDL_Texture *texture,
                                  const NpeFile *file,const NpeFont *font,
                                  NpeWorld *world,int direction){
    unsigned old_row=world->shop_row;
    int moved=npe_world_shop_move(file,world,direction);
    while(moved==NPE_SHOP_MOVE_PENDING){
        SDL_Event event;
        /* The native page call has not returned. Ordinary menu input cannot
           close the shop or start another operation until backfill finishes. */
        if(game_WaitEventTimeout(&event,10) && event.type==SDL_QUIT)return -2;
        moved=npe_world_shop_resume(file,world);
    }
    if(moved<0)return -1;
    if(!moved){retain_shop_frame(file,world,renderer,NULL);return 0;}
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
    memcpy(pixels,shop_frame_valid && shop_frame_file==file && shop_frame_renderer==renderer ?
                  shop_draw_frame:last_pixel_frame,sizeof(pixels));
    /* Select the native handler's actual branch. RAM edits can change page
       bounds without redrawing LCD, so cached-bound comparison is insufficient. */
    if(world->shop_navigation_page){
        if(npe_render_shop_page(file,font,world,pixels))return -1;
    }else if(npe_render_shop_cursor(file,font,world,old_row,pixels))return -1;
    draw_pixels(renderer,texture,pixels);retain_shop_frame(file,world,renderer,pixels);
    return 1;
}
static int show_music_choice(SDL_Renderer *renderer, SDL_Texture *texture,
                              const NpeFile *file, NpeWorld *world) {
    uint8_t base[NPE_WIDTH*NPE_HEIGHT], pixels[NPE_WIDTH*NPE_HEIGHT];
    unsigned selection;
    if (!world || world->game_mode != 6) return -1;
    if (last_pixel_renderer == renderer)
        memcpy(base, last_pixel_frame, sizeof(base));
    else {
        if (npe_render_world(file, world, base)) return -1;
        npe_render_system_menu_overlay(file, world, 3, base);
        npe_render_system_submenu_overlay(file, world, 2, base);
    }
    /* Source37:CF66..CF9A initially selects the opposite setting. */
    if(npe_world_music_controller(world,0)!=1)return -1;
    selection = world->music_choice==1?0:1;
    for (;;) {
        SDL_Event event;
        memcpy(pixels, base, sizeof(pixels));
        if (npe_render_music_menu_overlay(file, selection, pixels)) return -1;
        draw_pixels(renderer, texture, pixels);
        for (;;) {
            if (!game_WaitEvent(&event) || event.type == SDL_QUIT) return -1;
            if (event.type != SDL_KEYDOWN) continue;
            SDL_Keycode key = native_menu_navigation_key(event.key.keysym.sym);
            if (key == SDLK_UP || key == SDLK_DOWN) {
                if(npe_world_music_controller(world,3)!=1)return -1;
                selection=world->music_choice==1?0:1;break;
            }
            if (key == SDLK_F4 || key == SDLK_ESCAPE) {
                /* Original menu input leaves all mode-6 levels for the map.
                   The separate cancel input and left/right are ignored here. */
                return npe_world_close_system_menu(world) == 1 ? 0 : -1;
            }
            if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
                if(npe_world_music_controller(world,1)!=1)return -1;
                /* Source37:D010..D024 sets map initialization after the
                   direct setting request. Source37:1312..1366 requests
                   the retained map track once more when music is on. */
                if(npe_world_music_controller(world,2)!=1)return -1;
                return 1;
            }
        }
    }
}

static int show_shop_quantity(SDL_Renderer *renderer,SDL_Texture *texture,
                               const NpeFile *file,const NpeFont *font,NpeWorld *world) {
    uint8_t base[NPE_WIDTH*NPE_HEIGHT],pixels[sizeof(base)];
    if(!world->shop_list_ready && npe_world_shop_begin(file,world))return -1;
    if(last_pixel_renderer!=renderer)return -1;
    if(world->shop_selected_item==0xffff)return 0;
    unsigned item_id=world->shop_selected_item;
    memcpy(base,shop_frame_valid && shop_frame_file==file && shop_frame_renderer==renderer?
                shop_draw_frame:last_pixel_frame,sizeof(base));
    world->shop_ui=2;world->shop_quantity=1;world->shop_choice=0;world->shop_description_start=0;
    memcpy(pixels,base,sizeof(pixels));
    if(npe_render_shop_quantity(file,font,world,pixels))return -1;
    draw_pixels(renderer,texture,pixels);
    for(;;){SDL_Event event={0};
            if(!game_WaitEvent(&event) || event.type==SDL_QUIT){if(event.type==SDL_QUIT)SDL_PushEvent(&event);return -1;}
            if(event.type!=SDL_KEYDOWN)continue;
            SDL_Keycode key=event.key.keysym.sym;
            if(key==SDLK_UP || key==SDLK_DOWN || key==SDLK_i || key==SDLK_m){
                if(npe_world_shop_quantity(world,item_id,key==SDLK_UP||key==SDLK_i?1:-1)||
                   npe_render_shop_quantity_counter(font,world,pixels))return -1;
                draw_pixels(renderer,texture,pixels);continue;
            }
            if(key==SDLK_j || key==SDLK_l){
                world->shop_choice=(uint8_t)(((unsigned)world->shop_choice+1u)%2u);
                npe_render_shop_choice_move(pixels);draw_pixels(renderer,texture,pixels);continue;
            }
            if(key==SDLK_LEFT || key==SDLK_RIGHT){
                int result=npe_render_shop_description(file,font,world,key==SDLK_RIGHT?1:-1,pixels);
                if(result<0)return -1;
                if(result)draw_pixels(renderer,texture,pixels);
                continue;
            }
            int back=key==SDLK_F4 || key==SDLK_BACKSPACE;
            if(!back && key!=SDLK_RETURN && key!=SDLK_KP_ENTER && key!=SDLK_k)continue;
            if(!back && world->shop_choice){
                int result=npe_world_shop_trade(file,world);
                if(result<0)return -1;
                if(!result){uint8_t notice[sizeof(pixels)];memcpy(notice,pixels,sizeof(notice));
                    if(npe_render_shop_no_money(font,notice))return -1;
                    draw_pixels(renderer,texture,notice);
                    Uint32 started=SDL_GetTicks();
                    while((Uint32)(SDL_GetTicks()-started)<1500){
                        Uint32 elapsed=SDL_GetTicks()-started;
                        if(game_WaitEventTimeout(&event,(int)(1500-elapsed)) && event.type==SDL_QUIT){SDL_PushEvent(&event);return -1;}
                    }
                    draw_pixels(renderer,texture,pixels);continue;
                }
                if(world->shop_transaction_sell && (int16_t)world->condition_words[item_id]<=0){
                    if(npe_render_shop_page(file,font,world,base))return -1;
                }else if(npe_render_shop_totals(font,world,item_id,base))return -1;
            }
            world->shop_ui=1;draw_pixels(renderer,texture,base);
            retain_shop_frame(file,world,renderer,base);return 0;
    }
}

static int scene_frame_wait_parameter(unsigned parameter) {
    /* 03:4120 -> source50:317E multiplies 20 by10; 4B:FC6A
       arms the event timer (4E:7286), floor(ms*64/1000), min1.
       Its accepted input ends this frame's wait, never skips the scene.
       SDL rounds 12/64 seconds up to milliseconds; tick phase remains
       a host approximation, not a measured physical-device duration. */
    unsigned delay_ticks=parameter*10u*64u/1000u;if(!delay_ticks)delay_ticks=1;
    Uint32 deadline = SDL_GetTicks() + (delay_ticks*1000u+63u)/64u;
    do {
        SDL_Event event;
        while (game_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                SDL_PushEvent(&event); /* Main loop owns application exit. */
                return -1;
            }
            if (event.type == SDL_KEYDOWN) {
                SDL_Keycode key = event.key.keysym.sym;
                /* Original2E:FC6A accepts80..86,8D,8E after translation.
                   The delete key89 and literal letters keep waiting. */
                if (key == SDLK_UP || key == SDLK_DOWN || key == SDLK_LEFT ||
                    key == SDLK_RIGHT || key == SDLK_RETURN || key == SDLK_SPACE ||
                    key == SDLK_KP_ENTER || key == SDLK_F4 || key == SDLK_ESCAPE) return 0;
            }
        }
        game_Delay(1);
    } while ((Sint32)(SDL_GetTicks() - deadline) < 0);
    return 0;
}
static int scene_frame_wait(void) { return scene_frame_wait_parameter(20); }
static int play_scene_group(SDL_Renderer *renderer, SDL_Texture *texture,
                            const NpeFile *file, NpeWorld *world,
                            unsigned group, unsigned x, unsigned y,
                            unsigned draw_mode, unsigned erase_mode) {
    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT];
    unsigned frames, frame;
    /* Source41:ACCA..ACF8 rejects arguments before resource or sound work. */
    if (!group || draw_mode > 2 || erase_mode > 1) return 0;
    frames = npe_scene_animation_frames(file, group - 1);
    if (last_pixel_renderer == renderer)
        memcpy(pixels, last_pixel_frame, sizeof(pixels));
    else if (!world->map_number)
        memset(pixels, 0, sizeof(pixels));
    else if (npe_render_world(file, world, pixels)) return -1;
    if(file->rpg_runtime){
        int status=npe_rpg_runtime_scene_begin(file,(int16_t)group);
        /*4A:3764..37DC ignores the original animation service status;
          empty/invalid groups still dispatch their file-defined followup. */
        if(status)return status==-2?-1:0;
        frames=npe_rpg_runtime_scene_frames(file);
    }else if(!frames)return 0;
    npe_world_sound_scene_start(world, group);
    for (frame = 0; frame < frames || frames==255; ++frame) {
        if (draw_mode && erase_mode) {
            /* Source41:B01E..B038 clears and commits the LCD BEFORE
               composing each transparent/behind frame. Mode0 ignores erase. */
            memset(pixels, 0, sizeof(pixels));
            draw_pixels(renderer, texture, pixels);
        }
        if (npe_render_scene_animation_frame(file, group - 1, frame, x, y,
                                              draw_mode, pixels)) {
            npe_world_sound_stop(world);
            npe_rpg_runtime_scene_end(file);
            return -1;
        }
        draw_pixels(renderer, texture, pixels);
        if (scene_frame_wait()) {
            npe_world_sound_stop(world);
            npe_rpg_runtime_scene_end(file);
            return -2; /* Host close: do not dispatch the scene's followup. */
        }
    }
    /* Source41:B0D4 also stops when music was disabled. */
    npe_rpg_runtime_scene_end(file);
    npe_world_sound_stop(world);
    return 1;
}
static int play_scene_animation(SDL_Renderer *renderer, SDL_Texture *texture,
                                const NpeFile *file, NpeWorld *world) {
    NpeMapAnimationEvent event;
    int result;
    if (!world->map_number)
        return -1;
    world->map_init_pending=1;
    result = npe_world_scene_event(world, world->story_event_id, &event);
    if (result <= 0) return result;
    return play_scene_group(renderer, texture, file, world, event.id,
                            event.x, event.y, event.draw_mode, event.erase_mode);
}
typedef struct {SDL_Renderer *renderer;SDL_Texture *texture;} NpcMoveDisplay;
typedef NpcMoveDisplay WalkDisplay;

static void show_controller_help(void){
#ifndef __ANDROID__
    show_settings_page(1);return;
#endif
    SDL_Window *window=SDL_GetKeyboardFocus();
    SDL_Renderer *renderer=window?SDL_GetRenderer(window):NULL;
    if(!renderer || renderer!=last_pixel_renderer)return;
    NpeFont font={0};
    if(load_native_fonts(&font))return;
    SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,NPE_WIDTH,NPE_HEIGHT);
    if(!texture){npe_font_free(&font);return;}
    uint8_t saved[NPE_WIDTH*NPE_HEIGHT],help[NPE_WIDTH*NPE_HEIGHT]={0};
    memcpy(saved,last_pixel_frame,sizeof(saved));
    static const uint8_t line0[]={0xca,0xd6,0xb1,0xfa,0xb0,0xb4,0xbc,0xfc,0xcb,0xb5,0xc3,0xf7};
    static const uint8_t line1[]={0xb7,0xbd,0xcf,0xf2,0x2f,0xd7,0xf3,0xd2,0xa1,0xb8,0xcb,0x3a,0xd2,0xc6,0xb6,0xaf,0xd1,0xa1,0xd4,0xf1};
    static const uint8_t line2[]={0x41,0x3a,0xc8,0xb7,0xc8,0xcf,0x2f,0xb7,0xad,0xd2,0xb3,0x20,0x20,0x58,0x3a,0xb6,0xd4,0xbb,0xb0};
    static const uint8_t line3[]={0x42,0x3a,0xb7,0xb5,0xbb,0xd8,0x2f,0xb2,0xcb,0xb5,0xa5,0x20,0x20,0x59,0x3a,0xd0,0xb6,0xd7,0xb0};
    static const uint8_t line4[]={0x4c,0x42,0x2f,0x52,0x42,0x3a,0xcb,0xb5,0xc3,0xf7,0xb7,0xad,0xd2,0xb3};
    static const uint8_t line5[]={0xb4,0xf0,0xcc,0xe2,0x20,0x41,0x2f,0x42,0x2f,0x58,0x2f,0x59,0x20,0xb6,0xd4,0xd3,0xa6,0x20,0x41,0x2f,0x42,0x2f,0x43,0x2f,0x44};
    static const uint8_t line6[]={0x53,0x74,0x61,0x72,0x74,0x20,0xbb,0xf2,0x20,0x42,0x3a,0xb9,0xd8,0xb1,0xd5,0xcb,0xb5,0xc3,0xf7};
    (void)npe_draw_gbk_text(&font,line0,sizeof(line0),4,4,help);
    (void)npe_draw_gbk_text(&font,line1,sizeof(line1),4,20,help);
    (void)npe_draw_gbk_text(&font,line2,sizeof(line2),4,36,help);
    (void)npe_draw_gbk_text(&font,line3,sizeof(line3),4,52,help);
    (void)npe_draw_gbk_text(&font,line4,sizeof(line4),4,68,help);
    (void)npe_draw_gbk_text(&font,line5,sizeof(line5),4,84,help);
    (void)npe_draw_gbk_text(&font,line6,sizeof(line6),4,100,help);
    draw_pixels(renderer,texture,help);
    game_pad_reset();
    for(;;){
        SDL_Event event;
        (void)npe_sound_pump(game_sound_player);
        if(!SDL_WaitEventTimeout(&event,20))continue;
        if(event.type==SDL_QUIT){SDL_PushEvent(&event);break;}
        if(event.type==SDL_KEYDOWN && (event.key.keysym.sym==SDLK_ESCAPE ||
           event.key.keysym.sym==SDLK_RETURN))break;
        if(event.type==SDL_CONTROLLERBUTTONDOWN && event.cbutton.which==game_pad_id &&
           (event.cbutton.button==SDL_CONTROLLER_BUTTON_START ||
            event.cbutton.button==SDL_CONTROLLER_BUTTON_B))break;
        if(game_audio_event(&event))continue;
        if(event.type==SDL_CONTROLLERDEVICEADDED || event.type==SDL_CONTROLLERDEVICEREMOVED ||
           event.type==SDL_WINDOWEVENT)(void)game_pad_event(&event);
        if(event.type==SDL_WINDOWEVENT && event.window.event==SDL_WINDOWEVENT_EXPOSED)
            draw_pixels(renderer,texture,help);
    }
    game_pad_reset();
    draw_pixels(renderer,texture,saved);
    SDL_DestroyTexture(texture);npe_font_free(&font);
}
static int commit_world_walk(void *,const uint8_t *,unsigned);
static int commit_npc_script_move(void *opaque,const uint8_t *pixels,unsigned wait_parameter) {
    NpcMoveDisplay *display=opaque;
    if(pixels)draw_pixels(display->renderer,display->texture,pixels);
    return wait_parameter && scene_frame_wait_parameter(wait_parameter)<0;
}
static int play_npc_move_animation(SDL_Renderer *renderer,SDL_Texture *texture,
                                   const NpeFile *file,NpeWorld *world) {
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];NpcMoveDisplay display={renderer,texture};
    int result=npe_world_npc_move_with_display(file,world,pixels,commit_npc_script_move,&display);
    if(result>=0)retain_world_frame(file,world,renderer,pixels,1);
    return result;
}
static int commit_script_walk(void *opaque,const uint8_t *pixels,unsigned wait_parameter) {
    /*3B walk poses use the millisecond wait before LCD submission;
      47:03E6 uses03:4120's tenfold wait after the entire step. */
    if(pixels)return commit_world_walk(opaque,pixels,wait_parameter);
    return wait_parameter && scene_frame_wait_parameter(wait_parameter)<0;
}
static int play_script_walk_animation(SDL_Renderer *renderer,SDL_Texture *texture,
                                      const NpeFile *file,NpeWorld *world) {
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];NpcMoveDisplay display={renderer,texture};
    int result=npe_world_script_walk_with_display(file,world,pixels,commit_script_walk,&display);
    if(result>=0)retain_world_frame(file,world,renderer,pixels,1);
    return result;
}
static int show_party_change(SDL_Renderer *renderer, SDL_Texture *texture,
                              const NpeFile *file, const NpeFont *font,
                              NpeWorld *world, const NpeMapPartyEvent *event) {
    int notice = npe_world_change_party(world, event);
    if (notice < 0) return -1;
    if (notice) {
        uint8_t base[NPE_WIDTH * NPE_HEIGHT], pixels[sizeof(base)];
        if (last_pixel_renderer != renderer) return -1;
        memcpy(base, last_pixel_frame, sizeof(base));
        memcpy(pixels, base, sizeof(pixels));
        if (npe_render_party_notice(file, font, world, (unsigned)notice - 1,
                                    event->add_member, pixels)) return -1;
        draw_pixels(renderer, texture, pixels);
        if (wait_timed_notice(2000)) return -2;
        draw_pixels(renderer, texture, base);
    }
    return npe_world_finish_party_event(world, event);
}

static int show_healing_event(SDL_Renderer *renderer,SDL_Texture *texture,
                               const NpeFile *file,const NpeFont *font,NpeWorld *world) {
    /* Original4C:A970/A988 fixed firmware messages, not game resources. */
    static const uint8_t healed[]={0xd0,0xdd,0xcf,0xa2,0xd2,0xbb,0xcc,0xec,0xba,0xf3,
        0xd6,0xf7,0xbd,0xc7,0xbb,0xd6,0xb8,0xb4,0xcc,0xe5,0xc1,0xa6};
    static const uint8_t short_money[]={0xc9,0xed,0xc9,0xcf,0xb5,0xc4,0xbd,0xf0,0xc7,0xae,0xb2,0xbb,0xd7,0xe3};
    uint8_t base[NPE_WIDTH*NPE_HEIGHT],pixels[sizeof(base)];
    if(last_pixel_renderer!=renderer)return -1;
    int result=npe_world_healing_effect(world);
    if(result<0)return -1;
    if(result){
        /*D594..D59E selects the retained plane, clears it to zero and
          commits that clear before the common notice service. */
        memset(base,0,sizeof(base));save_map_edges(file,renderer,base);
        draw_pixels(renderer,texture,base);
    }else memcpy(base,last_pixel_frame,sizeof(base));
    memcpy(pixels,base,sizeof(pixels));
    if(npe_render_timed_notice(font,result?healed:short_money,
                              result?sizeof(healed):sizeof(short_money),pixels))return -1;
    draw_pixels(renderer,texture,pixels);
    if(wait_timed_notice(2000))return -2;
    draw_pixels(renderer,texture,base);
    return npe_world_finish_healing_event(world);
}

static int show_money_event(SDL_Renderer *renderer,SDL_Texture *texture,
                             const NpeFont *font,NpeWorld *world) {
    NpeMapMoneyEvent event;uint16_t amount;int result;
    uint8_t base[NPE_WIDTH*NPE_HEIGHT],pixels[sizeof(base)];
    world->map_init_pending=1;
    result=npe_world_money_event(world,world->story_event_id,&event);
    if(result<=0){if(!result)world->game_mode=0;return result;}
    if(last_pixel_renderer!=renderer||npe_world_change_money(world,&event,&amount)<0)return -1;
    memcpy(base,last_pixel_frame,sizeof(base));memcpy(pixels,base,sizeof(pixels));
    if(npe_render_money_notice(font,amount,event.add_money,pixels))return -1;
    draw_pixels(renderer,texture,pixels);
    if(wait_timed_notice(2000))return -2;
    draw_pixels(renderer,texture,base);
    return npe_world_finish_money_event(world,&event);
}

static int show_item_event(SDL_Renderer *renderer,SDL_Texture *texture,
                            const NpeFile *file,const NpeFont *font,NpeWorld *world) {
    NpeMapItemEvent event;NpeItem item;int result;uint8_t base[NPE_WIDTH*NPE_HEIGHT],pixels[sizeof(base)];
    unsigned event_id=world->story_event_id;
    world->map_init_pending=1;result=npe_world_item_event(world,event_id,&event);
    if(result<=0){if(!result)world->game_mode=0;return result;}
    if(last_pixel_renderer!=renderer)return -1;
    memcpy(base,last_pixel_frame,sizeof(base));memcpy(pixels,base,sizeof(pixels));
    /*44:5B2C loads the item,5B54..5CBA writes its inventory address,
      and5CDA appends the live cached name only after that write. */
    if(npe_world_item(file,world,event.item_index,&item)||npe_world_change_item(world,&event)<0||
       npe_world_item_view(world,world->item_record_raw,&item)||
       npe_render_cached_item_notice(font,&item,event.add_item,0,pixels))return -1;
    draw_pixels(renderer,texture,pixels);
    if(wait_timed_notice(2000))return -2;
    draw_pixels(renderer,texture,base);
    /*5D5C/5D98 reload the same resident slot after the notice. A full
      inventory index can have changed its next mode or next event. */
    if(npe_world_item_event(world,event_id,&event)!=1)return -1;
    return npe_world_finish_item_event(world,&event);
}

static int show_skill_event(SDL_Renderer *renderer,SDL_Texture *texture,
                             const NpeFile *file,const NpeFont *font,NpeWorld *world) {
    NpeMapSkillEvent event;unsigned member;int result;uint8_t base[NPE_WIDTH*NPE_HEIGHT],pixels[sizeof(base)];
    world->map_init_pending=1;result=npe_world_skill_event(world,world->story_event_id,&event);
    if(result<=0){if(!result)world->game_mode=0;return result;}
    if(last_pixel_renderer!=renderer||npe_world_change_skill(world,&event,&member)<0)return -1;
    memcpy(base,last_pixel_frame,sizeof(base));memcpy(pixels,base,sizeof(pixels));
    save_map_edges(file,renderer,base);
    if(npe_render_world_skill_notice(file,font,world,event.skill_id,pixels))return -1;
    draw_pixels(renderer,texture,pixels);if(wait_timed_notice(2000))return -2;
    draw_pixels(renderer,texture,base);return npe_world_finish_skill_event(world,&event);
}

static int show_party_event(SDL_Renderer *renderer, SDL_Texture *texture,
                             const NpeFile *file, const NpeFont *font,
                             NpeWorld *world) {
    NpeMapPartyEvent event;
    int result;
    /*22:0F6E..0F76 requests the next mode's initialization before lookup. */
    world->map_init_pending=1;
    result = npe_world_party_event(world, world->story_event_id, &event);
    if (result < 0) return -1;
    if (!result) { world->game_mode = 0; return 0; }
    return show_party_change(renderer, texture, file, font, world, &event);
}

static int event_chain_quit_requested(void) {
    /* Host event servicing does not consume keys intended for the next NPE
       service. A queued close ends even a cycle with no text/input wait. */
    SDL_PumpEvents();
    return SDL_HasEvent(SDL_QUIT);
}

static int show_party_adjust_event(SDL_Renderer *renderer,SDL_Texture *texture,
                                  const NpeFile *file,NpeWorld *world) {
    NpeMapPartyAdjustEvent event;int result;uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
    world->map_init_pending=1;
    result=npe_world_party_adjust_event(world,world->story_event_id,&event);
    if(result<=0){if(!result)world->game_mode=0;return result;}
    result=npe_world_change_party_leader(world,&event);
    if(result<=0)return result;
    if(last_pixel_renderer!=renderer)return -1;
    memcpy(pixels,last_pixel_frame,sizeof(pixels));
    if(npe_render_world_static_player(file,world,pixels))return -1;
    /*46:F18A/F190 submits the new leader before assigning the followup. */
    draw_pixels(renderer,texture,pixels);
    if(event_chain_quit_requested())return -2;
    return npe_world_finish_party_adjust_event(world,&event);
}

static int show_teleport_event(SDL_Renderer *renderer,SDL_Texture *texture,
                               const NpeFile *file,const NpeFont *font,NpeWorld *world) {
    uint8_t previous_init=world->map_init_pending;
    int result=npe_world_apply_teleport_event(file,world);
    if(result<=0)return result;
    world->map_init_pending=previous_init;
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT],notice[NPE_WIDTH*NPE_HEIGHT];
    if(npe_render_world_load(file,world,pixels))return -1;
    restore_map_edges(file,renderer,pixels);
    draw_pixels(renderer,texture,pixels);
    retain_world_frame(file,world,renderer,pixels,1);
    memcpy(notice,pixels,sizeof(notice));
    if(npe_render_map_name_notice(file,font,world,notice))return -1;
    draw_pixels(renderer,texture,notice);
    if(wait_timed_notice(5000))return -2;
    draw_pixels(renderer,texture,pixels);
    world->map_init_pending=1;
    return result;
}

static int advance_world_events_dispatch(SDL_Window *window, SDL_Renderer *renderer,
                                 SDL_Texture *texture, const NpeFile *file,
                                 const NpeFont *font, NpeWorld *world,
                                 unsigned previous_mode,
                                 int quiz_requested) {
    int closed_dialogue=0;
    unsigned last_service_mode=0;
    if(previous_mode==3 && world->game_mode!=3){
        /* 33:C28E clears the retained plane after victory panels;
           33:BCD2 and 2F:C12E do the same on loss and successful flee.
           Map initialization must inherit these cleared margin rows. */
        uint8_t cleared[NPE_WIDTH*NPE_HEIGHT]={0};
        save_map_edges(file,renderer,cleared);
        world_map_frame_valid=world_map_frame_uncommitted=0;
    }
    for (;;) {
        int result;
        if(event_chain_quit_requested())return -2;
        /*4A:F498/F4A0 ->F5EE normalizes modes outside1..24 and requests
          map initialization without changing the event word. */
        if (!world->game_mode || world->game_mode > 24) {
            world->game_mode = 1;
            closed_dialogue = 1;
            break;
        }
        if(world->game_mode!=1)last_service_mode=world->game_mode;
        if (world->game_mode == 8) {
            if (previous_mode != 8 || quiz_requested || !world->quiz_ready)
                show_quiz_question(window, renderer, texture, file, font, world);
            if (world->game_mode == 8) break;
            continue;
        }
        if (world->game_mode == 2) {
            if (show_dialogue_pages(window, renderer, texture, file, world,
                                    font, world->mode2_dialogue_offset)) {
                fprintf(stderr, "mode-2 dialogue source is unsupported\n");
                break;
            }
            if (npe_world_finish_mode2(world) != 1) {
                fprintf(stderr, "mode-2 followup is unsupported\n");
                break;
            }
            closed_dialogue=1;
            continue;
        }
        if (world->game_mode == 5) {
            unsigned event_id = world->story_event_id;
            unsigned progress = world->story_progress;
            show_walk_story(window, renderer, texture, file, font, world);
            if (world->game_mode == 5 &&
                world->story_event_id == event_id &&
                world->story_progress == progress) break;
            continue;
        }
        if (world->game_mode == 3 && !world->battle_party_flags_saved && !world->battle_enemy_count) {
            if (npe_world_prepare_battle(file, world) < 0)
                fprintf(stderr, "battle entry is unsupported\n");
            break;
        }
        if (world->game_mode == 14) {
            uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
            /*4C:4C56..4C62 requests map initialization and redraws the
              current progress before the event writes its new progress. */
            world->map_init_pending=1;
            /*4C:6BB8..6BE4 takes the border rows from the retained LCD
              plane. A timed notice restores that plane before mode14. */
            if(npe_render_world_load(file,world,pixels))break;
            restore_map_edges(file,renderer,pixels);
            draw_pixels(renderer,texture,pixels);
            retain_world_frame(file,world,renderer,pixels,1);
            result = npe_world_execute_story_event(world,world->story_event_id);
        }
        else if (world->game_mode == 9) {
            result = show_skill_event(renderer,texture,file,font,world);
            if(result==-2)return -2;
        }
        else if (world->game_mode == 10) {
            result = show_party_event(renderer, texture, file, font, world);
            if (result == -2) return -2;
            closed_dialogue = 1; /*3F:0F76 sets the original map-init flag. */
        }
        else if (world->game_mode == 13){
            result = show_party_adjust_event(renderer,texture,file,world);
            if(result==-2)return -2;
        }
        else if (world->game_mode == 11){
            result = show_money_event(renderer,texture,font,world);
            if(result==-2)return -2;
        }
        else if (world->game_mode == 12){
            result = show_item_event(renderer,texture,file,font,world);
            if(result==-2)return -2;
        }
        else if (world->game_mode == 15) {
            int played = play_scene_animation(renderer, texture, file, world);
            if (played == -2) return -2;
            if (played < 0)
                fprintf(stderr, "scene animation is unsupported\n");
            result = npe_world_finish_animation_event(file, world);
        } else if (world->game_mode == 16) {
            result = show_teleport_event(renderer,texture,file,font,world);
            if(result==-2)return -2;
        }
        else if (world->game_mode == 18) {
            result=play_npc_move_animation(renderer, texture, file, world);
            if(result==-2)return -2;
            if (result < 0)
                fprintf(stderr, "NPC move animation is unsupported\n");
        } else if (world->game_mode == 19) {
            result=play_script_walk_animation(renderer,texture,file,world);
            if(result==-2)return -2;
            if(result<0)fprintf(stderr,"script walk animation is unsupported\n");
        } else if (world->game_mode == 20)
            result = npe_world_apply_move_event(file, world);
        else if (world->game_mode == 21)
            result = npe_world_finish_game(world);
        else if (world->game_mode == 22){
            result=show_healing_event(renderer,texture,file,font,world);
            if(result==-2)return -2;
        }
        else if (world->game_mode == 23)
            result=npe_world_apply_experience_event(world);
        else if (world->game_mode == 24)
            result=npe_world_apply_npc_state_event(world);
        else break;
        if (result < 0) {
            fprintf(stderr, "resource event is unsupported\n");
            break;
        }
    }
    if(event_chain_quit_requested())return -2;
    if(previous_mode==3 && world->game_mode==7){
        /* The title controller clears its plane before its first frame.
           This is separate from the loss clear at33:BCD6. */
        uint8_t cleared[NPE_WIDTH*NPE_HEIGHT]={0};
        draw_pixels(renderer,texture,cleared);
    }
    if(world->game_mode==1){
        world->event_blank_background=0;
        if(world->map_init_pending){
            /*37:1328..1366 restarts the retained slot, clears7AB4, then
              redraws. Playing the same section still restarts its cursor. */
            if(npe_world_music_controller(world,2)!=1)return -1;
            uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
            if(!npe_render_world_load(file,world,pixels)){
                /*37:1354 restores the retained plane before4C:6AB8;
                  4C:6BB8..6BE4 takes its border rows from that plane.
                  A dialogue over an exit prompt can retain zero rows
                  while the resident clear color still differs. */
                restore_map_edges(file,renderer,pixels);
                /* Original mode15 returns with the prepared map plane
                   retained. Public scene paths commit only animation
                   frames; the next map display submits the new plane. */
                if(last_service_mode!=15)draw_pixels(renderer,texture,pixels);
                retain_world_frame(file,world,renderer,pixels,1);
                if(last_service_mode==15)world_map_frame_submit_pending=1;
            }
        }else if(closed_dialogue){
            uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
            if(!npe_render_world_load(file,world,pixels)){
                restore_map_edges(file,renderer,pixels);draw_pixels(renderer,texture,pixels);
                retain_world_frame(file,world,renderer,pixels,1);
            }
        }
    }
    return 0;
}
static int advance_world_events(SDL_Window *window, SDL_Renderer *renderer,
                                 SDL_Texture *texture, const NpeFile *file,
                                 const NpeFont *font, NpeWorld *world,
                                 unsigned previous_mode, int quiz_requested) {
    uint8_t saved_scope=world->defer_story_events;
    world->defer_story_events=1;
    int status=advance_world_events_dispatch(window,renderer,texture,file,font,world,
                                            previous_mode,quiz_requested);
    world->defer_story_events=saved_scope;
    return status;
}

static int draw_battle_scene(SDL_Renderer *renderer, SDL_Texture *texture,
                             const NpeFile *file, unsigned enemy_id,
                             const NpeWorld *world, const NpeFont *font,
                             unsigned icon, unsigned menu_level,
                             unsigned menu_selection,
                             unsigned item_selection) {
    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT];
    static const uint8_t body_label[] = {0xcc, 0xe5};
    static const uint8_t qi_label[] = {0xc6, 0xf8};
    unsigned hp, max_hp, qi, max_qi;
    int animation=draw_battle_actor_advance(renderer,texture,file,world,font,icon,menu_level);
    if(animation)return animation<0 ? -1 : 0;
    animation=draw_battle_animation_commands(renderer,texture,file,world,font,icon);
    if(animation) return animation<0 ? -1 : 0;
    animation=draw_battle_cached_commands(renderer,texture,file,world,icon,
        menu_level,menu_selection,item_selection);
    if(animation) return animation<0 ? -1 : 0;
    if (world) {
        if (npe_render_battle_world_scene(file, world, pixels)) return -1;
    } else if (npe_render_battle_scene(file, enemy_id, pixels)) return -1;
    apply_battle_enemy_pointer_clears(world,pixels);
    if (world) {
        hp = world->hp[world->battle_actor];
        max_hp = world->max_hp[world->battle_actor];
        qi = world->qi[world->battle_actor];
        max_qi = world->max_qi[world->battle_actor];
    } else {
        NpeCharacter character;
        if (npe_character(file, 0, &character)) return -1;
        hp = character.hp; max_hp = character.max_hp;
        qi = character.qi; max_qi = character.max_qi;
    }
    if (font && font->data) {
        if (npe_draw_gbk_text(font, body_label, sizeof(body_label),
                              70, 88, pixels)) return -1;
        if (npe_draw_gbk_text(font, qi_label, sizeof(qi_label),
                              70, 104, pixels)) return -1;
    }
    npe_render_battle_hud_values(hp, max_hp, qi, max_qi, pixels);
    if (world && icon < 4) {
        if (npe_render_battle_icon_selection(icon, pixels)) return -1;
    }
    /* The round refresh writes status cells over the selected icon when
       an enemy's status extends into the controls. */
    npe_render_battle_status_overlay(world, pixels);
    if (menu_level >= 1 && menu_level <= 2) {
        if (npe_render_battle_command_menu(file, 1, menu_selection, pixels))
            return -1;
        if (menu_level == 2 &&
            npe_render_battle_command_menu(file, 2, item_selection, pixels))
            return -1;
    }
    draw_pixels(renderer, texture, pixels);
    return 0;
}

static char *game_save_path(const NpeFile *file, unsigned slot) {
    char *pref = SDL_GetPrefPath("NPE", "npe-sdl");
    char *path;
    size_t path_size;
    if (!pref) return NULL;
    path_size = strlen(pref) + 48;
    path = malloc(path_size);
    if (path) {
        if (slot == 0)
            snprintf(path, path_size, "%ssave-%016llx.npes", pref,
                     (unsigned long long)npe_game_fingerprint(file));
        else
            snprintf(path, path_size, "%ssave-%016llx-%u.npes", pref,
                     (unsigned long long)npe_game_fingerprint(file), slot + 1);
    }
    SDL_free(pref);
    return path;
}

static void game_save_slot_levels(const NpeFile *file,
                                  char *const paths[3], unsigned levels[3]) {
    for (unsigned slot = 0; slot < 3; ++slot) {
        NpeWorld saved;
        levels[slot] = paths[slot] &&
            npe_world_load(file, &saved, paths[slot]) == 0 ?
            saved.level[0] : 0;
    }
}

static int show_save_failure(SDL_Renderer *renderer, SDL_Texture *texture,
                              const NpeFont *font, int saving) {
    uint8_t base[NPE_WIDTH*NPE_HEIGHT], pixels[sizeof(base)];
    if (last_pixel_renderer != renderer) return -1;
    memcpy(base, last_pixel_frame, sizeof(base));
    memcpy(pixels, base, sizeof(pixels));
    if (npe_render_save_failure(font, saving, pixels)) return -1;
    draw_pixels(renderer, texture, pixels);
    if (wait_timed_notice(2000)) return -2;
    draw_pixels(renderer, texture, base);
    return 0;
}

static int show_system_map_return(const NpeFile *file, NpeWorld *world,
                                  SDL_Renderer *renderer, SDL_Texture *texture) {
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
    /*37:D094/D0C8/D13A sets map-init.37:1350..1366 selects plane1
      then redraws actors in load order. The map service clears the four
      edge rows via03:4214 (4D:1A54), using the system menu's background
      color0 set by49:0080..008A, rather than the old map's color2.
      This does not reload NPC patrol records on save or failed read. */
    world->map_init_pending=0;
    if (npe_render_world_load(file, world, pixels)) return -1;
    save_map_edges(file, renderer, pixels);
    draw_pixels(renderer, texture, pixels);
    retain_world_frame(file, world, renderer, pixels, 1);
    system_menu_background_valid=0;
    return 0;
}

static int restore_system_menu_background(const NpeFile *file,const NpeWorld *world,
                                           SDL_Renderer *renderer,SDL_Texture *texture){
    if(!system_menu_background_valid || system_menu_background_file!=file ||
       system_menu_background_renderer!=renderer)return -1;
    draw_pixels(renderer,texture,system_menu_background);
    retain_world_frame(file,world,renderer,system_menu_background,1);
    system_menu_background_valid=0;
    return 0;
}

static int initialize_title_controls(const NpeFile *file,SDL_Renderer *renderer,
                                     SDL_Texture *texture,unsigned selection){
    /*48:0DA2 clears and commits before resetting the three title bytes.
      Cancel passes the existing title selection; initial entry passes1. */
    uint8_t cleared[NPE_WIDTH*NPE_HEIGHT]={0};
    uint8_t one=1,selected=(uint8_t)(selection+1);
    draw_pixels(renderer,texture,cleared);
    return npe_rpg_runtime_write(file,0x527a7a,&one,1) ||
           npe_rpg_runtime_write(file,0x527a7c,&selected,1) ||
           npe_rpg_runtime_write(file,0x527a7e,&one,1) ? -1 : 0;
}

static void switch_title_slot_row(unsigned previous,unsigned selected){
    /*3A:8B96..8C6C /8DB2..8E88 invert the old and new inclusive
      rectangles on the retained LCD. Adjacent rows share their edge. */
    unsigned rows[2]={previous,selected};
    for(unsigned i=0;i<2;++i)
        for(unsigned y=56+rows[i]*17;y<=73+rows[i]*17;++y)
            for(unsigned x=166;x<=221;++x)
                last_pixel_frame[y*NPE_WIDTH+x]=15-last_pixel_frame[y*NPE_WIDTH+x];
}

static int start_title_game(const NpeFile *file, NpeWorld *world,
                            NpeSoundPlayer *sound,
                            SDL_Renderer *renderer, SDL_Texture *texture) {
    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT];
    if (npe_world_restart_game(file, world)) return -1;
    npe_sound_select(sound, world);
    if (npe_render_world_load(file, world, pixels)) return -1;
    /*46:D2F4 ->4C:6AB8 commits the constructor scene;3A:8EF4
      explicitly commits it again. The next mode1 initialization is
      submitted by the outer controller. Keep its native loading order. */
    draw_pixels(renderer, texture, pixels);
    draw_pixels(renderer, texture, pixels);
    retain_world_frame(file, world, renderer, pixels, 0);
    system_menu_background_valid = system_menu_frame_valid = 0;
    return 0;
}

typedef struct {
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    const NpeFont *font;
    uint8_t base[NPE_WIDTH*NPE_HEIGHT];
    uint8_t actors[NPE_WIDTH*NPE_HEIGHT];
    uint32_t epoch, pointer_epoch;
    uint8_t pointer_clear_mask;
    uint8_t prepared, awaiting_menu, cached, icon, menu_actor, poison, entry_cached;
    uint8_t menu_committed;
    uint8_t last_action;
} BattleAnimationDisplay;

static int draw_battle_animation_commands(SDL_Renderer *,SDL_Texture *,
    const NpeFile *,const NpeWorld *,const NpeFont *,unsigned);

static void retain_battle_entry(const NpeWorld *world,SDL_Renderer *renderer,
                                 unsigned stage,const uint8_t *pixels){
    if(!world||world->battle_animation_observer!=play_battle_animation||
       !world->battle_animation_context)return;
    BattleAnimationDisplay *d=world->battle_animation_context;
    if(d->renderer!=renderer)return;
    if(!stage){
        memcpy(d->actors,pixels,sizeof(d->actors));
        d->prepared=d->awaiting_menu=d->cached=d->entry_cached=d->poison=0;
        d->pointer_clear_mask=0;
    }else if(stage==3){
        memcpy(d->base,pixels,sizeof(d->base));
        d->epoch=world->battle_display_epoch;d->menu_actor=world->battle_actor;
        d->icon=1;d->cached=d->entry_cached=1;
    }
}
static const uint8_t *retained_battle_actors(const NpeWorld *world,SDL_Renderer *renderer){
    if(!world||world->battle_animation_observer!=play_battle_animation||
       !world->battle_animation_context)return NULL;
    const BattleAnimationDisplay *d=world->battle_animation_context;
    return d->renderer==renderer&&d->entry_cached&&d->epoch==world->battle_display_epoch?d->actors:NULL;
}

/* Existing callers pass identical start/end coordinates. Original1F:43D4
   still reads a single-frame group's payload, but draws zero steps then.
   Restore flags describe the caller's original display commits. */
static int play_battle_role_group(BattleAnimationDisplay *display,const NpeFile *file,
                                  unsigned group,int x,int y,uint8_t *pixels,
                                  unsigned initial,unsigned before,unsigned after){
    unsigned frames=0;
    if(file->rpg_runtime){
        int result=npe_rpg_runtime_role_begin(file,(int16_t)(uint16_t)group,
            (int16_t)x,(int16_t)y,(int16_t)x,(int16_t)y);
        if(result){npe_rpg_runtime_role_end(file);return result==-2?-1:0;}
        frames=npe_rpg_runtime_role_steps(file);
    }else{
        frames=group?npe_animation_frames(file,group-1):0;
        if(frames==1)frames=0;
    }
    if(frames && initial)draw_pixels(display->renderer,display->texture,display->base);
    for(unsigned frame=0;frame<frames || frames>=255;++frame){
        if(before)draw_pixels(display->renderer,display->texture,display->base);
        memcpy(pixels,display->base,NPE_WIDTH*NPE_HEIGHT);
        int result=file->rpg_runtime?npe_rpg_runtime_role_frame(file,2,pixels,NULL):
            npe_render_battle_animation_frame(file,group-1,(uint8_t)frame,x,y,pixels);
        if(result){npe_rpg_runtime_role_end(file);return -1;}
        draw_pixels(display->renderer,display->texture,pixels);
        /*03:4120 parameter15 uses the same original tick conversion
           and accepted-input wait as scene playback. */
        if(scene_frame_wait_parameter(15)){
            npe_rpg_runtime_role_end(file);return -1;
        }
        if(after==2 || (after==1 && frame+1<frames))
            draw_pixels(display->renderer,display->texture,display->base);
    }
    if(!frames)memcpy(pixels,display->base,NPE_WIDTH*NPE_HEIGHT);
    npe_rpg_runtime_role_end(file);return 0;
}

static void retain_battle_enemy_pointer_clear(const NpeWorld *world,unsigned target) {
    if (!world || world->battle_animation_observer!=play_battle_animation ||
        !world->battle_animation_context || target>=3) return;
    BattleAnimationDisplay *d=world->battle_animation_context;
    if(d->pointer_epoch!=world->battle_display_epoch)d->pointer_clear_mask=0;
    d->pointer_epoch=world->battle_display_epoch;
    d->pointer_clear_mask|=(uint8_t)(1u<<target);
}
static void apply_battle_enemy_pointer_clears(const NpeWorld *world,uint8_t *pixels) {
    if(!world || world->battle_animation_observer!=play_battle_animation ||
        !world->battle_animation_context)return;
    BattleAnimationDisplay *d=world->battle_animation_context;
    if(d->pointer_epoch!=world->battle_display_epoch)return;
    for(unsigned target=0;target<world->battle_enemy_count;++target)
        if(d->pointer_clear_mask&(1u<<target))npe_clear_battle_enemy_target(world,target,pixels);
}

static void play_battle_animation(void *context, const NpeWorld *world,
                                  unsigned action, unsigned target,
                                  unsigned phase) {
    BattleAnimationDisplay *display=context;
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
    unsigned retained_entry=display?display->entry_cached:0;
    if (!display || (action!=NPE_BATTLE_ANIMATION_COMBINE && action!=NPE_BATTLE_ANIMATION_ATTACK &&
        action!=NPE_BATTLE_ANIMATION_DEFEND && action!=NPE_BATTLE_ANIMATION_ENEMY_ATTACK &&
        action!=NPE_BATTLE_ANIMATION_ENEMY_RECOVERY && action!=NPE_BATTLE_ANIMATION_ENEMY_SPELL &&
        action!=NPE_BATTLE_ANIMATION_POISON && action!=NPE_BATTLE_ANIMATION_PARTY_SPELL &&
        action!=NPE_BATTLE_ANIMATION_ITEM && action!=NPE_BATTLE_ANIMATION_VICTORY &&
        action!=NPE_BATTLE_ANIMATION_LEVEL && action!=NPE_BATTLE_ANIMATION_DROP &&
        action!=NPE_BATTLE_ANIMATION_LEARN && action!=NPE_BATTLE_ANIMATION_FLEE &&
        action!=NPE_BATTLE_ANIMATION_FAILED_FLEE && action!=NPE_BATTLE_ANIMATION_DEFEAT) ||
        (!world->battle_enemy_count && action!=NPE_BATTLE_ANIMATION_FLEE) ||
        (phase!=NPE_BATTLE_ANIMATION_ROUND_END && phase!=NPE_BATTLE_ANIMATION_ROUND_RESTORE &&
         phase!=NPE_BATTLE_ANIMATION_ACTOR_RESTORE && action!=NPE_BATTLE_ANIMATION_DEFEND &&
         action!=NPE_BATTLE_ANIMATION_ENEMY_ATTACK && action!=NPE_BATTLE_ANIMATION_ENEMY_SPELL &&
         action!=NPE_BATTLE_ANIMATION_POISON && action!=NPE_BATTLE_ANIMATION_PARTY_SPELL &&
         action!=NPE_BATTLE_ANIMATION_ITEM && action!=NPE_BATTLE_ANIMATION_LEVEL &&
         action!=NPE_BATTLE_ANIMATION_DROP && action!=NPE_BATTLE_ANIMATION_LEARN && action!=NPE_BATTLE_ANIMATION_FLEE &&
         target>=world->battle_enemy_count) ||
        world->battle_enemy_count>3) return;
    if(action==NPE_BATTLE_ANIMATION_FAILED_FLEE){
        if(phase!=NPE_BATTLE_ANIMATION_BEGIN||last_pixel_renderer!=display->renderer)return;
        /* Source2F:BD32 selects the offscreen draw target. The temporary
           text is cleared atBD82 beforeBD88 commits the retained menu.
           Native capture confirms no visible "escape failed" popup. */
        game_Delay(10); /* Source2F:BD58; service time unit remains unverified. */
        draw_pixels(display->renderer,display->texture,last_pixel_frame);
        return;
    }
    /* Action preparation redraws actors; old selector erasures expire. */
    display->pointer_clear_mask=0;
    display->entry_cached=0;
    if(action==NPE_BATTLE_ANIMATION_DEFEAT){
        if(phase!=NPE_BATTLE_ANIMATION_BEGIN)return;
        /*33:BC8A restores poses, BC90 commits them, and BCD6 commits
          the cleared plane. Title initialization is a later controller. */
        if(!display->prepared && npe_render_battle_actors_scene(world->battle_file,world,display->base))return;
        if(npe_render_battle_round_restore(world->battle_file,world,0,display->base))return;
        draw_pixels(display->renderer,display->texture,display->base);
        memset(display->base,0,sizeof(display->base));
        draw_pixels(display->renderer,display->texture,display->base);
        display->prepared=display->cached=display->awaiting_menu=display->poison=0;
        return;
    }
    if(action==NPE_BATTLE_ANIMATION_FLEE){
        if(phase!=NPE_BATTLE_ANIMATION_BEGIN)return;
        if(npe_render_battle_actors_scene(world->battle_file,world,display->base))return;
        memcpy(pixels,display->base,sizeof(pixels));
        for(unsigned member=0;member<3;++member)if(world->party_active[member]){
            if((int16_t)world->hp[member]<=0)continue;
            if(npe_render_battle_flee_stage(world->battle_file,world,member,display->base))return;
            draw_pixels(display->renderer,display->texture,display->base);
            unsigned group=npe_world_battle_character(world,member)->animation_ids[0];
            /* Source2F:BE9A writes x-15 and BEF0 preserves y;
               C030 loads this character's original escape group. */
            if(play_battle_role_group(display,world->battle_file,group,
                npe_world_battle_coordinate(world,0,member,0)-15,npe_world_battle_coordinate(world,0,member,1),pixels,1,0,1))return;
            game_Delay(150); /* Source2F:C042; firmware time unit remains unverified. */
        }
        /* Source44:6B14 skips sums without victory, but still draws
           UI base+30. Source2F:C120 commits that zero panel once more. */
        if(npe_render_battle_reward_panel(world->battle_file,0,0,pixels))return;
        draw_pixels(display->renderer,display->texture,pixels);game_Delay(80);
        draw_pixels(display->renderer,display->texture,pixels);
        /*2F:C12E/C132 clear both LCD planes before the map controller
          restores plane1. Retained map margins must follow that clear. */
        memset(pixels,0,sizeof(pixels));
        save_map_edges(world->battle_file,display->renderer,pixels);
        world_map_frame_valid=world_map_frame_uncommitted=0;
        display->prepared=display->cached=display->awaiting_menu=display->poison=0;
        return;
    }
    if(action==NPE_BATTLE_ANIMATION_VICTORY){
        if(phase!=NPE_BATTLE_ANIMATION_BEGIN)return;
        if(!display->prepared && npe_render_battle_actors_scene(world->battle_file,world,display->base))return;
        if(npe_render_battle_round_restore(world->battle_file,world,0,display->base))return;
        draw_pixels(display->renderer,display->texture,display->base);
        memcpy(pixels,display->base,sizeof(pixels));
        if(npe_render_battle_rewards(world->battle_file,world,pixels))return;
        draw_pixels(display->renderer,display->texture,pixels);
        memcpy(display->base,pixels,sizeof(pixels));
        game_Delay(80); /* Source44:6E04; real firmware time unit remains unverified. */
        display->prepared=display->cached=display->awaiting_menu=0;
        return;
    }
    if(action==NPE_BATTLE_ANIMATION_LEVEL||action==NPE_BATTLE_ANIMATION_DROP||action==NPE_BATTLE_ANIMATION_LEARN){
        int result=action==NPE_BATTLE_ANIMATION_LEVEL?
            npe_render_battle_level_panel(world->battle_file,display->font,world,target&255u,target>>8,phase,display->base):
            action==NPE_BATTLE_ANIMATION_DROP?
            npe_render_battle_drop_panel(world->battle_file,display->font,world,target,display->base):
            npe_render_battle_learn_panel(world->battle_file,display->font,world,target,display->base);
        if(result||(action==NPE_BATTLE_ANIMATION_LEVEL&&phase==NPE_BATTLE_ANIMATION_BEGIN))return;
        draw_pixels(display->renderer,display->texture,display->base);
        game_Delay(action==NPE_BATTLE_ANIMATION_LEARN?50:80); /* Native service units remain unverified. */
        return;
    }
    if(phase==NPE_BATTLE_ANIMATION_ACTOR_RESTORE){
        if(!display->prepared)return;
        if(npe_render_battle_round_restore(world->battle_file,world,0,display->base))return;
        draw_pixels(display->renderer,display->texture,display->base);
        return;
    }
    if (phase==NPE_BATTLE_ANIMATION_ROUND_RESTORE) {
        if (!display->prepared) return;
        if(!display->poison)draw_pixels(display->renderer,display->texture,display->base);
        display->poison=0;
        if (npe_render_battle_round_restore(world->battle_file,world,0,display->base)) return;
        draw_pixels(display->renderer,display->texture,display->base);
        return;
    }
    if (phase==NPE_BATTLE_ANIMATION_ROUND_END) {
        if (!display->prepared) return;
        if (npe_render_battle_round_restore(world->battle_file,world,1,display->base)) return;
        draw_pixels(display->renderer,display->texture,display->base);
        memcpy(display->actors,display->base,sizeof(display->actors));
        display->epoch=world->battle_display_epoch; display->cached=1;
        display->awaiting_menu=1;
        /*33:C7FE..C81C draws the returned menu before39:5AD6 clears the
          dispatch byte. The outer redraw consumes menu_committed once. */
        if(world->battle_animation_observer==play_battle_animation &&
           draw_battle_animation_commands(display->renderer,display->texture,
             world->battle_file,world,display->font,
             (unsigned)battle_menu_icon(world))>0)
            display->menu_committed=1;
        return;
    }
    if (phase==NPE_BATTLE_ANIMATION_DAMAGE) {
        if(action==NPE_BATTLE_ANIMATION_POISON){
            if(!display->prepared && npe_render_battle_actors_scene(world->battle_file,world,display->base))return;
            display->prepared=display->poison=1;
        }
        if (!display->prepared) return;
        for (unsigned step=0;step<3;++step) {
            memcpy(pixels,display->base,sizeof(pixels));
            int numbers=action==NPE_BATTLE_ANIMATION_POISON?
                npe_render_battle_poison_numbers(world,step,pixels):
                (action==NPE_BATTLE_ANIMATION_PARTY_SPELL || action==NPE_BATTLE_ANIMATION_ITEM)?
                npe_render_battle_party_spell_numbers(world,step,pixels):
                action==NPE_BATTLE_ANIMATION_ENEMY_RECOVERY?
                npe_render_battle_enemy_recovery_numbers(world,step,pixels):
                (action==NPE_BATTLE_ANIMATION_ENEMY_ATTACK || action==NPE_BATTLE_ANIMATION_ENEMY_SPELL)?
                npe_render_battle_party_damage_numbers(world,step,pixels):
                npe_render_battle_damage_numbers(world,step,pixels);
            if(numbers)return;
            draw_pixels(display->renderer,display->texture,pixels);
            /*39:601E commits the temporary defense plane before604A
              restores its backing plane. Poison first draws over that old
              LCD;36:7660 then exposes the restored backing for later steps. */
            if(!step && action==NPE_BATTLE_ANIMATION_POISON &&
               display->last_action==NPE_BATTLE_ANIMATION_DEFEND &&
               npe_render_battle_defend_restore(world->battle_file,world,display->base))return;
            /* Source30:2592/25A0 waits5 after the initial position,30
               afterwards. WASM commits hold the latter for about 19 ticks at30Hz;
               each service unit is approximately20ms, not one SDL millisecond. */
            game_Delay((action==NPE_BATTLE_ANIMATION_POISON?30u:step ? 30u : 5u)*20u);
            if (!step && action!=NPE_BATTLE_ANIMATION_POISON) {
                int restored=action==NPE_BATTLE_ANIMATION_ENEMY_RECOVERY?
                    npe_render_battle_enemy_recovery_stage(world->battle_file,world,target,1,display->base):
                    (action==NPE_BATTLE_ANIMATION_PARTY_SPELL || action==NPE_BATTLE_ANIMATION_ITEM)?
                    npe_render_battle_party_spell_stage(world->battle_file,world,target,1,display->base):
                    action==NPE_BATTLE_ANIMATION_ENEMY_SPELL?
                    npe_render_battle_enemy_spell_stage(world->battle_file,world,target,1,display->base):
                    action==NPE_BATTLE_ANIMATION_ENEMY_ATTACK?
                    npe_render_battle_enemy_attack_stage(world->battle_file,world,target,1,display->base):
                    action==NPE_BATTLE_ANIMATION_ATTACK?
                    npe_render_battle_attack_stage(world->battle_file,world,target,2,display->base):
                    npe_render_battle_combine_restore(world->battle_file,world,display->base);
                if(restored)return;
            }
            draw_pixels(display->renderer,display->texture,display->base);
            SDL_PumpEvents();
        }
        return;
    }
    if(phase==NPE_BATTLE_ANIMATION_SETTLE && action==NPE_BATTLE_ANIMATION_COMBINE){
        if(display->prepared)draw_pixels(display->renderer,display->texture,display->base);
        return;
    }
    if (phase!=NPE_BATTLE_ANIMATION_BEGIN) return;
    /* Source33:C868..C876 refreshes the actors between actual actions.
       HP has committed by the next BEGIN; retain the previous pose cache
       so a newly fallen enemy is cleared without reconstructing scenery. */
    if (display->prepared && display->epoch==world->battle_display_epoch) {
        /* The explicit ACTOR_RESTORE phase has already committed this
           plane while the execution cursor still named the previous actor. */
    } else if (display->cached && (world->battle_scene_refreshed || retained_entry) &&
        world->battle_display_epoch==display->epoch+1) {
        /* The first action starts from the committed entry LCD. A RAM
           change to enemy HP does not erase its already drawn bitmap. */
        memcpy(display->base,display->actors,sizeof(display->base));
    } else if (npe_render_battle_actors_scene(world->battle_file,world,display->base)) return;
    display->prepared=display->awaiting_menu=display->poison=0;
    display->epoch=world->battle_display_epoch;
    display->cached=0;
    display->last_action=(uint8_t)action;
    if(action==NPE_BATTLE_ANIMATION_PARTY_SPELL || action==NPE_BATTLE_ANIMATION_ITEM){
        unsigned actor=npe_world_battle_action_actor(world);
        unsigned own,group,effect;
        const NpeCharacterMetadata *character=npe_world_battle_character(world,actor);
        if(!character)return;
        if(action==NPE_BATTLE_ANIMATION_ITEM){
            NpeItem item;uint8_t raw[41];
            if(npe_world_battle_item_view(world,actor,raw,&item))return;
            own=item.target_side==1;group=item.group==1;effect=0;
        }else{
            NpeSkill skill;uint8_t raw[42];
            if(npe_world_battle_magic_view(world,actor,raw,&skill))return;
            own=skill.raw[38]==1;group=skill.raw[37]==1;effect=own?0:skill.effect_type;
        }
        if(npe_render_battle_party_spell_stage(world->battle_file,world,target,0,display->base))return;
        display->prepared=1;draw_pixels(display->renderer,display->texture,display->base);
        /* Source2F:CF6A/D0FA selects the character's hostile/own-side
           casting group. File85/86 are IDs, never fixed game resources. */
        unsigned casting=character->animation_ids[own?2:1];
        if(casting){
            if(play_battle_role_group(display,world->battle_file,casting,
                npe_world_battle_coordinate(world,0,actor,0),npe_world_battle_coordinate(world,0,actor,1),pixels,0,0,2))return;
            /* Source39:5EBA commits the restored caster scene once more. */
            draw_pixels(display->renderer,display->texture,display->base);
        }
        /* Own-side spells set7A90=0 at source2F:D124; their character
           animation is the entire effect, irrespective of skill raw12. */
        if(effect){
            int x=group?20:npe_world_battle_coordinate(world,1,target,0);
            int y=group?10:npe_world_battle_coordinate(world,1,target,1);
            if(play_battle_role_group(display,world->battle_file,effect,x,y,pixels,0,0,2))return;
        }
        return;
    }
    if(action==NPE_BATTLE_ANIMATION_ENEMY_RECOVERY || action==NPE_BATTLE_ANIMATION_ENEMY_SPELL){
        NpeSkill skill;uint8_t raw[42];
        unsigned id=world->battle_enemy_last_skill[world->battle_animation_enemy];
        if(!id||world->skill_record_id!=id||npe_world_skill_view(world,raw,&skill))return;
        display->prepared=1;
        unsigned recovery=action==NPE_BATTLE_ANIMATION_ENEMY_RECOVERY;
        if(recovery?npe_render_battle_enemy_recovery_stage(world->battle_file,world,target,0,display->base):
            npe_render_battle_enemy_spell_stage(world->battle_file,world,target,0,display->base))return;
        draw_pixels(display->renderer,display->texture,display->base);
        unsigned group=skill.effect_type;
        int x,y;
        if(recovery){x=npe_world_battle_coordinate(world,1,target,0);y=npe_world_battle_coordinate(world,1,target,1);}
        else {
                x=npe_world_battle_coordinate(world,0,target,0)-32;y=npe_world_battle_coordinate(world,0,target,1);
        }
        /* Source2F:DC46/DE4C: group hostile (110,25), own-side (20,20).
           DAFC/DB9C places a single hostile effect left32 of its receiver. */
        if(skill.raw[37]==1){x=recovery?20:110;y=recovery?20:25;}
        if(group && play_battle_role_group(display,world->battle_file,group,x,y,pixels,0,0,2))return;
        return;
    }
    if(action==NPE_BATTLE_ANIMATION_ENEMY_ATTACK){
        display->prepared=1;
        if(npe_render_battle_enemy_attack_stage(world->battle_file,world,target,0,display->base))return;
        draw_pixels(display->renderer,display->texture,display->base);
        return;
    }
    if(action==NPE_BATTLE_ANIMATION_DEFEND){
        display->prepared=1;
        if(npe_render_battle_defend_stage(world->battle_file,world,display->base))return;
        draw_pixels(display->renderer,display->texture,display->base);
        return;
    }
    if(action==NPE_BATTLE_ANIMATION_ATTACK){
        display->prepared=1;
        if(npe_render_battle_attack_stage(world->battle_file,world,target,0,display->base))return;
        draw_pixels(display->renderer,display->texture,display->base);
        /* Source39:5DFA..5E28: fallback pose5, wait10, then pose6.
           Use the battle service unit (20ms), keeping this pose visible. */
        game_Delay(200);
        if(npe_render_battle_attack_stage(world->battle_file,world,target,1,display->base))return;
        draw_pixels(display->renderer,display->texture,display->base);
        return;
    }
    if(npe_render_battle_combine_stage(world->battle_file,world,display->base))return;
    display->prepared=1;
    /* Source2F:D542 selects one-based group15 for the engine's type3 action.
       Frame count, dimensions and pixels always come from the loaded NPE. */
    if(play_battle_role_group(display,world->battle_file,15,
        npe_world_battle_coordinate(world,1,target,0),
        npe_world_battle_coordinate(world,1,target,1),pixels,0,1,0))return;
}

static int draw_battle_animation_commands(SDL_Renderer *renderer, SDL_Texture *texture,
                                          const NpeFile *file, const NpeWorld *world,
                                          const NpeFont *font, unsigned icon) {
    static const uint8_t body_label[]={0xcc,0xe5},qi_label[]={0xc6,0xf8};
    if (!world || world->battle_animation_observer!=play_battle_animation ||
        !world->battle_animation_context) return 0;
    BattleAnimationDisplay *display=world->battle_animation_context;
    if(display->menu_committed){display->menu_committed=0;return 1;}
    if (!display->awaiting_menu || world->game_mode!=3) return 0;
    uint8_t *pixels=display->base;
    unsigned actor=world->battle_actor;
    if (font && font->data &&
        (npe_draw_gbk_text(font,body_label,sizeof(body_label),70,88,pixels) ||
         npe_draw_gbk_text(font,qi_label,sizeof(qi_label),70,104,pixels))) return -1;
    npe_render_battle_hud_values(world->hp[actor],world->max_hp[actor],
        world->qi[actor],world->max_qi[actor],pixels);
    draw_pixels(renderer,texture,pixels);
    if (npe_render_battle_controls(file,icon,pixels)) return -1;
    draw_pixels(renderer,texture,pixels);
    if (npe_render_battle_cursor(file,world,pixels)) return -1;
    draw_pixels(renderer,texture,pixels);
    npe_render_battle_status_overlay(world,pixels);
    draw_pixels(renderer,texture,pixels);
    display->icon=(uint8_t)icon; display->menu_actor=(uint8_t)actor;
    display->prepared=display->awaiting_menu=0;
    return 1;
}

static int draw_battle_actor_advance(SDL_Renderer *renderer,SDL_Texture *texture,
    const NpeFile *file,const NpeWorld *world,const NpeFont *font,unsigned icon,unsigned menu_level) {
    if(!world || world->game_mode!=3 || icon>2 || menu_level ||
       world->battle_animation_observer!=play_battle_animation || !world->battle_animation_context ||
       last_pixel_renderer!=renderer)return 0;
    BattleAnimationDisplay *display=world->battle_animation_context;
    unsigned previous=display->menu_actor;
    if(!display->cached || display->awaiting_menu || display->epoch!=world->battle_display_epoch ||
       previous>=3 || world->battle_actor<=previous ||
       !(world->battle_pending_mask&(1u<<previous)))return 0;
    unsigned kind=world->battle_native_kinds[previous];
    int attack=icon==1 && kind==0;
    int effect=(icon==0 && kind==1)||(icon==2 && kind==2);
    int grouped=effect && world->battle_item_groups[previous]!=0;
    if(!attack && !effect && !(icon==2 && kind==5))return 0;
    unsigned compact=0;
    for(unsigned m=0;m<previous;++m)compact+=world->party_active[m]!=0;
    int left=npe_world_battle_coordinate(world,0,previous,0)+12+
        (int)((3*((2*compact+1)%4)+1+2*compact)/2);
    int top=npe_world_battle_coordinate(world,0,previous,1)-12;
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
    if(grouped){
        /* Direct group confirmation has no target page to retain. */
        if(npe_render_battle_actors_scene(file,world,pixels))return -1;
        draw_pixels(renderer,texture,pixels);
        if(npe_render_battle_controls(file,icon,pixels))return -1;
        draw_pixels(renderer,texture,pixels);
    }else memcpy(pixels,last_pixel_frame,sizeof(pixels));
    /* Single selectors clear the target arrow before the old actor cursor.
       Group confirmation rebuilds actors and controls first. Defense
       repeats its old-cursor clear. HUD and next cursor remain separate. */
    if(attack || (effect && !grouped)){
        if(effect){
            unsigned target=world->battle_commands[previous].target;
            if(world->battle_item_sides[previous]==1){
                if(npe_clear_battle_party_target(world,target,pixels))return -1;
            }else if(npe_clear_battle_enemy_target(world,target,pixels))return -1;
        }else apply_battle_enemy_pointer_clears(world,pixels);
        draw_pixels(renderer,texture,pixels);
    }
    for(int y=top;y<=top+4;++y)for(int x=left&~3;x<=left+8;++x)
        if(x>=0&&x<NPE_WIDTH&&y>=0&&y<NPE_HEIGHT)pixels[(unsigned)y*NPE_WIDTH+(unsigned)x]=0;
    draw_pixels(renderer,texture,pixels);
    if(!attack && !effect)draw_pixels(renderer,texture,pixels);
    /* The retained button plane can differ from7AC8 after a resident
      write. Advancing an actor restores that plane before the next HUD. */
    if(attack || effect){
        /* Target pages have no flag list rectangle to restore. Keep the
           old HUD until its own update; group controls are already drawn. */
        if(!grouped){
            for(unsigned y=88;y<NPE_HEIGHT;++y)
                memcpy(pixels+y*NPE_WIDTH,display->base+y*NPE_WIDTH,68);
            draw_pixels(renderer,texture,pixels);
        }
    }else if(draw_battle_command_return_plane(renderer,texture,file,font,world,icon,0,display->base))return -1;
    if(draw_battle_command_return_plane(renderer,texture,file,font,world,icon,1,display->base))return -1;
    memcpy(pixels,last_pixel_frame,sizeof(pixels));
    if(npe_render_battle_cursor(file,world,pixels))return -1;
    draw_pixels(renderer,texture,pixels);
    memcpy(display->base,pixels,sizeof(display->base));
    display->menu_actor=world->battle_actor;display->icon=(uint8_t)icon;
    return 1;
}

static int draw_battle_cached_commands(SDL_Renderer *renderer, SDL_Texture *texture,
    const NpeFile *file, const NpeWorld *world, unsigned icon, unsigned menu_level,
    unsigned selection, unsigned item_selection) {
    if (!world || world->battle_animation_observer!=play_battle_animation ||
        !world->battle_animation_context || world->game_mode!=3 || icon>=4 ||
        menu_level>2) return 0;
    BattleAnimationDisplay *display=world->battle_animation_context;
    if (!display->cached || (!world->battle_scene_refreshed&&!display->entry_cached) ||
        display->epoch!=world->battle_display_epoch ||
        display->menu_actor!=world->battle_actor) return 0;
    /* Native direction input redraws the controls and selects the icon on
       the current LCD. Do not reapply overlapping status cells afterwards. */
    /*31:DC08 /31:D4EA overlay the list without updating the buttons.
      A resident phase write can leave a different button on the LCD. */
    if (display->icon!=icon && !menu_level) {
        if (npe_render_battle_controls(file,icon,display->base)) return -1;
        display->icon=(uint8_t)icon;
    }
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
    memcpy(pixels,display->base,sizeof(pixels));
    apply_battle_enemy_pointer_clears(world,pixels);
    if (menu_level>=1 && npe_render_battle_command_menu(file,1,selection,pixels)) return -1;
    if (menu_level==2 && npe_render_battle_command_menu(file,2,item_selection,pixels)) return -1;
    draw_pixels(renderer,texture,pixels);
    return 1;
}

static int battle_status_input(SDL_Renderer *renderer,SDL_Texture *texture,
    const NpeFile *file,const NpeFont *font,NpeWorld *world,unsigned key){
    if(!world || world->game_mode!=3)return 0;
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
    if(world->battle_menu_phase==31 && world->battle_menu_row==4 && key==16){
        if(battle_resources_set(file,world,0))return -1;
        /*42:8EB2/E4CA displays the actor without writing7AAC. */
        world->battle_menu_phase=35;
        memset(pixels,0,sizeof(pixels));
        if(npe_render_native_battle_member_view(file,font,world,world->battle_actor,1,pixels))return -1;
        draw_pixels(renderer,texture,pixels);return 1;
    }
    if(world->battle_menu_phase!=35)return 0;
    if(key==1 || key==2){
        unsigned count=world->battle_record_party_count;
        if(!count){count=3;while(count&&!world->party_active[count-1])--count;}
        if(count==1)return 1;
        /*47:8C4E and49:A76A wrap only at their named endpoints. */
        uint8_t *member=&world->battle_status_member;
        *member=key==2?(*member==count-1?0:(uint8_t)(*member+1)):
                       (*member==0?(uint8_t)(count-1):(uint8_t)(*member-1));
        if(last_pixel_renderer!=renderer)return -1;
        memcpy(pixels,last_pixel_frame,sizeof(pixels));
        if(npe_render_native_battle_member_view(file,font,world,*member,0,pixels))return -1;
        draw_pixels(renderer,texture,pixels);return 1;
    }
    if(key==64){
        /*36:E520..E568 restores actors, HUD, cursor, statuses, controls
           and the flag list in six separate native LCD submissions. */
        if(battle_resources_set(file,world,1))return -1;
        world->battle_menu_phase=31;
        BattleAnimationDisplay *display=world->battle_animation_observer==play_battle_animation?
            world->battle_animation_context:NULL;
        if(display&&display->cached&&display->epoch==world->battle_display_epoch)
            memcpy(pixels,display->actors,sizeof(pixels));
        else if(npe_render_battle_actors_scene(file,world,pixels))return -1;
        draw_pixels(renderer,texture,pixels);
        if(draw_battle_command_return_stage(renderer,texture,file,font,world,2,1))return -1;
        memcpy(pixels,last_pixel_frame,sizeof(pixels));
        if(npe_render_battle_cursor(file,world,pixels))return -1;
        draw_pixels(renderer,texture,pixels);
        npe_render_battle_status_refresh(world,pixels);draw_pixels(renderer,texture,pixels);
        if(npe_render_battle_controls(file,2,pixels))return -1;
        draw_pixels(renderer,texture,pixels);
        if(display){memcpy(display->base,pixels,sizeof(pixels));display->icon=2;
            display->menu_actor=world->battle_actor;display->cached=1;}
        if(npe_render_battle_command_menu(file,1,world->battle_menu_row,pixels))return -1;
        draw_pixels(renderer,texture,pixels);return 1;
    }
    return 1; /* Unrelated inputs do not submit another status page. */
}

enum { WORLD_TIMER_MS = 4000 };

static int game_world_timer(const NpeFile *file, NpeWorld *world,
                             Uint32 now, Uint32 *deadline, int modal) {
    if (!SDL_TICKS_PASSED(now, *deadline)) return 0;
    /* 03:4154 -> source 4E:71D2 converts milliseconds to floor(ms*64/1000)
       ticks in shared 0496. The original 64-Hz ISR decrements it; event
       4 is consumed once and source 3C:D732..D73C rearms 4000 ms. */
    *deadline = now + WORLD_TIMER_MS;
    if (modal || world->game_mode != 1 || world->exit_choice) return 0;
    /* Source3C:D5E4..D5E8 runs chase first, then NPCs even if chase
       selected a battle. NPC movement shares the resident random stream. */
    int result = npe_world_update_chase(file, world);
    if (result < 0 || npe_world_update_npcs(file, world) < 0) return -1;
    return result;
}

static int game_world_timer_display(const NpeFile *file,NpeWorld *world,
                                     Uint32 now,Uint32 *deadline,int modal,
                                     SDL_Renderer *renderer,SDL_Texture *texture) {
    if(modal || world->game_mode!=1 || world->exit_choice || !SDL_TICKS_PASSED(now,*deadline))
        return game_world_timer(file,world,now,deadline,modal);
    NpeWorld before=*world;
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
    if(world_map_frame_valid && world_map_frame_file==file &&
       world_map_frame_renderer==renderer &&
       same_display_world(&world_map_frame_state,world))
        memcpy(pixels,world_map_frame,sizeof(pixels));
    else if(npe_render_world(file,world,pixels))return -1;
    int result=game_world_timer(file,world,now,deadline,modal);
    if(result<0)return result;
    if(npe_render_world_periodic(file,&before,world,pixels))return -1;
    if(world->game_mode!=1){
        /* Native type4 commits the NPC pass before deferred battle setup. */
        draw_pixels(renderer,texture,pixels);return result;
    }
    memcpy(world_map_frame,pixels,sizeof(pixels));world_map_frame_state=*world;
    world_map_frame_file=file;world_map_frame_renderer=renderer;world_map_frame_valid=1;
    world_map_frame_uncommitted=0;
    save_map_edges(file,renderer,pixels);
    return result;
}

static int commit_world_walk(void *opaque,const uint8_t *pixels,unsigned wait_parameter) {
    WalkDisplay *display=opaque;
    /* Original 46:BDD8..BDE2: wait(10 ms), then commit the retained LCD.
       4E:7286 arms max(1, floor(ms*64/1000)) on the shared 64-Hz clock;
       it does not restart that clock. Preserve the current tick phase. */
    if(wait_parameter){
        Uint32 start=SDL_GetTicks();
        Uint64 tick_count=(Uint64)wait_parameter*64u/1000u;
        if(!tick_count)tick_count=1;
        Uint64 next=(Uint64)start*64u/1000u+tick_count;
        Uint32 deadline=(Uint32)((next*1000u+63u)/64u);
        for(;;){
            Uint32 now=SDL_GetTicks();if(SDL_TICKS_PASSED(now,deadline))break;
            SDL_Event event;
            if(game_WaitEventTimeout(&event,(int)(deadline-now)) &&
               event.type==SDL_QUIT){SDL_PushEvent(&event);return 1;}
        }
    }
    draw_pixels(display->renderer,display->texture,pixels);
    return 0;
}

static int show_world_exit_notice(const NpeFile *file,const NpeWorld *world,
                                  SDL_Renderer *renderer,SDL_Texture *texture,const NpeFont *font,
                                  const uint8_t *pixels) {
    uint8_t notice[NPE_WIDTH*NPE_HEIGHT];memcpy(notice,pixels,sizeof(notice));
    if(npe_render_map_name_notice(file,font,world,notice))return -1;
    draw_pixels(renderer,texture,notice);
    /* Keep a bounded automatic timeout; confirmation may dismiss earlier. */
    Uint32 deadline=SDL_GetTicks()+5000;
    for(;;){Uint32 now=SDL_GetTicks();if(SDL_TICKS_PASSED(now,deadline))break;
        SDL_Event event;
        if(game_WaitEventTimeout(&event,(int)(deadline-now))){
            if(event.type==SDL_QUIT){SDL_PushEvent(&event);return -2;}
            /* Web WASM runtime: confirm dismisses the map label immediately.
               Consume it here, without triggering an action on the new map. */
            if(event.type==SDL_KEYDOWN && (event.key.keysym.sym==SDLK_RETURN ||
               event.key.keysym.sym==SDLK_KP_ENTER || event.key.keysym.sym==SDLK_k))break;
        }}
    draw_pixels(renderer,texture,pixels);return 0;
}

static void retain_world_frame(const NpeFile *file,const NpeWorld *world,SDL_Renderer *renderer,
                               const uint8_t *pixels,int uncommitted) {
    if(pixels!=world_map_frame)memcpy(world_map_frame,pixels,sizeof(world_map_frame));
    world_map_frame_state=*world;
    world_map_frame_file=file;world_map_frame_renderer=renderer;world_map_frame_valid=1;
    world_map_frame_uncommitted=uncommitted;
}

static int game_exit_choice_input(const NpeFile *file,NpeWorld *world,SDL_Keycode key,
                                  SDL_Renderer *renderer,SDL_Texture *texture,const NpeFont *font) {
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
    if(key==SDLK_UP || key==SDLK_DOWN){
        if(npe_world_select_exit(world,key==SDLK_UP?1:2))return -1;
        memcpy(pixels,last_pixel_frame,sizeof(pixels));npe_render_exit_choice_move(pixels);
        draw_pixels(renderer,texture,pixels);retain_world_frame(file,world,renderer,pixels,1);
    }else if(key==SDLK_RETURN || key==SDLK_k){
        if(npe_world_confirm_exit(file,world)<0 || npe_render_world_load(file,world,pixels))return -1;
        restore_map_edges(file,renderer,pixels);
        draw_pixels(renderer,texture,pixels);
        int shown=show_world_exit_notice(file,world,renderer,texture,font,pixels);if(shown)return shown;
        retain_world_frame(file,world,renderer,pixels,0);
    }else if(world_map_frame_valid)world_map_frame_uncommitted=1;
    return 0;
}

static int game_story_choice_input(const NpeFile *file,NpeWorld *world,SDL_Keycode key,
                                   SDL_Renderer *renderer,SDL_Texture *texture){
    if(!file||!world||world->game_mode!=17)return -1;
    if(key==SDLK_LEFT||key==SDLK_RIGHT||key==SDLK_j||key==SDLK_l){
        world->event_context_id=(uint16_t)((uint16_t)(world->event_context_id+1u)%2u);
        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];memcpy(pixels,last_pixel_frame,sizeof(pixels));
        npe_render_choice_event_move(pixels);draw_pixels(renderer,texture,pixels);
    }else if(key==SDLK_RETURN||key==SDLK_k){
        if(npe_world_select_choice(file,world,world->event_context_id==1?0:1)!=1)return -1;
        if(story_choice_background_valid && story_choice_background_file==file &&
           story_choice_background_renderer==renderer){
            /* Select the retained source without an extra LCD submission. */
            memcpy(last_pixel_frame,story_choice_background,sizeof(last_pixel_frame));
            save_map_edges(file,renderer,story_choice_background);
            story_choice_background_valid=0;
        }
        return 0;
    }
    return 0;
}

static int game_world_step_display(const NpeFile *file,NpeWorld *world,
                                   unsigned direction,SDL_Renderer *renderer,
                                   SDL_Texture *texture,const NpeFont *font) {
    if(world->game_mode!=1)return NPE_MOVE_BLOCKED;
    submit_pending_world_frame(file,renderer,texture);
    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
    if(world_map_frame_valid && world_map_frame_file==file &&
       world_map_frame_renderer==renderer &&
       same_display_world(&world_map_frame_state,world))
        memcpy(pixels,world_map_frame,sizeof(pixels));
    else if(npe_render_world(file,world,pixels))return -1;
    /* A resident party/state change can invalidate the host map cache.
       The native walking redraw still preserves the retained edge rows,
       including the cleared plane left by battle exit. */
    restore_map_edges(file,renderer,pixels);
    if(!world_map_saved_edges_valid || world_map_saved_edges_file!=file ||
       world_map_saved_edges_renderer!=renderer)save_map_edges(file,renderer,pixels);
    WalkDisplay display={renderer,texture};
    int result=npe_world_step_with_display(file,world,direction,pixels,commit_world_walk,&display);
    if(result<0)return result;
    if(result==NPE_MOVE_EXIT){int shown=show_world_exit_notice(file,world,renderer,texture,font,pixels);
        if(shown)return shown;}
    else if(result==NPE_MOVE_EXIT_CHOICE){
        memset(pixels,0,sizeof(pixels));draw_pixels(renderer,texture,pixels);
        if(npe_render_exit_choice(file,font,world,pixels))return -1;
        draw_pixels(renderer,texture,pixels);
    }
    world_map_frame_valid=0;
    if(world->game_mode==1 || world->game_mode==6){
        /* Trigger redraw reaches the ordinary map controller at37:1366,
           which calls4C:6AB8 instead of committing the chase LCD. */
        if(result==NPE_MOVE_EVENT && world->game_mode==1 &&
           npe_render_world_load(file,world,pixels))return -1;
        /* Menu entry restores the last committed walking background before
           drawing its panels. Chase has already updated the retained LCD and
           world state, but those uncommitted pixels are not its background. */
        memcpy(world_map_frame,world->game_mode==6 ? last_pixel_frame : pixels,sizeof(pixels));
        world_map_frame_state=*world;
        world_map_frame_file=file;world_map_frame_renderer=renderer;world_map_frame_valid=1;
        world_map_frame_uncommitted=(result==NPE_MOVE_OK || result==NPE_MOVE_BLOCKED ||
                                     result==NPE_MOVE_EXIT_CHOICE);
    }
    return result;
}

static int run(int argc, char **argv) {
    game_sound_player=NULL;
    world_map_frame_valid=0;
    world_map_frame_uncommitted=0;
    world_map_saved_edges_valid=0;
    system_menu_background_valid=system_menu_frame_valid=0;
    NpeFile file;
    uint8_t *bytes = NULL;
    size_t size = 0;
    char error[160], caption[256];
    unsigned section = 0, index = 0;
    unsigned map_index = 0, left = 0, top = 0;
    unsigned animation_group = 0, animation_frame = 0;
    NpeMap map;
    NpeWorld world={0};
    game_pad_mode=&world.game_mode;
    game_pad_show_help=show_controller_help;
    BattleAnimationDisplay battle_animation_display={0};
    unsigned system_menu_selection = 0;
    unsigned system_submenu_selection = 0;
    unsigned save_slot_selection = 0;
    unsigned save_slot_levels[3] = {0};
    unsigned status_member = 0;
    unsigned title_selection = 0;
    unsigned battle_skip_redraw = 0;
    unsigned map_ignored_input = 0;
    unsigned displayed_mode=0;
    int system_submenu_open = 0;
    int equipment_page_open = 0;
    int save_slots_open = 0, save_slot_saving = 0;
    int title_slots_retained = 0, title_skip_redraw = 0;
    unsigned start_map, start_x, start_y, start_left, start_top;
    unsigned start_progress = 0, npc_id, story_index, character_index, skill_id,
             enemy_index, encounter_map, battle_enemy_id;
    int start_fields=0, consumed=0;
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;
    NpeSoundPlayer sound = {0};
    NpeFont font = {0};
    char *save_paths[3] = {NULL, NULL, NULL};
    int running = 1, result = 1, inspect, inspect_maps, inspect_npc, inspect_story,
        inspect_character, inspect_skill, inspect_enemy, inspect_encounters;
    int map_preview, walk_preview, title_game, new_game, continue_game, resources,
        animation_preview,
        battle_asset_preview, battle_preview,
        animation_playing = 0;
    Uint32 world_timer_deadline;
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "usage: npe-sdl game.npe\n"
                        "       npe-sdl --new-game game.npe\n"
                        "       npe-sdl --continue game.npe\n"
                        "       npe-sdl --resources game.npe\n"
                        "       npe-sdl [--inspect|--inspect-maps|--map-preview] game.npe\n"
                        "       npe-sdl --inspect-npc id game.npe\n"
                        "       npe-sdl --inspect-story index game.npe\n"
                        "       npe-sdl --inspect-character index game.npe\n"
                        "       npe-sdl --inspect-skill id game.npe\n"
                        "       npe-sdl --inspect-enemy index game.npe\n"
                        "       npe-sdl --inspect-encounters map game.npe\n"
                        "       npe-sdl --animation-preview group game.npe\n"
                        "       npe-sdl --battle-assets game.npe\n"
                        "       npe-sdl --battle-preview enemy-id game.npe\n"
                        "       npe-sdl --walk-preview map,x,y[,left,top[,progress]] game.npe\n"); return 2;
    }
    inspect = argc == 3 && strcmp(argv[1], "--inspect") == 0;
    inspect_maps = argc == 3 && strcmp(argv[1], "--inspect-maps") == 0;
    map_preview = argc == 3 && strcmp(argv[1], "--map-preview") == 0;
    title_game = argc == 2;
    new_game = argc == 3 && strcmp(argv[1], "--new-game") == 0;
    continue_game = argc == 3 && strcmp(argv[1], "--continue") == 0;
    resources = argc == 3 && strcmp(argv[1], "--resources") == 0;
    walk_preview = argc == 4 && strcmp(argv[1], "--walk-preview") == 0;
    inspect_npc = argc == 4 && strcmp(argv[1], "--inspect-npc") == 0;
    inspect_story = argc == 4 && strcmp(argv[1], "--inspect-story") == 0;
    inspect_character = argc == 4 && strcmp(argv[1], "--inspect-character") == 0;
    inspect_skill = argc == 4 && strcmp(argv[1], "--inspect-skill") == 0;
    inspect_enemy = argc == 4 && strcmp(argv[1], "--inspect-enemy") == 0;
    inspect_encounters = argc == 4 &&
                         strcmp(argv[1], "--inspect-encounters") == 0;
    animation_preview = argc == 4 && strcmp(argv[1], "--animation-preview") == 0;
    battle_asset_preview = argc == 3 && strcmp(argv[1], "--battle-assets") == 0;
    battle_preview = argc == 4 && strcmp(argv[1], "--battle-preview") == 0;
    if ((argc == 3 && !inspect && !inspect_maps && !map_preview && !new_game &&
         !continue_game && !resources && !battle_asset_preview) ||
        (argc == 4 && !walk_preview && !inspect_npc && !inspect_story &&
         !inspect_character && !inspect_skill && !inspect_enemy && !inspect_encounters &&
         !animation_preview && !battle_preview)) {
        fprintf(stderr, "unknown option\n"); return 2;
    }
    if (animation_preview) {
        consumed = 0;
        if (sscanf(argv[2], "%u%n", &animation_group, &consumed) != 1 ||
            argv[2][consumed]) {
            fprintf(stderr, "invalid animation group\n"); return 2;
        }
    }
    if (inspect_npc) {
        consumed = 0;
        if (sscanf(argv[2], "%u%n", &npc_id, &consumed) != 1 ||
            argv[2][consumed] || !npc_id) {
            fprintf(stderr, "invalid NPC ID\n"); return 2;
        }
    }
    if (inspect_story) {
        consumed = 0;
        if (sscanf(argv[2], "%u%n", &story_index, &consumed) != 1 ||
            argv[2][consumed]) {
            fprintf(stderr, "invalid story index\n"); return 2;
        }
    }
    if (inspect_character) {
        consumed = 0;
        if (sscanf(argv[2], "%u%n", &character_index, &consumed) != 1 ||
            argv[2][consumed] || character_index >= 3) {
            fprintf(stderr, "invalid character index\n"); return 2;
        }
    }
    if (inspect_skill) {
        consumed = 0;
        if (sscanf(argv[2], "%u%n", &skill_id, &consumed) != 1 ||
            argv[2][consumed] || !skill_id) {
            fprintf(stderr, "invalid skill ID\n"); return 2;
        }
    }
    if (inspect_enemy) {
        consumed = 0;
        if (sscanf(argv[2], "%u%n", &enemy_index, &consumed) != 1 ||
            argv[2][consumed]) {
            fprintf(stderr, "invalid enemy index\n"); return 2;
        }
    }
    if (inspect_encounters) {
        consumed = 0;
        if (sscanf(argv[2], "%u%n", &encounter_map, &consumed) != 1 ||
            argv[2][consumed] || !encounter_map) {
            fprintf(stderr, "invalid map number\n"); return 2;
        }
    }
    if (battle_preview) {
        consumed = 0;
        if (sscanf(argv[2], "%u%n", &battle_enemy_id, &consumed) != 1 ||
            argv[2][consumed] || !battle_enemy_id) {
            fprintf(stderr, "invalid enemy ID\n"); return 2;
        }
    }
    if (walk_preview) {
        consumed = 0;
        start_fields = sscanf(argv[2], "%u,%u,%u,%u,%u,%u%n", &start_map,
                              &start_x, &start_y, &start_left, &start_top,
                              &start_progress, &consumed);
        if (start_fields != 6 || argv[2][consumed]) {
            consumed = 0;
            start_fields = sscanf(argv[2], "%u,%u,%u,%u,%u%n", &start_map,
                              &start_x, &start_y, &start_left, &start_top, &consumed);
        }
        if ((start_fields != 5 && start_fields != 6) || argv[2][consumed]) {
            consumed = 0;
            start_fields = sscanf(argv[2], "%u,%u,%u%n", &start_map,
                                  &start_x, &start_y, &consumed);
        }
        if ((start_fields != 3 && start_fields != 5 && start_fields != 6) ||
            argv[2][consumed] || start_progress > UINT16_MAX) {
            fprintf(stderr, "invalid start: expected map,x,y[,left,top[,progress]]\n"); return 2;
        }
    }
    if (SDL_Init(inspect || inspect_maps || inspect_npc || inspect_story ||
                 inspect_character || inspect_skill || inspect_enemy || inspect_encounters ?
                 0 : SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1;
    }
    if(SDL_WasInit(SDL_INIT_VIDEO)){
        settings_load();
        if(!SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER))game_pad_open();
    }
    if (read_file(argv[argc - 1], &bytes, &size)) goto done;
    if (npe_parse(&file, bytes, size, error, sizeof(error))) {
        fprintf(stderr, "invalid NPE: %s\n", error); goto done;
    }
    bytes = NULL;
    print_info(&file);
    if (title_game || new_game || continue_game || walk_preview)
        for (unsigned slot = 0; slot < 3; ++slot)
            save_paths[slot] = game_save_path(&file, slot);
    if (inspect) { result = 0; goto parsed_done; }
    if (inspect_maps) { result = print_map_info(&file); goto parsed_done; }
    if (inspect_npc) { result = print_npc_info(&file, npc_id); goto parsed_done; }
    if (inspect_story) { result = print_story_info(&file, story_index); goto parsed_done; }
    if (inspect_character) {
        result = print_character_info(&file, character_index); goto parsed_done;
    }
    if (inspect_skill) { result = print_skill_info(&file, skill_id); goto parsed_done; }
    if (inspect_enemy) { result = print_enemy_info(&file, enemy_index); goto parsed_done; }
    if (inspect_encounters) {
        result = print_encounter_info(&file, encounter_map); goto parsed_done;
    }
    if (battle_preview) {
        NpeEnemy enemy;
        if (npe_enemy(&file, battle_enemy_id - 1, &enemy) ||
            file.battle_bitmap_count < 29) {
            fprintf(stderr, "no supported battle scene resources\n");
            goto parsed_done;
        }
    } else if (battle_asset_preview) {
        if (!file.battle_bitmap_count) {
            fprintf(stderr, "no supported battle bitmap table\n");
            goto parsed_done;
        }
    } else if (animation_preview) {
        if (animation_group >= file.animation_group_count ||
            !npe_animation_frames(&file, animation_group)) {
            fprintf(stderr, "no such animation group\n"); goto parsed_done;
        }
    } else if (map_preview) {
        if (npe_map(&file, map_index, &map)) {
            fprintf(stderr, "no supported map records\n"); goto parsed_done;
        }
    } else if (title_game) {
        memset(&world, 0, sizeof(world));
        world.game_mode = 7;
        world.map_init_pending = 1;
        world.lcd_background = 2;
        world.music_enabled = 1;
        walk_preview = 1;
    } else if (new_game) {
        if (npe_world_new_game(&file, &world)) {
            fprintf(stderr, "unsupported new-game position\n"); goto parsed_done;
        }
        walk_preview = 1;
    } else if (continue_game) {
        if (!save_paths[0] || npe_world_load(&file, &world, save_paths[0])) {
            fprintf(stderr, "no valid C save for this NPE file%s%s\n",
                    save_paths[0] ? ": " : "",
                    save_paths[0] ? save_paths[0] : "");
            goto parsed_done;
        }
        walk_preview = 1;
    } else if (walk_preview) {
        if (npe_world_init(&file, &world, start_map, start_x, start_y)) {
            fprintf(stderr, "invalid starting map or position\n"); goto parsed_done;
        }
        if (start_fields == 6)
            world.story_progress = (uint16_t)start_progress;
        if (start_fields >= 5) {
            NpeMap start;
            if (npe_map(&file, start_map - 1, &start)) goto parsed_done;
            if (start_left > start_x || start_top > start_y ||
                start_x - start_left >= 15 || start_y - start_top >= 7 ||
                start_left + 15 > start.width || start_top + 7 > start.height) {
                fprintf(stderr, "invalid starting viewport\n"); goto parsed_done;
            }
            world.view.offset_x = start_left;
            world.view.offset_y = start_top;
            world.view.screen_x = start_x - start_left;
            world.view.screen_y = start_y - start_top;
        }
    } else {
        section = first_bitmap(&file);
        if (section == file.section_count) {
            fprintf(stderr, "no supported full-screen bitmap section\n"); goto parsed_done;
        }
    }
    if ((walk_preview ? npe_world_resources_open(&file,&world) :
         (battle_asset_preview||battle_preview ? npe_rpg_runtime_open(&file,NULL) : 0))) {
        fprintf(stderr,"cannot initialize RPG resource runtime\n");goto parsed_done;
    }
    if(walk_preview && continue_game && npe_rpg_runtime_map_load(&file,&world,1)){
        fprintf(stderr,"cannot restore RPG map runtime\n");goto parsed_done;
    }
    if (walk_preview && !continue_game) {
        npe_world_seed(&world, (uint32_t)SDL_GetPerformanceCounter());
        /* Host starting seed is installed before the audible map choice.
           The engine's deterministic initialization request was unbound. */
        if (world.game_mode == 1) {
            if (new_game) npe_world_sound_map_resume(&world);
            else npe_world_sound_map_loaded(&world);
        }
    }
    window = SDL_CreateWindow("诺亚舟-NPE-引擎", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, 960, 480, SDL_WINDOW_RESIZABLE);
    if (!window) goto sdl_error;
    desktop_settings_callback=request_settings_page;
#ifdef __APPLE__
    npe_macos_settings_menu();
#endif
    game_view_file=&file;game_view_world=&world;game_view_playing=&walk_preview;
    if(desktop_menu_attach(window)<0)fprintf(stderr,"cannot create desktop menu\n");
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) goto sdl_error;
    SDL_RenderSetLogicalSize(renderer, NPE_WIDTH, NPE_HEIGHT);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, NPE_WIDTH, NPE_HEIGHT);
    if (!texture) goto sdl_error;
    if (walk_preview){npe_sound_open(&sound, &file);game_sound_player=&sound;}
    if ((walk_preview || battle_preview) && load_native_fonts(&font)) goto parsed_done;
    if (title_game) {
        int played;
        npe_sound_select(&sound, &world);
        /* Source3C:D45C..D492: current-NPE group1, (0,0), mode0/erase0.
           D498's 4000 arms the periodic timer; it is NOT a post-scene delay. */
        played = play_scene_group(renderer, texture, &file, &world, 1, 0, 0, 0, 0);
        if (played == -2) { result = 0; goto parsed_done; }
        if (played < 0)
            fprintf(stderr, "opening scene animation is unsupported\n");
    }
    world_timer_deadline = SDL_GetTicks() + WORLD_TIMER_MS;
    battle_animation_display.renderer=renderer;
    battle_animation_display.texture=texture;
    battle_animation_display.font=&font;
    while (running) {
        SDL_Event event;
        Uint32 ignored_map_deadline=world_timer_deadline;
        int skip_ignored_map_draw=map_ignored_input && world.game_mode==1;
        map_ignored_input=0;
        /* A scope belongs to one input/dispatch pass. Early continue and
           warm constructors must not change numeric APIs outside it. */
        world.defer_story_events=0;
        if (walk_preview) {
            world.battle_animation_observer=play_battle_animation;
            world.battle_animation_context=&battle_animation_display;
            npe_sound_select(&sound, &world);
        }
        if (walk_preview &&
            game_world_timer_display(&file, &world, SDL_GetTicks(),
                               &world_timer_deadline, save_slots_open,renderer,texture) < 0) {
            fprintf(stderr, "world timer hit unsupported map data\n");
            goto parsed_done;
        }
        skip_ignored_map_draw=skip_ignored_map_draw && world.game_mode==1 &&
            world_timer_deadline==ignored_map_deadline;
        if (battle_preview)
            snprintf(caption, sizeof(caption),
                     "NPE battle layout preview - enemy %u/%u",
                     battle_enemy_id, file.offset_table_records[4]);
        else if (battle_asset_preview)
            snprintf(caption, sizeof(caption),
                     "NPE battle assets - image %u/%u",
                     index + 1, file.battle_bitmap_count);
        else if (walk_preview)
            snprintf(caption, sizeof(caption), "诺亚舟-NPE-引擎");
        else if (animation_preview)
            snprintf(caption, sizeof(caption),
                     "NPE animation - group %u/%u, frame %u/%u%s",
                     animation_group + 1, file.animation_group_count,
                     animation_frame + 1,
                     npe_animation_frames(&file, animation_group),
                     animation_playing ? " (playing)" : "");
        else if (map_preview)
            snprintf(caption, sizeof(caption),
                     "NPE map preview - record %u, map %u, %ux%u, view %u,%u",
                     map_index, map.number, map.width, map.height, left, top);
        else
            snprintf(caption, sizeof(caption),
                     "NPE resource viewer - section %u, image %u/%u",
                     section, index + 1, file.sections[section].bitmap_count);
        SDL_SetWindowTitle(window, caption);
        if (battle_preview) {
            if (draw_battle_scene(renderer, texture, &file,
                                  battle_enemy_id, NULL, &font,
                                  1, 0, 0, 0))
                goto parsed_done;
        } else if (battle_asset_preview) {
            if (draw_battle_asset(renderer, texture, &file, index)) goto parsed_done;
        } else if (animation_preview) {
            if (draw_animation(renderer, texture, &file, animation_group,
                               animation_frame)) goto parsed_done;
        } else if (walk_preview) {
            int drawn;
            if(world.game_mode==7 && title_skip_redraw && !world.map_init_pending){
                title_skip_redraw=0;drawn=0;
            } else if (world.game_mode == 7) {
                title_skip_redraw=0;
                int initialize_title=world.map_init_pending==1;
                if(initialize_title){
                    /*3A:8A4E..8A6A calls48:0DA2 with selection1,
                      then consumes7AB4 after the title submission. */
                    title_selection=save_slot_selection=0;
                    save_slots_open=save_slot_saving=0;
                    title_slots_retained=0;
                    if(initialize_title_controls(&file,renderer,texture,0))goto parsed_done;
                }
                uint8_t selected=(uint8_t)(title_selection+1);
                if(npe_rpg_runtime_write(&file,0x527a7c,&selected,1))goto parsed_done;
                uint8_t title[NPE_WIDTH * NPE_HEIGHT];
                if(save_slots_open && title_slots_retained){
                    memcpy(title,last_pixel_frame,sizeof(title));
                } else {
                    if (npe_render_native_title_screen(&file, &font,
                                                       title_selection, title)) {
                        fprintf(stderr, "cannot render title from current NPE\n");
                        goto parsed_done;
                    }
                    if (save_slots_open &&
                        npe_render_save_slots_overlay(&file, 158, 45,
                            save_slot_levels, save_slot_selection,
                            font.data16, title)) {
                        fprintf(stderr, "cannot render NPE save slots\n");
                        goto parsed_done;
                    }
                }
                draw_pixels(renderer, texture, title);
                title_slots_retained=save_slots_open;
                if(initialize_title)world.map_init_pending=0;
                drawn = 0;
            } else if(world.game_mode==1 && !displayed_mode && !world_map_frame_valid) {
                /* Source4C:6AB8..6BF8: the first map display uses load
                   order, not the generic preview's Y-sorted actors.
                   Keep this actual entry plane for subsequent input. */
                uint8_t loaded[NPE_WIDTH*NPE_HEIGHT];
                drawn=npe_render_world_load(&file,&world,loaded);
                if(!drawn){
                    draw_pixels(renderer,texture,loaded);
                    retain_world_frame(&file,&world,renderer,loaded,0);
                }
            } else if(skip_ignored_map_draw){
                drawn=0;
            } else if(world.game_mode==3 && (battle_skip_redraw || world.battle_menu_phase==35 || battle_menu_icon(&world)<0)){
                battle_skip_redraw=0;drawn=0;
            } else drawn = world.game_mode == 3 ?
                        (displayed_mode!=3 ? draw_battle_entry(renderer,texture,&file,&world,&font) :
                         draw_battle_scene(renderer, texture, &file,
                                          world.battle_enemy_id, &world, &font,
                                          (unsigned)battle_menu_icon(&world), battle_menu_level(&world),
                                          world.battle_menu_row,
                                          world.battle_item_row)) :
                        draw_world(renderer, texture, &file, &world,
                                   system_menu_selection, status_member,
                                   system_submenu_open,
                                   system_submenu_selection,
                                   equipment_page_open,
                                   save_slots_open, save_slot_levels,
                                   save_slot_selection, &font);
            if (drawn) {
                fprintf(stderr, "walk preview cannot render current state\n");
                goto parsed_done;
            }
        } else if (map_preview) {
            if (draw_map(renderer, texture, &file, &map, left, top)) {
                fprintf(stderr, "map %u references an invalid 16x16 tile\n", map.number);
                goto parsed_done;
            }
        } else draw_bitmap(renderer, texture, &file, section, index);
        if(walk_preview)displayed_mode=world.game_mode;
        {
            int wait_ms = animation_preview && animation_playing ? 120 : -1;
            if (walk_preview) {
                Uint32 now = SDL_GetTicks();
                wait_ms = SDL_TICKS_PASSED(now, world_timer_deadline) ? 0 :
                          (int)(world_timer_deadline - now);
            }
            if (!game_WaitEventTimeout(&event, wait_ms)) {
                if (animation_preview && animation_playing) {
                    animation_frame = (animation_frame + 1) %
                        npe_animation_frames(&file, animation_group);
                    continue;
                }
                if (walk_preview) continue;
                break;
            }
        }
        if (event.type == SDL_QUIT) running = 0;
        else if (event.type == SDL_KEYDOWN) {
            SDL_Keycode key = event.key.keysym.sym;
            if(walk_preview && (world.game_mode==1 || world.game_mode==6))
                key=native_menu_navigation_key(key);
            /*3A:8A72..9146 ignores unsupported title/slot keys. F4
              cancels only stage2; no unchanged LCD submission follows. */
            if(walk_preview && world.game_mode==7 &&
               key!=SDLK_UP && key!=SDLK_DOWN && key!=SDLK_i && key!=SDLK_m &&
               key!=SDLK_RETURN && key!=SDLK_KP_ENTER && key!=SDLK_k &&
               key!=SDLK_ESCAPE && !(save_slots_open&&(key==SDLK_F4||key==SDLK_BACKSPACE))){
                title_skip_redraw=1;continue;
            }
            if (walk_preview && save_slots_open) {
                unsigned previous_slot_selection=save_slot_selection;
                if (key == SDLK_DOWN || key == SDLK_m)
                    save_slot_selection = (save_slot_selection + 1) % 3;
                else if (key == SDLK_UP || key == SDLK_i)
                    save_slot_selection = (save_slot_selection + 2) % 3;
                else if (key == SDLK_F4 || key == SDLK_ESCAPE ||
                         (key == SDLK_BACKSPACE && world.game_mode!=6)) {
                    save_slots_open = 0;
                    if (world.game_mode == 7) {
                        /*3A:912A..9144 restores stage1 before48:0DA2.
                          The initializer retains the title choice, resets
                          slot1 and submits a cleared plane before title. */
                        uint8_t stage=1;
                        if(npe_rpg_runtime_write(&file,0x527a7e,&stage,1) ||
                           initialize_title_controls(&file,renderer,texture,title_selection))
                            goto parsed_done;
                        save_slot_selection=0;
                    } else if (world.game_mode == 6) {
                        system_submenu_open = 0;
                        npe_world_close_system_menu(&world);
                        if(restore_system_menu_background(&file,&world,renderer,texture))
                            goto parsed_done;
                    }
                } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER ||
                           key == SDLK_k) {
                    const char *path = save_paths[save_slot_selection];
                    if (save_slot_saving) {
                        NpeWorld saved = world;
                        saved.game_mode = 1;
                        if (path && !npe_world_save(&file, &saved, path)) {
                            save_slot_levels[save_slot_selection] =
                                saved.level[0];
                            save_slots_open = 0;
                            system_submenu_open = 0;
                            npe_world_close_system_menu(&world);
                            /* System save sets the native map-init flag. */
                            npe_world_sound_map_resume(&world);
                            if (show_system_map_return(&file, &world, renderer, texture))
                                goto parsed_done;
                        } else {
                            int shown = show_save_failure(renderer, texture, &font, 1);
                            if (shown < 0) running = 0;
                            else {
                                save_slots_open = system_submenu_open = 0;
                                npe_world_close_system_menu(&world);
                                npe_world_sound_map_resume(&world);
                                if (show_system_map_return(&file, &world, renderer, texture))
                                    goto parsed_done;
                            }
                        }
                    } else {
                        unsigned load_mode = world.game_mode;
                        if(load_mode==7 && !save_slot_levels[save_slot_selection]){
                            /*3A:8FB0..9020 tests the published slot level.
                              Empty slots start a new game without a read. */
                            if(start_title_game(&file,&world,&sound,renderer,texture))
                                goto parsed_done;
                            save_slots_open=system_submenu_open=0;
                        } else if (path && !npe_world_restore(&file, &world, path)) {
                            save_slots_open = 0;
                            system_submenu_open = 0;
                            /* 37:D088 sets map-init; title 3A:90E2 does not. */
                            if (load_mode == 6) {
                                npe_world_sound_map_resume(&world);
                                if (show_system_map_return(&file, &world, renderer, texture))
                                    goto parsed_done;
                            } else if (load_mode == 7) {
                                /* The slot loader rebuilds the map using the
                                   resident graphics background. The title
                                   controller commits it once, without replay. */
                                uint8_t loaded[NPE_WIDTH * NPE_HEIGHT];
                                if (npe_render_world_load(&file, &world, loaded))
                                    goto parsed_done;
                                retain_world_frame(&file, &world, renderer, loaded, 0);
                            }
                        } else if (load_mode == 6) {
                            int shown = show_save_failure(renderer, texture, &font, 0);
                            if (shown < 0) running = 0;
                            else {
                                save_slots_open = system_submenu_open = 0;
                                npe_world_close_system_menu(&world);
                                npe_world_sound_map_resume(&world);
                                if (show_system_map_return(&file, &world, renderer, texture))
                                    goto parsed_done;
                            }
                        } else if (load_mode == 7) {
                            /*3A:9076..90DE leaves mode1, stops playback and
                              sets the exit word after an occupied read fails.
                              Final teardown releases the host resources. */
                            uint8_t exit_word[2]={0,1};
                            world.game_mode=1;
                            npe_world_sound_stop(&world);
                            if(npe_rpg_runtime_write(&file,0x527a3c,exit_word,2))goto parsed_done;
                            running=0;
                        }
                    }
                } else continue;
                if(world.game_mode==7 && save_slots_open){
                    /*3A:8C7E/8E9E publishes the one-based chooser row.
                      These bytes are shared with later original services. */
                    uint8_t slot=(uint8_t)(save_slot_selection+1);
                    if(npe_rpg_runtime_write(&file,0x527a7a,&slot,1))goto parsed_done;
                    if(previous_slot_selection!=save_slot_selection && title_slots_retained)
                        switch_title_slot_row(previous_slot_selection,save_slot_selection);
                }
            }
            else if(walk_preview && world.game_mode==1 && world.exit_choice &&
                    key!=SDLK_F4 && key!=SDLK_SPACE){
                int chosen=game_exit_choice_input(&file,&world,key,renderer,texture,&font);
                if(chosen==-2){result=0;goto parsed_done;}
                if(chosen<0)goto parsed_done;
            }
            else if ((key == SDLK_ESCAPE || key == SDLK_F4) && walk_preview &&
                world.game_mode == 1) {
                if (npe_world_open_system_menu(&world) == 1) {
                    system_menu_background_valid=0;
                    system_menu_selection = 0;
                    system_submenu_open = 0;
                }
            }
            else if (key == SDLK_F4 && walk_preview && world.game_mode == 6) {
                /* Native menu event 0x8e leaves every mode-6 menu level. */
                system_submenu_open = 0;
                npe_world_close_system_menu(&world);
                if(restore_system_menu_background(&file,&world,renderer,texture))
                    goto parsed_done;
            }
            else if (key == SDLK_ESCAPE && walk_preview &&
                     world.game_mode == 6) {
                if (system_submenu_open) system_submenu_open = 0;
                else npe_world_close_system_menu(&world);
            }
            else if (key == SDLK_ESCAPE && walk_preview &&
                     world.game_mode == 3 && battle_menu_level(&world))
                world.battle_menu_phase=world.battle_menu_phase==32?31:30;
            else if (key == SDLK_ESCAPE && walk_preview &&
                     world.game_mode == 4 && equipment_page_open) {
                /* Host escape while native key delivery is unresolved. */
                if (npe_world_close_menu(&world) == 1)
                    equipment_page_open = 0;
            }
            else if (key == SDLK_ESCAPE) running = 0;
            else if (battle_preview) {
                unsigned count = file.offset_table_records[4];
                if (key == SDLK_RIGHT || key == SDLK_DOWN)
                    battle_enemy_id = battle_enemy_id % count + 1;
                else if (key == SDLK_LEFT || key == SDLK_UP)
                    battle_enemy_id = (battle_enemy_id + count - 2) % count + 1;
            }
            else if (battle_asset_preview) {
                unsigned count = file.battle_bitmap_count;
                if (key == SDLK_RIGHT || key == SDLK_DOWN)
                    index = (index + 1) % count;
                else if (key == SDLK_LEFT || key == SDLK_UP)
                    index = (index + count - 1) % count;
            }
            else if (animation_preview) {
                unsigned count = npe_animation_frames(&file, animation_group);
                if (key == SDLK_SPACE) animation_playing = !animation_playing;
                else if (key == SDLK_RIGHT || key == SDLK_DOWN)
                    animation_frame = (animation_frame + 1) % count;
                else if (key == SDLK_LEFT || key == SDLK_UP)
                    animation_frame = (animation_frame + count - 1) % count;
                else if (key == SDLK_PAGEDOWN || key == SDLK_PAGEUP) {
                    unsigned candidate = animation_group, attempts;
                    for (attempts = 0; attempts < file.animation_group_count; ++attempts) {
                        candidate = key == SDLK_PAGEDOWN ?
                            (candidate + 1) % file.animation_group_count :
                            (candidate + file.animation_group_count - 1) %
                            file.animation_group_count;
                        if (npe_animation_frames(&file, candidate)) {
                            animation_group = candidate;
                            animation_frame = 0;
                            break;
                        }
                    }
                }
            }
            else if (walk_preview) {
                unsigned direction = key == SDLK_DOWN ? 0 : key == SDLK_RIGHT ? 1 :
                                     key == SDLK_UP ? 2 : key == SDLK_LEFT ? 3 : 4;
                /*37:19D8 returns directly for unrecognized map keys. Do
                  not submit/advance an unrelated map continuation here. */
                if(world.game_mode==1 && direction>=4 && key!=SDLK_SPACE &&
                   key!=SDLK_RETURN && key!=SDLK_k){map_ignored_input=1;continue;}
                unsigned previous_mode = world.game_mode;
                if(world.game_mode==1)submit_pending_world_frame(&file,renderer,texture);
                int quiz_requested = 0;
                world.defer_story_events=1;
                if (world.game_mode == 7) {
                    if (key == SDLK_DOWN || key == SDLK_m)
                        title_selection = (title_selection + 1) % 3;
                    else if (key == SDLK_UP || key == SDLK_i)
                        title_selection = (title_selection + 2) % 3;
                    else if (key == SDLK_RETURN ||
                             key == SDLK_KP_ENTER || key == SDLK_k) {
                        if (title_selection == 0) {
                            if (start_title_game(&file, &world, &sound,
                                                 renderer, texture))
                                goto parsed_done;
                        } else if (title_selection == 1) {
                            /*3A:8F26..8F42 opens the occupied-slot chooser
                              without an audio stop.8F74 belongs to exit. */
                            game_save_slot_levels(&file, save_paths,
                                                  save_slot_levels);
                            save_slot_selection = 0;
                            save_slot_saving = 0;
                            save_slots_open = 1;
                            title_slots_retained = 0;
                            uint8_t stage=2,slot=1;
                            if(npe_rpg_runtime_write(&file,0x527a7e,&stage,1) ||
                               npe_rpg_runtime_write(&file,0x527a7a,&slot,1))goto parsed_done;
                        } else running = 0;
                    }
                }
                else if (world.game_mode == 6) {
                    if(world.property_page==9 || world.property_page==10){
                        if(world.property_page==10 && key==SDLK_u){
                            int changed=npe_world_unequip_selected(&file,&world);
                            if(changed<0)goto parsed_done;
                            if(!changed)continue;
                            uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
                            memcpy(pixels,last_pixel_frame,sizeof(pixels));
                            if(npe_render_equipment_unequip(&world,pixels))goto parsed_done;
                            draw_pixels(renderer,texture,pixels);
                            /* U commits once, retaining the page controls and indices.
                               Do not let generic navigation redraw its selector. */
                            system_menu_frame_state=world;
                            system_menu_frame_valid=1;
                            continue;
                        }else if(world.property_page==10 && (key==SDLK_n || key==SDLK_p)){
                            int changed=npe_world_equipment_description_page(&file,&world,key==SDLK_n?1:-1);
                            if(changed<0)goto parsed_done;
                            if(!changed){
                                /* Original boundary return commits no LCD. A live index
                                   change must not cause the next host iteration to redraw. */
                                if(system_menu_frame_valid && system_menu_background_file==&file &&
                                   system_menu_background_renderer==renderer)
                                    system_menu_frame_state=world;
                                continue;
                            }
                        }else if(world.property_page==10 && (key==SDLK_LEFT || key==SDLK_j || key==SDLK_RIGHT || key==SDLK_l)){
                            world.equipment_view_slot=(uint8_t)((world.equipment_view_slot+(key==SDLK_LEFT||key==SDLK_j?5:1))%6);
                        }else if(key==SDLK_UP || key==SDLK_i || key==SDLK_DOWN || key==SDLK_m){
                            unsigned count=world.menu_party_count;
                            if(count<=1)continue;
                            world.property_member=(uint8_t)((world.property_member+(key==SDLK_UP||key==SDLK_i?count-1:1))%count);
                        }else continue;
                    }else{
                    unsigned *selection = system_submenu_open ?
                        &system_submenu_selection : &system_menu_selection;
                    unsigned selection_count =
                        system_submenu_open &&
                        (system_menu_selection == 0 ||
                         system_menu_selection == 2) ? 2 : 4;
                    if (key == SDLK_DOWN)
                        *selection = (*selection + 1) % selection_count;
                    else if (key == SDLK_UP)
                        *selection = (*selection + selection_count - 1) %
                                     selection_count;
                    else if (key == SDLK_RETURN ||
                             key == SDLK_KP_ENTER || key == SDLK_k) {
                        if (system_submenu_open) {
                            if (system_menu_selection == 0) {
                                if (system_submenu_selection == 0) {
                                    status_member = 0;
                                    equipment_page_open = 0;
                                    system_submenu_open = 0;
                                    world.property_page=9;world.property_member=0;
                                } else {
                                    status_member = 0;
                                    equipment_page_open = 0;
                                    system_submenu_open = 0;
                                    world.property_page=10;world.property_member=0;world.equipment_view_slot=0;
                                }
                            } else if (system_menu_selection == 2) {
                                if (system_submenu_selection == 0)
                                    show_use_item(window, renderer, texture,
                                                  &file, &font, &world, 0);
                                else
                                    show_native_equip_choice(window, renderer,
                                                             texture, &file,
                                                             &font, &world);
                            } else if (system_submenu_selection == 0) {
                                game_save_slot_levels(&file, save_paths,
                                                      save_slot_levels);
                                save_slot_selection = 0;
                                save_slot_saving = 0;
                                save_slots_open = 1;
                            } else if (system_submenu_selection == 1) {
                                game_save_slot_levels(&file, save_paths,
                                                      save_slot_levels);
                                save_slot_selection = 0;
                                save_slot_saving = 1;
                                save_slots_open = 1;
                            } else if (system_submenu_selection == 2) {
                                int choice=show_music_choice(renderer, texture, &file, &world);
                                if(choice<0)running=0;
                                else if(choice ? show_system_map_return(&file,&world,renderer,texture) :
                                                  restore_system_menu_background(&file,&world,renderer,texture))
                                    goto parsed_done;
                                system_submenu_open = 0;
                            } else running = 0;
                        }
                        else if (system_menu_selection == 0) {
                            system_submenu_open = 1;
                            system_submenu_selection = 0;
                        }
                        else if (system_menu_selection == 1)
                            show_magic_list(window, renderer, texture,
                                            &file, &font, &world);
                        else if (system_menu_selection == 2) {
                            system_submenu_open = 1;
                            system_submenu_selection = 0;
                        }
                        else {
                            system_submenu_open = 1;
                            system_submenu_selection = 0;
                        }
                    }
                    else continue;
                    }
                }
                else if(world.game_mode==4 && world.story_event_id!=2){
                    if(key==SDLK_UP || key==SDLK_DOWN || key==SDLK_i || key==SDLK_m){
                        if(world.shop_ui!=1 || (world.shop_kind!=1 && world.shop_kind!=2))continue;
                        int moved=game_shop_move_display(renderer,texture,&file,&font,&world,key==SDLK_UP||key==SDLK_i?-1:1);
                        if(moved==-2){result=0;goto parsed_done;}
                        if(moved<0){fprintf(stderr,"invalid shop navigation\n");goto parsed_done;}
                        continue;
                    }else if(key==SDLK_RETURN || key==SDLK_KP_ENTER || key==SDLK_k){
                        if(world.shop_ui!=1 || (int16_t)world.shop_selected_item<=0)continue;
                        if(show_shop_quantity(renderer,texture,&file,&font,&world)<0)running=0;
                        continue;
                    }else if(key==SDLK_l){
                        /*4C:70B0 calls4F:6BC2 without the UI=2 gate used for J.
                          Rectangle writes update the drawing plane; this service
                          submits the previous visible plane. The next list draw
                          exposes the writes retained here. */
                        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
                        memcpy(pixels,last_pixel_frame,sizeof(pixels));
                        for(unsigned y=87;y<=99;++y){
                            for(unsigned x=67;x<=100;++x)shop_draw_frame[y*NPE_WIDTH+x]=15-shop_draw_frame[y*NPE_WIDTH+x];
                            for(unsigned x=141;x<=175;++x)shop_draw_frame[y*NPE_WIDTH+x]=15-shop_draw_frame[y*NPE_WIDTH+x];
                        }
                        world.shop_choice=(uint8_t)((world.shop_choice+1)%2);
                        draw_pixels(renderer,texture,pixels);retain_shop_frame(&file,&world,renderer,NULL);
                        continue;
                    }else if(key==SDLK_F4 || key==SDLK_BACKSPACE){
                        if(game_shop_return_display(renderer,texture,&file,&world)<0)goto parsed_done;
                        continue;
                    }else continue;
                }
                else if (world.game_mode == 4 &&
                         (key == SDLK_LEFT || key == SDLK_RIGHT ||
                          key == SDLK_UP || key == SDLK_DOWN)) {
                    int delta = key == SDLK_RIGHT || key == SDLK_DOWN ? 1 : -1;
                    unsigned attempts;
                    if (equipment_page_open) continue;
                    for (attempts = 0; attempts < 3; ++attempts) {
                        status_member = delta > 0 ?
                            (status_member + 1) % 3 :
                            (status_member + 2) % 3;
                        if (world.party_active[status_member]) break;
                    }
                }
                else if (world.game_mode == 4 &&
                         (key == SDLK_BACKSPACE || key == SDLK_F4)) {
                    if (equipment_page_open) continue;
                    if (npe_world_close_menu(&world) != 1)
                        fprintf(stderr, "menu return is unsupported\n");
                }
                else if (world.game_mode == 4 &&
                         (key == SDLK_RETURN || key == SDLK_SPACE)) {
                    /* The native status page is already on screen. The
                       firmware's 0x86 branch at 4C:70B8..70E0 does not
                       display the desktop's English summary dialog. */
                }
                else if (world.game_mode == 17) {
                    if (game_story_choice_input(&file,&world,key,renderer,texture))
                        fprintf(stderr, "choice event is unsupported\n");
                }
                else if (world.game_mode == 8) {
                    quiz_requested = 1;
                }
                else if (world.game_mode == 3) {
                    key=native_menu_navigation_key(key);
                    int handled = 0;
                    unsigned flag_action=0;
                    unsigned native_key=key==SDLK_UP?1:key==SDLK_DOWN?2:key==SDLK_LEFT?4:
                        key==SDLK_RIGHT?8:key==SDLK_RETURN?16:
                        key==SDLK_F4 || key==SDLK_BACKSPACE?64:0;
                    /*47:536A..54AA returns for unsupported raw keys. Only
                      a flag-list confirmation may request defense/flee. */
                    if(!native_key){battle_skip_redraw=1;continue;}
                    int status_input=battle_status_input(renderer,texture,&file,&font,&world,native_key);
                    if(status_input<0)goto parsed_done;
                    int navigation=status_input?0:battle_menu_navigation(&world,native_key);
                    if(status_input){handled=1;battle_skip_redraw=1;}
                    else if(navigation){
                        handled=1;
                        if(navigation==2){
                            for(unsigned stage=0;stage<2;++stage)
                                if(draw_battle_command_return_stage(renderer,texture,&file,&font,&world,
                                    (unsigned)battle_menu_icon(&world),stage))goto parsed_done;
                            battle_skip_redraw=1;
                        }else if(navigation==3)battle_skip_redraw=1;
                    }else if (world.battle_menu_phase == 32) {
                        handled = 1;
                        if (key == SDLK_RETURN) {
                            unsigned item_mode = world.battle_item_row + 1;
                            int chosen=show_use_item(window,renderer,texture,&file,&font,&world,(int)item_mode);
                            if(!chosen)world.battle_menu_phase=32;
                        }
                    } else if (world.battle_menu_phase == 31) {
                        handled = 1;
                        if (key == SDLK_RETURN) {
                            if (world.battle_menu_row == 2) {
                                flag_action=1;
                                handled = 0;
                            } else if (world.battle_menu_row == 3) {
                                flag_action=2;
                                handled = 0;
                            } else if (!world.battle_lost) {
                                if (npe_world_battle_group_attack(&world) < 0)
                                    fprintf(stderr, "group attack unavailable\n");
                            }
                        } else {
                            /* Flag-list wait consumes unsupported keys
                               without another LCD submission. */
                            battle_skip_redraw=1;
                        }
                    } else if (key == SDLK_F4 || key == SDLK_BACKSPACE) {
                        if (npe_world_battle_previous_actor(&world) == 0) world.battle_menu_phase=10;
                        handled = 1;
                    } else if (key == SDLK_RETURN) {
                        if (world.battle_menu_phase == 20) {
                            show_battle_skill(window, renderer, texture,
                                              &file, &font, &world);
                            handled = 1;
                        } else if (world.battle_menu_phase == 10) {
                            handled = show_battle_enemy_target(renderer, texture,
                                &file, &font, &world, 1) != 1;
                        } else if (world.battle_menu_phase == 40) {
                            if (npe_world_battle_combine_attack(&world) < 0)
                                fprintf(stderr, "combined attack unavailable\n");
                            else world.battle_menu_phase = 10;
                            handled = 1;
                        }
                    }
                    if (handled) { /* The native menu consumed this key. */ }
                    else if (flag_action==1 && !world.battle_lost) {
                        unsigned actor = world.battle_actor;
                        int battle_result = npe_world_battle_defend(&world);
                        if (battle_result < 0) {
                            fprintf(stderr, "unsupported defense state\n");
                            goto parsed_done;
                        }
                        printf("actor %u defends: battle defense %u; battle %s\n",
                               world.party_ids[actor],
                               world.battle_party_defenses[actor],
                               battle_result == NPE_BATTLE_LOST ?
                               "lost" : "continues");
                        fflush(stdout);
                    }
                    else if (flag_action==2 && !world.battle_lost) {
                        int scripted_escape = world.story_event_id != 0;
                        int battle_result = npe_world_battle_flee(&world);
                        if (battle_result < 0)
                            fprintf(stderr, "escape unavailable in this battle\n");
                        else {
                            /* Source2F:C628 resets scripted failure to sword;
                               random failure keeps the original base icon. */
                            if (scripted_escape && world.game_mode == 3 &&
                                battle_result == NPE_BATTLE_CONTINUE) world.battle_menu_phase = 10;
                            printf("escape %s; mode %u, HP %u\n",
                                   battle_result == NPE_BATTLE_FLED ?
                                   "succeeded" : battle_result == NPE_BATTLE_LOST ?
                                   "failed and party defeated" : "failed",
                                   world.game_mode, world.hp[0]);
                            fflush(stdout);
                        }
                    }
                    else if (key == SDLK_RETURN &&
                             !world.battle_lost) {
                        unsigned target = world.battle_target;
                        int battle_result = npe_world_battle_attack(&world,
                                                                     target);
                        if (battle_result < 0) {
                            fprintf(stderr, "unsupported battle state\n");
                            goto parsed_done;
                        }
                        printf("attack target %u: enemy HP %u, next actor %u HP %u; battle %s\n",
                               target + 1, world.battle_enemy_hps[target],
                               world.party_ids[world.battle_actor],
                               world.hp[world.battle_actor],
                               battle_result == NPE_BATTLE_WON ?
                               "won" : battle_result == NPE_BATTLE_LOST ?
                               "lost" : "continues");
                        fflush(stdout);
                    }
                }
                else if (key == SDLK_SPACE && world.game_mode == 1){
                    int talk=npe_world_begin_talk(&file,&world);
                    if(talk<0){fprintf(stderr,"NPC selector is unsupported\n");goto parsed_done;}
                    /*37:13C2 returns directly for selectorFFFF. Keep the
                      retained LCD even when there is no exit prompt. */
                    if(!talk)world_map_frame_uncommitted=1;
                }
                else if ((key == SDLK_k || key == SDLK_RETURN) &&
                         world.game_mode == 1){
                    int action=show_facing_action(window,renderer,texture,&file,&font,&world,0);
                    if(action==-2){result=0;goto parsed_done;}
                    if(action<0){fprintf(stderr,"object action failed\n");goto parsed_done;}
                    /*37:142E/148C returns without a new LCD for no object
                      or an already-taken object. */
                    if(!action)world_map_frame_uncommitted=1;
                }
                else if (direction < 4) {
                    /* Source37 resets the four-second periodic timer
                       before the directional walking service. */
                    if(world.game_mode==1)
                        world_timer_deadline=SDL_GetTicks()+WORLD_TIMER_MS;
                    int moved = game_world_step_display(&file,&world,direction,renderer,texture,&font);
                    if(moved==-2){result=0;goto parsed_done;}
                    if (moved < 0) {
                        fprintf(stderr, "walk preview hit unsupported map data\n");
                        goto parsed_done;
                    }
                    printf("map %u, position %u,%u, direction %u, result %d\n",
                           world.map_number, world.x, world.y, direction, moved);
                    if (moved == NPE_MOVE_EVENT && world.game_mode == 3 &&
                        world.battle_enemy_count) {
                        unsigned member;
                        printf(world.battle_chase_slot ?
                               "chase encounter:" : "random encounter:");
                        for (member = 0; member < world.battle_enemy_count;
                             ++member)
                            printf(" enemy %u (HP %u)",
                                   world.battle_enemy_ids[member],
                                   world.battle_enemy_hps[member]);
                        putchar('\n');
                    }
                    fflush(stdout);
                }
                int dispatch_status=advance_world_events(window, renderer, texture, &file,
                                                         &font, &world, previous_mode,
                                                         quiz_requested);
                world.defer_story_events=0;
                if(dispatch_status==-2){
                    running=0;
                    break;
                }
                /* Other synchronous dialogue waits consume their timer.
                   Mode6 may return immediately before a pending type4
                   event;3C:D5C8..D5E8 then runs one NPC/chase pass. */
                if (previous_mode!=6 && SDL_TICKS_PASSED(SDL_GetTicks(), world_timer_deadline))
                    game_world_timer(&file, &world, SDL_GetTicks(),
                                       &world_timer_deadline, 1);
                if(previous_mode!=6 && world.game_mode==6 && world.map_init_pending){
                    system_menu_selection=status_member=system_submenu_selection=save_slot_selection=0;
                    system_submenu_open=equipment_page_open=save_slots_open=0;
                }
                if (previous_mode != 4 && world.game_mode == 4)
                    status_member = 0;
            }
            else if (map_preview) {
                if (key == SDLK_LEFT && left) left--;
                else if (key == SDLK_RIGHT && left + 15 < map.width) left++;
                else if (key == SDLK_UP && top) top--;
                else if (key == SDLK_DOWN && top + 7 < map.height) top++;
                else if (key == SDLK_PAGEDOWN || key == SDLK_PAGEUP) {
                    unsigned count = file.offset_table_records[8];
                    unsigned candidate = map_index;
                    unsigned attempts;
                    for (attempts = 0; attempts < count; ++attempts) {
                        candidate = key == SDLK_PAGEDOWN ?
                            (candidate + 1) % count : (candidate + count - 1) % count;
                        if (!npe_map(&file, candidate, &map)) {
                            map_index = candidate; left = top = 0; break;
                        }
                    }
                }
            }
            else if (key == SDLK_RIGHT || key == SDLK_DOWN) {
                index = (index + 1) % file.sections[section].bitmap_count;
            } else if (key == SDLK_LEFT || key == SDLK_UP) {
                index = (index + file.sections[section].bitmap_count - 1) %
                        file.sections[section].bitmap_count;
            } else if (key == SDLK_PAGEDOWN || key == SDLK_PAGEUP) {
                unsigned step = key == SDLK_PAGEDOWN ? 1 : file.section_count - 1;
                unsigned candidate = section;
                do {
                    candidate = (candidate + step) % file.section_count;
                } while (!file.sections[candidate].bitmap_count);
                section = candidate; index = 0;
            }
        }
    }
    result = 0;
    goto parsed_done;
sdl_error:
    fprintf(stderr, "SDL: %s\n", SDL_GetError());
parsed_done:
    for (unsigned slot = 0; slot < 3; ++slot) free(save_paths[slot]);
    npe_font_free(&font);
    game_sound_player=NULL;
    npe_sound_close(&sound);
    game_view_close();game_view_file=NULL;game_view_world=NULL;game_view_playing=NULL;
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    desktop_menu_detach();
    if (window) SDL_DestroyWindow(window);
    npe_free(&file);
done:
    free(bytes);
    game_pad_close();game_pad_mode=NULL;
    SDL_Quit();
    return result;
}

int main(int argc, char **argv) {
#ifndef __ANDROID__
    if(argc==2 && !strcmp(argv[1],"--settings")){
        if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER))return 1;
        settings_load();game_pad_open();show_settings();game_pad_close();SDL_Quit();
        if(!settings_last_saved)return 0;
        argc=1; /* Saving standalone preferences continues into game selection. */
    }
#endif
#ifdef __APPLE__
    if(argc==1){
        char *selected=npe_macos_choose_game();
        if(!selected)return 0;
        char *args[]={argv[0],selected,NULL};
        int result=run(2,args);free(selected);return result;
    }
#endif
#ifdef _WIN32
    char *selected=NULL;
    if(argc==1){selected=desktop_choose_game(NULL);if(!selected)return 0;}
    int result;
    for(;;){
        char *args[]={argv[0],selected,NULL};
        result=selected?run(2,args):run(argc,argv);
        free(selected);selected=desktop_next_game;desktop_next_game=NULL;
        if(!selected)break;
        last_pixel_renderer=NULL;
        game_pad_focus=1;
    }
    return result;
#else
    return run(argc, argv);
#endif
}
