#ifndef NPE_TEXT_H
#define NPE_TEXT_H

#include "npe.h"

#include <stddef.h>
#include <stdint.h>

#define NPE_NATIVE_ITEM_COLUMNS 3
#define NPE_NATIVE_ITEM_PAGE_SIZE 12
#define NPE_NATIVE_ITEM_MAX 255

typedef struct {
    uint8_t *data;
    uint8_t *data16;
} NpeFont;
/* Host layout retains original page boundaries and native glyph bitmaps. */
#define NPE_TEXT_PAGE_GLYPHS 256
typedef struct { uint8_t bits[32],width; } NpeTextGlyph;
typedef struct { NpeTextGlyph glyphs[NPE_TEXT_PAGE_GLYPHS];unsigned count;int valid; } NpeTextPage;
unsigned npe_text_page_lines(const NpeTextPage *,unsigned width);
void npe_text_page_draw(const NpeTextPage *,uint8_t *,unsigned,unsigned,unsigned,unsigned,unsigned);
int npe_render_world_story_page_layout(const NpeFile *,const NpeFont *,NpeWorld *,uint8_t *,NpeTextPage *);
int npe_render_world_npc_page_layout(const NpeFile *,const NpeFont *,NpeWorld *,uint8_t *,
                                    int,NpeTextPageLoad,void *,NpeTextPage *);
/* Draw one page through the original guest text-buffer service. */
int npe_render_world_story_page(const NpeFile *,const NpeFont *,NpeWorld *,uint8_t *);
int npe_render_world_npc_page(const NpeFile *,const NpeFont *,NpeWorld *,uint8_t *,
                              int entry,NpeTextPageLoad,void *);
int npe_render_shop_buy(const NpeFile *file,const NpeFont *font,const NpeWorld *world,
                         unsigned context,unsigned first,unsigned selection,
                         uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_shop_list(const NpeFile *,const NpeFont *,const NpeWorld *,
                         uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_shop_cursor(const NpeFile *,const NpeFont *,const NpeWorld *,
                           unsigned old_row,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_shop_page(const NpeFile *,const NpeFont *,const NpeWorld *,
                         uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_shop_quantity(const NpeFile *,const NpeFont *,NpeWorld *,
                             uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
/* direction0 enters, +/-1 pages; returns1 draw,0 boundary,-1 error. */
int npe_render_shop_description(const NpeFile *,const NpeFont *,NpeWorld *,int direction,uint8_t *);
int npe_render_shop_quantity_counter(const NpeFont *,const NpeWorld *,uint8_t *);
void npe_render_shop_choice_move(uint8_t *);
int npe_render_shop_totals(const NpeFont *,const NpeWorld *,unsigned item_id,
                           uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_shop_no_money(const NpeFont *,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
/* Mode3 entry: actors, current-member cursor, then controls/HUD (ordinary)
   or HUD/controls (scripted). Stages1..3 retain stage0's pixels. */
int npe_render_battle_entry_frame(const NpeFile *,const NpeFont *,const NpeWorld *,
                                  unsigned,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_native_menu_money(const NpeFile *,const NpeFont *,const NpeWorld *,
                                  uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_exit_choice(const NpeFile *,const NpeFont *,const NpeWorld *,
                            uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
void npe_render_exit_choice_move(uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);

/* Resident timed notice geometry; caller retains/restores the parent LCD. */
int npe_render_party_notice(const NpeFile *file, const NpeFont *font,
                            const NpeWorld *world, unsigned member, int adding,
                            uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_timed_notice(const NpeFont *font,const uint8_t *message,
                            size_t size,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_money_notice(const NpeFont *font,uint16_t amount,int adding,
                             uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_item_notice(const NpeFile *file,const NpeFont *font,unsigned item_id,int adding,
                            uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_object_notice(const NpeFile *,const NpeFont *,unsigned,
                             uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_cached_item_notice(const NpeFont *,const NpeItem *,int,int,
                                   uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_skill_notice(const NpeFile *file,const NpeFont *font,unsigned skill_id,
                             uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_world_skill_notice(const NpeFile *,const NpeFont *,NpeWorld *,unsigned,
                                  uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_map_name_notice_layout(const NpeFile *,const NpeFont *,const NpeWorld *,uint8_t *,uint16_t [4]);
int npe_render_map_name_notice(const NpeFile *file,const NpeFont *font,
                               const NpeWorld *world,uint8_t *pixels);
/* Original resident timed save/load failure prompt over the retained LCD. */
int npe_render_save_failure(const NpeFont *font, int saving,
                            uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);

/* Host file paths are UTF-8, matching SDL_GetBasePath on Windows. */
int npe_font_load(NpeFont *font, const char *path);
int npe_font_load16(NpeFont *font, const char *path);
void npe_font_free(NpeFont *font);
/* Draw GBK glyphs on an existing LCD frame, without adding a dialogue box. */
int npe_draw_gbk_text(const NpeFont *font, const uint8_t *text, size_t size,
                      unsigned left, unsigned top,
                      uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_draw_gbk_text_color(const NpeFont *font, const uint8_t *text,
                            size_t size, unsigned left, unsigned top,
                            uint8_t color,
                            uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_draw_gbk_text16_color(const NpeFont *font, const uint8_t *text,
                              size_t size, unsigned left, unsigned top,
                              uint8_t color,
                              uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_native_title_screen(const NpeFile *file, const NpeFont *font,
                                    unsigned selection,
                                    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_native_member_screen(const NpeFile *file, const NpeFont *font,
                                    const NpeWorld *world, unsigned member,
                                    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
/*36:3BCA passes first=0 on member changes; retain the existing LCD. */
int npe_render_native_member_view(const NpeFile *,const NpeFont *,
                                  const NpeWorld *,const NpeWorld *before,
                                  uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
/*4E:E4CA passes a copied physical battle record to3D:8CF0. */
int npe_render_native_battle_member_view(const NpeFile *,const NpeFont *,
                                         const NpeWorld *,unsigned,int,uint8_t *);
int npe_render_native_equipment_screen(const NpeFile *file,
                                       const NpeWorld *world, unsigned member,
                                       uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
/* Source38:11FA; before supplies retained LCD for slot/member updates. */
int npe_render_equipment_view(const NpeFile *,const NpeFont *,NpeWorld *,
                              const NpeWorld *before,uint8_t *pixels);
/*3F:87B6..8818 retains the existing LCD and selection-frame residue. */
int npe_render_equipment_unequip(const NpeWorld *,uint8_t *pixels);
int npe_render_map_item_target_screen(const NpeFile *file, const NpeFont *font,
                                      const NpeWorld *world, unsigned member,
                                      unsigned item_id, unsigned quantity,
                                      uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
/*2F:5452 skips the static frame on member changes and repeated use. */
int npe_render_map_item_target_view(const NpeFile *,const NpeFont *,const NpeWorld *,
                                    unsigned,unsigned,unsigned,int,uint8_t *);
/* rejection 0: equipment, 1: enemy-only, 2: unmatched quest item. */
int npe_render_map_item_rejection(const NpeFont *font, unsigned rejection,
                                  uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_native_item_target_screen(const NpeFile *file,
                                         const NpeFont *font,
                                         const NpeWorld *world,
                                         unsigned member, unsigned item_id,
                                         unsigned quantity,
                                         uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_equip_target_screen(const NpeFile *,const NpeFont *,NpeWorld *,
                                    unsigned,unsigned,unsigned,int,uint8_t *);
int npe_render_native_equip_target_screen(const NpeFile *file, const NpeFont *font,
                                          const NpeWorld *world, unsigned member,
                                          unsigned item_id, unsigned quantity,
                                          uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_single_item_screen(const NpeFile *file, const NpeFont *font,
                                   unsigned item_id, unsigned quantity,
                                   uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_update_item_list_screen(const NpeFile *file, const NpeFont *font,
                                  const unsigned *item_ids, const unsigned *quantities,
                                  unsigned count, unsigned selection,
                                  uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
/* Original3F:0A42: full grid loads each displayed name before selected detail.
   Same-page movement preserves names and quantities on the resident LCD. */
int npe_render_map_item_list(const NpeFile *,const NpeFont *,NpeWorld *,
                            const unsigned *,const unsigned *,unsigned,unsigned,
                            unsigned,int,uint8_t *);
int npe_render_battle_item_list(const NpeFile *,const NpeFont *,NpeWorld *,
                               const unsigned *,const unsigned *,unsigned,unsigned,
                               unsigned,int,int,uint8_t *);
/* Original3F:0E78 entry(direction0),4D:A946/8EFE pages(+/-1);
   1 commit,0 ignored boundary,-1 invalid resource. */
int npe_render_map_item_description(const NpeFile *,const NpeFont *,NpeWorld *,
                                   int,uint8_t *);
int npe_render_item_list_screen(const NpeFile *file, const NpeFont *font,
                                const unsigned *item_ids,
                                const unsigned *quantities, unsigned count,
                                unsigned selection,
                                uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_empty_inventory_screen(const NpeFont *font,
                                      uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_battle_empty_item_return(const NpeFile *file, const NpeFont *font,
                                         uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_empty_magic_screen(const NpeFont *font,
                                  uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_equipment_rejected_screen(const NpeFile *file,
                                          const NpeFont *font,
                                          unsigned item_id,
                                          unsigned quantity,
                                          uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_equipment_rejected_overlay(const NpeFont *font,
                                           uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_equipment_notice(const NpeFont *,unsigned,uint8_t *);
/* Native mode-6 magic inspection page for one learned spell. */
int npe_render_native_magic_screen(const NpeFile *file, const NpeFont *font,
                                   const NpeWorld *world, unsigned member,
                                   unsigned skill_id,
                                   uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_native_magic_list_screen(const NpeFile *file, const NpeFont *font,
                                        const NpeWorld *world, unsigned member,
                                        const unsigned ids[18], unsigned count,
                                        unsigned first, unsigned selection,
                                        uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_update_native_magic_list_screen(const NpeFile *file, const NpeFont *font,
                                        const NpeWorld *world, unsigned member,
                                        const unsigned ids[18], unsigned count,
                                        unsigned first, unsigned selection,
                                        unsigned previous_first, unsigned previous_selection,
                                        uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_battle_insufficient_qi(const NpeFont *font,
                                      uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_battle_item_rejection(const NpeFont *font, int throwing,
                                      uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_native_magic_target_screen(const NpeFile *file,
                                          const NpeFont *font,
                                          const NpeWorld *world,
                                          unsigned caster, unsigned target,
                                          unsigned skill_id,
                                          int insufficient,
                                          uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_magic_member_overlay(const NpeFile *file, const NpeFont *font,
                                     const NpeWorld *world, unsigned selection,
                                     uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_map_magic_message(const NpeFont *font, int unavailable,
                                  uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
/* Runtime skill detail/cost/name use the shared record. List names come
   from NPE resources; same-window updates preserve the retained top. */
int npe_render_map_magic_list(const NpeFile *file, const NpeFont *font, NpeWorld *world,
                              unsigned member, const unsigned ids[18], unsigned count,
                              unsigned first, unsigned selection, unsigned previous_first,
                              unsigned previous_selection, int full, uint8_t *pixels);
/* full=0 refreshes portrait and fixed-width values over the retained LCD. */
/* Original112-byte skill description; direction0 entry,+/-1 navigation. */
int npe_render_map_magic_description(const NpeFile *,const NpeFont *,NpeWorld *,unsigned id,int direction,uint8_t *);
int npe_render_map_magic_target(const NpeFile *file, const NpeFont *font, NpeWorld *world,
                                unsigned caster, unsigned target, unsigned skill_id,
                                int full, uint8_t *pixels);
/* Locate an ASCII page separator without treating a GBK trail byte as '{'. */
size_t npe_gbk_page_end(const uint8_t *text, size_t size, size_t start);
/* Draw one GBK page over an existing indexed LCD frame. Returns -1 when
   the input is malformed or cannot fit the five-line panel. */
int npe_render_gbk_dialogue(const NpeFont *font, const uint8_t *text,
                            size_t size, int has_more,
                            uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
/* Original RPG dialogue uses the firmware 16x16/8x16 font and a plain
   white panel whose height grows with the number of lines. */
int npe_render_native_dialogue(const NpeFont *font, const uint8_t *text,
                               size_t size,
                               uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_native_story_page(const NpeFile *file, const NpeFont *font,
                                 const NpeStoryRecord *story, size_t start,
                                 size_t *next,
                                 uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
/* Style0 replaces the fixed bottom text rectangle on the retained LCD. */
int npe_render_native_window_story_page(const NpeFont *font,
                                        const NpeStoryRecord *story,size_t start,
                                        size_t *next,
                                        uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_battle_level_panel(const NpeFile *file,const NpeFont *font,
                                  const NpeWorld *world,unsigned member,
                                  unsigned ordinal,unsigned phase,
                                  uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_battle_drop_panel(const NpeFile *file,const NpeFont *font,
                                 const NpeWorld *world,unsigned target,
                                 uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_battle_learn_panel(const NpeFile *file,const NpeFont *font,
                                  const NpeWorld *world,unsigned target,
                                  uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_gbk_quiz_answers(const NpeFont *font,
                                const NpeQuizQuestion *question,
                                uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_quiz_page(const NpeFile *file, const NpeFont *font,
                         const NpeWorld *world,
                         uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);

#endif
