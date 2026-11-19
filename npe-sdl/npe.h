#ifndef NPE_H
#define NPE_H
#include <stddef.h>
#include <stdint.h>
typedef struct NpeFile NpeFile;
typedef struct NpeRpgRuntime NpeRpgRuntime;

enum { NPE_MAX_SECTIONS = 100, NPE_WIDTH = 240, NPE_HEIGHT = 120,
       NPE_BITMAP_BYTES = 10 + NPE_WIDTH * NPE_HEIGHT / 2 };
enum { NPE_NPC_POSITION_NONE = UINT16_MAX, NPE_NPC_MOVE_NONTERMINATING = -3 };
/* Three15-pair story changes and eight independently collected objects. */
enum { NPE_TILE_OVERRIDES = 53, NPE_MAP_SNAPSHOT_WORDS = 32767, NPE_MAP_BUFFER_WORDS = 3500 };
/* Number of currently bound World bytes, not the complete guest RAM. */
enum { NPE_WORLD_RESIDENT_BYTES = 4502 };

typedef struct {
    uint32_t offset;
    uint32_t size;
    unsigned bitmap_count;
} NpeSection;

/* Resident2C:0C56 metadata. Controls retain original address names because
   their codec meaning is not yet established. Input is six file-order bytes. */
typedef struct {
    uint16_t type_revision, format, count, control2e, source_rate;
    uint16_t control32, control34, control36, bitrate, output_rate;
} NpeAudioHeader;

typedef struct {
    const uint8_t *bytes; /* complete format-13 stream, including last byte */
    size_t size;
    uint16_t header_count; /* resource header value, not decoded PCM frames */
    NpeAudioHeader header;
} NpeAudioResource;

typedef struct {
    uint16_t x, y, width, height;
    uint32_t row_bytes;
    const uint8_t *pixels;
    unsigned resource_index;
    uint8_t indexed_resource; /* Metadata provenance;0 for other resources. */
} NpeIndexedBitmap;

typedef struct {
    unsigned number;
    uint16_t header_word;
    uint8_t width, height;
    uint8_t name[12];
    const uint8_t *tile_words;
    const uint8_t *tail;
    size_t tail_size;
    /* Optional dense/sparse runtime views; the NPE tile bytes stay immutable. */
    const uint16_t *snapshot_tiles;
    unsigned snapshot_count;
    const uint16_t *override_indices, *override_tiles;
    unsigned override_count;
    const NpeFile *resident_file; /* Current live World map owner only. */
} NpeMap;

typedef struct {
    uint8_t x, y, kind;
    uint16_t destination_map, min_progress;
    uint8_t destination_x, destination_y;
    uint8_t next_mode;
    uint16_t next_id;
    uint16_t second_map, second_min_progress;
    uint8_t second_x, second_y, second_mode;
    uint16_t second_id;
    const uint8_t *raw;
} NpeMapExit;

typedef struct {
    uint16_t id, item_index; /* one-based item number */
    uint8_t x, y;
    const uint8_t *raw;
} NpeMapObject;

/* Group7: manual use of a type24 item, at most five 13-byte records. */
typedef struct {
    uint16_t record_id, min_progress, item_id, next_id, npc_id;
    uint8_t next_mode, x, y;
    const uint8_t *raw;
} NpeMapQuestItem;

/* Four 13-byte map formations also place visible monsters on the map. */
typedef struct {
    uint8_t enemy_id, x, y, left_enemy_id, right_enemy_id;
    uint16_t min_progress, max_progress, graphic, npc_id;
    const uint8_t *raw;
} NpeMapMonster;

/* Indexed group 10: up to two moving chase formations. */
typedef struct {
    uint16_t graphic, min_progress, max_progress, enemy_id;
    uint8_t x, y;
    const uint8_t *raw;
} NpeMapChase;

/* Group 0: at most three progress thresholds, fifteen tile writes each. */
typedef struct {
    uint16_t progress, indices[15], tiles[15];
    uint8_t count;
} NpeMapTileChange;

typedef struct {
    uint16_t id, story_progress, next_id;
    uint8_t mode;
    const uint8_t *raw;
} NpeMapStoryEvent;

typedef struct {
    uint16_t id;
    uint8_t mode[2];
    uint16_t next_id[2];
    const uint8_t *raw;
} NpeMapChoiceEvent;

typedef struct {
    uint16_t id, amount, next_id;
    uint8_t add_money, next_mode;
    const uint8_t *raw;
} NpeMapMoneyEvent;

typedef struct {
    uint16_t quantity, item_index, next_id;
    uint8_t add_item, next_mode;
    const uint8_t *raw;
} NpeMapItemEvent;

typedef struct {
    uint16_t record_id, id, x, y, next_id;
    uint8_t marker; /* secondary coordinates, unused by 41:ACBA */
    uint16_t x2, y2;
    uint8_t draw_mode, erase_mode, next_mode;
    const uint8_t *raw;
} NpeMapAnimationEvent;

typedef struct {
    /* raw_word preserves file +4/+5 for diagnostics: reserved byte and X. */
    uint16_t id, npc_id, next_id, raw_word, x, y;
    uint8_t next_mode;
    const uint8_t *raw;
} NpeMapNpcMoveEvent;

typedef struct {
    uint16_t npc_id, next_id, x, y;
    uint8_t next_mode;
} NpeNpcMoveCache;

typedef struct {
    uint16_t id, destination_map, next_id;
    uint8_t x, y, marker, next_mode;
    const uint8_t *raw;
} NpeMapTeleportEvent;

typedef struct {
    uint16_t id, x, next_id;
    uint8_t marker, y, next_mode;
    const uint8_t *raw;
} NpeMapScriptWalkEvent;

typedef struct {
    uint16_t x, y, next_id;
    uint8_t marker, next_mode;
} NpeScriptWalkCache;

enum { NPE_SCRIPT_WALK_NONTERMINATING = -3 };

typedef struct {
    uint16_t id, next_id;
    uint8_t add_member, character_id, next_mode;
    const uint8_t *raw;
} NpeMapPartyEvent;

typedef struct {
    uint8_t character_id, next_mode;
    uint16_t next_id;
    const uint8_t *raw; /* Loader discards the first word and following byte. */
} NpeMapPartyAdjustEvent;

typedef struct {
    uint16_t id, next_id;
    uint8_t character_id, skill_id, next_mode;
    const uint8_t *raw;
} NpeMapSkillEvent;

typedef struct {
    uint8_t formation_key, win_mode, lose_mode;
    uint16_t win_event_id, lose_event_id;
    const uint8_t *raw;
} NpeMapBattleEvent;

typedef struct {
    const uint8_t *text[5]; /* question, then answers A through D */
    size_t text_size[5];
    uint8_t correct_answer;
    uint8_t input_type; /* control byte 7: zero = grid, nonzero = text */
    uint8_t followup_mode[2]; /* correct, incorrect */
    uint16_t followup_event_id[2];
} NpeQuizQuestion;

typedef struct {
    uint16_t tile_index;
    uint8_t mode;
    uint16_t event_id;
    uint16_t min_progress, max_progress;
    const uint8_t *raw;
} NpeMapMoveTrigger;

typedef struct {
    const uint8_t *name, *description;
    size_t name_size, description_size;
    const uint8_t *raw;
    uint8_t effect_type;
    uint8_t flags, group, target_side; /* item +35/+36/+37; +35 meaning pending */
    uint16_t hp_restore, qi_restore;
    uint16_t price; /* Native741E: signed word from item+38. */
    int16_t attack_bonus, defense_bonus;
    int16_t status_effects[4]; /* UI 毒/眠/乱/封: +29/+31/+27/+33; full state mechanics still traced. */
} NpeItem;

typedef struct {
    const uint8_t *raw, *items;
    uint8_t id, count;
} NpeShopCatalog;

typedef struct {
    const uint8_t *name, *description, *raw;
    size_t name_size, description_size;
    uint8_t effect_type, required_level;
    uint16_t hp_effect, qi_effect, parameter_a, parameter_b, qi_cost;
} NpeSkill;

typedef struct {
    const uint8_t *name, *equipped_items, *raw;
    size_t size;
    uint16_t hp, max_hp, qi, max_qi;
    uint16_t attack, defense;
    uint16_t experience, experience_to_next;
    uint8_t level, growth[7];
    uint16_t other_stats[3];
    uint8_t damage_floor_stat; /* Derived current level, not first skill. */
} NpeCharacter;

typedef struct {
    const uint8_t *name, *raw;
    size_t name_size;
    uint8_t graphic;
    uint16_t max_hp, max_qi, attack, defense;
    uint16_t speed; /* Enemy +25 -> battle record 64A8. */
    uint16_t luck; /* Enemy +27 -> battle record 64AA. */
    uint16_t gold_reward, experience_reward;
    uint8_t damage_floor_stat; /* Enemy level parameter at raw+14. */
    uint8_t drop_item_ids[2], skill_ids[3]; /* Skills at raw+29..31. */
} NpeEnemy;

typedef struct {
    unsigned screen_x, screen_y;
    unsigned offset_x, offset_y;
} NpeViewport;

typedef struct {
    uint8_t kind, target;
    uint16_t resource_id;
} NpeBattleCommand;

typedef struct NpeWorld NpeWorld;
/* Character loader:69D7,69F4..6A03,6A4E..6A50. The battle copy
   has the same offsets relative to6450 as the map copy to69C6. */
typedef struct {
    uint8_t sex;
    uint16_t pose_ids[8];
    uint8_t animation_ids[3]; /* Escape, hostile casting, own-side casting. */
} NpeCharacterMetadata;
int npe_character_metadata(const NpeFile *file, unsigned index, NpeCharacterMetadata *metadata);
enum { NPE_CHARACTER_RECORD_BYTES=148, NPE_CHARACTER_RECORD_SAVE_BYTES=3*148 };
/* Firmware20:431C/4320 dispatches to42:77DA and4E:5300. Addresses
   here are full guest byte addresses; the adapter owns RAM and faults. */
typedef struct {
    void *context;
    uint16_t (*read_word)(void *context,uint32_t address);
    void (*write_word)(void *context,uint32_t address,uint16_t value);
    void (*allocation_failed)(void *context,uint16_t rounded_request);
} NpeResidentHeap;
enum { NPE_RESIDENT_HEAP_BITMAP=0x5127ea, NPE_RESIDENT_HEAP_LIMIT=0x5129a0,
       NPE_RESIDENT_HEAP_HEAD=0x514a40, NPE_RESIDENT_HEAP_BYTES=0xf000 };
void npe_resident_heap_initialize(const NpeResidentHeap *heap);
uint16_t npe_resident_heap_allocate(const NpeResidentHeap *heap,uint16_t request);
uint32_t npe_resident_heap_lock(const NpeResidentHeap *heap,uint16_t handle);
void npe_resident_heap_compact(const NpeResidentHeap *heap);
void npe_resident_heap_unlock(const NpeResidentHeap *heap,uint16_t handle);
void npe_resident_heap_release(const NpeResidentHeap *heap,uint16_t handle);
/* Original31:4974: store the allocated handle even on failure; only a
   successful allocation writes its locked pointer. The owner prompt is
   separate from the allocator's failure callback. No buffer clearing. */
typedef struct {
    NpeResidentHeap heap;
    void *context;
    void (*owner_failed)(void *context);
} NpeResidentHeapOwnerService;
int npe_resident_heap_allocate_owned(const NpeResidentHeapOwnerService *service,
                                    uint32_t handle_address,uint32_t request,
                                    uint32_t pointer_address);
/* Allocation prefix of original1B:23CE..2496. Caller owns heap reset,
   filesystem initialization and the later launcher cleanup/lifecycle. */
int npe_resident_heap_launcher_buffers(const NpeResidentHeapOwnerService *service);
/* Original33:1DF6 skips handle0, otherwise unlocks then releases it.
   Launcher cleanup reads each of its four live handle slots in order. */
void npe_resident_heap_cleanup(const NpeResidentHeap *heap,uint16_t handle);
int npe_resident_heap_launcher_cleanup(const NpeResidentHeap *heap);
/* Original29:D1C8..D1FC allocates7000 and resolves even handle0. */
int npe_rpg_map_buffer_allocate(const NpeResidentHeap *heap);
typedef struct {
    NpeResidentHeap heap;
    void *context;
    void (*seek)(void *context,uint32_t position);
    void (*read_guest)(void *context,uint32_t address,uint16_t count);
} NpeRpgResourceService;
/* Original31:8B80..8BC4 allocation and31:DEF6 packed-resource read.
   Reads live79BE length,79C2 file position and79A6 destination; ignores
   IO return counts just as the original caller does. */
int npe_rpg_battle_packed_load(const NpeRpgResourceService *service);
int npe_rpg_battle_resources_allocate(const NpeRpgResourceService *service);
int npe_rpg_battle_resources_release(const NpeResidentHeap *heap);
int npe_rpg_resources_release(const NpeResidentHeap *heap);
int npe_rpg_finish_game_service(const NpeResidentHeap *heap);
/* Original28:8ED8: reads/draws through the live guest address space.
   IO callbacks retain their own cursor/short-read behavior. read_guest
   writes to the supplied full guest address, including state aliases. */
typedef struct {
    NpeResidentHeap heap;
    void *context;
    void (*seek)(void *context,uint32_t position);
    void (*read_local)(void *context,uint8_t *bytes,uint16_t count);
    void (*read_guest)(void *context,uint32_t address,uint16_t count);
    void (*draw)(void *context,uint32_t address,int16_t x,int16_t y,uint8_t kind);
} NpeBattleBitmapService;
/* Original PRG3F:8D52. draw receives the four copied rectangle words and
   the still-locked guest buffer. IO must retain local bytes on short reads.
   read_seed models the initial caller local; its native EOF value is unproven. */
typedef struct {
    NpeResidentHeap heap;
    void *context;
    void (*seek)(void *,uint32_t);
    void (*read_local)(void *,uint8_t *,uint16_t);
    void (*draw)(void *,uint32_t,const uint16_t [4],int clear);
    void (*backdrop)(void *);
    void (*commit)(void *);
    uint8_t read_seed;
} NpeTextPageService;
int npe_story_text_page_service(const NpeTextPageService *);
/* Original3B:715C..76CC NPC reader. Entry allocates/locks before load,
   consumes one prefix byte, then uses the same61-byte cap as continuations.
   load may be NULL when the caller already loaded the NPC event. */
/* Shared original substring and NUL length services. Length cursor kinds:
   0 signed16 (substring),1 unsigned8 (item/shop),2 unsigned16 (skill). */
uint16_t npe_text_length_service(const NpeResidentHeap *,uint32_t,int cursor_kind);
uint16_t npe_text_substring_service(const NpeResidentHeap *,uint32_t destination,
                                    uint32_t source,uint16_t start,uint16_t count);
int npe_rpg_runtime_text_substring(const NpeFile *,uint32_t destination,uint32_t source,
                                    uint16_t start,uint16_t count,uint16_t *result);
int npe_rpg_runtime_text_length(const NpeFile *,uint32_t source,int cursor_kind,uint16_t *result);
/* Caller-local buffer adapter; it does not model the original stack bank. */
int npe_rpg_runtime_text_slice(const NpeFile *,uint32_t source,uint16_t start,uint16_t count,
                                uint8_t *output,unsigned capacity,uint16_t *result);
typedef int (*NpeTextPageLoad)(void *);
int npe_npc_text_page_service(const NpeTextPageService *,int entry,NpeTextPageLoad,void *);
typedef void (*NpeTextBufferDraw)(void *,uint32_t,const uint16_t [4],int);
int npe_rpg_runtime_story_page(const NpeFile *,NpeTextBufferDraw,void *,uint8_t *);
int npe_rpg_runtime_text_prefix(const NpeFile *);
int npe_rpg_runtime_npc_page(const NpeFile *,NpeTextBufferDraw,void *,uint8_t *,
                             int entry,NpeTextPageLoad,void *);
/* Original0C:D238..D584, three resident slots with uncapped byte pair
   counts, live progress/pointer and signed16 tile address offsets. */
int npe_map_tile_record_service(const NpeBattleBitmapService *,uint8_t count);
/* Original CPU2F:E15A; copies105 words into52:70B4, including
   out-of-grid signed16 positions and overlapping source/destination. */
int npe_map_viewport_cache_refresh(const NpeResidentHeap *);
int npe_map_grid_stream_service(const NpeBattleBitmapService *);
typedef void (*NpeMapTileObserver)(void *,uint16_t index,uint16_t tile);
/* Original25:3082 type2: live three-slot update and cache refresh
   after each applied slot. Observer only records completed tile writes. */
int npe_map_tile_update_service(const NpeResidentHeap *,NpeMapTileObserver,void *);
int npe_battle_bitmap_service(const NpeBattleBitmapService *service,
                             uint16_t number,int16_t x,int16_t y,uint8_t kind);
/* Original2B:0000. Zero-based signed index, signed16 shifted directory
   offsets, two separate4-byte locals, original return0/-1/-3. */
int npe_indexed_bitmap_service(const NpeBattleBitmapService *,int16_t index,
                               int16_t x,int16_t y,int16_t kind);
/* Original scene service24:ACBA resource lifetime. One buffer sized from
   the first descriptor, sequential same-length reads for every frame. */
typedef struct {
    uint32_t address,length;
    uint16_t handle,group;
    uint8_t frames,position,active;
} NpeAnimationStream;
int npe_scene_animation_stream_open(const NpeBattleBitmapService *,int16_t group,NpeAnimationStream *);
int npe_role_animation_stream_open(const NpeBattleBitmapService *,int16_t group,NpeAnimationStream *);
int npe_animation_stream_draw(const NpeBattleBitmapService *,NpeAnimationStream *,
                              int16_t x,int16_t y,uint8_t kind);
void npe_animation_stream_close(const NpeResidentHeap *,NpeAnimationStream *);
/* Original03:42C0/42C4/42C8 reads a10-byte guest header without checking
   its signature. Width/height are bytes6/8; negative origins draw nothing.
     kind0 overwrites,1 fills zero destination pixels,other skips zero source;
     original odd-column branch/mask/retained-byte quirks are preserved.
   pixels is the caller's existing240x120 LCD image, not cleared here. */
int npe_draw_guest_bitmap(const NpeResidentHeap *memory,uint32_t address,
                          int16_t x,int16_t y,uint8_t kind,
                          uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
/* File-owned runtime RAM/heap/cursor. Optional bindings retain live world
   owners; context must outlive the runtime. Unmapped RAM remains in the
   runtime. No runtime memory or ownership is serialized in a world save. */
typedef struct {
    void *context;
    int (*read)(void *,uint32_t,uint8_t *,unsigned);
    int (*write)(void *,uint32_t,const uint8_t *,unsigned);
} NpeResidentBindings;
int npe_rpg_runtime_open(NpeFile *file,const NpeResidentBindings *bindings);
void npe_rpg_runtime_close(NpeFile *file);
int npe_rpg_runtime_read(const NpeFile *,uint32_t,uint8_t *,unsigned);
int npe_rpg_runtime_open_idle(NpeFile *,const NpeResidentBindings *);
/* Group0 load phase, using the current map-buffer pointer. The grid,
   viewport and post-load services are separate. -2: unavailable RAM. */
int npe_rpg_runtime_map_tile_records(const NpeFile *,const NpeMap *,uint16_t progress);
int npe_rpg_runtime_viewport_refresh(const NpeFile *);
int npe_rpg_runtime_map_grid(const NpeFile *,const NpeMap *);
int npe_rpg_runtime_map_load(const NpeFile *,const NpeWorld *,int restore);
/* Type1 map service: negative slot scans100 flags/eight objects; nonnegative
   slot addresses the physical object record without a slot-count clamp. */
int npe_map_object_clear_service(const NpeResidentHeap *,uint16_t slot);
int npe_rpg_runtime_map_objects_clear(const NpeFile *,uint16_t slot);
/* Tile/cache APIs return1 when the requested map is loaded,0 when it
   is not current,-2 for unavailable RAM. Indices are signed16 offsets. */
int npe_rpg_runtime_map_tile_write(const NpeFile *,unsigned,uint16_t,uint16_t);
int npe_rpg_runtime_map_tile(const NpeFile *,unsigned,uint16_t,uint16_t *);
int npe_rpg_runtime_map_cache(const NpeFile *,unsigned,unsigned,unsigned,uint16_t [105]);
int npe_rpg_runtime_map_current(const NpeFile *,unsigned);
int npe_rpg_runtime_map_update(const NpeFile *,NpeMapTileObserver,void *);
int npe_rpg_runtime_shutdown(const NpeFile *);
int npe_rpg_runtime_finish_game(const NpeFile *);
int npe_world_battle_resources(const NpeFile *,NpeWorld *,int);
int npe_rpg_runtime_write(const NpeFile *,uint32_t,const uint8_t *,unsigned);
uint32_t npe_rpg_runtime_cursor(const NpeFile *);
unsigned npe_rpg_runtime_reads(const NpeFile *);
unsigned npe_rpg_runtime_failures(const NpeFile *);
void *npe_rpg_runtime_owner(const NpeFile *);
int npe_rpg_runtime_battle_resources(const NpeFile *,int active);
int npe_rpg_runtime_draw(const NpeFile *,uint16_t,int16_t,int16_t,uint8_t,
                         uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
/* Returns the original service status; -2 denotes unavailable guest RAM. */
int npe_rpg_runtime_draw_indexed(const NpeFile *,int16_t,int16_t,int16_t,int16_t,
                                 uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_world_resources_open(NpeFile *file,NpeWorld *world);
int npe_rpg_runtime_scene_begin(const NpeFile *,int16_t group);
unsigned npe_rpg_runtime_scene_frames(const NpeFile *);
int npe_rpg_runtime_scene_frame(const NpeFile *,int16_t x,int16_t y,uint8_t kind,
                               uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
void npe_rpg_runtime_scene_end(const NpeFile *);
/* Source3C:43D4. Caller supplies start/end; returns original status or -2
   for an unavailable RAM adapter. Single-frame groups retain one payload. */
int npe_rpg_runtime_role_begin(const NpeFile *,int16_t group,int16_t x,int16_t y,int16_t end_x,int16_t end_y);
unsigned npe_rpg_runtime_role_steps(const NpeFile *);
int npe_rpg_runtime_role_frame(const NpeFile *,uint8_t kind,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT],int16_t position[2]);
void npe_rpg_runtime_role_end(const NpeFile *);
int npe_character_record_seed(const NpeFile *file,unsigned index,uint8_t record[148]);
/* Zero-extended live124/125 coordinates. Party member is a physical
   party slot; enemy member is relative to the fixed party record prefix.
   Unprepared inspection scenes use the original initializer layout. */
int npe_world_battle_coordinate(const NpeWorld *world,unsigned enemy,unsigned member,unsigned axis);
int npe_world_battle_record_image(const NpeWorld *world,unsigned slot,uint8_t record[148]);
int npe_world_map_record_image(const NpeWorld *world,unsigned slot,uint8_t record[148]);
/* Write bytes of one physical resident record without consulting NPE defaults.
   Map and battle owners remain distinct while the battle view is active. */
int npe_world_record_write(NpeWorld *world,int battle,unsigned slot,unsigned offset,
                           const uint8_t *bytes,unsigned count);
int npe_world_resident_records_read(const NpeWorld *world,uint32_t address,
                                    uint8_t *bytes,unsigned count);
int npe_world_resident_records_write(NpeWorld *world,uint32_t address,
                                     const uint8_t *bytes,unsigned count);
/* Mapped resident records, inventory, resource caches and cache IDs.
   Unsupported address spans return-1 without writing; no NPE reload. */
int npe_world_resident_read(const NpeWorld *world,uint32_t address,uint8_t *bytes,unsigned count);
int npe_world_resident_write(NpeWorld *world,uint32_t address,const uint8_t *bytes,unsigned count);
const NpeCharacterMetadata *npe_world_battle_character(const NpeWorld *world, unsigned member);
unsigned npe_world_battle_pose(const NpeWorld *world, unsigned member, unsigned pose);
const uint8_t *npe_world_battle_name(const NpeWorld *world, unsigned member);
uint16_t npe_world_inventory_word(const NpeWorld *world, unsigned item);
int npe_world_set_inventory_word(NpeWorld *world, unsigned item, uint16_t value);
uint16_t npe_world_portrait_index(const NpeFile *file, const NpeWorld *world,
                                  unsigned member);
int npe_world_portrait_bitmap(const NpeFile *file, const NpeWorld *world,
                              unsigned member, NpeIndexedBitmap *bitmap);
enum { NPE_BATTLE_ANIMATION_ATTACK = 1, NPE_BATTLE_ANIMATION_COMBINE = 3,
       NPE_BATTLE_ANIMATION_DEFEND = 5, NPE_BATTLE_ANIMATION_ENEMY_ATTACK = 7,
       NPE_BATTLE_ANIMATION_ENEMY_RECOVERY = 8,
       NPE_BATTLE_ANIMATION_ENEMY_SPELL = 9,
       NPE_BATTLE_ANIMATION_POISON = 10,
       NPE_BATTLE_ANIMATION_PARTY_SPELL = 11,
       NPE_BATTLE_ANIMATION_ITEM = 12, NPE_BATTLE_ANIMATION_VICTORY = 13,
       NPE_BATTLE_ANIMATION_LEVEL = 14, NPE_BATTLE_ANIMATION_DROP = 15,
       NPE_BATTLE_ANIMATION_LEARN = 16, NPE_BATTLE_ANIMATION_FLEE = 17,
       NPE_BATTLE_ANIMATION_FAILED_FLEE = 18, NPE_BATTLE_ANIMATION_DEFEAT = 19 };
enum { NPE_BATTLE_ANIMATION_BEGIN, NPE_BATTLE_ANIMATION_DAMAGE,
       NPE_BATTLE_ANIMATION_ROUND_END, NPE_BATTLE_ANIMATION_SETTLE,
       NPE_BATTLE_ANIMATION_ACTOR_RESTORE, NPE_BATTLE_ANIMATION_ROUND_RESTORE };
typedef void (*NpeBattleAnimationObserver)(void *context, const NpeWorld *world,
                                          unsigned action, unsigned target,
                                          unsigned phase);
typedef void (*NpeSoundObserver)(void *context, unsigned section, int loop);
enum {NPE_SOUND_CALLBACK_MAP=0x31a3f4,NPE_SOUND_CALLBACK_BATTLE=0x32649c};
typedef void (*NpeSoundRangeObserver)(void *context,uint32_t offset,int32_t length,uint32_t callback);

struct NpeWorld {
    /* Opaque bytes retain fields without named owners. Record readers overlay
       the live named fields; copies and swaps preserve the entire residue. */
    uint8_t party_record_residue[3][148],battle_record_residue[6][148];
    uint8_t battle_map_records[3][148]; /* Map's shared fields during battle; runtime only. */
    uint8_t battle_record_ids[3],battle_record_party_count; /* Physical battle prefix; runtime only. */
    unsigned map_number;
    unsigned x, y;
    unsigned direction;
    uint8_t companion_directions[2]; /* Other character records' +14 bytes; moved with each record. */
    NpeViewport view;
    uint8_t npc_state[256];
    uint16_t npc_override_x[20], npc_override_y[20]; /* Byte coordinates, UINT16_MAX means no override. */
    uint8_t npc_override_direction[20];
    uint8_t npc_records[20][14]; /* Resident52:6CA2; byte13 survives map loading. */
    uint8_t npc_record_views[20][13]; /* Derived file-layout view for world services. */
    uint8_t npc_records_ready;
    uint8_t monster_records[4][14], monster_record_views[4][13]; /*52:761A; byte8 survives loading. */
    uint8_t chase_records[2][14], chase_record_views[2][10]; /*52:783A; byte13 and absent fields survive. */
    uint8_t map_encounter_records_ready;
    uint16_t condition_words[255];
    /* Resident52:67C6: signed inventory indexFFFF aliases bytes146/147
       of physical battle record5. Runtime only, outside the slot codec. */
    uint16_t inventory_prefix_word;
    uint8_t equipped_items[3][6];
    uint8_t party_ids[3], party_active[3];
    uint8_t party_attack_groups[3]; /* Map69F2, copied to battle647C and back on exit. */
    NpeCharacterMetadata party_characters[3];
    uint8_t party_names[3][12]; /* Map69C6; word255 of inventory aliases the first two bytes. */
    uint8_t skill_slots[3][18], learned_skill_count[3];
    uint16_t hp[3], max_hp[3], qi[3], max_qi[3];
    uint16_t attack[3], defense[3];
    uint16_t experience[3], experience_to_next[3];
    uint8_t level[3], growth[3][7];
    uint16_t other_stats[3][3];
    uint8_t damage_floor_stat[3];
    uint16_t status_guard[3][4]; /* Map 6A04..6A0A; independent of battle states. */
    /* Own-save replay before resources attach; map changes clear ready. */
    uint16_t terrain_snapshot_cells[NPE_MAP_SNAPSHOT_WORDS];
    uint16_t terrain_snapshot_count;
    uint8_t terrain_snapshot_ready,terrain_snapshot_width,terrain_snapshot_height;
    uint8_t terrain_buffer_snapshot_ready; /* Own V24 retains all7000 map-buffer bytes in the same cell array. */
    uint8_t tile_record_bytes[192]; /* Resident52:724E..730D; own V23 saves the full byte image. */
    NpeMapTileChange tile_changes[3];
    uint8_t tile_change_count, tile_change_applied, tile_override_count;
    uint16_t tile_override_indices[NPE_TILE_OVERRIDES], tile_override_tiles[NPE_TILE_OVERRIDES];
    NpeMapStoryEvent story_events[15]; /* Resident74F4 stride6; absent map slots survive. */
    uint8_t scene_event_records[5][16]; /* Resident754E stride16; partial group1 load. */
    uint8_t choice_event_records[5][6]; /* Resident781C; partial group4 load. */
    uint8_t battle_event_records[4][8]; /* Resident77FC; group8, padding3 preserved. */
    uint8_t random_enemy_records[8]; /* Resident7916: compacted nonzero map enemy bytes. */
    uint8_t story_event_padding[15]; /* Slot byte5; untouched by map loading. */
    uint8_t exit_records[12][24]; /* Resident6B82..6CA1; loader preserves slot byte3. */
    uint8_t exit_records_ready;
    uint8_t object_records[8][8]; /* Resident759E..75DD:ID,graphic,item BE words,x,y. */
    uint8_t object_records_ready;
    NpeMapMoneyEvent money_events[5]; /* Resident76DA stride8, partial group12 load; runtime only. */
    uint8_t money_event_padding[5][2]; /* Bytes6/7 survive all map loads. */
    NpeMapItemEvent item_events[5]; /* Resident76B0 stride8, partial group13 load; runtime only. */
    NpeMapTeleportEvent teleport_events[2]; /* Resident788E stride10, partial group5 load; runtime only. */
    uint8_t teleport_padding[2][2]; /* Resident bytes2/3, untouched by loading. */
    NpeMapSkillEvent skill_events[2]; /* Resident790A stride6, partial group15 load; runtime only. */
    uint8_t skill_event_padding[2]; /* Slot byte5 survives map loads. */
    NpeMapPartyAdjustEvent party_adjust_event; /* Resident79A2 stride4, group3 partial load; runtime only. */
    uint8_t story_event_count; /* Current file group11 count; not a service slot gate. */
    uint8_t condition5_bytes[200];
    uint16_t story_progress;
    uint16_t story_event_id;
    uint16_t event_context_id; /* Transient native 7938; quest entry NPC gate. */
    uint8_t event_blank_background; /* Quest use clears LCD before the event chain. */
    uint8_t lcd_background; /* Resident graphics clear color; excluded from saves. */
    uint16_t map_init_pending; /* Complete resident7AB4 word; mode1 consumes it before map input. Runtime only. */
    uint8_t defer_story_events; /* Host display scope: queue mode14 until its entry LCD commit. Not saved. */
    uint8_t property_page, property_member, equipment_view_slot; /* Mode6 pages9/10; runtime only. */
    uint8_t menu_party_count; /* Resident7AB0, captured on main menu initialization. */
    NpeMapPartyEvent party_events[3]; /* Resident78A2 stride6; map load overwrites only present entries. */
    uint8_t party_event_padding[3]; /* Resident byte3, untouched by loading. */
    NpeNpcMoveCache npc_move_events[5]; /* Resident7652 stride10; runtime only, absent slots survive load. */
    NpeScriptWalkCache script_walk_events[4]; /* Resident7728 stride10; same partial map-load lifetime. */
    uint8_t npc_move_padding[5]; /* Resident slot byte3, untouched by loading. */
    uint8_t script_walk_padding[4][2]; /* Resident slot bytes1/3, untouched by loading. */
    uint8_t equipment_description_item; /* Resident7AB2, independent of shop7A4C. */
    uint8_t item_record_id; /* Resident7A72; shared loader cache, distinct from7AB2. */
    uint8_t item_record_bytes[142]; /* Logical bytes739E..742B; partial description writes. */
    uint8_t item_record_raw[41]; /* Derived file-order header for existing attribute consumers. */
    uint8_t skill_record_id; /* Resident7A74, independent of item7A72. */
    uint8_t skill_record_bytes[144]; /*730E..739D; description tail and byte7385 survive loads. */
    uint8_t skill_record_raw[42]; /* Derived file-order view, not additional firmware state. */
    uint16_t item_description_length; /* Resident7ABA, outside the slot codec. */
    uint32_t equipment_description_revision; /* LCD commit invalidation, runtime only. */
    uint8_t equipment_description_local[67]; /* Host copy of caller local; never guest RAM/save data. */
    uint8_t equipment_description_local_ready;
    uint16_t game_mode; /* Complete52:7934 word. */
    uint8_t system_menu_stage, music_choice; /*52:7A54/7A8A; adjacent bytes remain RAM-owned. */
    uint8_t exit_choice, exit_slot; /* Resident7AC4/7A84; retained after confirmation. */
    uint32_t mode2_dialogue_offset;
    uint32_t text_file_position; /*797A/797C absolute current file pointer. */
    uint16_t text_rect[4]; /*791E..7925; formatter tail remains runtime RAM. */
    uint16_t text_offset; /*7A1A: word primary-read count; lookahead excluded. */
    uint8_t text_more, text_style; /*7A1C /7ACE, exact byte values. */
    uint16_t mode2_followup_event_id; /* Shared resident7A82; runtime only. */
    uint8_t mode2_followup_mode, mode2_ready; /* Resident7A0A + host NPC activity. */
    uint8_t story_control_ready; /* Host flag: mode5 loaded shared7A0A/7A82. Runtime only. */
    uint8_t mode2_context; /* Native7A68: selected map NPC byte+12, monsters0. */
    uint8_t shop_ready, shop_selection, shop_first; /* Runtime UI, excluded from saves. */
    uint8_t shop_kind; /* Resident7A0E; retained independently of catalog activity. */
    uint8_t shop_transaction_sell; /* Resident7AC2; any nonzero byte means selling. */
    uint8_t shop_scan_first, shop_scan_last, shop_inventory_only; /*7A10/12/14. */
    uint8_t shop_catalog_present[255]; /*6DBA..6EB8, retained outside buy entry. */
    uint8_t shop_catalog_ready; /* Host view availability, not a guest field. */
    /* Host continuations for40:C59E..C61E, excluded from guest RAM/saves. */
    uint8_t shop_page_pending, shop_page_backfill_count, shop_navigation_page;
    uint8_t shop_ui, shop_quantity, shop_choice;
    uint8_t shop_list_ready, shop_row;
    uint8_t inventory_column; /* Shared7A22, runtime only. */
    uint16_t shop_first_item, shop_last_item, shop_selected_item;
    uint16_t shop_description_start, shop_description_end;
    uint16_t npc_guard_index;
    uint16_t npc_guard_word_index;
    uint8_t npc_guard_a, npc_guard_b;
    uint8_t battle_magic_types[3], battle_magic_groups[3], battle_magic_sides[3]; /*64D0 and queued flags. */
    uint8_t battle_item_types[3], battle_item_groups[3], battle_item_sides[3];
    uint8_t battle_item_ids[3]; /* Native64D5 is independent of the current command. */
    uint8_t battle_skill_ids[3]; /* Native64D6 persists through cancel, back and round reset. */
    uint8_t battle_item_type_copies[3]; /* Native64D2, also changed by throwing31. */
    uint8_t battle_native_kinds[3]; /* Native64CF; defense survives round reset. */
    uint8_t battle_native_targets[3]; /* Native64CE, one-based combined party/enemy records;0 for defense. */
    uint8_t battle_attack_groups[3]; /* Resident647C, copied to64D3 at sword selection. */
    NpeCharacterMetadata battle_characters[3]; /* Transient record copies; saved via map on exit. */
    uint8_t battle_names[3][12]; /* Battle6450; copied from and back to map records. */
    uint8_t battle_map_party_active[3], battle_party_flags_saved;
    uint8_t battle_party_record_active[3]; /*647B: copied flag byte, independent of the host battle prefix. */
    /* Map flags stay resident while the host battle view uses the native prefix. */
    uint8_t battle_menu_previous_kind; /* Global7AAE retained when backing to another actor. */
    uint8_t battle_menu_row, battle_item_row; /*52:7A9C/7AA2, retained list selections. */
    uint8_t battle_display_target; /*52:7AC0, independent one-based physical target. */
    uint16_t battle_selection_item_type; /*52:7AA0, independent selector word. */
    uint16_t battle_selection_magic_type; /*52:7AA6. */
    uint16_t battle_selection_type_copy; /*52:7AAA. */
    uint8_t battle_menu_phase; /*52:7AC8; native base/list/selector/exit stage byte. */
    uint8_t battle_status_member; /*52:7AAC; independent retained status-page selector. */
    uint16_t battle_resource_flags; /*52:7AA4; high byte live packed-buffer flag, low byte retained. */
    uint8_t battle_item_converted_mask; /* Prepared scalar words for native type31. */
    /* Tentative64D0/64D3/64D4 metadata is written before target selection. */
    uint32_t money;
    uint8_t object_done[256]; /*742C flags; bytes100..199 also own7490 walk triggers. */
    uint32_t random_state;
    /* Native 7A50/7A04 and pending ROM playback request. Runtime only:
       device availability must not change the shared random sequence. */
    uint16_t music_enabled; /*52:7A50; compare the complete original word. */
    uint8_t map_music_slot, sound_section, sound_loop;
    NpeSoundObserver sound_observer;
    NpeSoundRangeObserver sound_range_observer;
    void *sound_context;
    uint16_t encounter_steps;
    uint16_t encounters_enabled; /* Resident7A5C:nonzero permits chance attempts. */
    uint16_t chase_x[2], chase_y[2];
    uint8_t chase_cooldown[2], battle_chase_slot;
    uint16_t quiz_previous_index, quiz_question_index;
    uint16_t quiz_category; /* original7A52: category0..9 or fixed question ID */
    uint8_t quiz_ready, quiz_input_type;
    uint16_t quiz_category_correct[10], quiz_category_wrong[10];
    uint16_t quiz_fixed_correct, quiz_fixed_wrong; /* original76AC/76AE */
    uint16_t quiz_answer_count;
    uint8_t quiz_wrong_count;
    uint8_t quiz_input_length;
    char quiz_input[33]; /* 32 resident bytes + host NUL; outside save codec */
    uint8_t battle_enemy_count;
    uint8_t battle_enemy_ids[3]; /*Host lookup keys; native record byte12 is separate. */
    uint8_t battle_enemy_records_loaded; /*Resident pose/name bytes drive loaded enemies. */
    uint16_t battle_selected_ids[3]; /*52:792E/7930/7932, independent of host lookup keys. */
    uint8_t battle_formation_count; /*52:7AF0, retained after combat. */
    uint8_t battle_scripted_entry; /*52:7A9A, only1 selects the map formation. */
    uint16_t battle_enemy_hps[3];
    int16_t battle_enemy_status[3][4]; /* 648E..6494, signed words */
    /* Source38:E110..E146 clears 648E..6494 on each battle entry.
       These words are independent of map character status_guard (6A04). */
    int16_t battle_party_status[3][4];
    uint16_t battle_enemy_max_hps[3], battle_enemy_spell_stats[3];
    uint8_t battle_enemy_skill_ids[3][3];
    uint8_t battle_enemy_last_skill[3], battle_enemy_last_target[3];
    uint16_t battle_enemy_attacks[3], battle_enemy_defenses[3];
    uint16_t battle_enemy_base_attacks[3], battle_enemy_base_defenses[3];
    uint16_t battle_enemy_qis[3], battle_enemy_max_qis[3];
    uint16_t battle_enemy_attack_turns[3], battle_enemy_defense_turns[3];
    uint16_t battle_enemy_speeds[3];
    uint16_t battle_enemy_lucks[3];
    uint16_t battle_enemy_gold_rewards[3];
    uint16_t battle_enemy_experience_rewards[3];
    uint8_t battle_enemy_drop_ids[3][2];
    uint8_t battle_drop_names[3][2][13]; /*39:43CE/45AC copies names before any drop LCD. */
    uint8_t battle_enemy_damage_floor_stats[3];
    uint16_t battle_party_defenses[3];
    uint16_t battle_skill_attack_bonus[3], battle_skill_defense_bonus[3];
    /* Native 6496/6498 are independent word counters. Defense action5
       shares 6498 with defense effects; each positive effect writes4. */
    uint16_t battle_attack_turns[3], battle_defense_turns[3];
    uint8_t battle_enemy_id;
    uint16_t battle_enemy_hp;
    uint8_t battle_target;
    uint8_t battle_actor;
    uint8_t battle_action_ready; /*52:7AA8; dispatch requires exactly1. */
    uint8_t battle_execution_slot; /*52:7ACC; independent physical cursor. */
    uint8_t battle_lost;
    uint8_t battle_win_mode, battle_lose_mode;
    uint16_t battle_win_event_id, battle_lose_event_id;
    /* Battle-only command state. Map saves cannot be written in mode3;
       no resource pointers or transient commands enter the host save. */
    const NpeFile *battle_file;
    NpeBattleCommand battle_commands[3];
    uint8_t battle_item_reservations[3]; /* One item debited at selection per actor. */
    uint8_t battle_pending_mask, battle_executing;
    uint8_t battle_last_order[6], battle_last_order_count;
    uint8_t battle_enemy_shifted_mask; /* Erased scenery retained after hostile group animation. */
    uint8_t battle_party_refresh_mask; /* Selective final pose rectangle clearing. */
    uint8_t battle_scene_refreshed; /* Full round redraw, unlike icon-only repaint. */
    uint16_t battle_variance_roll;
    uint16_t battle_attribute_roll;
    uint32_t battle_display_epoch; /* Runtime round sequence, excluded from saves. */
    uint16_t battle_enemy_damage_values[3]; /* Prepared words for native floating text. */
    uint16_t battle_party_damage_values[3]; /* Display-only native floating text, excluded from saves. */
    uint8_t battle_animation_enemy; /* Display-only attacking physical enemy slot. */
    /* Display-only hooks are neither serialized nor used by battle arithmetic. */
    NpeBattleAnimationObserver battle_animation_observer;
    void *battle_animation_context;
    uint8_t map_margin_color; /* Retained LCD margin; transient display state. */
};

typedef struct {
    unsigned case_count;
    const uint8_t *raw;
    uint32_t dialogue_offset[3];
} NpeNpcAction;

typedef struct {
    unsigned case_index, case_count;
    uint8_t action_type;
    int succeeded;
    uint32_t dialogue_offset;
    uint8_t followup_mode;
    uint16_t followup_event_id;
} NpeNpcEvent;

typedef struct {
    NpeResidentHeap heap;
    void *context;
    void (*seek)(void *,uint32_t);
    void (*read_local)(void *,uint8_t *,uint16_t);
    void (*read_guest)(void *,uint32_t,uint16_t);
} NpeNpcEventService;
/* Local input preserves short-read residue; ordinary runtime starts it at0.
   Unknown stack residue/banks remain outside this adapter's proven scope. */
int npe_npc_event_service(const NpeNpcEventService *,uint16_t,uint8_t local[44],NpeNpcEvent *);
int npe_npc_condition_service(const NpeResidentHeap *,uint8_t,uint16_t);
int npe_rpg_runtime_npc_event(const NpeFile *,uint16_t,NpeNpcEvent *);

typedef struct {
    const uint8_t *bytes;
    size_t size;
    uint8_t terminator;
} NpeDialoguePage;

typedef struct {
    uint32_t header_value;
    uint8_t control_byte;
    uint16_t control_word;
    const uint8_t *text;
    size_t text_size;
} NpeStoryRecord;

enum { NPE_MOVE_BLOCKED = 0, NPE_MOVE_OK = 1, NPE_MOVE_EXIT = 2,
       NPE_MOVE_EVENT = 3, NPE_MOVE_EXIT_CHOICE = 4, NPE_MOVE_EXIT_DENIED = 5 };
enum { NPE_BATTLE_CONTINUE = 0, NPE_BATTLE_WON = 1,
       NPE_BATTLE_LOST = 2, NPE_BATTLE_FLED = 3 };

struct NpeFile {
    NpeRpgRuntime *rpg_runtime; /* Owned here; not copied with NpeWorld. */
    uint8_t *bytes;
    size_t size;
    uint8_t title[13];
    uint16_t vectors[9];
    NpeSection sections[NPE_MAX_SECTIONS];
    unsigned section_count;
    unsigned indexed_bitmap_count;
    unsigned battle_bitmap_count;
    unsigned animation_group_count;
    unsigned animation_frame_count;
    unsigned scene_animation_group_count, scene_animation_frame_count;
    unsigned offset_table_records[NPE_MAX_SECTIONS];
};

/* On success takes ownership of bytes; release it with npe_free.
   On failure the caller retains bytes. Error is always NUL terminated. */
int npe_parse(NpeFile *out, uint8_t *bytes, size_t size,
              char *error, size_t error_size);
void npe_free(NpeFile *file);
/* GBK text decoder used by the NPE dialogue and item UI. Caller frees it. */
char *npe_gbk_to_utf8(const uint8_t *bytes, size_t size);
/* Returns 0 for a valid bitmap, -1 for an invalid index. Pixels are raw 0..15. */
int npe_bitmap(const NpeFile *file, unsigned section, unsigned index,
               uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
/* Section 0 indexes section 1. Pixel rows are padded to a 16-bit word. */
int npe_indexed_bitmap(const NpeFile *file, unsigned index,
                       NpeIndexedBitmap *bitmap);
/* Header groups5..8 use their live original RAM slots when attached.
   Resource drawing ignores original -1/-3 statuses like the UI callers;
   adapter failures return -1. No dimensions/signature guard in that path. */
uint16_t npe_image_group_index(const NpeFile *,unsigned group,uint16_t offset);
int npe_render_indexed_resource(const NpeFile *,uint16_t index,int16_t x,int16_t y,
                                 uint8_t kind,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
/* Zero-based battle draw index. One-based1..29 use section9's resident
   LE24 table; higher IDs use the original separate6-byte file directory. */
int npe_battle_bitmap(const NpeFile *file, unsigned index,
                      NpeIndexedBitmap *bitmap);
uint8_t npe_indexed_pixel(const NpeIndexedBitmap *bitmap,
                          unsigned x, unsigned y);
/* Section 6 groups section 7 bitmaps into ordered animation frames. */
unsigned npe_animation_frames(const NpeFile *file, unsigned group);
/* Scene animation uses the independent section13/14 tables (RAM79F6/79FA). */
unsigned npe_scene_animation_frames(const NpeFile *file, unsigned group);
int npe_scene_animation_frame(const NpeFile *file, unsigned group,
                              unsigned frame, NpeIndexedBitmap *bitmap);
/* Compose into the retained LCD: 0 replaces, 1 fills zero destinations,
   2 overlays nonzero source pixels. Coordinates are service arguments. */
int npe_render_scene_animation_frame(const NpeFile *file, unsigned group,
                                     unsigned frame, unsigned x, unsigned y,
                                     unsigned draw_mode, uint8_t *pixels);
int npe_animation_frame(const NpeFile *file, unsigned group, unsigned frame,
                        NpeIndexedBitmap *bitmap);
/* Sections such as 2/3/4/8/10/11/12 hold big-endian u32 offset tables. */
int npe_record(const NpeFile *file, unsigned section, unsigned index,
               const uint8_t **data, size_t *size);
/* Sections 16..37 hold one format-13 sound stream each. This only validates
   the outer stream boundary; it does not decode audio. */
int npe_audio_resource(const NpeFile *file, unsigned section,
                       NpeAudioResource *resource);
/* Validate a current-file compressed range without requiring a section ID. */
int npe_audio_resource_range(const NpeFile *,uint32_t offset,int32_t length,NpeAudioResource *);
/* Original header returns1 for a zero bitrate (unsupported format),0 otherwise.
   Host invalid pointers/short input return-1 without changing the output. */
int npe_audio_header(const uint8_t *bytes,size_t size,NpeAudioHeader *header);
int npe_map(const NpeFile *file, unsigned index, NpeMap *map);
/* Reads one count from the validated 17-group variable table. */
int npe_map_group_count(const NpeMap *map, unsigned group, unsigned *count);
int npe_map_chase(const NpeMap *map, unsigned slot, NpeMapChase *chase);
uint16_t npe_map_tile(const NpeMap *map, unsigned x, unsigned y);
/* Static tile branch of original PRG source 45:0C76..0F90.
   Dynamic exit/object state is handled separately by the original engine. */
int npe_tile_base_passable(uint16_t tile);
int npe_map_tile_changes(const NpeMap *map, NpeMapTileChange changes[3],
                          unsigned *count);
int npe_world_load_tile_records(NpeWorld *,const NpeMap *);
int npe_world_load_tiles(NpeWorld *world, const NpeMap *map);
int npe_world_update_tiles(NpeWorld *world);
uint16_t npe_world_tile(const NpeWorld *world, const NpeMap *map,
                         unsigned x, unsigned y);
/* The fixed map tail begins with 8 objects, 20 NPCs and 12 exits. */
int npe_map_object(const NpeMap *map, unsigned index, const uint8_t **raw);
int npe_map_object_info(const NpeMap *map, unsigned index,
                        NpeMapObject *object);
int npe_world_load_object_records(NpeWorld *world,const NpeMap *map);
/* Original pickup flag addressing: sign extend the word after subtracting1.
   Returns the byte, or-2 when the guest address is unavailable. */
int npe_world_object_flag(const NpeWorld *world,unsigned id);
int npe_world_object(const NpeWorld *world,const NpeMap *map,unsigned index,NpeMapObject *object);
/* Returns 1 record, 0 absent, -1 invalid group. Slot is zero-based. */
int npe_map_quest_item(const NpeMap *map, unsigned slot,
                        NpeMapQuestItem *event);
/* Reads indexed 7-byte map story events where the 17-group tail index is
   recognized; older layouts retain a conservative fallback scan. */
int npe_map_story_events(const NpeMap *map, NpeMapStoryEvent events[15],
                         unsigned *count);
/* Indexed map-tail group 4 contains up to five 8-byte two-way transitions. */
int npe_map_choice_event(const NpeMap *map, unsigned event_id,
                         NpeMapChoiceEvent *choice);
/* Indexed map-tail group 12: mode 11 money change and next mode. */
int npe_map_money_event(const NpeMap *map, unsigned event_id,
                        NpeMapMoneyEvent *event);
/* Indexed map-tail group 13: mode 12 inventory change and next mode. */
int npe_map_item_event(const NpeMap *map, unsigned event_id,
                       NpeMapItemEvent *event);
/* Indexed group 1: mode 15 scene animation and its followup. */
int npe_map_animation_event(const NpeMap *map, unsigned event_id,
                            NpeMapAnimationEvent *event);
/* Indexed group 2: mode 18 NPC movement and its followup. */
int npe_map_npc_move_event(const NpeMap *map, unsigned event_id,
                           NpeMapNpcMoveEvent *event);
int npe_map_teleport_event(const NpeMap *map, unsigned event_id,
                            NpeMapTeleportEvent *event);
int npe_map_script_walk_event(const NpeMap *map, unsigned event_id,
                              NpeMapScriptWalkEvent *event);
/* Indexed group 6: mode 10 party membership and followup. */
int npe_map_party_event(const NpeMap *map, unsigned event_id,
                        NpeMapPartyEvent *event);
/* Indexed group 3: mode 13 operand and followup. */
int npe_map_party_adjust_event(const NpeMap *map, unsigned event_id,
                               NpeMapPartyAdjustEvent *event);
int npe_map_skill_event(const NpeMap *map, unsigned event_id,
                        NpeMapSkillEvent *event);
int npe_map_battle_event(const NpeMap *map, unsigned event_id,
                         NpeMapBattleEvent *event);
/* Section 15 question records; index is zero-based as in mode 8. */
int npe_quiz_question(const NpeFile *file, unsigned index,
                      NpeQuizQuestion *question);
int npe_quiz_category_range(const NpeFile *file, unsigned category,
                            unsigned *first, unsigned *count);
/* Group 14 of the map tail index holds 11-byte movement event records;
   the loader rearranges 9 bytes into 52:7490 at a 10-byte RAM stride.
   Returns the first matching event, as the original input scanner does. */
int npe_map_move_trigger(const NpeMap *map, unsigned x, unsigned y,
                         unsigned progress, NpeMapMoveTrigger *trigger);
/* Reads one indexed group-14 record without testing position/progress;
   firmware mode 20 uses the saved mode and event fields this way. */
int npe_map_move_event(const NpeMap *map, unsigned event_id,
                       NpeMapMoveTrigger *trigger);
int npe_item(const NpeFile *file, unsigned index, NpeItem *item);
/* Native7938 values11 and above select section11 record(context-11).
   Catalog bytes are ID,count,then item IDs; duplicates collapse in the UI. */
int npe_shop_catalog(const NpeFile *file, unsigned context, NpeShopCatalog *shop);
int npe_shop_items(const NpeFile *file, unsigned context, uint8_t ids[254], unsigned *count);
int npe_world_shop_items(const NpeFile *,const NpeWorld *,uint8_t ids[254],unsigned *count);
int npe_world_shop_quantity(NpeWorld *,unsigned item_id,int direction);
int npe_world_shop_begin(const NpeFile *,NpeWorld *);
/* Mode4/event!=2 dispatcher:0 retains LCD,1 draws the initial list,-1 invalid data. */
int npe_world_shop_mode_begin(const NpeFile *,NpeWorld *);
/* Common F4:0 restores the list layer,1 requests map initialization. */
int npe_world_shop_return(NpeWorld *);
int npe_world_shop_move(const NpeFile *,NpeWorld *,int direction);
/* Navigation returns0 (no LCD),1 (draw), or2 (original backfill still busy). */
enum { NPE_SHOP_MOVE_PENDING = 2 };
int npe_world_shop_resume(const NpeFile *,NpeWorld *);
int npe_world_shop_trade(const NpeFile *,NpeWorld *);
/* Commit the original quantity-confirm transaction. Returns1 applied,
   0 insufficient money,-1 invalid data. UI quantity bounds are separate. */
int npe_world_trade_item(const NpeFile *file, NpeWorld *world,
                         unsigned item_id, uint8_t quantity, int selling);
/* Runtime loader: reads ordinal offsets without checking record count;
   the offset sum wraps to 32 bits; failed seeks continue after the offset
   word, as in Source3F:C370. Host file bounds still apply. */
int npe_native_item(const NpeFile *file, unsigned item_id, NpeItem *item);
int npe_item_record(const uint8_t *,size_t,NpeItem *);
int npe_world_item(const NpeFile *,NpeWorld *,unsigned,NpeItem *);
int npe_battle_item(const NpeFile *file, unsigned item_id, NpeItem *item);
int npe_item_bitmap(const NpeFile *file, unsigned index,
                    NpeIndexedBitmap *bitmap);
int npe_battle_item_bitmap(const NpeFile *file, unsigned item_id,
                            NpeIndexedBitmap *bitmap);
int npe_native_item_bitmap(const NpeFile *file, unsigned item_id,
                          NpeIndexedBitmap *bitmap);
int npe_native_item_cursor_bitmap(const NpeFile *file, uint16_t item_id,
                                 NpeIndexedBitmap *bitmap);
const uint8_t *npe_native_counter_glyph(char ch);
int npe_skill(const NpeFile *file, unsigned index, NpeSkill *skill);
int npe_skill_record(const uint8_t *,size_t,NpeSkill *);
int npe_native_skill(const NpeFile *,unsigned,NpeSkill *);
int npe_world_skill(const NpeFile *,NpeWorld *,unsigned,NpeSkill *);
int npe_world_skill_view(const NpeWorld *,uint8_t raw[42],NpeSkill *);
int npe_world_item_view(const NpeWorld *,uint8_t raw[41],NpeItem *);
int npe_world_battle_item_view(const NpeWorld *,unsigned,uint8_t raw[41],NpeItem *);
int npe_world_battle_magic_view(const NpeWorld *,unsigned,uint8_t raw[42],NpeSkill *);
int npe_skill_is_simple_enemy_attack(const NpeSkill *skill);
enum { NPE_BATTLE_EFFECT_ANIMATION_ONLY = 3 };
/* Returns 1 for HP/stat effects, 2 for four-state effects, 16..19 for
   revival effects, or ANIMATION_ONLY for the original default dispatch.
   Returns 0 for an invalid record or unsupported group/side flags. */
int npe_skill_battle_effect(const NpeSkill *skill);
int16_t npe_skill_status_effect(const NpeSkill *skill, unsigned status);
unsigned npe_world_available_skills(const NpeFile *file, const NpeWorld *world,
                                    unsigned member, unsigned ids[18]);
/* Map menu prefix scan loads the shared record before each level check;
   leaves the last candidate resident and records caster/count in7ADC/7A18. */
unsigned npe_world_available_cached_skills(const NpeFile *file, NpeWorld *world,
                                           unsigned member, unsigned ids[18]);
/* Returns -5 when a negative fourth battle state blocks spell selection;
   a previously queued spell remains executable after that state changes. */
int npe_world_battle_cast_magic(const NpeFile *file, NpeWorld *world,
                                unsigned skill_id, unsigned target);
/* Prepare the tentative command without marking this actor ready. */
int npe_world_battle_select_magic(const NpeFile *file, NpeWorld *world, unsigned skill_id);
/* Section 12 has three 94-byte initial character records. */
int npe_character(const NpeFile *file, unsigned index,
                  NpeCharacter *character);
/* Section 4 holds 39-byte enemy records. Other attributes are still being
   matched to the original battle state. */
int npe_enemy(const NpeFile *file, unsigned index, NpeEnemy *enemy);
int npe_enemy_bitmap(const NpeFile *file, const NpeEnemy *enemy,
                     NpeIndexedBitmap *bitmap);
int npe_map_npc(const NpeMap *map, unsigned index, const uint8_t **raw);
/* Fixed NPC records carry inclusive story-progress bounds at +8/+10.
   Zero/zero means the NPC is present at every progress value. */
int npe_map_npc_active(const uint8_t *raw, unsigned story_progress);
int npe_map_monster(const NpeMap *map, unsigned index,
                    NpeMapMonster *monster);
int npe_map_monster_active(const NpeMapMonster *monster,
                           unsigned story_progress);
int npe_map_exit(const NpeMap *map, unsigned index, NpeMapExit *exit);
int npe_world_load_exit_records(NpeWorld *world,const NpeMap *map);
int npe_world_exit(const NpeWorld *world,const NpeMap *map,unsigned index,NpeMapExit *exit);
int npe_world_select_exit(NpeWorld *world,unsigned choice);
int npe_world_confirm_exit(const NpeFile *file,NpeWorld *world);
int npe_render_world_static_player(const NpeFile *,const NpeWorld *,uint8_t *);
/* Eight one-based enemy IDs occupy the last eight bytes of the fixed map
   tail. A zero slot is empty. */
int npe_map_encounter(const NpeMap *map, unsigned slot,
                      unsigned *enemy_id);
/* Section 5 NPC table: one-based NPC IDs, 38-byte conditional cases. */
int npe_npc_action(const NpeFile *file, unsigned npc_id, unsigned case_index,
                   NpeNpcAction *action);
/* Original PRG source 35:443E..4504 clamps the saved NPC state byte to the
   final case before choosing a 38-byte record. Does not execute that record. */
int npe_npc_action_for_state(const NpeFile *file, unsigned npc_id,
                             uint8_t state, NpeNpcAction *action,
                             unsigned *selected_case);
/* World-state entry point for the original NPC selector. The caller must
   execute the selected action before reporting whether it succeeded. */
int npe_world_npc_action(const NpeFile *file, NpeWorld *world,
                         unsigned npc_id, NpeNpcAction *action,
                         unsigned *selected_case);
int npe_world_npc_commit(const NpeFile *file, NpeWorld *world,
                         unsigned npc_id, int action_succeeded);
/* Executes the verified type 0..3 paths when their conditions are supported.
   Returns -2 when the event needs a still-unimplemented firmware service. */
int npe_world_npc_event(const NpeFile *file, NpeWorld *world,
                        unsigned npc_id, NpeNpcEvent *event);
/* Called after dialogue has been displayed. Mode 14 applies the indexed
   map event; other valid modes and event IDs are restored as in firmware. */
int npe_world_finish_npc_event(NpeWorld *world, const NpeNpcEvent *event);
/* Applies an event selected by the original engine. Does not decide when
   NPC dialogue or another action selects that event. Returns 1 on success,
   0 when that event slot is not known, -1 for invalid arguments. */
int npe_world_apply_story_event(NpeWorld *world, unsigned event_id);
/* Mode14 service body, called by the display dispatcher AFTER drawing the
   old progress. apply_story_event queues it when defer_story_events is set;
   otherwise the numeric/headless API executes it immediately. */
int npe_world_execute_story_event(NpeWorld *world, unsigned event_id);
int npe_world_load_story_event_cache(NpeWorld *world,const NpeMap *map);
int npe_world_story_event(const NpeWorld *world,unsigned id,NpeMapStoryEvent *event);
/* Closes mode-5 text and restores its followup mode/event. Mode 14 applies
   the indexed map event; mode 5 selects another text record. */
int npe_world_finish_story(const NpeFile *file, NpeWorld *world);
/* Mode5 entry loads the same continuation words used by NPC dialogue.
   A displayed story finishes from this resident cache; numeric callers
   without a prepared page load its NPE control fields on demand. */
int npe_world_prepare_story(const NpeFile *file, NpeWorld *world);
/* A fresh mode-2 entry selects the NPC action using the current event ID.
   Preparing an already open dialogue does not advance the NPC again. */
int npe_world_prepare_mode2(const NpeFile *file, NpeWorld *world);
/* Map Space selects the facing NPC before testing the exit-choice byte.
   Returns1 for a fresh mode2 entry,0 with no target,-1 on invalid data. */
int npe_world_begin_talk(const NpeFile *file, NpeWorld *world);
/* Closes the prepared mode-2 dialogue and applies its current followup.
   Returns 0 when no open dialogue is prepared. */
int npe_world_finish_mode2(NpeWorld *world);
/* Mode 17 selects one of the two outcomes for the current event ID.
   choice_index is 0 or 1; mode 0 normalizes to map mode at dispatch. */
int npe_world_select_choice(const NpeFile *file, NpeWorld *world,
                            unsigned choice_index);
int npe_world_load_choice_events(NpeWorld *world,const NpeMap *map);
int npe_world_choice_event(const NpeFile *file,const NpeWorld *world,
                           unsigned id,NpeMapChoiceEvent *event);
int npe_render_choice_event(const NpeFile *file,
                            uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
void npe_render_choice_event_move(uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
/* Mode22: effect returns1 if paid/healed,0 if short,-1 for invalid mode.
   Both paths finish after the notice; the complete event word is retained. */
int npe_world_healing_effect(NpeWorld *world);
int npe_world_finish_healing_event(NpeWorld *world);
int npe_world_apply_money_event(const NpeFile *file, NpeWorld *world);
int npe_world_load_money_events(NpeWorld *world,const NpeMap *map);
int npe_world_money_event(const NpeWorld *world,unsigned id,NpeMapMoneyEvent *event);
int npe_world_change_money(NpeWorld *world,const NpeMapMoneyEvent *event,uint16_t *notice_amount);
int npe_world_finish_money_event(NpeWorld *world,const NpeMapMoneyEvent *event);
int npe_world_load_item_events(NpeWorld *world,const NpeMap *map);
int npe_world_item_event(const NpeWorld *world,unsigned id,NpeMapItemEvent *event);
int npe_world_change_item(NpeWorld *world,const NpeMapItemEvent *event);
int npe_world_finish_item_event(NpeWorld *world,const NpeMapItemEvent *event);
int npe_world_apply_item_event(const NpeFile *file, NpeWorld *world);
int npe_world_apply_move_event(const NpeFile *file, NpeWorld *world);
int npe_world_load_walk_triggers(NpeWorld *world,const NpeMap *map);
int npe_world_move_event(const NpeWorld *world,unsigned id,NpeMapMoveTrigger *event);
int npe_world_move_trigger(const NpeWorld *world,const NpeMap *map,
                           unsigned x,unsigned y,NpeMapMoveTrigger *event);
int npe_world_finish_game(NpeWorld *world);
int npe_world_close_menu(NpeWorld *world);
/* Mode 6 is the exploration system menu. Escape enters it from mode 1;
   Back exits to mode 1 without changing the current event number. */
int npe_world_open_system_menu(NpeWorld *world);
int npe_world_equipment_description_page(const NpeFile *,NpeWorld *,int direction);
int npe_world_close_system_menu(NpeWorld *world);
int npe_world_finish_animation_event(const NpeFile *file, NpeWorld *world);
/* Numeric resolution returns NPE_NPC_MOVE_NONTERMINATING without mutation
   when the native byte loop never exits. Display service can be cancelled. */
int npe_world_apply_npc_move_event(const NpeFile *file, NpeWorld *world);
int npe_world_load_npc_move_events(NpeWorld *world, const NpeMap *map);
int npe_world_npc_move_event(const NpeWorld *world, unsigned id, NpeMapNpcMoveEvent *event);
int npe_world_apply_teleport_event(const NpeFile *file, NpeWorld *world);
int npe_world_load_teleport_events(NpeWorld *world,const NpeMap *map);
int npe_world_teleport_event(const NpeWorld *world,unsigned id,NpeMapTeleportEvent *event);
int npe_world_apply_script_walk_event(const NpeFile *file, NpeWorld *world);
int npe_world_load_script_walk_events(NpeWorld *world, const NpeMap *map);
int npe_world_script_walk_event(const NpeWorld *world,unsigned id,NpeScriptWalkCache *event);
/* Return changed record slot+1,0 for no notice,-1 on error. Finish the
   event only after displaying its notice. */
int npe_world_change_party(NpeWorld *world, const NpeMapPartyEvent *event);
int npe_world_load_party_events(NpeWorld *world, const NpeMap *map);
int npe_world_party_event(const NpeWorld *world, unsigned id, NpeMapPartyEvent *event);
int npe_world_finish_party_event(NpeWorld *world, const NpeMapPartyEvent *event);
int npe_world_apply_party_event(const NpeFile *file, NpeWorld *world);
int npe_world_apply_skill_event(const NpeFile *file, NpeWorld *world);
int npe_world_load_skill_events(NpeWorld *world,const NpeMap *map);
int npe_world_load_scene_events(NpeWorld *world,const NpeMap *map);
int npe_world_scene_event(const NpeWorld *world,unsigned id,NpeMapAnimationEvent *event);
int npe_world_skill_event(const NpeWorld *world,unsigned id,NpeMapSkillEvent *event);
int npe_world_change_skill(NpeWorld *world,const NpeMapSkillEvent *event,unsigned *member);
int npe_world_finish_skill_event(NpeWorld *world,const NpeMapSkillEvent *event);
/* Mode 13 has no table records in Tianjiang; nonempty tables remain
   unsupported until the native party-reorder parameters are verified. */
int npe_world_apply_mode13_event(const NpeFile *file, NpeWorld *world);
int npe_world_load_party_adjust_event(NpeWorld *, const NpeMap *);
int npe_world_party_adjust_event(const NpeWorld *, unsigned, NpeMapPartyAdjustEvent *);
int npe_world_change_party_leader(NpeWorld *, const NpeMapPartyAdjustEvent *);
int npe_world_finish_party_adjust_event(NpeWorld *, const NpeMapPartyAdjustEvent *);
int npe_world_prepare_scripted_battle(const NpeFile *file, NpeWorld *world);
/* Prepare an uninitialized mode3 entered by a resource event. Event0 uses
   ordinary entry; other event words select the resident scripted table. */
int npe_world_prepare_battle(const NpeFile *file,NpeWorld *world);
int npe_world_select_battle_enemies(const NpeFile *file,NpeWorld *world);
int npe_enemy_record_service(const NpeBattleBitmapService *service);
int npe_party_battle_positions_initialize(const NpeResidentHeap *heap);
int npe_party_battle_place(const NpeResidentHeap *heap);
int npe_party_battle_audio(const NpeResidentHeap *heap);
typedef struct {uint32_t filename,offset;int32_t length;} NpeBattleSoundRequest;
/*3E:F85E. Returns1 for playback,0 for no request,-1 for a bad adapter. */
int npe_battle_sound_request(const NpeResidentHeap *,NpeBattleSoundRequest *);
typedef struct {uint32_t filename,offset,length,callback;} NpeRpgSoundRequest;
typedef NpeRpgSoundRequest NpeMapSoundRequest;
int npe_scene_sound_request(const NpeResidentHeap *,unsigned,NpeRpgSoundRequest *);
int npe_battle_background_start(const NpeResidentHeap *,unsigned,NpeRpgSoundRequest *);
int npe_battle_background_replay(const NpeResidentHeap *,NpeRpgSoundRequest *);
/* Phase0=scene slot,1=battle random roll,2=battle EOF selected range. */
int npe_rpg_runtime_background_sound(const NpeFile *,unsigned,unsigned,NpeRpgSoundRequest *);
int npe_world_sound_battle_replay(NpeWorld *);
int npe_map_sound_request(const NpeResidentHeap *,NpeMapSoundRequest *);
int npe_rpg_runtime_map_sound(const NpeFile *,NpeMapSoundRequest *);
int npe_rpg_runtime_battle_sound(const NpeFile *,NpeBattleSoundRequest *);
int npe_world_battle_sound(NpeWorld *);
/* Phase0: initialize coordinate table and place party; phase1: voice cache. */
int npe_rpg_runtime_party_descriptors(const NpeFile *file,unsigned party_count,int phase);
int npe_rpg_runtime_load_enemies(const NpeFile *file,unsigned party_count);
int npe_world_load_battle_enemies(const NpeFile *file,NpeWorld *world);
int npe_world_load_battle_events(NpeWorld *world,const NpeMap *map);
int npe_world_load_random_enemy_records(NpeWorld *world,const NpeMap *map);
int npe_world_battle_event(const NpeFile *file,const NpeWorld *world,
                           unsigned id,NpeMapBattleEvent *event);
/* Returns 0 after writing both coordinates, -1 for invalid arguments. */
int npe_world_npc_position(const NpeWorld *world, const uint8_t *npc,
                             unsigned slot, unsigned *x, unsigned *y);
int npe_world_load_npc_records(NpeWorld *world, const NpeMap *map);
int npe_world_npc(const NpeWorld *world, const NpeMap *map,
                  unsigned slot, const uint8_t **raw);
int npe_world_load_map_encounter_records(NpeWorld *world, const NpeMap *map);
int npe_world_monster(const NpeWorld *world,const NpeMap *map,
                      unsigned slot,NpeMapMonster *monster);
int npe_world_chase(const NpeWorld *world,const NpeMap *map,
                    unsigned slot,NpeMapChase *chase);
/* Initializes a mode-8 question. Event IDs 0..9 select a random category;
   IDs >= 10 name a fixed, zero-based question. */
int npe_world_prepare_question(const NpeFile *file, NpeWorld *world,
                               NpeQuizQuestion *question);
/* Answers the prepared mode-8 question. Returns 1 for correct, 0 for wrong. */
int npe_world_answer_question(const NpeFile *file, NpeWorld *world,
                              unsigned answer_index);
int npe_world_answer_text(const NpeFile *file, NpeWorld *world);
/* Original key codes. 0 ignored, 1 LCD changed, 2 submit, 3 category exit. */
int npe_world_quiz_key(const NpeFile *file, NpeWorld *world, unsigned key);
int npe_world_finish_quiz(NpeWorld *world, uint16_t *award);
/* Mode 23: event ID is a percent of the first character's XP threshold. */
int npe_world_apply_experience_event(NpeWorld *world);
/* Mode24 decrements valid NPC state bytes with original byte wrap. */
int npe_world_apply_npc_state_event(NpeWorld *world);
/* Reads one GBK dialogue stream up to ASCII @, &, or NUL. */
int npe_dialogue_page(const NpeFile *file, uint32_t offset,
                      NpeDialoguePage *page);
/* Section 10: indexed 7-byte header followed by one GBK text stream. */
int npe_story_record(const NpeFile *file, unsigned index,
                     NpeStoryRecord *record);
/* Recreates the original exit-spawn viewport calculation. */
int npe_map_spawn_viewport(const NpeMap *map, unsigned x, unsigned y,
                           NpeViewport *viewport);
int npe_actor_bitmap(const NpeFile *file, unsigned actor, unsigned direction,
                     unsigned frame, NpeIndexedBitmap *bitmap);
int npe_npc_bitmap(const NpeFile *file, unsigned graphic, unsigned direction,
                   NpeIndexedBitmap *bitmap);
int npe_world_init(const NpeFile *file, NpeWorld *world,
                   unsigned map_number, unsigned x, unsigned y);
/* Reconstructs the original new-game position from character record 0.
   The firmware sets screen position (7,1), map 1 and story progress 1. */
int npe_world_new_game(const NpeFile *file, NpeWorld *world);
/* Title new-game / empty-slot construction in an initialized runtime.
   Resets RPG data while retaining native resident caches, RNG and music. */
int npe_world_restart_game(const NpeFile *file, NpeWorld *world);
/* Portable host save for map mode, separate from the NP560 native save format.
   Loads transactionally and rejects a save from another NPE byte stream. */
int npe_world_save(const NpeFile *file, const NpeWorld *world,
                   const char *path);
int npe_world_load(const NpeFile *file, NpeWorld *world,
                   const char *path);
/* Restore into an initialized runtime. Retains resident RNG/music and live
   observers; emits no sound. The title/system caller handles map reentry. */
int npe_world_restore(const NpeFile *file, NpeWorld *world,
                      const char *path);
uint64_t npe_game_fingerprint(const NpeFile *file);
/* Sets the recovered 32-bit resident RNG state, including valid state zero. */
void npe_world_seed(NpeWorld *world, uint32_t seed);
void npe_world_sound_bind(NpeWorld *world, NpeSoundObserver observer, void *context);
void npe_world_sound_map_loaded(NpeWorld *world);
void npe_world_sound_battle_start(NpeWorld *world);
/* One-based valid scene group; opening=1. Other scenes draw even with music off. */
void npe_world_sound_scene_start(NpeWorld *world, unsigned group);
void npe_world_sound_map_resume(NpeWorld *world);
/* Original CPU31:A3F4 EOF callback: unconditional live-directory replay. */
int npe_world_sound_map_replay(NpeWorld *world);
void npe_world_sound_stop(NpeWorld *world);
typedef void (*NpeMusicControllerObserver)(void *,unsigned);
int npe_music_controller_apply(const NpeResidentHeap *,unsigned,NpeMusicControllerObserver,void *);
int npe_rpg_runtime_music_controller(const NpeFile *,unsigned,NpeMusicControllerObserver,void *);
int npe_world_music_controller(NpeWorld *,unsigned);
void npe_world_music_set(NpeWorld *world, int enabled);
/* Normal attack branch of PRG 2D:385E..3A54, given the two RNG remainders.
   The floor statistic is battle-record byte +0x18, not character level. */
int npe_battle_attribute_multiplier(unsigned actor, unsigned target, unsigned roll);
unsigned npe_battle_normal_damage(unsigned attack, unsigned defense,
                                  unsigned floor_stat, unsigned variance_roll);
/* Returns a battle result or -1 for invalid state/target. The current
   party round scheduler is provisional; damage is source-based. */
int npe_world_battle_attack(NpeWorld *world, unsigned target);
/* Prepare ordinary attack metadata before entering its target selector. */
int npe_world_battle_select_attack(NpeWorld *world, unsigned target);
/* Select a physical host slot and write the original combined record index. */
int npe_world_battle_select_target(NpeWorld *world, unsigned target, int party);
/* Leaving a resource list restores the first hostile record as target. */
int npe_world_battle_cancel_target(NpeWorld *world);
/* Source 32:E09A..E18E: reduce the RNG result modulo100, choose from
   all active party slots, then scan forward (with wrap) past fallen members.
   Does not advance RNG; returns a physical party slot, or -1 if none live. */
int npe_battle_party_target(const NpeWorld *world, unsigned random_roll);
/* Source2D:3DD6..3E3A: offensive type1 spell adds signed current ATK/3
   to the NPE HP word. Low-word arithmetic; nonpositive result becomes12. */
unsigned npe_battle_party_spell_damage(unsigned hp_effect, unsigned current_attack);
uint16_t npe_battle_revival_hp(unsigned effect_type, uint16_t max_hp);
unsigned npe_battle_enemy_normal_damage(unsigned attack, unsigned defense,
                                        unsigned level, unsigned variance_roll,
                                        int defending);
/* Physical actor indices 0..2 are party, 3..5 enemies. Highest signed
   speed first, stable record order for ties; skips dead/sleep/confused. */
int npe_battle_next_actor(const NpeWorld *world, unsigned acted_mask);
/* Action services read7ACC while executing; menu callers read7A9E. */
unsigned npe_world_battle_action_actor(const NpeWorld *world);
int npe_world_battle_group_attack(NpeWorld *world);
int npe_world_battle_combine_attack(NpeWorld *world);
int npe_world_battle_prepare_group(NpeWorld *world);
int npe_world_battle_prepare_combine(NpeWorld *world);
int npe_world_battle_defend(NpeWorld *world);
/* Escape action: random battle uses native 90/100 threshold and RNG step;
   failed escape replaces party commands with ordinary attacks. */
int npe_world_battle_flee(NpeWorld *world);
/* Battle item: battle result, -1 invalid, -2 rejected by native side/revival gate. */
int npe_world_battle_use_item(const NpeFile *file, NpeWorld *world,
                              unsigned item_id, unsigned character);
/* Selection debits once; cancellation/backtracking refunds the reservation.
   NPE item +37 selects the side: 0 enemy, 1 party, 2 rejected here. */
int npe_world_battle_reserve_throw_item(const NpeFile *file, NpeWorld *world,
                                        unsigned item_id);
int npe_world_battle_reserve_use_item(const NpeFile *file, NpeWorld *world,
                                     unsigned item_id);
int npe_world_battle_cancel_item_selection(NpeWorld *world);
int npe_world_battle_previous_actor(NpeWorld *world);
int npe_world_battle_capture_selector(NpeWorld *world);
int npe_world_battle_throw_item(const NpeFile *file, NpeWorld *world,
                                unsigned item_id, unsigned target);
int npe_world_battle_cast_skill(const NpeFile *file, NpeWorld *world,
                                unsigned skill_id, unsigned target);
/* Mode-6 field casting: 1 on the observed successful path, -3 without QI,
   -4 if not learned, -2 when this field effect is not yet modeled. */
int npe_world_menu_cast_skill(const NpeFile *file, NpeWorld *world,
                              unsigned caster, unsigned target,
                              unsigned skill_id);
/* Observed positive HP recovery skills target a living party member. */
int npe_world_battle_cast_heal_skill(const NpeFile *file, NpeWorld *world,
                                     unsigned skill_id, unsigned target);
/* Offensive skill targets an enemy formation slot. */
int npe_world_battle_cast_attack_skill(const NpeFile *file,
                                       NpeWorld *world,
                                       unsigned skill_id, unsigned target);
/* Cycle through living targets, preserving formation slots. */
int npe_world_battle_move_icon(const NpeWorld *world, unsigned icon, unsigned direction);
int npe_world_battle_cycle_target(NpeWorld *world, int direction);
int npe_world_step(const NpeFile *file, NpeWorld *world, unsigned direction);
typedef int (*NpeMapDisplayCommit)(void *context,const uint8_t *pixels,
                                   unsigned wait_parameter);
/* Mode18 commits its entry map and retained-background NPC steps. The
   callback receives the ROM03:4120 parameter after each drawn step. */
int npe_world_npc_move_with_display(const NpeFile *file,NpeWorld *world,
                                    uint8_t *pixels,NpeMapDisplayCommit commit,
                                    void *context);
/* Script steps use the same retained walk compositor, without the map
   controller's encounter/chase/exit continuation. A NULL callback frame
   requests a wait only and must not submit another LCD frame. */
int npe_world_script_walk_with_display(const NpeFile *file,NpeWorld *world,
                                       uint8_t *pixels,NpeMapDisplayCommit commit,
                                       void *context);
/* Same movement controller, with retained-LCD commits before numeric
   movement/encounters. A nonzero display callback aborts with -2. */
int npe_world_step_with_display(const NpeFile *file,NpeWorld *world,
                                unsigned direction,uint8_t *pixels,
                                NpeMapDisplayCommit commit,void *context);
int npe_world_update_chase(const NpeFile *file, NpeWorld *world);
int npe_world_update_npcs(const NpeFile *file, NpeWorld *world);
/* Returns 1 with NPC ID when one occupies the tile in front, 0 if none. */
int npe_world_facing_npc(const NpeFile *file, const NpeWorld *world,
                         unsigned *npc_id);
/* Object action returns 1 for a new event, 0 for no object or a completed
   object, -1 for invalid map data, -2 for unsupported data or full count.
   It increments the indexed inventory word and marks the completion flag. */
int npe_world_object_action(const NpeFile *file, NpeWorld *world,
                            NpeMapObject *object);
int npe_world_object_action_with_display(const NpeFile *,NpeWorld *,NpeMapObject *,
                                        NpeMapDisplayCommit,void *);
/* Map item IDs are one-based. Returns 1 consumed, 0 equipment rejected,
   -1 invalid/missing data or inventory, -2 enemy-only, -3 quest not matched.
   Battle use has its own reserve/execute API above. */
int npe_world_use_item(const NpeFile *file, NpeWorld *world,
                       unsigned item_id, unsigned character);
int npe_world_equip_item(const NpeFile *file, NpeWorld *world,
                         unsigned item_id, unsigned character);
int npe_world_unequip_item(const NpeFile *file, NpeWorld *world,
                           unsigned character, unsigned slot);
/* Original wear-page U: consumes cached7AB2; returns1 change,0 empty,-1 error. */
int npe_world_unequip_selected(const NpeFile *,NpeWorld *);
int npe_render_world(const NpeFile *file, const NpeWorld *world,
                     uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_world_view(const NpeFile *file,const NpeWorld *world,unsigned cols,unsigned rows,
    uint8_t *pixels,unsigned *view_left,unsigned *view_top);
int npe_render_world_frame(const NpeFile *file, const NpeWorld *world,
                           unsigned actor_frame,
                           uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
/* Source4C:6AB8: map-load pass, with actors in slot order and final margins. */
int npe_render_world_load(const NpeFile *file,const NpeWorld *world,
                           uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
/* Apply the NPC periodic erase/redraw to the retained LCD. before is the
   state immediately before NPC movement, after immediately after it.
   Chase rendering must already have been applied by the caller. */
int npe_render_npc_periodic(const NpeFile *file, const NpeWorld *before,
                             const NpeWorld *after,
                             uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
/* Chase erase/draw in slot order, then NPC periodic composition. Includes
   the map commit preceding a chase battle's separate battle rendering. */
int npe_render_world_periodic(const NpeFile *file,const NpeWorld *before,
                               const NpeWorld *after,
                               uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
/* Original four walking poses at the old viewport position: 0,1,2,0.
   Mutates the retained LCD; does not move actors or consume RNG. */
int npe_render_world_walk_pose(const NpeFile *file,const NpeWorld *world,
                               unsigned step,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
/* Blocked-attempt actor pass. Returns1 for its immediate static commit,
   0 when four walking poses follow, negative on unsupported data. */
int npe_render_world_walk_blocked(const NpeFile *file,const NpeWorld *world,
                                  uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
/* After the fourth old-position pose: erase, scroll the retained map,
   then NPC/fixed-monster slot passes and the original LCD commits. */
int npe_render_world_walk_finish(const NpeFile *file,const NpeWorld *before,
                                 const NpeWorld *after,uint8_t *pixels,
                                 NpeMapDisplayCommit commit,void *context);


void npe_render_system_menu_overlay(const NpeFile *file,
                                    const NpeWorld *world,
                                    unsigned selection,
                                    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
void npe_render_system_submenu_overlay(const NpeFile *file,
                                       const NpeWorld *world,
                                       unsigned selection,
                                       uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
/* RPG2.1 music popup: 0 is on, 1 is off; pixels retain its parent LCD. */
int npe_render_music_menu_overlay(const NpeFile *file, unsigned selection,
                                   uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
void npe_render_property_submenu_overlay(const NpeFile *file,
                                         const NpeWorld *world,
                                         unsigned selection,
                                         uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
void npe_render_item_submenu_overlay(const NpeFile *file,
                                     const NpeWorld *world,
                                     unsigned selection,
                                     uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_save_slots_overlay(const NpeFile *file, unsigned left,
                                  unsigned top, const unsigned levels[3],
                                  unsigned selection, const uint8_t *title_font16,
                                  uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);

/* Composes the verified one-enemy battle art layout. This renders only the
   resource layers; battle state and HUD text are separate work. */
int npe_render_battle_scene(const NpeFile *file, unsigned enemy_id,
                            uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_battle_formation_scene(const NpeFile *file,
                                      const uint8_t enemy_ids[3],
                                      unsigned enemy_count,
                                      uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_battle_world_scene(const NpeFile *file, const NpeWorld *world,
                                  uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_battle_combine_stage(const NpeFile *file, const NpeWorld *world,
                                    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_battle_combine_prepare(const NpeFile *file, const NpeWorld *world,
                                      uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_battle_attack_stage(const NpeFile *, const NpeWorld *, unsigned,
                                    unsigned, uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_battle_flee_stage(const NpeFile *, const NpeWorld *, unsigned,
                                 uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_battle_enemy_attack_stage(const NpeFile *, const NpeWorld *, unsigned, unsigned,
                                       uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_battle_party_damage_numbers(const NpeWorld *, unsigned,
                                         uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_battle_enemy_recovery_stage(const NpeFile *, const NpeWorld *, unsigned,
                                         unsigned, uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_battle_enemy_recovery_numbers(const NpeWorld *, unsigned,
                                           uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_battle_enemy_spell_stage(const NpeFile *, const NpeWorld *, unsigned,
                                      unsigned, uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_battle_poison_numbers(const NpeWorld *, unsigned,
                                    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_battle_party_spell_stage(const NpeFile *, const NpeWorld *, unsigned,
                                       unsigned, uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_battle_party_spell_numbers(const NpeWorld *, unsigned,
                                         uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_battle_defend_stage(const NpeFile *, const NpeWorld *,
                                    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_battle_defend_restore(const NpeFile *,const NpeWorld *,
                                      uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
/* Runtime31:8756 mode2: zero is transparent; every nonzero pixel replaces. */
int npe_render_battle_animation_frame(const NpeFile *file, unsigned group,
                                      unsigned frame, int left, int top,
                                      uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_battle_combine_restore(const NpeFile *file, const NpeWorld *world,
                                      uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_battle_damage_numbers(const NpeWorld *world, unsigned step,
                                     uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_battle_actors_scene(const NpeFile *file, const NpeWorld *world,
                                   uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_battle_round_restore(const NpeFile *file, const NpeWorld *world,
                                     unsigned selective,
                                     uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_battle_controls(const NpeFile *file, unsigned icon,
                                uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_battle_cursor(const NpeFile *file, const NpeWorld *world,
                              uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
void npe_render_battle_status_overlay(const NpeWorld *world,
                                       uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
/*38:8296 explicit refresh after restoring a selector's saved LCD plane. */
void npe_render_battle_status_refresh(const NpeWorld *world,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
int npe_render_battle_party_target(const NpeFile *file, const NpeWorld *world,
                                   unsigned target,
                                   uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_clear_battle_party_target(const NpeWorld *world, unsigned target, uint8_t *pixels);
int npe_draw_battle_party_target_pointer(const NpeFile *file, const NpeWorld *world,
                                        unsigned target, uint8_t *pixels);
int npe_update_battle_party_target(const NpeFile *file, const NpeWorld *world,
                                   unsigned previous, unsigned target,
                                   uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_battle_enemy_target(const NpeFile *file, const NpeWorld *world,
                                    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_clear_battle_enemy_target(const NpeWorld *world, unsigned target,
                                  uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_draw_battle_enemy_target_pointer(const NpeFile *file, const NpeWorld *world,
                                        unsigned target, uint8_t *pixels);
int npe_update_battle_enemy_target(const NpeFile *file, const NpeWorld *world,
                                    unsigned previous,
                                    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
/* Invert exactly the 14x14 native command-icon selection square. */
int npe_render_battle_icon_selection(unsigned icon,
                                    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
/* Native battle command bitmap 1 (five choices) or 2 (item submenu). */
int npe_render_battle_command_menu(const NpeFile *file, unsigned menu,
                                   unsigned selection,
                                   uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);

/* Native reward panel and signed word totals, over the retained battle scene. */
int npe_render_battle_rewards(const NpeFile *file, const NpeWorld *world,
                              uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
int npe_render_battle_reward_panel(const NpeFile *, uint16_t, uint16_t,
                                   uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]);
/* Original battle counter glyphs and positions, paired with 12x12 体／气. */
void npe_render_battle_hud_values(unsigned hp, unsigned max_hp, unsigned qi,
                                  unsigned max_qi,
                                  uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
/* Renders the 15x7 16-pixel tile viewport into raw 4-bit pixels. */
int npe_render_map(const NpeFile *file, const NpeMap *map,
                   unsigned left, unsigned top,
                   uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]);
#endif
