#include "npe.h"
#include "battle_status_glyphs.inc"
#include "battle_spell_glyphs.inc"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

unsigned npe_world_battle_action_actor(const NpeWorld *world){
    if(!world)return UINT_MAX;
    /*39:5D34 and the action services address the physical record through
      7ACC.7A9E remains the independently resident menu selection. */
    return world->battle_executing?(unsigned)(uint8_t)(world->battle_execution_slot-1):
        world->battle_actor;
}

static unsigned world_record_party_count(const NpeWorld *w){
    if(w->battle_party_flags_saved)return w->battle_record_party_count;
    unsigned n=3;while(n&&!w->party_active[n-1])--n;return n;
}
static void record_word(uint8_t *r,unsigned o,unsigned v){r[o]=(uint8_t)(v>>8);r[o+1]=(uint8_t)v;}
int npe_world_battle_record_image(const NpeWorld *w,unsigned slot,uint8_t r[148]){
    if(!w||!r||slot>=6)return -1;
    memcpy(r,w->battle_record_residue[slot],148);
    /* The physical battle bank exists before its first entry as well as
       after exit. Map fields cannot synthesize or change that bank. */
    if(!w->battle_party_flags_saved){
        if(slot==5)record_word(r,146,w->inventory_prefix_word);
        return 0;
    }
    unsigned count=world_record_party_count(w);
    if(count>3)return -1;
    if(slot<count){
        unsigned m=slot;const NpeCharacterMetadata *c=&w->battle_characters[m];
        memcpy(r,w->battle_names[m],12);r[12]=w->battle_party_flags_saved?w->battle_record_ids[m]:w->party_ids[m];r[17]=c->sex;
        memcpy(r+18,w->equipped_items[m],6);r[24]=w->level[m];
        memcpy(r+25,w->skill_slots[m],18);r[43]=w->battle_party_record_active[m];r[44]=w->battle_attack_groups[m];
        for(unsigned i=0;i<8;++i)record_word(r,46+2*i,c->pose_ids[i]);
        for(unsigned i=0;i<4;++i)record_word(r,62+2*i,(uint16_t)w->battle_party_status[m][i]);
        record_word(r,70,w->battle_attack_turns[m]);record_word(r,72,w->battle_defense_turns[m]);
        record_word(r,74,w->experience_to_next[m]);record_word(r,76,w->experience[m]);
        record_word(r,78,w->max_hp[m]);record_word(r,80,w->hp[m]);
        record_word(r,82,w->max_qi[m]);record_word(r,84,w->qi[m]);
        for(unsigned i=0;i<3;++i)record_word(r,86+2*i,w->other_stats[m][i]);
        record_word(r,92,w->attack[m]);record_word(r,94,w->defense[m]);
        record_word(r,96,(uint16_t)(w->attack[m]+w->battle_skill_attack_bonus[m]));
        record_word(r,98,(uint16_t)(w->battle_party_defenses[m]+w->battle_skill_defense_bonus[m]));
        r[126]=w->battle_native_targets[m];r[127]=w->battle_native_kinds[m];
        r[128]=w->battle_magic_types[m];r[129]=w->battle_item_types[m];
        r[130]=w->battle_item_type_copies[m];r[131]=w->battle_item_groups[m];
        r[132]=w->battle_item_sides[m];r[133]=w->battle_item_ids[m];r[134]=w->battle_skill_ids[m];
        memcpy(r+136,c->animation_ids,3);memcpy(r+139,w->growth[m],7);
    }else if(slot-count<3){
        unsigned e=slot-count;
        r[24]=w->battle_enemy_damage_floor_stats[e];
        memcpy(r+25,w->battle_enemy_skill_ids[e],3);
        for(unsigned i=0;i<4;++i)record_word(r,62+2*i,(uint16_t)w->battle_enemy_status[e][i]);
        record_word(r,70,w->battle_enemy_attack_turns[e]);record_word(r,72,w->battle_enemy_defense_turns[e]);
        record_word(r,76,w->battle_enemy_experience_rewards[e]);record_word(r,78,w->battle_enemy_max_hps[e]);
        record_word(r,80,w->battle_enemy_hps[e]);record_word(r,82,w->battle_enemy_max_qis[e]);
        record_word(r,84,w->battle_enemy_qis[e]);record_word(r,86,w->battle_enemy_spell_stats[e]);
        record_word(r,88,w->battle_enemy_speeds[e]);record_word(r,90,w->battle_enemy_lucks[e]);
        record_word(r,92,w->battle_enemy_base_attacks[e]);record_word(r,94,w->battle_enemy_base_defenses[e]);
        record_word(r,96,w->battle_enemy_attacks[e]);record_word(r,98,w->battle_enemy_defenses[e]);
        record_word(r,100,w->battle_enemy_gold_rewards[e]);memcpy(r+122,w->battle_enemy_drop_ids[e],2);
    }
    if(slot==5)record_word(r,146,w->inventory_prefix_word);
    return 0;
}
#include "resident_records.inc"
#include "resident_exits.inc"
#include "resident_objects.inc"
#include "resident_npcs.inc"
#include "resident_map_encounters.inc"
#include "resident_move_events.inc"
#include "resident_party_teleport.inc"
#include "resident_story_events.inc"
#include "resident_walk_triggers.inc"
#include "resident_money_items.inc"
#include "resident_scene_skill.inc"
#include "resident_choice_events.inc"
#include "resident_battle_events.inc"
#include "resident_resources.inc"
static int world_resource_read(void *context,uint32_t address,uint8_t *bytes,unsigned count){
    return npe_world_resident_read(context,address,bytes,count);
}
static int world_resource_write(void *context,uint32_t address,const uint8_t *bytes,unsigned count){
    return npe_world_resident_write(context,address,bytes,count);
}
int npe_world_resources_open(NpeFile *file,NpeWorld *world){
    if(!world)return -1;
    const NpeResidentBindings bindings={world,world_resource_read,world_resource_write};
    /* A map/title owner does not preallocate the entry's32KiB buffer.
       Existing battle owners and retained flee buffers need packed art. */
    int result=world->battle_party_flags_saved || (world->battle_resource_flags>>8)==1?
        npe_rpg_runtime_open(file,&bindings):npe_rpg_runtime_open_idle(file,&bindings);
    if(result || !world->map_number)return result;
    if(npe_rpg_runtime_map_load(file,world,world->terrain_snapshot_ready)){npe_rpg_runtime_close(file);return -1;}
    return 0;
}
int npe_world_battle_resources(const NpeFile *file,NpeWorld *world,int active){
    if(!world)return -1;
    if(file && file->rpg_runtime){
        if(npe_rpg_runtime_owner(file)!=world || npe_rpg_runtime_battle_resources(file,active))return -1;
    }
    world->battle_resource_flags=(uint16_t)((active?0x0100u:0u)|(world->battle_resource_flags&255u));
    return 0;
}
#include "world_enemy_loader.inc"
static unsigned world_learning_record_byte(const NpeWorld *w,unsigned member,uint8_t index){
    unsigned o=148*member+25+index;uint8_t record[148];
    if(npe_world_battle_record_image(w,o/148,record))return 0;
    return record[o%148];
}

const NpeCharacterMetadata *npe_world_battle_character(const NpeWorld *world,unsigned member) {
    if(!world || member>=3)return NULL;
    return world->battle_party_flags_saved ? &world->battle_characters[member] :
                                             &world->party_characters[member];
}
unsigned npe_world_battle_pose(const NpeWorld *world,unsigned member,unsigned pose) {
    const NpeCharacterMetadata *character=npe_world_battle_character(world,member);
    return character && pose<8 ? character->pose_ids[pose] : 0;
}
const uint8_t *npe_world_battle_name(const NpeWorld *world,unsigned member) {
    if(!world || member>=3)return NULL;
    return world->battle_party_flags_saved ? world->battle_names[member] : world->party_names[member];
}
uint16_t npe_world_portrait_index(const NpeFile *file,const NpeWorld *world,
                                  unsigned member) {
    /*3D:8D66..8D72 and38:12A2..12C2 read map6A02, the eighth
      image word.7AF8 is the indexed image group boundary vectors[6];
      the separate section9 battle table does not include portraits. */
    if(!file || !world || member>=3)return UINT16_MAX;
    return npe_image_group_index(file,6,world->party_characters[member].pose_ids[7]);
}
int npe_world_portrait_bitmap(const NpeFile *file,const NpeWorld *world,
                              unsigned member,NpeIndexedBitmap *bitmap) {
    if(!file || !world || member>=3)return -1;
    return npe_indexed_bitmap(file,npe_world_portrait_index(file,world,member),bitmap);
}
uint16_t npe_world_inventory_word(const NpeWorld *world,unsigned item) {
    if(!world)return 0;
    if(item<255)return world->condition_words[item];
    if(item==255)return (uint16_t)((unsigned)world->party_names[0][0]<<8|world->party_names[0][1]);
    return 0;
}
int npe_world_set_inventory_word(NpeWorld *world,unsigned item,uint16_t value) {
    if(!world || item>255)return -1;
    if(item<255)world->condition_words[item]=value;
    else{world->party_names[0][0]=(uint8_t)(value>>8);world->party_names[0][1]=(uint8_t)value;}
    return 0;
}
static void world_refund_battle_item(NpeWorld *world,unsigned item) {
    if(item && item<=255)
        npe_world_set_inventory_word(world,item,(uint16_t)(npe_world_inventory_word(world,item)+1u));
}

enum {
    COMMAND_ATTACK, COMMAND_ATTACK_SKILL, COMMAND_ITEM, COMMAND_THROW,
    COMMAND_GROUP, COMMAND_DEFEND, COMMAND_HEAL_SKILL, COMMAND_BUFF_SKILL,
    COMMAND_MAGIC, COMMAND_COMBINE
};

static int world_rebuild_equipment_status(const NpeFile *file,
                                           NpeWorld *world, unsigned character);

static int render_world_chase_only(const NpeFile *,const NpeWorld *,
                                   const NpeWorld *,uint8_t *);
static int draw_world_monster_slot(const NpeFile *,const NpeMap *,
                                    const NpeWorld *,unsigned,uint8_t *);
static int world_step_service(const NpeFile *,NpeWorld *,unsigned,uint8_t *,
                              NpeMapDisplayCommit,void *,int);

static int world_record_tile(NpeWorld *world,uint16_t index,uint16_t tile){
    unsigned i;
    for (i = 0; i < world->tile_override_count; ++i)
        if (world->tile_override_indices[i] == index) break;
    if(world->terrain_snapshot_ready && index<world->terrain_snapshot_count)
        world->terrain_snapshot_cells[index]=tile;
    if(i>=NPE_TILE_OVERRIDES){
        /* The physical grid remains authoritative after its own53-pair
           save projection fills; recording must not abort the native loop. */
        if(world->terrain_snapshot_ready || (world->battle_file &&
           npe_rpg_runtime_owner(world->battle_file)==world &&
           npe_rpg_runtime_map_current(world->battle_file,world->map_number)==1))return 0;
        return -1;
    }
    if (i == world->tile_override_count) ++world->tile_override_count;
    world->tile_override_indices[i] = index;
    world->tile_override_tiles[i] = tile;
    return 0;
}
static int world_write_tile(NpeWorld *world,uint16_t index,uint16_t tile){
    if(world->battle_file && npe_rpg_runtime_owner(world->battle_file)==world &&
       npe_rpg_runtime_map_tile_write(world->battle_file,world->map_number,index,tile)<0)return -1;
    return world_record_tile(world,index,tile);
}
typedef struct {NpeWorld *world;int error;} WorldTileObservation;
static void world_observe_tile(void *context,uint16_t index,uint16_t tile){
    WorldTileObservation *o=context;if(world_record_tile(o->world,index,tile))o->error=1;
}

int npe_world_load_tile_records(NpeWorld *world,const NpeMap *map){
    unsigned count;
    if(!world || npe_map_tile_changes(map,world->tile_changes,&count))return -1;
    world->tile_change_count=(uint8_t)count;
    /* This standalone projection seeds only the declared file fields.
       The actual runtime loader handles all byte counts and address aliases. */
    for(unsigned i=0;i<3;++i){
        uint8_t *record=world->tile_record_bytes+64u*i;
        if(i<count){
            const NpeMapTileChange *change=&world->tile_changes[i];
            record_word(record,0,change->progress);
            for(unsigned j=0;j<change->count;++j){
                record_word(record,2+2u*j,change->indices[j]);
                record_word(record,32+2u*j,change->tiles[j]);
            }
            record[62]=0;
        }else memset(record,0,62);
    }
    return 0;
}
int npe_world_load_tiles(NpeWorld *world, const NpeMap *map) {
    if(npe_world_load_tile_records(world,map))return -1;
    world->terrain_snapshot_ready=0;world->terrain_buffer_snapshot_ready=0;
    unsigned count=world->tile_change_count;
    world->tile_change_applied = world->tile_override_count = 0;
    memset(world->tile_override_indices, 0, sizeof(world->tile_override_indices));
    memset(world->tile_override_tiles, 0, sizeof(world->tile_override_tiles));
    /*29:C13E calls25:3082(type1,-1) before the story tile service.
      Unlike the pickup guard, bulk clearing tests flags equal to1. */
    for (unsigned i = 0; i < 8; ++i) {
        NpeMapObject object;
        if (npe_world_object(world, map, i, &object)) return -1;
        /* Source42:31EE stops the bulk flag loop at100; single pickup
           still accepts an ID outside this bulk initialization range. */
        if (object.id && object.id <= 100 &&
            world->object_done[object.id - 1] == 1)
            if(world_write_tile(world, (uint16_t)(object.y * map->width + object.x), 53))return -1;
    }
    /* Loader 29:D36C..D43E applies all declared pairs at/above the
       threshold, including index zero, and initializes applied flags to 0. */
    for (unsigned i = 0; i < count; ++i) {
        const NpeMapTileChange *change = &world->tile_changes[i];
        if (world->story_progress < change->progress) continue;
        for (unsigned j = 0; j < change->count; ++j)
            if(world_write_tile(world, change->indices[j], change->tiles[j]))return -1;
    }
    return 0;
}

int npe_world_update_tiles(NpeWorld *world) {
    int updated = 0;
    if (!world || world->tile_change_count > 3 || world->tile_override_count > NPE_TILE_OVERRIDES) return -1;
    if(world->battle_file && npe_rpg_runtime_owner(world->battle_file)==world &&
       npe_rpg_runtime_map_current(world->battle_file,world->map_number)){
        WorldTileObservation observation={world,0};
        updated=npe_rpg_runtime_map_update(world->battle_file,world_observe_tile,&observation);
        if(updated<0 || observation.error)return -1;
        world->tile_change_applied=0;
        for(unsigned i=0;i<world->tile_change_count;++i){
            uint8_t flag;if(npe_rpg_runtime_read(world->battle_file,UINT32_C(0x52728c)+64u*i,&flag,1))return -1;
            if(flag)world->tile_change_applied|=(uint8_t)(1u<<i);
        }
        return updated;
    }
    /* Source 42:328A..346A applies each nonzero threshold once. Its
       signed index <= 0 terminates the pair loop, unlike the loader. */
    for (unsigned i = 0; i < world->tile_change_count; ++i) {
        const NpeMapTileChange *change = &world->tile_changes[i];
        if (!change->progress || world->story_progress < change->progress ||
            (world->tile_change_applied & (1u << i))) continue;
        for (unsigned j = 0; j < change->count && change->indices[j] && change->indices[j]<0x8000u; ++j)
            if(world_write_tile(world, change->indices[j], change->tiles[j]))return -1;
        world->tile_change_applied |= (uint8_t)(1u << i);
        if(world->battle_file && npe_rpg_runtime_owner(world->battle_file)==world){
            uint8_t flag=1;
            if(npe_rpg_runtime_write(world->battle_file,UINT32_C(0x52728c)+64u*i,&flag,1))return -1;
        }
        ++updated;
    }
    return updated;
}

static void world_bind_tiles(const NpeWorld *world, NpeMap *map) {
    map->resident_file=world->battle_file && npe_rpg_runtime_owner(world->battle_file)==world?
        world->battle_file:NULL;
    if(world->terrain_snapshot_ready){
        map->snapshot_tiles=world->terrain_snapshot_cells;map->snapshot_count=world->terrain_snapshot_count;
        map->width=world->terrain_snapshot_width;map->height=world->terrain_snapshot_height;
    }
    if(map->resident_file && npe_rpg_runtime_map_current(map->resident_file,map->number)){
        uint8_t width,height;
        if(!npe_rpg_runtime_read(map->resident_file,UINT32_C(0x527a56),&width,1) &&
           !npe_rpg_runtime_read(map->resident_file,UINT32_C(0x527a5a),&height,1)){
            map->width=width;map->height=height;
        }
    }
    map->override_indices = world->tile_override_indices;
    map->override_tiles = world->tile_override_tiles;
    map->override_count = world->tile_override_count;
}

uint16_t npe_world_tile(const NpeWorld *world, const NpeMap *map,
                         unsigned x, unsigned y) {
    NpeMap view;
    if (!world || !map) return UINT16_MAX;
    if(world->battle_file && npe_rpg_runtime_owner(world->battle_file)==world &&
       npe_rpg_runtime_map_current(world->battle_file,map->number)){
        uint8_t width;if(npe_rpg_runtime_read(world->battle_file,UINT32_C(0x527a56),&width,1))return UINT16_MAX;
        uint16_t tile;int result=npe_rpg_runtime_map_tile(world->battle_file,map->number,
            (uint16_t)(y*width+x),&tile);
        if(result)return result<0?UINT16_MAX:tile;
    }
    view = *map; world_bind_tiles(world, &view);
    return npe_map_tile(&view, x, y);
}

static int world_load_story_events(NpeWorld *world, const NpeMap *map) {
    if (npe_world_load_random_enemy_records(world, map)) return -1;
    if (npe_world_load_battle_events(world, map)) return -1;
    if (npe_world_load_choice_events(world, map)) return -1;
    if (npe_world_load_scene_events(world, map)) return -1;
    if (npe_world_load_object_records(world, map)) return -1;
    if (npe_world_load_exit_records(world, map)) return -1;
    if (npe_world_load_npc_records(world, map)) return -1;
    if (npe_world_load_map_encounter_records(world, map)) return -1;
    if (npe_world_load_tiles(world, map)) return -1;
    if (npe_world_load_party_events(world, map)) return -1;
    if (npe_world_load_npc_move_events(world, map)) return -1;
    if (npe_world_load_script_walk_events(world, map)) return -1;
    if (npe_world_load_money_events(world, map)) return -1;
    if (npe_world_load_item_events(world, map)) return -1;
    if (npe_world_load_teleport_events(world, map)) return -1;
    if (npe_world_load_skill_events(world, map)) return -1;
    if (npe_world_load_party_adjust_event(world, map)) return -1;
    world->battle_chase_slot = 0;
    if(npe_world_load_walk_triggers(world,map))return -1;
    if(npe_world_load_story_event_cache(world,map))return -1;
    if(world->map_number==map->number && world->battle_file &&
       npe_rpg_runtime_owner(world->battle_file)==world)
        return npe_rpg_runtime_map_load(world->battle_file,world,0);
    return 0;
}

int npe_world_npc_position(const NpeWorld *world, const uint8_t *npc,
                             unsigned slot, unsigned *x, unsigned *y) {
    if (!world || !npc || !x || !y) return -1;
    *x = slot < 20 && world->npc_override_x[slot] != NPE_NPC_POSITION_NONE ?
         world->npc_override_x[slot] : npc[2];
    *y = slot < 20 && world->npc_override_y[slot] != NPE_NPC_POSITION_NONE ?
         world->npc_override_y[slot] : npc[3];
    return 0;
}

int npe_world_healing_effect(NpeWorld *world) {
    if(!world || world->game_mode!=22)return -1;
    /*4C:D4C4..D524 sign-extends the complete16-bit event word,
      compares signed money, then subtracts with32-bit wrap. */
    int32_t fee=(int16_t)world->story_event_id;
    if((int32_t)world->money<fee)return 0;
    world->money-=(uint32_t)fee;
    /*D528..D58C restores all three map records, including inactive ones. */
    for(unsigned character=0;character<3;++character){
        world->hp[character]=world->max_hp[character];
        world->qi[character]=world->max_qi[character];
    }
    return 1;
}

int npe_world_finish_healing_event(NpeWorld *world) {
    if(!world || world->game_mode!=22)return -1;
    /*4C:D5BA..D5CE runs after either notice, retaining the fee word. */
    world->game_mode=1;
    world->map_init_pending=1;
    return 1;
}

int npe_world_apply_npc_state_event(NpeWorld *world) {
    if(!world || world->game_mode!=24)return -1;
    /*4D:3898 zero-extends the byte, subtracts and writes its low byte.
      38E6 zero-extends again: the signed negative clamp cannot fire.
      In particular0 becomes255; invalid complete event words skip it. */
    if(world->story_event_id>0 && world->story_event_id<250)
        --world->npc_state[world->story_event_id];
    world->game_mode=1;
    world->map_init_pending=1;
    return 1;
}

static void world_resolve_simple_mode(NpeWorld *world) {
    if (world->game_mode == 22) {
        /* The display caller must retain mode22 until its notice ends. */
        if(!world->defer_story_events){
            npe_world_healing_effect(world);
            npe_world_finish_healing_event(world);
        }
    } else if (world->game_mode == 23)
        npe_world_apply_experience_event(world);
    else if (world->game_mode == 24)
        npe_world_apply_npc_state_event(world);
    if(world->game_mode==1) {
        world->event_blank_background=0;
        if (!world->map_init_pending && !world->sound_section) npe_world_sound_map_resume(world);
    }
}

int npe_actor_bitmap(const NpeFile *file, unsigned actor, unsigned direction,
                     unsigned frame, NpeIndexedBitmap *bitmap) {
    unsigned start, index;
    if (!file || !bitmap || actor > 254 || direction > 255 || frame > 2) return -1;
    start = (unsigned)file->vectors[3] + 1;
    /*46:BE02..BE72 uses the complete direction byte in its index. A
      direction4 from the original second character can cross actor rows. */
    index = start + actor * 12 + direction * 3 + frame;
    if (npe_indexed_bitmap(file, index, bitmap)) return -1;
    return 0;
}

int npe_npc_bitmap(const NpeFile *file, unsigned graphic, unsigned direction,
                   NpeIndexedBitmap *bitmap) {
    unsigned start, end, index;
    /* NPC records carry a byte image offset. Values such as 4 are used
       by static figures; native 4B:25BE..25C2 adds it without masking. */
    if (!file || !bitmap || direction > 255) return -1;
    start = (unsigned)file->vectors[1] + 1;
    end = file->vectors[2];
    if (start > end || graphic >= (end - start + 1) / 4) return -1;
    index = start + graphic * 4 + direction;
    if (index > end || npe_indexed_bitmap(file, index, bitmap) ||
        bitmap->width != 16 || bitmap->height != 32) return -1;
    return 0;
}

static int world_init(const NpeFile *file, NpeWorld *world,
                      unsigned map_number, unsigned x, unsigned y,
                      int choose_map_music) {
    NpeMap map;
    NpeViewport view;
    unsigned character;
    if (!world || !map_number || npe_map(file, map_number - 1, &map) ||
        npe_map_spawn_viewport(&map, x, y, &view)) return -1;
    /* Cold construction must not read caller storage, including runtime
       fields added after the original individual initializers. The native
       constructor sets progress before the map loader applies tile pairs. */
    memset(world, 0, sizeof(*world));
    world->view = view;
    world->story_progress = 1;
    memset(world->npc_state, 0, sizeof(world->npc_state));
    world->lcd_background = 2;
    world->property_page=world->property_member=world->equipment_view_slot=0;
    world->menu_party_count=0;
    memset(world->party_events, 0, sizeof(world->party_events));
    memset(world->npc_override_x, 255, sizeof(world->npc_override_x));
    memset(world->npc_override_y, 255, sizeof(world->npc_override_y));
    memset(world->npc_override_direction, 255,
           sizeof(world->npc_override_direction));
    /* New-game source 46:D138..D152 clears 0x1FE bytes at 52:67C8. */
    memset(world->condition_words, 0, sizeof(world->condition_words));
    memset(world->equipped_items, 0, sizeof(world->equipped_items));
    world->party_ids[0] = 1; world->party_ids[1] = 2;
    world->party_ids[2] = 3;
    world->party_active[0] = 1; world->party_active[1] = 0;
    world->party_active[2] = 0;
    memset(world->skill_slots, 0, sizeof(world->skill_slots));
    memset(world->learned_skill_count, 0,
           sizeof(world->learned_skill_count));
    memset(world->hp, 0, sizeof(world->hp));
    memset(world->max_hp, 0, sizeof(world->max_hp));
    memset(world->qi, 0, sizeof(world->qi));
    memset(world->max_qi, 0, sizeof(world->max_qi));
    memset(world->companion_directions, 0, sizeof(world->companion_directions));
    memset(world->party_attack_groups, 0, sizeof(world->party_attack_groups));
    memset(world->party_characters,0,sizeof(world->party_characters));
    memset(world->battle_characters,0,sizeof(world->battle_characters));
    memset(world->party_names,0,sizeof(world->party_names));
    memset(world->battle_names,0,sizeof(world->battle_names));
    memset(world->attack, 0, sizeof(world->attack));
    memset(world->defense, 0, sizeof(world->defense));
    world->battle_file = file;
    world->battle_pending_mask = world->battle_executing = 0;
    world->battle_last_order_count = 0;
    world->battle_variance_roll = 0;
    world->battle_attribute_roll = 0;
    memset(world->battle_party_status, 0, sizeof(world->battle_party_status));
    memset(world->battle_enemy_max_hps, 0, sizeof(world->battle_enemy_max_hps));
    memset(world->battle_enemy_spell_stats, 0, sizeof(world->battle_enemy_spell_stats));
    memset(world->battle_enemy_lucks, 0, sizeof(world->battle_enemy_lucks));
    memset(world->battle_enemy_skill_ids, 0, sizeof(world->battle_enemy_skill_ids));
    memset(world->battle_enemy_last_skill, 0, sizeof(world->battle_enemy_last_skill));
    memset(world->battle_enemy_last_target, 0, sizeof(world->battle_enemy_last_target));
    memset(world->battle_commands, 0, sizeof(world->battle_commands));
    memset(world->battle_item_reservations, 0, sizeof(world->battle_item_reservations));
    memset(world->experience, 0, sizeof(world->experience));
    memset(world->experience_to_next, 0,
           sizeof(world->experience_to_next));
    memset(world->level, 0, sizeof(world->level));
    memset(world->growth, 0, sizeof(world->growth));
    memset(world->other_stats, 0, sizeof(world->other_stats));
    memset(world->damage_floor_stat, 0, sizeof(world->damage_floor_stat));
    memset(world->status_guard, 0, sizeof(world->status_guard));
    memset(world->story_events, 0, sizeof(world->story_events));
    memset(world->story_event_padding, 0, sizeof(world->story_event_padding));
    world->story_event_count = 0;
    if (world_load_story_events(world, &map)) return -1;
    if (file->section_count > 12 && file->offset_table_records[12] >= 3) {
        for (character = 0; character < 3; ++character) {
            NpeCharacter record;
            if (npe_character(file, character, &record)) return -1;
            if(npe_character_record_seed(file,character,world->party_record_residue[character]))return -1;
            memcpy(world->party_names[character],record.name,12);
            if(npe_character_metadata(file,character,&world->party_characters[character]))return -1;
            memcpy(world->equipped_items[character], record.equipped_items, 6);
            /* Source 2E:7CB2..8258 writes 18 consecutive file bytes
               beginning at +58 into runtime 52:69DF..69F0. The runtime
               offset +13 is not the file offset: preceding fields are
               rearranged during character loading. */
            memcpy(world->skill_slots[character], record.raw + 58, 18);
            world->hp[character] = record.hp;
            world->max_hp[character] = record.max_hp;
            world->qi[character] = record.qi;
            world->max_qi[character] = record.max_qi;
            world->attack[character] = record.attack;
            world->defense[character] = record.defense;
            world->experience[character] = record.experience;
            world->experience_to_next[character] = record.experience_to_next;
            world->level[character] = record.level;
            memcpy(world->growth[character], record.growth, 7);
            memcpy(world->other_stats[character], record.other_stats,
                   sizeof(record.other_stats));
            world->damage_floor_stat[character] = record.damage_floor_stat;
            world->party_attack_groups[character] = record.raw[79];
            if(character)world->companion_directions[character-1]=record.raw[78];
        }
    }
    /* New-game source 46:D18C..D1A6 clears 0xC8 bytes at 52:7186. */
    memset(world->condition5_bytes, 0, sizeof(world->condition5_bytes));
    /* New-game source 46:D120..D134 starts on map 1 at progress 1. The
       caller may choose a different map/position for diagnostics. */
    world->story_progress = 1;
    world->story_event_id = 0;
    world->event_context_id = 0;
    world->game_mode = 1;
    world->npc_guard_index = 0;
    world->exit_choice = world->exit_slot = 0;
    world->npc_guard_word_index = 0;
    world->npc_guard_a = world->npc_guard_b = 0;
    /* New-game source 46:D0E8..D0F8 initializes 52:799E to 100. */
    world->money = 100;
    /*46:D154..D16E clears only100 object flags, before map loading.
      The already loaded7490 trigger table shares the following bytes. */
    memset(world->object_done, 0, 100);
    world->random_state = 0x9e3779b9u;
    world->music_enabled = 1;
    world->map_music_slot = world->sound_section = world->sound_loop = 0;
    world->sound_observer = NULL;
    world->sound_range_observer = NULL;
    world->sound_context = NULL;
    world->encounter_steps = 0;
    world->quiz_previous_index = 0;
    world->quiz_question_index = 0;
    world->quiz_category = 0;
    world->quiz_ready = 0;
    memset(world->quiz_category_correct, 0,
           sizeof(world->quiz_category_correct));
    memset(world->quiz_category_wrong, 0,
           sizeof(world->quiz_category_wrong));
    world->quiz_answer_count = 0;
    world->quiz_wrong_count = 0;
    world->battle_enemy_count = 0;
    memset(world->battle_enemy_ids, 0, sizeof(world->battle_enemy_ids));
    memset(world->battle_enemy_hps, 0, sizeof(world->battle_enemy_hps));
    memset(world->battle_enemy_status, 0,
           sizeof(world->battle_enemy_status));
    memset(world->battle_enemy_attacks, 0,
           sizeof(world->battle_enemy_attacks));
    memset(world->battle_enemy_defenses, 0,
           sizeof(world->battle_enemy_defenses));
    memset(world->battle_enemy_gold_rewards, 0,
           sizeof(world->battle_enemy_gold_rewards));
    memset(world->battle_enemy_experience_rewards, 0,
           sizeof(world->battle_enemy_experience_rewards));
    memset(world->battle_enemy_drop_ids, 0,
           sizeof(world->battle_enemy_drop_ids));
    memset(world->battle_enemy_damage_floor_stats, 0,
           sizeof(world->battle_enemy_damage_floor_stats));
    memset(world->battle_enemy_speeds, 0, sizeof(world->battle_enemy_speeds));
    memset(world->battle_party_defenses, 0,
           sizeof(world->battle_party_defenses));
    memset(world->battle_defense_turns, 0,
           sizeof(world->battle_defense_turns));
    memset(world->battle_skill_attack_bonus, 0,
           sizeof(world->battle_skill_attack_bonus));
    memset(world->battle_skill_defense_bonus, 0,
           sizeof(world->battle_skill_defense_bonus));
    memset(world->battle_attack_turns, 0,
           sizeof(world->battle_attack_turns));
    world->battle_enemy_id = 0;
    world->battle_enemy_hp = 0;
    world->battle_target = 0;
    world->battle_actor = 0;
    world->battle_animation_observer = NULL;
    world->battle_animation_context = NULL;
    world->map_margin_color = 2;
    memset(world->battle_enemy_damage_values, 0, sizeof(world->battle_enemy_damage_values));
    world->battle_display_epoch = 0;
    world->battle_lost = 0;
    world->battle_win_mode = world->battle_lose_mode = 0;
    world->battle_win_event_id = world->battle_lose_event_id = 0;
    world->map_number = map_number;
    world->x = x;
    world->y = y;
    world->direction = 0;
    world->mode2_ready = 0;
    world->mode2_context = 0;
    world->mode2_dialogue_offset = 0;
    world->mode2_followup_mode = 0;
    world->mode2_followup_event_id = 0;
    if(npe_rpg_runtime_owner(file)==world && npe_rpg_runtime_map_load(file,world,0))return -1;
    if (choose_map_music) npe_world_sound_map_loaded(world);
    return 0;
}

static int world_initial_item_cache(const NpeFile *file,NpeWorld *world) {
    /*2E:88C8 calls39:188E after each character record. The six
      item loads follow69D8,69DB,69D9,69DA,69DC,69DD. */
    static const uint8_t slots[]={0,3,1,2,4,5};
    if(file->section_count>12&&file->offset_table_records[12]>=3)
        for(unsigned member=0;member<3;++member)for(unsigned i=0;i<6;++i){
            NpeItem item;
            if(npe_world_item(file,world,world->equipped_items[member][slots[i]],&item))return -1;
        }
    return 0;
}

int npe_world_init(const NpeFile *file, NpeWorld *world,
                   unsigned map_number, unsigned x, unsigned y) {
    return world_init(file, world, map_number, x, y, 1);
}

static int world_new_game(const NpeFile *file, NpeWorld *world) {
    NpeCharacter first;
    NpeMap map;
    unsigned left, top, x, y;
    if (!file || !world || npe_character(file, 0, &first) ||
        first.size < 82 || first.raw[78] > 3 || npe_map(file, 0, &map)) return -1;
    /* Source 2E:8348..83B8 loads these record bytes to 52:6A42/6A43;
       new-game source 46:D28A..D2AA copies them to scroll X/Y.
       Source 46:D2DC..D2F0 sets screen X/Y to 7/1. */
    left = first.raw[80]; top = first.raw[81];
    x = left + 7; y = top + 1;
    if (x >= map.width || y >= map.height ||
        world_init(file, world, 1, x, y, 0)) return -1;
    world->view.offset_x = left;
    world->view.offset_y = top;
    world->view.screen_x = 7;
    world->view.screen_y = 1;
    /* Source 2E:827E..82C8 reads file bytes 77/78 together into
       runtime 69D3/69D4 (frame/direction). Use the current NPE record;
       title new-game and empty-slot construction share this loader. */
    world->direction = first.raw[78];
    return 0;
}

int npe_world_item(const NpeFile *file,NpeWorld *world,unsigned item_id,NpeItem *item) {
    /*3F:C382 consumes the low byte; ID0 always clears142 bytes.
      C3CA..C3DE returns immediately for the already loaded ID. */
    static const uint8_t words[]={114,116,118,120,122,124,126,134,130,132,136};
    if(!world||!item)return -1;
    item_id&=255u;
    if(!item_id){world->item_record_id=0;memset(world->item_record_bytes,0,142);}
    else if(item_id!=world->item_record_id){
        NpeItem source;
        if(npe_native_item(file,item_id,&source))return -1;
        uint8_t *cache=world->item_record_bytes;
        memcpy(cache,source.raw,12);cache[12]=0;cache[138]=source.raw[12];
        for(unsigned i=0;i<11;++i)memcpy(cache+words[i],source.raw+13+i*2,2);
        memcpy(cache+139,source.raw+35,3);memcpy(cache+128,source.raw+38,2);
        /*C7C4..C842 reads offsets0..100 inclusive, stopping at NUL.
          Bytes after that terminator survive loading another record. */
        size_t n=source.description_size;
        if(source.description+source.description_size<file->bytes+file->size)++n;
        if(n>101)n=101;
        memcpy(cache+13,source.description,n);
        world->item_record_id=(uint8_t)item_id;
    }
    return npe_world_item_view(world,world->item_record_raw,item);
}

int npe_world_item_view(const NpeWorld *world,uint8_t raw[41],NpeItem *item) {
    static const uint8_t words[]={114,116,118,120,122,124,126,134,130,132,136};
    if(!world || !raw || !item)return -1;
    const uint8_t *cache=world->item_record_bytes;
    memcpy(raw,cache,12);raw[12]=cache[138];
    for(unsigned i=0;i<11;++i)memcpy(raw+13+i*2,cache+words[i],2);
    memcpy(raw+35,cache+139,3);memcpy(raw+38,cache+128,2);raw[40]=0;
    if(npe_item_record(raw,41,item))return -1;
    const uint8_t *end=memchr(cache,0,12);item->name=cache;item->name_size=end?(size_t)(end-cache):12;
    item->description=cache+13;end=memchr(cache+13,0,129);
    item->description_size=end?(size_t)(end-cache-13):129;
    return 0;
}

int npe_world_battle_item_view(const NpeWorld *world,unsigned actor,
                                 uint8_t raw[41],NpeItem *item) {
    if(!world || actor>=3 || world->item_record_id!=world->battle_item_ids[actor] ||
       npe_world_item_view(world,raw,item))return -1;
    if(world->battle_commands[actor].kind==COMMAND_ITEM ||
       world->battle_commands[actor].kind==COMMAND_THROW){
        /*38:4930/4A38/4A8C saves64D1/64D4/64D3 before target selection.
          2D:4580..4A36 dispatches these fields after22:C370 reloads words. */
        raw[12]=world->battle_item_types[actor];
        raw[36]=world->battle_item_groups[actor];raw[37]=world->battle_item_sides[actor];
        item->effect_type=raw[12];item->group=raw[36];item->target_side=raw[37];
        if(world->battle_item_converted_mask&(1u<<actor)){
            /*2D:462A..4644 moves ATK to prepared HP, leaving the shared
              142-byte resource intact. Readers see the prepared words. */
            item->hp_restore=(uint16_t)item->attack_bonus;item->attack_bonus=0;
        }
    }
    return 0;
}

int npe_world_skill(const NpeFile *file,NpeWorld *world,unsigned id,NpeSkill *skill) {
    static const uint8_t words[]={120,122,124,126,130,132,134,140,136,138,142};
    if(!world || !skill)return -1;
    id&=255u;
    if(!id){world->skill_record_id=0;memset(world->skill_record_bytes,0,144);}
    else if(id!=world->skill_record_id){
        NpeSkill source;if(npe_native_skill(file,id,&source))return -1;
        uint8_t *cache=world->skill_record_bytes;
        memcpy(cache,source.raw,12);cache[12]=0;memcpy(cache+114,source.raw+12,3);
        for(unsigned i=0;i<11;++i)memcpy(cache+words[i],source.raw+15+i*2,2);
        memcpy(cache+117,source.raw+37,2);memcpy(cache+128,source.raw+39,2);
        size_t n=source.description_size;
        if(source.description+n<file->bytes+file->size)++n;
        if(n>101)n=101;
        memcpy(cache+13,source.description,n);world->skill_record_id=(uint8_t)id;
        /*3F:23C4 leaves7385 (offset119) and bytes past the terminator. */
    }
    return npe_world_skill_view(world,world->skill_record_raw,skill);
}

int npe_world_skill_view(const NpeWorld *world,uint8_t raw[42],NpeSkill *skill) {
    static const uint8_t words[]={120,122,124,126,130,132,134,140,136,138,142};
    if(!world || !raw || !skill)return -1;
    const uint8_t *cache=world->skill_record_bytes;
    memcpy(raw,cache,12);memcpy(raw+12,cache+114,3);
    for(unsigned i=0;i<11;++i)memcpy(raw+15+i*2,cache+words[i],2);
    memcpy(raw+37,cache+117,2);memcpy(raw+39,cache+128,2);raw[41]=0;
    if(npe_skill_record(raw,42,skill))return -1;
    const uint8_t *end=memchr(cache,0,12);skill->name=cache;skill->name_size=end?(size_t)(end-cache):12;
    skill->description=cache+13;end=memchr(cache+13,0,131);
    skill->description_size=end?(size_t)(end-cache-13):131;
    return 0;
}

int npe_world_battle_magic_view(const NpeWorld *world,unsigned actor,
                                  uint8_t raw[42],NpeSkill *skill) {
    if(!world || actor>=3 || world->skill_record_id!=world->battle_skill_ids[actor] ||
       npe_world_skill_view(world,raw,skill))return -1;
    /* Source2D:3D64/3E6A/3DB6 dispatches the queued64D0/64D3/64D4,
       while scalar/status words and the fee come from the shared record. */
    if(world->battle_commands[actor].kind==COMMAND_MAGIC){
        raw[14]=world->battle_magic_types[actor];
        raw[37]=world->battle_magic_groups[actor];raw[38]=world->battle_magic_sides[actor];
    }
    return 0;
}

int npe_world_new_game(const NpeFile *file, NpeWorld *world) {
    if (world_new_game(file, world) || world_initial_item_cache(file,world)) return -1;
    npe_world_sound_map_resume(world);
    return 0;
}

int npe_world_restart_game(const NpeFile *file, NpeWorld *world) {
    NpeWorld restarted;
    NpeMap map;
    uint8_t battle_records[6][148];
    if (!world || npe_world_resident_records_read(world,UINT32_C(0x526450),
            &battle_records[0][0],sizeof(battle_records)) ||
        world_new_game(file, &restarted) || npe_map(file, 0, &map)) return -1;
    /*46:D0BA never clears6450..67C7. Freeze the physical image, including
      an active battle view, before rebuilding the map party. */
    memcpy(restarted.battle_record_residue,battle_records,sizeof(battle_records));
    restarted.battle_record_party_count=world->battle_record_party_count;
    /*2E:7378..872C reloads scattered character fields, leaving record gaps
      and companions' screen coordinates resident across warm construction. */
    if(file->section_count>12 && file->offset_table_records[12]>=3)
        for(unsigned m=0;m<3;++m){
            uint8_t *r=restarted.party_record_residue[m];
            if(npe_world_map_record_image(world,m,r) || npe_character_record_seed(file,m,r))return -1;
            /* The character loader clears four status words and two turn
               words; these are not part of its file-backed scattered reads. */
            memset(r+62,0,12);
        }
    /* Original46:D0BA clears RPG data and loads current-NPE characters,
       but leaves these resident UI words, RNG, music and graphics state.
       Host activity flags are rebuilt by the next service entry. */
    memcpy(restarted.battle_selected_ids,world->battle_selected_ids,sizeof(restarted.battle_selected_ids));
    restarted.battle_formation_count=world->battle_formation_count;
    restarted.battle_scripted_entry=world->battle_scripted_entry;
    restarted.battle_menu_row=world->battle_menu_row;
    restarted.battle_item_row=world->battle_item_row;
    restarted.battle_actor=world->battle_actor;
    restarted.battle_menu_previous_kind=world->battle_menu_previous_kind;
    restarted.battle_menu_phase=world->battle_menu_phase;
    restarted.battle_status_member=world->battle_status_member;
    restarted.battle_resource_flags=world->battle_resource_flags;
    restarted.battle_display_target=world->battle_display_target;
    restarted.battle_selection_item_type=world->battle_selection_item_type;
    restarted.battle_selection_magic_type=world->battle_selection_magic_type;
    restarted.battle_selection_type_copy=world->battle_selection_type_copy;
    restarted.battle_action_ready=world->battle_action_ready;
    restarted.battle_execution_slot=world->battle_execution_slot;
    restarted.story_event_id = world->story_event_id;
    restarted.event_context_id = world->event_context_id;
    restarted.mode2_context = world->mode2_context;
    restarted.mode2_followup_mode = world->mode2_followup_mode;
    restarted.mode2_followup_event_id = world->mode2_followup_event_id;
    /* Original warm constructors reset NPC/condition arrays, but retain
       paired-condition indices7A52/7AB8 and flags7A94/7A92. */
    restarted.npc_guard_index = world->npc_guard_index;
    restarted.npc_guard_word_index = world->npc_guard_word_index;
    restarted.npc_guard_a = world->npc_guard_a;
    restarted.npc_guard_b = world->npc_guard_b;
    restarted.text_file_position=world->text_file_position;
    memcpy(restarted.text_rect,world->text_rect,sizeof(restarted.text_rect));
    restarted.text_offset=world->text_offset;
    restarted.text_more=world->text_more;restarted.text_style=world->text_style;
    restarted.property_page = world->property_page;
    restarted.property_member = world->property_member;
    restarted.equipment_view_slot = world->equipment_view_slot;
    restarted.menu_party_count = world->menu_party_count;
    restarted.equipment_description_item = world->equipment_description_item;
    /*46:D0FC..D104 resets7A72. Character loading then calls39:188E
      for each of the three records, including inactive companions. */
    restarted.item_record_id=0;
    /*46:D110 resets only7A74; character loads do not load skills. */
    restarted.skill_record_id=0;
    memcpy(restarted.skill_record_bytes,world->skill_record_bytes,144);
    memcpy(restarted.skill_record_raw,world->skill_record_raw,42);
    memcpy(restarted.item_record_bytes,world->item_record_bytes,142);
    memcpy(restarted.item_record_raw,world->item_record_raw,41);
    if(world_initial_item_cache(file,&restarted))return -1;
    restarted.item_description_length = world->item_description_length;
    restarted.equipment_description_revision = world->equipment_description_revision;
    restarted.shop_ui = world->shop_ui;
    restarted.shop_kind = world->shop_kind;
    restarted.shop_transaction_sell = world->shop_transaction_sell;
    restarted.shop_scan_first = world->shop_scan_first;
    restarted.shop_scan_last = world->shop_scan_last;
    restarted.shop_inventory_only = world->shop_inventory_only;
    memcpy(restarted.shop_catalog_present,world->shop_catalog_present,255);
    restarted.shop_catalog_ready = world->shop_catalog_ready;
    restarted.shop_quantity = world->shop_quantity;
    restarted.shop_choice = world->shop_choice;
    restarted.shop_row = world->shop_row;
    restarted.inventory_column = world->inventory_column;
    restarted.inventory_prefix_word = world->inventory_prefix_word;
    restarted.shop_first_item = world->shop_first_item;
    restarted.shop_last_item = world->shop_last_item;
    restarted.shop_selected_item = world->shop_selected_item;
    restarted.shop_description_start = world->shop_description_start;
    restarted.shop_description_end = world->shop_description_end;
    restarted.exit_choice = world->exit_choice;
    restarted.exit_slot = world->exit_slot;
    memcpy(restarted.exit_records,world->exit_records,sizeof(restarted.exit_records));
    if(npe_world_load_exit_records(&restarted,&map))return -1;
    restarted.lcd_background = world->lcd_background;
    /*46:D1A8 clears all22 statistics words, but session/input words are
      outside that range and survive both warm title constructors. */
    restarted.quiz_answer_count = world->quiz_answer_count;
    restarted.quiz_wrong_count = world->quiz_wrong_count;
    restarted.quiz_previous_index = world->quiz_previous_index;
    restarted.quiz_category = world->quiz_category;
    restarted.quiz_question_index = world->quiz_question_index;
    restarted.quiz_input_type = world->quiz_input_type;
    restarted.quiz_input_length = world->quiz_input_length;
    memcpy(restarted.quiz_input, world->quiz_input, 32);
    restarted.quiz_input[32] = 0;
    memcpy(restarted.party_events, world->party_events, sizeof(restarted.party_events));
    memcpy(restarted.party_event_padding, world->party_event_padding, sizeof(restarted.party_event_padding));
    if (npe_world_load_party_events(&restarted, &map)) return -1;
    memcpy(restarted.npc_move_events, world->npc_move_events, sizeof(restarted.npc_move_events));
    memcpy(restarted.npc_move_padding, world->npc_move_padding, sizeof(restarted.npc_move_padding));
    memcpy(restarted.script_walk_padding, world->script_walk_padding, sizeof(restarted.script_walk_padding));
    memcpy(restarted.npc_records, world->npc_records, sizeof(restarted.npc_records));
    if (npe_world_load_npc_records(&restarted, &map)) return -1;
    memcpy(restarted.monster_records,world->monster_records,sizeof(restarted.monster_records));
    memcpy(restarted.chase_records,world->chase_records,sizeof(restarted.chase_records));
    memcpy(restarted.chase_x,world->chase_x,sizeof(restarted.chase_x));
    memcpy(restarted.chase_y,world->chase_y,sizeof(restarted.chase_y));
    memcpy(restarted.chase_cooldown,world->chase_cooldown,sizeof(restarted.chase_cooldown));
    if (npe_world_load_map_encounter_records(&restarted, &map)) return -1;
    memcpy(restarted.script_walk_events, world->script_walk_events, sizeof(restarted.script_walk_events));
    memcpy(restarted.object_done+100,world->object_done+100,100);
    if(npe_world_load_walk_triggers(&restarted,&map))return -1;
    memcpy(restarted.story_events, world->story_events, sizeof(restarted.story_events));
    memcpy(restarted.story_event_padding, world->story_event_padding, sizeof(restarted.story_event_padding));
    memcpy(restarted.money_events, world->money_events, sizeof(restarted.money_events));
    memcpy(restarted.money_event_padding, world->money_event_padding, sizeof(restarted.money_event_padding));
    memcpy(restarted.item_events, world->item_events, sizeof(restarted.item_events));
    memcpy(restarted.teleport_events, world->teleport_events, sizeof(restarted.teleport_events));
    memcpy(restarted.teleport_padding, world->teleport_padding, sizeof(restarted.teleport_padding));
    memcpy(restarted.skill_events, world->skill_events, sizeof(restarted.skill_events));
    memcpy(restarted.skill_event_padding,world->skill_event_padding,sizeof(restarted.skill_event_padding));
    memcpy(restarted.scene_event_records,world->scene_event_records,sizeof(restarted.scene_event_records));
    memcpy(restarted.choice_event_records,world->choice_event_records,sizeof(restarted.choice_event_records));
    memcpy(restarted.battle_event_records,world->battle_event_records,sizeof(restarted.battle_event_records));
    restarted.party_adjust_event=world->party_adjust_event;
    if (npe_world_load_npc_move_events(&restarted, &map)) return -1;
    if (npe_world_load_script_walk_events(&restarted, &map)) return -1;
    if (npe_world_load_story_event_cache(&restarted, &map)) return -1;
    if (npe_world_load_money_events(&restarted, &map)) return -1;
    if (npe_world_load_item_events(&restarted, &map)) return -1;
    if (npe_world_load_teleport_events(&restarted, &map)) return -1;
    if (npe_world_load_skill_events(&restarted, &map)) return -1;
    if (npe_world_load_scene_events(&restarted, &map)) return -1;
    if (npe_world_load_choice_events(&restarted, &map)) return -1;
    if (npe_world_load_battle_events(&restarted, &map)) return -1;
    if (npe_world_load_party_adjust_event(&restarted, &map)) return -1;
    restarted.random_state = world->random_state;
    restarted.music_enabled = world->music_enabled;
    restarted.system_menu_stage=world->system_menu_stage;
    restarted.music_choice=world->music_choice;
    restarted.map_music_slot = world->map_music_slot;
    restarted.sound_section = world->sound_section;
    restarted.sound_loop = world->sound_loop;
    restarted.sound_observer = world->sound_observer;
    restarted.sound_range_observer = world->sound_range_observer;
    restarted.sound_context = world->sound_context;
    restarted.battle_animation_observer = world->battle_animation_observer;
    restarted.battle_animation_context = world->battle_animation_context;
    memcpy(restarted.tile_record_bytes,world->tile_record_bytes,192);
    if(npe_world_load_tile_records(&restarted,&map))return -1;
    *world = restarted;
    if(npe_rpg_runtime_owner(file)==world && npe_rpg_runtime_map_load(file,world,0))return -1;
    /* Title new/empty constructors stop and resume the retained map slot
       only when music is enabled. Neither route consumes a random draw. */
    if (world->music_enabled) {
        npe_world_sound_stop(world);
        npe_world_sound_map_resume(world);
    }
    return 0;
}

void npe_world_seed(NpeWorld *world, uint32_t seed) {
    if (world) world->random_state = seed;
}

static int world_divide_signed_word(uint16_t dividend, uint16_t divisor_word) {
    int numerator = (int16_t)dividend;
    int divisor = (int16_t)divisor_word;
    if (divisor) {
        int quotient = numerator / divisor;
        if (quotient >= -32768 && quotient <= 32767) return quotient;
    }
    /* DMCU DIV preserves its dividend when V is set. */
    return numerator;
}

int npe_battle_attribute_multiplier(unsigned actor, unsigned target, unsigned roll) {
    /* Source2D:37A4..37C2 /33:6F16..6F34. Every ADD/MUL keeps its
       low word. DIV is signed16; zero and quotient overflow leave R7
       unchanged. The second divisor80 is safe after the low-word MUL. */
    int numerator = world_divide_signed_word((uint16_t)actor, (uint16_t)(actor + target));
    int product = (int16_t)(uint16_t)(numerator * (int)(roll % 100u));
    return product / 80 + 1;
}

static unsigned world_party_normal_damage(unsigned attack, unsigned defense,
                                           unsigned floor_stat, unsigned variance_roll,
                                           int multiplier) {
    /* Source2D:3944..3952 keeps the low word of the attack/defense
       difference (and multiplier product), then compares it as signed. */
    int damage = (int16_t)(uint16_t)((uint16_t)(attack - defense) *
                                    (unsigned)(uint16_t)multiplier);
    int floor = (int)(floor_stat / 5u) + 4;
    /* PRG 2D:39BC..3A54: clamp before adding the signed -3..+2 roll.
       Its preceding64AA-stat multiplier has already been evaluated. */
    if (damage < floor) damage = floor;
    if (damage > 996) damage = 996;
    damage += (int)(variance_roll % 6u) - 3;
    return damage > 0 ? (unsigned)damage : 0;
}

unsigned npe_battle_normal_damage(unsigned attack, unsigned defense,
                                  unsigned floor_stat, unsigned variance_roll) {
    return world_party_normal_damage(attack, defense, floor_stat, variance_roll, 1);
}

static uint16_t world_random15(NpeWorld *world) {
    /* Resident 03:4124 -> 33:6566 -> 31:BB5A. The source body at
       4E:BB80..BBEA multiplies the 32-bit state by 0x514F111F, adds 1,
       stores it, then returns its high word masked to 15 bits. */
    world->random_state = world->random_state * UINT32_C(0x514f111f) + 1u;
    return (uint16_t)((world->random_state >> 16) & 0x7fffu);
}

static void world_sound_request(NpeWorld *world, unsigned section, int loop) {
    world->sound_section = (uint8_t)section;
    world->sound_loop = (uint8_t)(loop != 0);
    if (world->sound_observer)
        world->sound_observer(world->sound_context, section, loop);
}

void npe_world_sound_bind(NpeWorld *world, NpeSoundObserver observer, void *context) {
    if (!world) return;
    world->sound_observer = observer;
    world->sound_range_observer = NULL;
    world->sound_context = context;
    if (observer) observer(context, world->sound_section, world->sound_loop);
}

int npe_world_battle_sound(NpeWorld *world){
    if(!world || !world->battle_file)return -1;
    const NpeFile *file=world->battle_file;
    NpeBattleSoundRequest request;int result;
    if(file->rpg_runtime){
        if(npe_rpg_runtime_owner(file)!=world)return -2;
        result=npe_rpg_runtime_battle_sound(file,&request);
    }else{
        /* Numeric inspection without persistent RAM uses the same selector. */
        WorldEnemyIo *io=calloc(1,sizeof(*io));if(!io)return -2;
        io->file=file;io->world=world;
        const NpeResidentHeap h={io,world_enemy_word,world_enemy_word_write,NULL};
        world_enemy_byte_write(io,UINT32_C(0x527afa),(uint8_t)world_record_party_count(world));
        const unsigned sections[]={30,31,32,35};
        const uint32_t addresses[]={0x52794a,0x527972,0x52796a,0x527952};
        for(unsigned i=0;i<4;++i)if(file->section_count>sections[i]){
            uint32_t size=file->sections[sections[i]].size,offset=file->sections[sections[i]].offset;
            world_enemy_word_write(io,addresses[i],(uint16_t)(size>>16));
            world_enemy_word_write(io,addresses[i]+2,(uint16_t)size);
            world_enemy_word_write(io,addresses[i]+4,(uint16_t)(offset>>16));
            world_enemy_word_write(io,addresses[i]+6,(uint16_t)offset);
        }
        result=npe_battle_sound_request(&h,&request);if(io->error)result=-2;free(io);
    }
    if(result==1 && world->sound_range_observer)
        world->sound_range_observer(world->sound_context,request.offset,request.length,0);
    return result;
}

void npe_world_sound_stop(NpeWorld *world) {
    if (world) world_sound_request(world, 0, 0);
}

static int world_map_sound_request(NpeWorld *world){
    const NpeFile *file=world->battle_file;
    if(world->sound_range_observer && file){
        NpeMapSoundRequest request;int result;
        if(file->rpg_runtime){
            if(npe_rpg_runtime_owner(file)!=world)return -2;
            result=npe_rpg_runtime_map_sound(file,&request);
        }else{
            WorldEnemyIo *io=calloc(1,sizeof(*io));if(!io)return -2;io->file=file;io->world=world;
            const NpeResidentHeap h={io,world_enemy_word,world_enemy_word_write,NULL};
            for(unsigned i=0;i<3;++i)if(file->section_count>20+i){
                uint32_t size=file->sections[20+i].size,offset=file->sections[20+i].offset;
                world_enemy_word_write(io,0x5278f2+4u*i,(uint16_t)(size>>16));
                world_enemy_word_write(io,0x5278f4+4u*i,(uint16_t)size);
                world_enemy_word_write(io,0x5278feu+4u*i,(uint16_t)(offset>>16));
                world_enemy_word_write(io,0x527900+4u*i,(uint16_t)offset);
            }
            result=npe_map_sound_request(&h,&request);if(io->error)result=-2;free(io);
        }
        if(result!=1)return result;
        world->sound_section=(uint8_t)(20u+world->map_music_slot);world->sound_loop=1;
        int32_t length=request.length<=INT32_MAX?(int32_t)request.length:
            (int32_t)(request.length-UINT32_C(0x80000000))+INT32_MIN;
        world->sound_range_observer(world->sound_context,request.offset,length,request.callback);
    }else world_sound_request(world,20u+world->map_music_slot,1);
    return 1;
}
int npe_world_sound_map_replay(NpeWorld *world){
    return world?world_map_sound_request(world):-1;
}
void npe_world_sound_map_resume(NpeWorld *world) {
    /*37:1336/133A gates the controller call on the complete word==1. */
    if(world && world->music_enabled==1)world_map_sound_request(world);
}

void npe_world_sound_map_loaded(NpeWorld *world) {
    if (!world || !world->music_enabled) return;
    /* Source48:23C0..23D0: one shared random draw, remainder / 3. */
    world->map_music_slot = (uint8_t)(world_random15(world) % 3u);
    npe_world_sound_map_resume(world);
}

static int world_background_sound_request(NpeWorld *world,unsigned phase,unsigned value,unsigned section){
    const NpeFile *file=world->battle_file;NpeRpgSoundRequest request;int result=1;
    if(file && file->rpg_runtime && npe_rpg_runtime_owner(file)==world){
        result=npe_rpg_runtime_background_sound(file,phase,value,&request);
    }else if(file && world->sound_range_observer){
        if(file->rpg_runtime)return -2;
        WorldEnemyIo *io=calloc(1,sizeof(*io));if(!io)return -2;io->file=file;io->world=world;
        const NpeResidentHeap h={io,world_enemy_word,world_enemy_word_write,NULL};
        for(unsigned i=0;i<11;++i){
            unsigned s=i<4?16+i:23+i-4;
            uint32_t size_address=i<4?0x5278c4+4u*i:0x527856+4u*(i-4);
            uint32_t offset_address=i<4?0x5278d4+4u*i:0x527872+4u*(i-4);
            if(file->section_count<=s)continue;
            world_enemy_word_write(io,size_address,(uint16_t)(file->sections[s].size>>16));
            world_enemy_word_write(io,size_address+2,(uint16_t)file->sections[s].size);
            world_enemy_word_write(io,offset_address,(uint16_t)(file->sections[s].offset>>16));
            world_enemy_word_write(io,offset_address+2,(uint16_t)file->sections[s].offset);
        }
        if(phase==2 && section>=23 && section<=29 && section<file->section_count){
            uint32_t offset=file->sections[section].offset,size=file->sections[section].size;
            world_enemy_word_write(io,0x52799a,(uint16_t)(offset>>16));world_enemy_word_write(io,0x52799c,(uint16_t)offset);
            world_enemy_word_write(io,0x527996,(uint16_t)(size>>16));world_enemy_word_write(io,0x527998,(uint16_t)size);
        }
        if(!phase)result=npe_scene_sound_request(&h,value,&request);
        else if(phase==1)result=npe_battle_background_start(&h,value,&request);
        else result=npe_battle_background_replay(&h,&request);
        if(io->error)result=-2;
        free(io);
    }else{
        if((!phase && world->music_enabled!=1) || (phase && !world->music_enabled))return 0;
        world_sound_request(world,section,phase!=0);return 1;
    }
    if(result!=1)return result;
    world->sound_section=(uint8_t)section;world->sound_loop=(uint8_t)(request.callback!=0);
    if(world->sound_range_observer){
        int32_t length=request.length<=INT32_MAX?(int32_t)request.length:
            (int32_t)(request.length-UINT32_C(0x80000000))+INT32_MIN;
        world->sound_range_observer(world->sound_context,request.offset,length,request.callback);
    }else if(world->sound_observer)world->sound_observer(world->sound_context,section,request.callback!=0);
    return 1;
}
int npe_world_sound_battle_replay(NpeWorld *world){
    return world?world_background_sound_request(world,2,0,world->sound_section):-1;
}
void npe_world_sound_battle_start(NpeWorld *world) {
    if (!world || !world->music_enabled) return;
    /* Source4D:05B0..05C4: first UDIV leaves roll%60 in R6;
       second divides that word by10 and selects quotient R7, 0..5. */
    unsigned roll=world_random15(world);
    (void)world_background_sound_request(world,1,roll,23u+(roll%60u)/10u);
}

void npe_world_sound_scene_start(NpeWorld *world, unsigned group) {
    unsigned slot;
    if (!world || !group) return;
    /* Source41:AEFA..AF28: opening avoids RNG; all other valid scenes
       draw before the exact-one music gate, including while disabled.
       Source41:AF2C..AFAA passes zero continuation pointers: finite. */
    slot = group == 1 ? 0 : 1u + world_random15(world) % 3u;
    if (world->music_enabled == 1)(void)world_background_sound_request(world,0,slot,16u+slot);
}

static void world_music_control_request(void *context,unsigned event){
    NpeWorld *world=context;
    if(event==1)world_map_sound_request(world);
    else npe_world_sound_stop(world);
}
int npe_world_music_controller(NpeWorld *world,unsigned operation){
    if(!world || operation>3)return -1;
    const NpeFile *file=world->battle_file;
    if(file && file->rpg_runtime){
        if(npe_rpg_runtime_owner(file)!=world)return -2;
        return npe_rpg_runtime_music_controller(file,operation,world_music_control_request,world);
    }
    WorldEnemyIo *io=calloc(1,sizeof(*io));if(!io)return -2;
    io->world=world;io->file=file;
    const NpeResidentHeap heap={io,world_enemy_word,world_enemy_word_write,NULL};
    int result=npe_music_controller_apply(&heap,operation,world_music_control_request,world);
    if(io->error)result=-2;
    free(io);return result;
}

void npe_world_music_set(NpeWorld *world, int enabled) {
    if (!world) return;
    /* Source37:CFEC..D024: off stops; on requests the retained map slot.
       It does not pause an audio cursor and does not roll a new track. */
    if(enabled)world_map_sound_request(world);
    else npe_world_sound_stop(world);
    /*37:CFF8/D00C stores only after the play/stop service returns. */
    world->music_enabled=(uint16_t)(enabled!=0);
}

static unsigned world_enemy_normal_damage(unsigned attack, unsigned defense,
                                          unsigned level, unsigned variance_roll,
                                          int defending, int multiplier,
                                          uint16_t *prepared) {
    int variance = (int)(variance_roll % 6u) - 3;
    int damage = (int16_t)(uint16_t)((uint16_t)(attack - defense + variance) *
                                    (unsigned)(uint16_t)multiplier);
    int floor = (int)(level / 5u) + 8;
    /* Source33:7088..70A6 adds variance before the low-word multiplier.
       There is no999 cap. 70FA..715A uses the floor on <= and jumps
       over defense; only the above-floor branch quarters the low word
       of damage*25 at71F2..720C. 7234..723C adds variance again. */
    if (damage <= floor) damage = floor;
    else if (defending) damage = (int16_t)(uint16_t)(damage * 25) / 100;
    damage = (int16_t)(uint16_t)(damage + variance);
    if(prepared)*prepared=(uint16_t)damage;
    return damage > 0 ? (unsigned)damage : 0;
}

unsigned npe_battle_enemy_normal_damage(unsigned attack, unsigned defense,
                                        unsigned level, unsigned variance_roll,
                                        int defending) {
    return world_enemy_normal_damage(attack, defense, level, variance_roll, defending, 1, NULL);
}

static unsigned world_normal_hit(NpeWorld *world, unsigned attack,
                                 unsigned defense, unsigned floor_stat,
                                 unsigned actor_luck, unsigned target_luck) {
    unsigned variance = world->battle_variance_roll;
    unsigned attribute_roll = world->battle_attribute_roll;
    if (!world->battle_executing) {
        variance = world_random15(world) % 6u;
        attribute_roll = world_random15(world) % 100u;
    }
    return world_party_normal_damage(attack, defense, floor_stat, variance,
        npe_battle_attribute_multiplier(actor_luck, target_luck, attribute_roll));
}

static int world_living_party_from(const NpeWorld *world, unsigned first) {
    /* Side-exit source33:BF92..C01E tests only zero. A negative word
       prevents total defeat, although it cannot act or be an AI target. */
    unsigned member;
    for (member = first; member < 3; ++member)
        if (world->party_active[member] && world->hp[member])
            return (int)member;
    return -1;
}

static unsigned world_battle_stat(unsigned base, unsigned bonus) {
    /* Current words are already capped when the native effect is applied.
       Preserve a wrapped signed sum, including a negative current word. */
    return (uint16_t)(base + bonus);
}

static uint16_t world_positive_stat_effect(uint16_t base, uint16_t effect) {
    uint16_t total = (uint16_t)(base + effect);
    /* Source2A:1E0A/1F3E compares the LOW word as signed against999. */
    return (int16_t)total > 999 ? 999 : total;
}

static void world_apply_party_stat_effects(NpeWorld *world, unsigned member,
                                          uint16_t attack, uint16_t defense) {
    /* Source2A:1D60..1FC4 writes only a signed-positive field. Each
       current word is based on64AC/64AE, not the earlier current word;
       only its corresponding counter is refreshed to4. */
    if ((int16_t)attack > 0) {
        world->battle_skill_attack_bonus[member] = (uint16_t)(
            world_positive_stat_effect(world->attack[member], attack) - world->attack[member]);
        world->battle_attack_turns[member] = 4;
    }
    if ((int16_t)defense > 0) {
        world->battle_party_defenses[member] = world->defense[member];
        world->battle_skill_defense_bonus[member] = (uint16_t)(
            world_positive_stat_effect(world->defense[member], defense) - world->defense[member]);
        world->battle_defense_turns[member] = 4;
    }
}

unsigned npe_battle_party_spell_damage(unsigned hp_effect, unsigned current_attack) {
    int effect = (int16_t)(uint16_t)(hp_effect + (int16_t)current_attack / 3);
    return effect > 0 ? (unsigned)effect : 12;
}

static void world_apply_party_scalar_effects(NpeWorld *world, unsigned member,
                                             uint16_t hp, uint16_t qi,
                                             uint16_t attack, uint16_t defense) {
    /* Source2A:1A90..1D50: positive signed effects only; each recovery
       checks current signed HP separately, after the preceding service. */
    if ((int16_t)hp > 0 && (int16_t)world->hp[member] > 0) {
        uint16_t value = (uint16_t)(world->hp[member] + hp);
        world->hp[member] = (int16_t)value > (int16_t)world->max_hp[member] ?
                            world->max_hp[member] : value;
    }
    if ((int16_t)qi > 0 && (int16_t)world->hp[member] > 0) {
        uint16_t value = (uint16_t)(world->qi[member] + qi);
        world->qi[member] = (int16_t)value > (int16_t)world->max_qi[member] ?
                            world->max_qi[member] : value;
    }
    world_apply_party_stat_effects(world, member, attack, defense);
}

static uint16_t world_subtract_scalar(uint16_t base, uint16_t effect) {
    uint16_t value = (uint16_t)(base - effect);
    return (int16_t)value < 0 ? 0 : value;
}

static void world_apply_enemy_scalar_effects(NpeWorld *world, unsigned enemy,
                                             uint16_t hp, uint16_t qi,
                                             uint16_t attack, uint16_t defense) {
    /* Source2A:107A..1A70: low-word subtraction, signed-negative clamp;
       current stats are rebuilt from base, each with its own -4 counter. */
    if ((int16_t)hp > 0)
        world->battle_enemy_hps[enemy] = world_subtract_scalar(world->battle_enemy_hps[enemy], hp);
    if ((int16_t)qi > 0)
        world->battle_enemy_qis[enemy] = world_subtract_scalar(world->battle_enemy_qis[enemy], qi);
    if ((int16_t)attack > 0) {
        world->battle_enemy_attacks[enemy] = world_subtract_scalar(world->battle_enemy_base_attacks[enemy], attack);
        world->battle_enemy_attack_turns[enemy] = (uint16_t)-4;
    }
    if ((int16_t)defense > 0) {
        world->battle_enemy_defenses[enemy] = world_subtract_scalar(world->battle_enemy_base_defenses[enemy], defense);
        world->battle_enemy_defense_turns[enemy] = (uint16_t)-4;
    }
}

static void world_apply_battle_status_effects(NpeWorld *world, unsigned own_side,
                                             unsigned group, unsigned target,
                                             const int16_t effects[4]) {
    unsigned party_slots[3], party_count = 0;
    for (unsigned member = 0; member < 3; ++member)
        if (world->party_active[member]) party_slots[party_count++] = member;
    /* Source2D:3F2C..414A (magic) / 4744..4A04 (items) uses exactly
       three compact records for a group, including fallen/unused slots.
       Own groups can extend past the compact party into enemy records. */
    for (unsigned local = 0; local < (group ? 3u : 1u); ++local) {
        int16_t *states;
        if (!group) states = own_side ? world->battle_party_status[target] :
                                       world->battle_enemy_status[target];
        else if (!own_side) states = world->battle_enemy_status[local];
        else if (local < party_count) states = world->battle_party_status[party_slots[local]];
        else states = world->battle_enemy_status[local - party_count];
        for (unsigned status = 0; status < 4; ++status)
            states[status] = (int16_t)(uint16_t)((uint16_t)states[status] + (uint16_t)effects[status]);
    }
}

uint16_t npe_battle_revival_hp(unsigned effect_type, uint16_t max_hp) {
    if (effect_type == 19) return max_hp;
    if (effect_type < 16 || effect_type > 18) return 0;
    /* Source2A:0E3C..0E90: MUL produces R6:R7, but C2 DIV reads only
       signed R7. Preserve the low-word wrap before dividing by 100. */
    unsigned percent = 10u + 20u * (effect_type - 16u);
    return (uint16_t)((int16_t)(uint16_t)(percent * max_hp) / 100);
}

unsigned npe_world_available_skills(const NpeFile *file, const NpeWorld *world,
                                    unsigned member, unsigned ids[18]) {
    unsigned count = 0;
    if (!file || !world || !ids || member >= 3 || !world->party_active[member])
        return 0;
    for (unsigned slot = 0; slot < 18 && world->skill_slots[member][slot]; ++slot) {
        NpeSkill skill;
        unsigned id = world->skill_slots[member][slot];
        if (npe_skill(file, id - 1, &skill) || world->level[member] < skill.required_level)
            break;
        ids[count++] = id;
    }
    return count;
}

unsigned npe_world_available_cached_skills(const NpeFile *file,NpeWorld *world,
                                            unsigned member,unsigned ids[18]) {
    unsigned count=0;
    if(!world || !ids || member>=3 || !world->party_active[member])return 0;
    world->property_member=(uint8_t)member;world->property_page=0;
    /*49:E512/E586 reads map69DF/69DE even while battle6450 is active. */
    uint8_t map_record[148];
    if(npe_world_map_record_image(world,member,map_record))return 0;
    for(unsigned slot=0;slot<18 && map_record[25+slot];++slot){
        unsigned id=map_record[25+slot];NpeSkill skill;
        if(npe_world_skill(file,world,id,&skill) || map_record[24]<skill.required_level)break;
        ids[count++]=id;
    }
    world->property_page=(uint8_t)count;return count;
}

static uint8_t world_native_target(const NpeWorld *world,unsigned target,int party){
    unsigned count=0;
    for(unsigned i=0;i<3;++i)count+=world->party_active[i]!=0;
    return (uint8_t)(target+1+(party?0:count));
}

static void world_prepare_effect_record(NpeWorld *world,unsigned actor,
                                        unsigned target,unsigned own_side,
                                        unsigned group,unsigned value,int item,
                                        int numeric){
    if(actor>=6 || group>1)return;
    unsigned party_count=world_record_party_count(world);
    unsigned physical_target=target+(own_side?0:party_count);
    if(physical_target>=6)return;
    uint8_t prepared[12];
    memcpy(prepared,world->battle_record_residue[actor]+102,sizeof(prepared));
    if(!group){
        /*2D:419C..4298 /4A56..4B52 clears only three words, then
          marks the selected physical target.35:EF1A replaces that
          marker with DEF/ATK/QI if nonzero, otherwise HP even if zero. */
        memset(prepared,0,6);
        record_word(prepared,2*physical_target,numeric?value:1);
    }else{
        /*2D:46DC..4742 marks word0 for a group item; group spells
          do not set a marker.35:F198..F96C overwrites only the target
          side's prefix for numeric effects and leaves other words. */
        if(item)record_word(prepared,0,1);
        unsigned first=own_side?0:party_count;
        unsigned count=own_side?party_count:world->battle_enemy_count;
        if(numeric)for(unsigned i=first;i<first+count&&i<6;++i)
            record_word(prepared,2*i,value);
    }
    (void)npe_world_record_write(world,1,actor,102,prepared,sizeof(prepared));
}

static void world_clear_battle_commands(NpeWorld *world) {
    memset(world->battle_commands, 0, sizeof(world->battle_commands));
    memset(world->battle_item_reservations, 0, sizeof(world->battle_item_reservations));
    memset(world->battle_item_ids,0,sizeof(world->battle_item_ids));
    world->battle_pending_mask = 0;
}

static void world_clear_battle_effects(NpeWorld *world) {
    world_clear_battle_commands(world);
    memset(world->battle_native_targets,0,sizeof(world->battle_native_targets));
    memset(world->battle_skill_ids,0,sizeof(world->battle_skill_ids));
    memset(world->battle_magic_types,0,sizeof(world->battle_magic_types));
    memset(world->battle_magic_groups,0,sizeof(world->battle_magic_groups));
    memset(world->battle_magic_sides,0,sizeof(world->battle_magic_sides));
    memset(world->battle_native_kinds,0,sizeof(world->battle_native_kinds));
    world->battle_menu_previous_kind=0;
    memset(world->battle_item_types,0,sizeof(world->battle_item_types));
    memset(world->battle_item_type_copies,0,sizeof(world->battle_item_type_copies));
    memset(world->battle_item_groups,0,sizeof(world->battle_item_groups));
    memset(world->battle_item_sides,0,sizeof(world->battle_item_sides));
    world->battle_item_converted_mask=0;
    memset(world->battle_party_status, 0, sizeof(world->battle_party_status));
    memset(world->battle_defense_turns, 0,
           sizeof(world->battle_defense_turns));
    memset(world->battle_skill_attack_bonus, 0,
           sizeof(world->battle_skill_attack_bonus));
    memset(world->battle_skill_defense_bonus, 0,
           sizeof(world->battle_skill_defense_bonus));
    memset(world->battle_attack_turns, 0,
           sizeof(world->battle_attack_turns));
    /* Source38:E182..E216 copies base attack/defense back to the
       current battle fields after clearing effects. Attack is represented
       by the base value plus its now-zero bonus; defense has a copy. */
    for(unsigned member=0;member<3;++member)
        if(world->party_active[member]){
            world->battle_party_defenses[member]=world->defense[member];
        }
    /* Source38:E0CA..E6F6 clears the six battle words only for party
       records. Defeated enemy words remain until the next battle loads. */
    for(unsigned member=0;member<world_record_party_count(world);++member){
        const uint8_t zero[12]={0};
        (void)npe_world_record_write(world,1,member,102,zero,sizeof(zero));
    }
}

static void world_reset_victory_records(NpeWorld *world) {
    /* 33:C298 resets six battle words only on victory. The exit copy
       (2F:C1E2 / 33:C73A) writes the resulting record back to the map;
       successful flee and defeat retain their current battle words. */
    world_clear_battle_effects(world);
    /*38:E14A..E158 rebuilds the six map equipment slots, including
      ID0 loads, after postwin panels. Entry uses the same service
      through world_refresh_entry_equipment; avoid loading twice. */
    if(world->battle_file)
        for(unsigned member=0;member<3;++member)
            if(world->party_active[member])
                world_rebuild_equipment_status(world->battle_file,world,member);
}

static void world_finish_battle_state(NpeWorld *world, int victory) {
    /* Source33:C294 (win), 33:BCCE (loss), 2F:C12E (escape). */
    npe_world_sound_stop(world);
    for (unsigned member = 0; member < 3; ++member)
        if (world->party_active[member]){
            uint8_t record[148];
            if(!npe_world_battle_record_image(world,member,record))
                npe_world_record_write(world,0,member,0,record,148);
            /*33:C73A copies first; C766..C788 then sets battle64E2 to1.
              The map retains the byte from before that write. */
            if(victory)world_set_record_flag(world,member,1);
            world->party_attack_groups[member]=world->battle_attack_groups[member];
            world->party_characters[member]=world->battle_characters[member];
            memcpy(world->party_names[member],world->battle_names[member],12);
            world->battle_map_party_active[member]=world->battle_party_record_active[member];
            for (unsigned status = 0; status < 4; ++status)
                world->status_guard[member][status] =
                    (uint16_t)world->battle_party_status[member][status];
        }
    if(world->battle_party_flags_saved){
        for(unsigned slot=0;slot<6;++slot){
            uint8_t record[148];
            if(!npe_world_battle_record_image(world,slot,record))
                memcpy(world->battle_record_residue[slot],record,148);
        }
        memcpy(world->party_active,world->battle_map_party_active,sizeof(world->party_active));
        world->battle_party_flags_saved=0;
    }
    /* Loss33:BE8C and flee2F:C1E2 copy the resident item ID intact.
       Retire the host queue only after preserving both physical copies. */
    if(!victory)world_clear_battle_commands(world);
    if (world->game_mode == 1) npe_world_sound_map_resume(world);
}

static int world_refresh_entry_equipment(const NpeFile *file, NpeWorld *world) {
    world->battle_menu_phase=10; /*49:8DF0..8DF8, before selector initialization. */
    world->battle_execution_slot=1; /*49:8E10. */
    world->battle_action_ready=0; /*4E:7FBC/8C0C and4F:0684 finish entry. */
    /*49:8D64..8DE0 starts with three records and trims zero flags only
      from the tail. A hole inside this prefix still enters combat. Keep
      the map flags separately; native exit copies preserve those bytes. */
    if(!world->battle_party_flags_saved){
        unsigned count=3;
        for(unsigned member=0;member<3;++member)
            npe_world_map_record_image(world,member,world->battle_map_records[member]);
        memcpy(world->battle_record_ids,world->party_ids,3);
        memcpy(world->battle_map_party_active,world->party_active,sizeof(world->party_active));
        while(count && !world->party_active[count-1])--count;
        world->battle_record_party_count=(uint8_t)count;
        for(unsigned member=0;member<3;++member)world->party_active[member]=(uint8_t)(member<count);
        world->battle_party_flags_saved=1;
    }
    for(unsigned member=0;member<3;++member)
        world->battle_native_targets[member]=world_native_target(world,0,0);
    world->battle_display_target=world_native_target(world,0,0); /*49:8E24. */
    memcpy(world->battle_attack_groups,world->party_attack_groups,sizeof(world->battle_attack_groups));
    memcpy(world->battle_characters,world->party_characters,sizeof(world->battle_characters));
    memcpy(world->battle_names,world->party_names,sizeof(world->battle_names));
    memcpy(world->battle_party_record_active,world->battle_map_party_active,sizeof(world->battle_party_record_active));
    unsigned record_count=world_record_party_count(world);
    for(unsigned m=0;m<record_count;++m){
        /*38:DF00..DF6C copies the map record before rebuilding its
          equipment. Keep untouched bytes from that physical snapshot. */
        memcpy(world->battle_record_residue[m],world->battle_map_records[m],148);
        /*38:E214..E28C initializes all six prepared damage words. */
        memset(world->battle_record_residue[m]+102,0,12);
    }
    int descriptor_status=world_load_party_battle_descriptors(file,world,0);
    if(descriptor_status)return descriptor_status;
    for(unsigned e=0;!world->battle_enemy_records_loaded&&e<3;++e)if(world->battle_enemy_ids[e]){
        NpeEnemy enemy;unsigned slot=record_count+e;
        if(slot<6&&!npe_enemy(file,world->battle_enemy_ids[e]-1,&enemy))
            memcpy(world->battle_record_residue[slot],enemy.name,12);
    }
    /*49:8E32..8EE4 initializes targets/acted flags only for the party
      prefix. Enemy loading retains146, including a former party slot. */
    for(unsigned slot=0;slot<record_count;++slot)
        world_set_record_flag(world,slot,0);
    memset(world->battle_skill_ids,0,sizeof(world->battle_skill_ids));
    memset(world->battle_magic_types,0,sizeof(world->battle_magic_types));
    memset(world->battle_magic_groups,0,sizeof(world->battle_magic_groups));
    memset(world->battle_magic_sides,0,sizeof(world->battle_magic_sides));
    memset(world->battle_native_kinds,0,sizeof(world->battle_native_kinds));
    world->battle_menu_previous_kind=0;
    memset(world->battle_item_ids,0,sizeof(world->battle_item_ids));
    memset(world->battle_item_types,0,sizeof(world->battle_item_types));
    memset(world->battle_item_type_copies,0,sizeof(world->battle_item_type_copies));
    memset(world->battle_item_groups,0,sizeof(world->battle_item_groups));
    memset(world->battle_item_sides,0,sizeof(world->battle_item_sides));
    world->battle_item_converted_mask=0;
    world->battle_scene_refreshed = 0;
    world->battle_display_epoch = 0;
    world->battle_enemy_shifted_mask = 0;
    world->battle_party_refresh_mask = 0;
    world->battle_animation_enemy = 0;
    memset(world->battle_party_damage_values,0,sizeof(world->battle_party_damage_values));
    /* 38:E14A calls 39:188E after clearing battle words. Equipment
       rebuilds the map record; it does not grant battle immunity. */
    for (unsigned member = 0; member < 3; ++member)
        if (world->party_active[member])
            world_rebuild_equipment_status(file, world, member);
    /*38:E548 follows the equipment/current-stat work. Voice selection
      reads the resident sex byte and current cached descriptors here. */
    return world_load_party_battle_descriptors(file,world,1);
}

static void world_initialize_enemy_immunity(NpeWorld *world) {
    /* Scripted branch source31:D718..D75C writes sleep +10 at physical
       record party_count+1. For a one-enemy formation this is an unused
       record; it must not grant immunity to enemy slot zero. */
    world->battle_enemy_status[1][1] = 10;
}

int npe_battle_party_target(const NpeWorld *world, unsigned random_roll) {
    unsigned slots[3], count = 0, start, step;
    if (!world) return -1;
    for (unsigned member = 0; member < 3; ++member)
        if (world->party_active[member]) slots[count++] = member;
    if (!count) return -1;
    start = (random_roll % 100u) % count;
    for (step = 0; step < count; ++step) {
        unsigned member = slots[(start + step) % count];
        if ((int16_t)world->hp[member] > 0) return (int)member;
    }
    return -1;
}

static int world_battle_damage_enemy(NpeWorld *world, unsigned target, unsigned damage);

static int world_battle_party_loss(NpeWorld *world) {
    if (world_living_party_from(world, 0) < 0) {
        world->battle_execution_slot=(uint8_t)(world_record_party_count(world)+world->battle_enemy_count);
        world->battle_menu_phase=59; /*33:BC9E defeat stage. */
        /*33:BCA2..BCCA frees packed images before the defeat display. */
        if(npe_world_battle_resources(world->battle_file,world,0))return -1;
        /* Native Tianjiang defeat trace switches mode 3 -> 7 and
           displays the title menu after the final enemy response. */
        if (world->battle_animation_observer)
            world->battle_animation_observer(world->battle_animation_context,world,
                NPE_BATTLE_ANIMATION_DEFEAT,0,NPE_BATTLE_ANIMATION_BEGIN);
        world->battle_action_ready=0; /*33:BCF6, after the defeat LCD. */
        world->battle_lost = 1;
        if(world->story_event_id){
            NpeMapBattleEvent event;
            if(npe_world_battle_event(world->battle_file,world,world->story_event_id,&event)!=1)return -1;
            world->battle_lose_mode=event.lose_mode;world->battle_lose_event_id=event.lose_event_id;
        }
        world->map_init_pending=1; /*33:BD86..BDAA requests native mode initialization. */
        world->game_mode = world->story_event_id ?
                           world->battle_lose_mode : 7;
        if (world->story_event_id)
            world->story_event_id = world->battle_lose_event_id;
        world->battle_enemy_count = 0;
        world->battle_enemy_id = 0;
        world->battle_target = 0;
        world->battle_actor = 0;
        world_finish_battle_state(world, 0);
        /*33:BE8C copies each party record; BECE..BF14 then restores
          an exactly-zero map HP to1. Battle HP remains zero. */
        for(unsigned member=0;member<world_record_party_count(world);++member)
            if(!world->hp[member])world->hp[member]=1;
        if (world->game_mode == 14)
            npe_world_apply_story_event(world, world->story_event_id);
        else
            world_resolve_simple_mode(world);
        if (!world->game_mode) world->game_mode = 1;
        return NPE_BATTLE_LOST;
    }
    return NPE_BATTLE_CONTINUE;
}

static unsigned world_enemy_skill(NpeWorld *world, unsigned member) {
    const uint8_t *skills = world->battle_enemy_skill_ids[member];
    unsigned roll, slot, threshold = world->story_event_id && member == 1 ? 80 : 90;
    if (!(skills[0] || skills[1] || skills[2])) return 0;
    /* Source32:D5DA..D690: consume policy roll before status checks. */
    roll = world_random15(world) % 100u;
    if (roll < threshold || world->battle_enemy_status[member][2] < 0 ||
        world->battle_enemy_status[member][3] < 0) return 0;
    roll = world_random15(world) % 100u;
    slot = roll < 50 ? 0 : roll < 75 ? 1 : 2;
    while (slot && !skills[slot]) --slot;
    return skills[slot];
}

static void world_clear_enemy_spell_cache(NpeWorld *world) {
    /* Only host animation/AI scratch is cleared here.39:AE9E clears
       the physical HP words; the loader overwrites participating slots.
       Other fields in unfilled slots remain resident across battles. */
    memset(world->battle_enemy_last_skill, 0, sizeof(world->battle_enemy_last_skill));
    memset(world->battle_enemy_last_target, 0, sizeof(world->battle_enemy_last_target));
}

static void world_restore_enemy_record_views(NpeWorld *world){
    if(world->battle_party_flags_saved||!world->battle_record_party_count)return;
    unsigned party=world_record_party_count(world);uint8_t records[3][148];
    /* A changed party size moves the enemy prefix within the same six
       physical records. Decode that RAM before clearing/loading new HP;
       do not overlay the previous battle's enemy-relative host arrays. */
    for(unsigned e=0;e<3;++e)memcpy(records[e],world->battle_record_residue[party+e],148);
    for(unsigned e=0;e<3;++e)
        (void)npe_world_record_write(world,1,party+e,0,records[e],148);
}

static int world_enemy_side_target(const NpeWorld *world, unsigned roll) {
    unsigned count = world->battle_enemy_count;
    if (!count) return -1;
    unsigned start = (roll % 100u) % count;
    for (unsigned step = 0; step < count; ++step) {
        unsigned target = (start + step) % count;
        if ((int16_t)world->battle_enemy_hps[target] > 0) return (int)target;
    }
    return -1;
}

static int world_enemy_hit(NpeWorld *world, unsigned member) {
    NpeSkill skill;
    unsigned id, damage = 0, count, animation_action = 0;
    uint16_t prepared_damage=0;
    int target, own_side = 0, group = 0;
    /* Source32:D474..D53A requires at least one signed-positive party
       HP before any skill/target random draw. E13C skips nonpositive HP. */
    if ((int16_t)world->battle_enemy_hps[member] <= 0 ||
        npe_battle_party_target(world, 0) < 0)
        return NPE_BATTLE_CONTINUE;
    id = world_enemy_skill(world, member);
    if (id) {
        if (!world->battle_file || npe_world_skill(world->battle_file,world,id,&skill)) return -1;
        own_side = skill.raw[38] == 1;
        group = skill.raw[37] == 1;
    }
    target = own_side ? world_enemy_side_target(world, world_random15(world)) :
                       npe_battle_party_target(world, world_random15(world));
    if (target < 0) return NPE_BATTLE_CONTINUE;
    world->battle_enemy_last_skill[member] = (uint8_t)id;
    world->battle_enemy_last_target[member] = (uint8_t)(own_side ? target + 3 : target);
    world->battle_display_target=world_native_target(world,(unsigned)target,!own_side);
    if(!id){
        unsigned slot=world_record_party_count(world)+member;
        /*32:DE52..E032 prepares ordinary enemy selector bytes in the
          same physical record as party commands. Skill/item IDs persist
          independently;38:FCCA clears the item ID during next setup. */
        uint8_t selector[]={(uint8_t)(target+1),0,0,0,1,world->battle_record_residue[slot][44],0};
        (void)npe_world_record_write(world,1,slot,126,selector,sizeof(selector));
    }
    /* Enemy AI publishes the physical target before39:5D7C sound selection. */
    if(id){
        unsigned slot=world_record_party_count(world)+member;
        uint8_t selector[9];memcpy(selector,world->battle_record_residue[slot]+126,sizeof(selector));
        /*32:D8AA..DBB4 saves the selected spell, both type bytes,
          group and side; unrelated item bytes remain resident. */
        selector[0]=world_native_target(world,(unsigned)target,!own_side);
        selector[1]=1;selector[2]=selector[4]=skill.raw[14];
        selector[5]=skill.raw[37];selector[6]=skill.raw[38];selector[8]=(uint8_t)id;
        (void)npe_world_record_write(world,1,slot,126,selector,sizeof(selector));
    }
    world->battle_variance_roll = world_random15(world) % 6u;
    world->battle_attribute_roll = world_random15(world) % 100u;
    (void)npe_world_battle_sound(world);
    if (!id) {
        damage = world_enemy_normal_damage(world->battle_enemy_attacks[member],
            world_battle_stat(world->battle_party_defenses[target], world->battle_skill_defense_bonus[target]),
            world->battle_enemy_damage_floor_stats[member], world->battle_variance_roll,
            world->battle_commands[target].kind == COMMAND_DEFEND,
            npe_battle_attribute_multiplier(world->battle_enemy_lucks[member],
                world->other_stats[target][2], world->battle_attribute_roll),
            &prepared_damage);
    } else if (skill.raw[14] == 1) {
        damage = skill.hp_effect;
        if (!own_side) {
            /* Source33:7522 DIV uses R7=target defense, divisor64A6;
               subtract quotient and signed variance, not full defense. */
            int divisor = (int16_t)world->battle_enemy_spell_stats[member];
            int defense = (int16_t)world_battle_stat(world->battle_party_defenses[target],
                                                   world->battle_skill_defense_bonus[target]);
            int reduction = world_divide_signed_word((uint16_t)defense, (uint16_t)divisor);
            int effect = (int16_t)(uint16_t)(damage - reduction -
                                          ((int)world->battle_variance_roll - 3));
            damage = effect > 0 ? (unsigned)effect : 12;
        }
        prepared_damage=(uint16_t)damage;
    }
    if(!id || skill.raw[14]==1 || skill.raw[14]==2){
        world->battle_animation_enemy=(uint8_t)member;
        memset(world->battle_party_damage_values,0,sizeof(world->battle_party_damage_values));
        if(!own_side){
            for(unsigned i=0;i<3;++i)
                if((id && group) || i==(unsigned)target)world->battle_party_damage_values[i]=(uint16_t)damage;
        }
        else {
            memset(world->battle_enemy_damage_values,0,sizeof(world->battle_enemy_damage_values));
            for(unsigned i=0;i<world->battle_enemy_count;++i)
                if(group || i==(unsigned)target)world->battle_enemy_damage_values[i]=(uint16_t)damage;
        }
        animation_action=!id?NPE_BATTLE_ANIMATION_ENEMY_ATTACK:
                own_side?NPE_BATTLE_ANIMATION_ENEMY_RECOVERY:NPE_BATTLE_ANIMATION_ENEMY_SPELL;
        if(world->battle_animation_observer)
            world->battle_animation_observer(world->battle_animation_context,world,
                animation_action,(unsigned)target,NPE_BATTLE_ANIMATION_BEGIN);
    }
    unsigned actor_slot=world_record_party_count(world)+member;
    /* Native action samples retain the old words during casting and
       publish the new prepared values before the floating display. */
    if(!id || skill.raw[14]==1){
        /*33:7246..7348 clears three words for an ordinary attack.
          33:7918..7A3E does the same for a single numeric spell;
          33:7608..769E overwrites only the group receiver prefix.
          Enemy own-side targets are the physical enemy prefix. */
        world_prepare_effect_record(world,actor_slot,(unsigned)target,
            !own_side,(unsigned)group,prepared_damage,0,1);
    }else if(!group){
        /*33:7918..799C also clears three words for single status
          spells, but7A44..7AF6 adds status without setting a marker. */
        const uint8_t zero[6]={0};
        (void)npe_world_record_write(world,1,actor_slot,102,zero,sizeof(zero));
    }
    if(animation_action&&world->battle_animation_observer)
        world->battle_animation_observer(world->battle_animation_context,world,
            animation_action,(unsigned)target,NPE_BATTLE_ANIMATION_DAMAGE);
    count = own_side ? world->battle_enemy_count : 3;
    for (unsigned slot = 0; slot < count; ++slot) {
        if (!group && slot != (unsigned)target) continue;
        if (!own_side && !world->party_active[slot]) continue;
        if (id && skill.raw[14] == 2) {
            /* Source33:776A..779E / 7AC0..7AF6 adds all four signed
               words, including fallen records; no invented immunity gate. */
            for (unsigned status = 0; status < 4; ++status) {
                uint16_t delta = (uint16_t)npe_skill_status_effect(&skill, status);
                if (own_side) world->battle_enemy_status[slot][status] =
                    (int16_t)(uint16_t)((uint16_t)world->battle_enemy_status[slot][status] + delta);
                else world->battle_party_status[slot][status] =
                    (int16_t)(uint16_t)((uint16_t)world->battle_party_status[slot][status] + delta);
            }
        } else if (own_side) {
            /* Source2A:1ADA..1BF8 only heals living records, clamps
               current+effect to that record's maximum; no enemy QI charge. */
            if ((int16_t)world->battle_enemy_hps[slot] > 0) {
                if ((int16_t)damage <= 0) continue;
                uint16_t hp = (uint16_t)(world->battle_enemy_hps[slot] + damage);
                unsigned maximum = world->battle_enemy_max_hps[slot];
                world->battle_enemy_hps[slot] = (int16_t)hp > (int16_t)maximum ? (uint16_t)maximum : hp;
            }
        } else {
            /* Source2A:26BA/2B10 subtracts first, then signed-clamps
               the low word. A negative group victim can wrap positive. */
            if (damage > 0) world->hp[slot] = world_subtract_scalar(world->hp[slot], (uint16_t)damage);
        }
    }
    world->battle_enemy_hp = world->battle_enemy_hps[world->battle_target];
    return world_battle_party_loss(world);
}

static int world_poison_round(NpeWorld *world, unsigned *displayed) {
    unsigned damage = 0, living = 0, first_party = 0;
    unsigned display_poison = 0;
    uint16_t party_values[3]={0},enemy_values[3]={0};
    while (first_party < 3 && !world->party_active[first_party]) ++first_party;
    /* Source36:7270..734A uses the first enemy level (or slot1 if
       slot0 is empty). The later enemy-poison display74DE overwrites
       the same local with first party level*2 before both HP loops. */
    for (unsigned slot = 0; slot < 3; ++slot)
        if (world->party_active[slot] && (int16_t)world->hp[slot] > 0 &&
            (int16_t)world->battle_party_status[slot][0] < 0){
            damage = 2u * world->battle_enemy_damage_floor_stats[
                world->battle_enemy_damage_floor_stats[0] ? 0 : 1];
            party_values[slot]=(uint16_t)damage;
            display_poison=1;
        }
    for (unsigned enemy = 0; enemy < world->battle_enemy_count; ++enemy)
        if ((int16_t)world->battle_enemy_hps[enemy] > 0 && world->battle_enemy_status[enemy][0] < 0){
            damage = first_party < 3 ? 2u * world->level[first_party] : 0;
            enemy_values[enemy]=(uint16_t)damage;
            display_poison=1;
        }
    /* Source36:71DE..7678 displays each side's own prepared amount before
       the later HP loops use the final shared damage word. No pose return. */
    *displayed=display_poison;
    if(display_poison){
        memcpy(world->battle_party_damage_values,party_values,sizeof(party_values));
        memcpy(world->battle_enemy_damage_values,enemy_values,sizeof(enemy_values));
        if(world->battle_animation_observer)
            world->battle_animation_observer(world->battle_animation_context,world,
                NPE_BATTLE_ANIMATION_POISON,0,NPE_BATTLE_ANIMATION_DAMAGE);
    }
    for (unsigned slot = 0; slot < 3; ++slot)
        if (world->party_active[slot] && (int16_t)world->hp[slot] > 0 &&
            (int16_t)world->battle_party_status[slot][0] < 0)
            world->hp[slot] = damage >= world->hp[slot] ? 0 : (uint16_t)(world->hp[slot] - damage);
    for (unsigned enemy = 0; enemy < world->battle_enemy_count; ++enemy) {
        if ((int16_t)world->battle_enemy_hps[enemy] > 0 && world->battle_enemy_status[enemy][0] < 0)
            world->battle_enemy_hps[enemy] = damage >= world->battle_enemy_hps[enemy] ? 0 :
                (uint16_t)(world->battle_enemy_hps[enemy] - damage);
        living += world->battle_enemy_hps[enemy] != 0;
    }
    if (world_living_party_from(world, 0) < 0) return world_battle_party_loss(world);
    if (!living) return world_battle_damage_enemy(world, 0, 0);
    world->battle_enemy_hp = world->battle_enemy_hps[world->battle_target];
    return NPE_BATTLE_CONTINUE;
}

static void world_finish_round_effects(NpeWorld *world) {
    unsigned member;
    world->battle_scene_refreshed = 1;
    /*38:F8F0..F96E clears only the first four prepared target-damage
      words. Targets at physical slots4/5 remain until the next action;
      nonpositive enemy records skip this setup, unlike fallen party. */
    unsigned party_count=world_record_party_count(world);
    for(unsigned slot=0;slot<party_count+world->battle_enemy_count&&slot<6;++slot){
        if(slot>=party_count&&(int16_t)world->battle_enemy_hps[slot-party_count]<=0)continue;
        const uint8_t zero[8]={0};
        (void)npe_world_record_write(world,1,slot,102,zero,sizeof(zero));
    }
    for (member = 0; member < 3; ++member) {
        /* Next-command setup33:C7E4 calls source38:F78C. Negative
           first four status words advance toward zero; positive immunity
           words do not decrement (FA6C checks index>=4 before SUB). */
        if (world->party_active[member]) {
            for (unsigned status = 0; status < 4; ++status)
                if (world->battle_party_status[member][status] < 0)
                    world->battle_party_status[member][status]++;
        }
        if ((int16_t)world->battle_enemy_hps[member] > 0)
            for (unsigned status = 0; status < 4; ++status)
                if (world->battle_enemy_status[member][status] < 0)
                    world->battle_enemy_status[member][status]++;
        if ((int16_t)world->battle_enemy_hps[member] > 0) {
            if ((int16_t)world->battle_enemy_attack_turns[member] < 0) ++world->battle_enemy_attack_turns[member];
            else if (world->battle_enemy_attack_turns[member]) --world->battle_enemy_attack_turns[member];
            if (!world->battle_enemy_attack_turns[member])
                world->battle_enemy_attacks[member] = world->battle_enemy_base_attacks[member];
            if ((int16_t)world->battle_enemy_defense_turns[member] < 0) ++world->battle_enemy_defense_turns[member];
            else if (world->battle_enemy_defense_turns[member]) --world->battle_enemy_defense_turns[member];
            if (!world->battle_enemy_defense_turns[member])
                world->battle_enemy_defenses[member] = world->battle_enemy_base_defenses[member];
        }
        /* Source38:FA68..FCA4 updates6496 and6498 independently.
           It includes fallen party records; enemy dead records skip this.
           A zero counter restores its own base word every setup pass. */
        if ((int16_t)world->battle_attack_turns[member] < 0) ++world->battle_attack_turns[member];
        else if (world->battle_attack_turns[member]) --world->battle_attack_turns[member];
        if (!world->battle_attack_turns[member]) world->battle_skill_attack_bonus[member] = 0;
        if ((int16_t)world->battle_defense_turns[member] < 0) ++world->battle_defense_turns[member];
        else if (world->battle_defense_turns[member]) --world->battle_defense_turns[member];
        if (!world->battle_defense_turns[member]) {
            world->battle_party_defenses[member] = world->defense[member];
            world->battle_skill_defense_bonus[member] = 0;
        }
    }
    /* Source38:FEA2..FEF2 refreshes only scripted middle-slot sleep
       immunity after the status loop, even if a spell reduced it to9. */
    if (world->story_event_id) world_initialize_enemy_immunity(world);
}

static int world_party_can_act(const NpeWorld *world, unsigned member) {
    /* Source39:5940..59C2 / 2F:CA8C..CB2E checks HP and signed
       sleep/confusion words; positive immunity does not block commands. */
    return world->party_active[member] && (int16_t)world->hp[member] > 0 &&
           (int16_t)world->battle_party_status[member][1] >= 0 &&
           (int16_t)world->battle_party_status[member][2] >= 0;
}

int npe_battle_next_actor(const NpeWorld *world, unsigned acted_mask) {
    int selected = -1, speed = 0;
    if (!world) return -1;
    /* Source39:5A34..5BEC repeatedly selects the greatest64A8;
       comparison >= keeps the earlier record on a tie. No RNG draw. */
    for (unsigned actor = 0; actor < 6; ++actor) {
        int current;
        if (acted_mask & (1u << actor)) continue;
        if (actor < 3) {
            if (!world_party_can_act(world, actor)) continue;
            current = (int16_t)world->other_stats[actor][1];
        } else {
            unsigned enemy = actor - 3;
            if (enemy >= world->battle_enemy_count ||
                (int16_t)world->battle_enemy_hps[enemy] <= 0 ||
                world->battle_enemy_status[enemy][1] < 0 ||
                world->battle_enemy_status[enemy][2] < 0) continue;
            current = (int16_t)world->battle_enemy_speeds[enemy];
        }
        if (selected < 0 || current > speed) { selected = (int)actor; speed = current; }
    }
    return selected;
}

static int world_command_party_from(const NpeWorld *world, unsigned first) {
    for (unsigned member = first; member < 3; ++member)
        if (world_party_can_act(world, member)) return (int)member;
    return -1;
}

static void world_prepare_round_record_flags(NpeWorld *world){
    unsigned party=world_record_party_count(world);
    /*39:58C8..5932 clears only participating physical records before
      each round's command collection;5936..5A30 then marks incapacity. */
    for(unsigned slot=0;slot<party+world->battle_enemy_count&&slot<6;++slot)
        world_set_record_flag(world,slot,0);
    for(unsigned m=0;m<party&&m<3;++m)
        if(!world_party_can_act(world,m))world_set_record_flag(world,m,1);
    for(unsigned e=0;e<world->battle_enemy_count&&e<3;++e)
        if((int16_t)world->battle_enemy_hps[e]<=0 || world->battle_enemy_status[e][1]<0 || world->battle_enemy_status[e][2]<0)
            world_set_record_flag(world,party+e,1);
}

/* Source40:1312 is called at39:5D4A and again at39:6058, before
   dispatch and after the completed action.64CE remains independently resident. */
static int world_resolve_command_target(NpeWorld *world,unsigned actor,unsigned *resolved){
    NpeBattleCommand command=world->battle_commands[actor];
    unsigned target = command.target;
    unsigned native=world->battle_native_targets[actor];
    unsigned party_count=world_native_target(world,0,0)-1u;
    if(command.kind==COMMAND_DEFEND && !native){
        unsigned enemy_living=0;
        for(unsigned e=0;e<world->battle_enemy_count;++e)
            enemy_living|=world->battle_enemy_hps[e]!=0;
        if(enemy_living){
            /*40:172E reads the HP of physical target-1 even for target0.
              If that word is zero, 153A..16DE advances within the party
              side. Defense is subject to this shared target resolver. */
            uint8_t raw[2]={0};
            if(npe_world_resident_read(world,UINT32_C(0x52640c),raw,2) &&
               world->battle_file && npe_rpg_runtime_owner(world->battle_file)==world)
                (void)npe_rpg_runtime_read(world->battle_file,UINT32_C(0x52640c),raw,2);
            if(!raw[0]&&!raw[1])
                for(unsigned m=0;m<party_count;++m)if(world->hp[m]){
                    native=world->battle_native_targets[actor]=(uint8_t)(m+1);break;
                }
        }
    }
    if(native)target=native<=party_count?native-1:native-party_count-1;
    int magic_own_side = 0, magic_revival = 0;
    if (command.kind == COMMAND_MAGIC) {
        magic_own_side = world->battle_magic_sides[actor] == 1;
        magic_revival = world->battle_magic_types[actor] >= 16 &&
                        world->battle_magic_types[actor] <= 19;
    }
    if (command.kind == COMMAND_ITEM) {
        magic_own_side = 1;
        magic_revival = world->battle_item_types[actor] >= 16 &&
                        world->battle_item_types[actor] <= 19;
    }
    if (command.kind == COMMAND_ATTACK || command.kind == COMMAND_GROUP ||
        command.kind == COMMAND_COMBINE ||
        command.kind == COMMAND_ATTACK_SKILL || command.kind == COMMAND_THROW ||
        (command.kind == COMMAND_MAGIC && !magic_own_side && !magic_revival)) {
        /* Source40:153A..176E scans forward and wraps within
           the same side when a preselected target has died. */
        if (target >= world->battle_enemy_count || !world->battle_enemy_hps[target]) {
            int replacement = -1;
            for (unsigned step = 1; step <= world->battle_enemy_count; ++step) {
                unsigned candidate = (target + step) % world->battle_enemy_count;
                if (world->battle_enemy_hps[candidate]) { replacement = (int)candidate; break; }
            }
            if (replacement < 0) return 0;
            target = (unsigned)replacement;
        }
    } else if (command.kind == COMMAND_HEAL_SKILL || command.kind == COMMAND_BUFF_SKILL ||
               ((command.kind == COMMAND_MAGIC || command.kind == COMMAND_ITEM) &&
                magic_own_side && !magic_revival)) {
        if (target >= 3 || !world->party_active[target] || !world->hp[target]) {
            int replacement = -1;
            for (unsigned step = 1; step <= 3; ++step) {
                unsigned candidate = (target + step) % 3;
                if (world->party_active[candidate] && world->hp[candidate]) {
                    replacement = (int)candidate; break;
                }
            }
            if (replacement < 0) return 0;
            target = (unsigned)replacement;
        }
    }
    /*40:153A..176E writes a replacement back into64CE before
      animation/dispatch; keep the host slot and native record together. */
    world->battle_commands[actor].target=(uint8_t)target;
    if(command.kind!=COMMAND_DEFEND){
        int party=command.kind==COMMAND_ITEM || command.kind==COMMAND_HEAL_SKILL ||
            command.kind==COMMAND_BUFF_SKILL || magic_own_side;
        world->battle_native_targets[actor]=world_native_target(world,target,party);
    }
    /*40:17BA publishes the resolved64CE, independently of the UI cursor. */
    world->battle_display_target=world->battle_native_targets[actor];
    *resolved=target;
    return 1;
}

static int world_run_round(NpeWorld *world) {
    unsigned acted, animation_action = 0;
    int result = NPE_BATTLE_CONTINUE, actor;
next_round:
    ++world->battle_display_epoch;
    animation_action = 0;
    acted = (~world->battle_pending_mask) & 7u;
    world->battle_last_order_count = 0;
    world->battle_executing = 1;
    world_prepare_round_record_flags(world);
    unsigned record_count=world_record_party_count(world);
    for (;;) {
        /* Source39:5936..5A30 repeats this scan before each selection.
           Newly fallen/sleeping/confused records get flag1 here. If no
           actual action follows, the source never calls poison. */
        for (unsigned member = 0; member < 3; ++member) {
            if (!world_party_can_act(world, member)) {
                acted |= 1u << member;
                if(member<record_count)world_set_record_flag(world,member,1);
            }
            if (member >= world->battle_enemy_count ||
                (int16_t)world->battle_enemy_hps[member] <= 0 ||
                world->battle_enemy_status[member][1] < 0 ||
                world->battle_enemy_status[member][2] < 0) {
                acted |= 1u << (member + 3);
                if(member<world->battle_enemy_count&&record_count+member<6)
                    world_set_record_flag(world,record_count+member,1);
            }
        }
        actor = npe_battle_next_actor(world, acted);
        if (actor < 0) break;
        acted |= 1u << actor;
        unsigned record_slot=actor<3?(unsigned)actor:record_count+(unsigned)actor-3;
        /*33:C868..C876 commits the preceding actor's restored scene before
          the scheduler publishes the next physical execution index. */
        if(animation_action && world->battle_animation_observer)
            world->battle_animation_observer(world->battle_animation_context,world,
                animation_action,world->battle_target,NPE_BATTLE_ANIMATION_ACTOR_RESTORE);
        /*39:5AE8/5BC6 selects a zero-based physical record;5D1E publishes
          its one-based index before checking the independent dispatch byte. */
        world->battle_execution_slot=(uint8_t)(record_slot+1);
        if(record_slot<6)world_set_record_flag(world,record_slot,1);
        world->battle_last_order[world->battle_last_order_count++] = (uint8_t)actor;
        if(world->battle_action_ready!=1)goto action_finished;
        if (actor >= 3) {
            /* Source39:5D30..5D4A chooses enemy AI at execution time. */
            result = world_enemy_hit(world, (unsigned)actor - 3);
            if(!world->battle_enemy_last_skill[(unsigned)actor-3])
                animation_action=NPE_BATTLE_ANIMATION_ENEMY_ATTACK;
            else {
                NpeSkill rendered_skill;
                uint8_t raw[42];
                if(!npe_world_skill_view(world,raw,&rendered_skill) &&
                   (rendered_skill.raw[14]==1 || rendered_skill.raw[14]==2))
                    animation_action=rendered_skill.raw[38]==1?
                        NPE_BATTLE_ANIMATION_ENEMY_RECOVERY:NPE_BATTLE_ANIMATION_ENEMY_SPELL;
            }
        } else {
            NpeBattleCommand command = world->battle_commands[actor];
            unsigned target;
            if(!world_resolve_command_target(world,(unsigned)actor,&target))break;
            /* Source39:5C38..5CBA marks the whole party acted when one
               type3 command is selected; only that actor executes it. */
            if (command.kind == COMMAND_COMBINE) acted |= 7u;
            /* Source2D:36A6/378C draws before the action dispatch,
               including skills, items and defense. */
            world->battle_variance_roll = world_random15(world) % 6u;
            world->battle_attribute_roll = world_random15(world) % 100u;
            /*39:5D4E prepares the action before5D7C selects sound. */
            (void)npe_world_battle_sound(world);
            switch (command.kind) {
            case COMMAND_ATTACK:
            case COMMAND_GROUP:
                /* Source2F:CDC2 and D624 use the same fallback poses;
                   group assignment still executes each queued actor. */
                animation_action=NPE_BATTLE_ANIMATION_ATTACK;
                result = npe_world_battle_attack(world,target); break;
            case COMMAND_COMBINE:
                animation_action = NPE_BATTLE_ANIMATION_COMBINE;
                if (world->battle_animation_observer)
                    world->battle_animation_observer(world->battle_animation_context,
                        world, NPE_BATTLE_ANIMATION_COMBINE, target,
                        NPE_BATTLE_ANIMATION_BEGIN);
                result = npe_world_battle_combine_attack(world); break;
            case COMMAND_ATTACK_SKILL:
                result = npe_world_battle_cast_attack_skill(world->battle_file, world,
                                                            command.resource_id, target); break;
            case COMMAND_ITEM:
                animation_action=NPE_BATTLE_ANIMATION_ITEM;
                result = npe_world_battle_use_item(world->battle_file, world,
                                                   command.resource_id, target); break;
            case COMMAND_THROW:
                animation_action=NPE_BATTLE_ANIMATION_ITEM;
                result = npe_world_battle_throw_item(world->battle_file, world,
                                                     command.resource_id, target); break;
            case COMMAND_DEFEND:
                animation_action=NPE_BATTLE_ANIMATION_DEFEND;
                if(world->battle_animation_observer)
                    world->battle_animation_observer(world->battle_animation_context,
                        world,animation_action,target,NPE_BATTLE_ANIMATION_BEGIN);
                result = npe_world_battle_defend(world); break;
            case COMMAND_HEAL_SKILL:
                result = npe_world_battle_cast_heal_skill(world->battle_file, world,
                                                          command.resource_id, target); break;
            case COMMAND_BUFF_SKILL:
                result = npe_world_battle_cast_skill(world->battle_file, world,
                                                     command.resource_id, target); break;
            case COMMAND_MAGIC:
                animation_action=NPE_BATTLE_ANIMATION_PARTY_SPELL;
                result = npe_world_battle_cast_magic(world->battle_file, world,
                                                      world->battle_skill_ids[actor], target); break;
            default: result = -1; break;
            }
            /* A command whose resource became unavailable has no effect;
               its selection must not consume it a second time. */
            if (result < 0) result = NPE_BATTLE_CONTINUE;
        }
action_finished:
        if (result != NPE_BATTLE_CONTINUE || world->game_mode != 3) break;
        if(actor<3){
            unsigned post_target;
            (void)world_resolve_command_target(world,(unsigned)actor,&post_target);
        }
        /* Source39:605C calls source36:6FBC after each actual action;
           36:6FFE..708C requires every record's acted flag to be set. */
        if ((acted & 63u) == 63u) {
            unsigned poison_displayed;
            result = world_poison_round(world,&poison_displayed);
            if(poison_displayed)animation_action=NPE_BATTLE_ANIMATION_POISON;
            if (result != NPE_BATTLE_CONTINUE) break;
        }
    }
    /* Source41:43DE..4456 restores queued defense and signed low-HP
       records at round end (the native branch also handles flee, whose C
       entry is immediate). Retain the mask before clearing the queue;
       other records keep their cached border. */
    world->battle_party_refresh_mask = 0;
    for (unsigned member = 0; member < 3; ++member)
        if (world->party_active[member] &&
            (world->battle_commands[member].kind == COMMAND_DEFEND ||
             (int16_t)world->hp[member] <= (int16_t)world->max_hp[member] / 5))
            world->battle_party_refresh_mask |= (uint8_t)(1u << member);
    world->battle_executing = 0;
    world->battle_pending_mask = 0;
    /*38:FCCA..FCEC clears the independent item ID for next commands;
      the type words remain resident until overwritten or battle entry. */
    memset(world->battle_item_ids,0,sizeof(world->battle_item_ids));
    memset(world->battle_commands, 0, sizeof(world->battle_commands));
    /*38:FD14..FD7A retains64CF=5 and clears other native commands.
      Host pending commands are separate from this resident selector state. */
    for(unsigned member=0;member<3;++member){
        if(world->battle_native_kinds[member]!=COMMAND_DEFEND)
            world->battle_native_kinds[member]=0;
        if(result==NPE_BATTLE_CONTINUE && world->game_mode==3)
            world->battle_native_targets[member]=world_native_target(world,0,0);
    }
    if(result==NPE_BATTLE_CONTINUE && world->game_mode==3){
        /*38:F8C6..F8D8 skips signed nonpositive enemy HP toFE7C.
          Such records retain their item/kind/target for the next round;
          party records still pass through38:FCCA..FDCC when fallen. */
        for(unsigned e=0;e<world->battle_enemy_count&&e<3;++e){
            if((int16_t)world->battle_enemy_hps[e]<=0)continue;
            unsigned slot=record_count+e;
            /* Record IO already rejects slots outside the six-entry bank.
               Apply the same bound before reading its command byte. */
            if (slot >= 6) break;
            uint8_t target=world_native_target(world,0,0),zero=0;
            (void)npe_world_record_write(world,1,slot,126,&target,1);
            if(world->battle_record_residue[slot][127]!=COMMAND_DEFEND)
                (void)npe_world_record_write(world,1,slot,127,&zero,1);
            (void)npe_world_record_write(world,1,slot,133,&zero,1);
        }
    }
    memset(world->battle_item_reservations, 0, sizeof(world->battle_item_reservations));
    if (result == NPE_BATTLE_CONTINUE && world->game_mode == 3) {
        world_finish_round_effects(world);
        world_prepare_round_record_flags(world);
        actor = world_command_party_from(world, 0);
        if (actor >= 0) {
            world->battle_actor = (uint8_t)actor;
            (void)npe_world_battle_capture_selector(world);
        }
        else if (npe_battle_party_target(world, 0) >= 0) {
            /* No command menu for a party entirely asleep/confused:
               advance empty party rounds until a command is available. */
            world->battle_execution_slot=(uint8_t)(record_count+world->battle_enemy_count+1);
            goto next_round;
        }
        if(animation_action && world->battle_animation_observer)
            world->battle_animation_observer(world->battle_animation_context,world,
                animation_action,world->battle_target,NPE_BATTLE_ANIMATION_ROUND_RESTORE);
        world->battle_execution_slot=(uint8_t)(record_count+world->battle_enemy_count+1);
        if (animation_action && world->battle_animation_observer)
            world->battle_animation_observer(world->battle_animation_context,
                world, animation_action, world->battle_target,
                NPE_BATTLE_ANIMATION_ROUND_END);
        world->battle_action_ready=0; /*39:5AD6 after returning from the selector. */
    }
    return result;
}

int npe_world_battle_cancel_item_selection(NpeWorld *world) {
    unsigned actor, item;
    if (!world || world->game_mode != 3 || world->battle_executing ||
        world->battle_actor >= 3) return -1;
    actor = world->battle_actor;
    item = world->battle_item_reservations[actor];
    if (!item) return 0;
    if(item>255)return -1;
    world_refund_battle_item(world,item);
    world->battle_item_reservations[actor] = 0;
    return 1;
}

int npe_world_battle_previous_actor(NpeWorld *world) {
    int previous;
    if (!world || world->game_mode != 3 || world->battle_executing ||
        world->battle_actor >= 3) return -1;
    /* Source36:DBE0..DEDE: base-icon Menu searches backward past
       fallen/sleeping/confused records and restores the selected item.
       A skipped record is not refunded by that search. */
    for (previous = (int)world->battle_actor - 1; previous >= 0; --previous)
        if (world_party_can_act(world, (unsigned)previous) &&
            (world->battle_pending_mask & (1u << previous))) break;
    if (previous < 0) return 0;
    unsigned id=world->battle_item_ids[world->battle_actor];
    world_refund_battle_item(world,id);
    world->battle_item_reservations[world->battle_actor]=0;
    world->battle_actor = (uint8_t)previous;
    world->battle_execution_slot=(uint8_t)(previous+1); /*36:DEB8. */
    id=world->battle_item_ids[previous];world_refund_battle_item(world,id);
    world->battle_item_reservations[previous]=0;
    /*36:DFBA..DFDA clears the returned-to record after its refund. */
    world->battle_item_ids[previous]=0;
    /*36:DE84 clears64CF; global7AAE remains from the current
      selector, not from the returned-to actor's resident command. */
    world->battle_native_kinds[previous]=0;
    memset(&world->battle_commands[previous], 0, sizeof(world->battle_commands[previous]));
    world->battle_pending_mask &= (uint8_t)~(1u << previous);
    return 1;
}

int npe_world_battle_select_target(NpeWorld *world,unsigned target,int party){
    unsigned actor;
    if(!world || world->game_mode!=3 || world->battle_executing ||
       (actor=world->battle_actor)>=3 || (party?target>=3 || !world->party_active[target]:
        target>=world->battle_enemy_count))return -1;
    world->battle_native_targets[actor]=world_native_target(world,target,party);
    world->battle_commands[actor].target=(uint8_t)target;
    return 1;
}

int npe_world_battle_cancel_target(NpeWorld *world){
    if(!world || world->game_mode!=3 || world->battle_executing || world->battle_actor>=3)return -1;
    world->battle_native_targets[world->battle_actor]=world_native_target(world,0,0);
    world->battle_display_target=world->battle_native_targets[world->battle_actor];
    world->battle_commands[world->battle_actor].target=3;
    return 1;
}

int npe_world_battle_select_attack(NpeWorld *world,unsigned target){
    unsigned actor;
    if(!world || world->game_mode!=3 || world->battle_executing ||
       (actor=world->battle_actor)>=3 || !world->party_active[actor] ||
       target>=world->battle_enemy_count)return -1;
    /*40:1A5A/1AA8/1AF6 and1B44 prepare the selector, even if it is
      canceled.64D5 remains independent;647C supplies the group byte. */
    world->battle_item_types[actor]=0;world->battle_item_type_copies[actor]=1;
    world->battle_item_sides[actor]=0;
    world->battle_item_groups[actor]=world->battle_attack_groups[actor];
    world->battle_commands[actor]=(NpeBattleCommand){COMMAND_ATTACK,(uint8_t)target,0};
    world->battle_native_kinds[actor]=world->battle_menu_previous_kind=0;
    (void)npe_world_battle_select_target(world,target,0);
    return 1;
}

int npe_world_battle_capture_selector(NpeWorld *world) {
    uint8_t record[148];
    if(!world || npe_world_battle_record_image(world,world->battle_actor,record))return -1;
    /*2F:C914..CA40 snapshots the selected physical record. These words
      remain independent while a selector edits the command bytes. */
    world->battle_menu_previous_kind=record[127];
    world->battle_selection_magic_type=record[128];
    world->battle_selection_item_type=record[129];
    world->battle_selection_type_copy=record[130];
    return 0;
}

static int world_queue_command(NpeWorld *world, unsigned kind,
                                unsigned target, unsigned resource_id) {
    /*2F:C81C/C8EE/CA64 retain the base button across normal selection. */
    world->battle_menu_phase=(uint8_t)(world->battle_menu_phase/10u*10u);
    unsigned actor = world->battle_actor;
    int next;
    unsigned previous_kind=world->battle_menu_previous_kind;
    if(kind==COMMAND_ATTACK){
        if(npe_world_battle_select_attack(world,target)<0)return -1;
    }else if(kind==COMMAND_DEFEND){
        /*2F:C95C saves the old64CF in7AAE;42:8D80 copies that
          byte to64D2. It is not always the selected defense-menu row. */
        world->battle_item_type_copies[actor]=(uint8_t)previous_kind;
    }
    world->battle_commands[actor] = (NpeBattleCommand){(uint8_t)kind,
        (uint8_t)target, (uint16_t)resource_id};
    unsigned native_kind=kind==COMMAND_THROW?2:
        kind==COMMAND_MAGIC || kind==COMMAND_HEAL_SKILL || kind==COMMAND_BUFF_SKILL?1:
        kind==COMMAND_COMBINE?3:kind;
    world->battle_native_kinds[actor]=world->battle_menu_previous_kind=(uint8_t)native_kind;
    if(kind==COMMAND_DEFEND)world->battle_native_targets[actor]=0;
    else {
        int party=kind==COMMAND_ITEM || kind==COMMAND_HEAL_SKILL || kind==COMMAND_BUFF_SKILL ||
            (kind==COMMAND_MAGIC && world->battle_magic_sides[actor]==1);
        (void)npe_world_battle_select_target(world,target,party);
    }
    world->battle_pending_mask |= (uint8_t)(1u << actor);
    next = world_command_party_from(world, actor + 1);
    if (next >= 0) {
        world->battle_actor = (uint8_t)next;
        world->battle_execution_slot=(uint8_t)(next+1); /*2F:CB3E. */
        if(kind==COMMAND_ATTACK){
            /* Ordinary selection resumes after40:17D2's signed-HP scan.
               A next actor still pointing at a fallen enemy must publish
               its replacement before the new command menu waits for input. */
            unsigned party_count=world_record_party_count(world);
            unsigned native=world->battle_native_targets[next];
            if(native>party_count && native<=party_count+world->battle_enemy_count){
                unsigned old=native-party_count-1;
                if((int16_t)world->battle_enemy_hps[old]<=0)
                    for(unsigned step=1;step<=world->battle_enemy_count;++step){
                        unsigned target=(old+step)%world->battle_enemy_count;
                        if((int16_t)world->battle_enemy_hps[target]>0){
                            world->battle_native_targets[next]=world_native_target(world,target,0);
                            world->battle_commands[next].target=(uint8_t)target;
                            break;
                        }
                    }
            }
        }
        /* Enemy selectors resume with the next actor's physical target;
           the friendly picker retains its entry target until list cancel. */
        if(kind==COMMAND_ATTACK || kind==COMMAND_GROUP || kind==COMMAND_THROW ||
           (kind==COMMAND_MAGIC && world->battle_magic_sides[actor]==0))
            world->battle_display_target=world->battle_native_targets[next];
        /*2F:C95C captures the new actor on normal selector entry. */
        (void)npe_world_battle_capture_selector(world);
        return NPE_BATTLE_CONTINUE;
    }
    /*2F:CA70 starts the next selector scan at0 before CB94 dispatches.
      That menu index stays independent throughout physical actions. */
    int first=world_command_party_from(world,0);
    world->battle_actor=(uint8_t)(first>=0?first:(int)world_record_party_count(world));
    world->battle_action_ready=1; /*2F:CB94 confirms the complete queue. */
    return world_run_round(world);
}

static int world_failed_flee_round(NpeWorld *world, int random_prompt) {
    world->battle_menu_phase=world->story_event_id?10:(uint8_t)(world->battle_menu_phase/10u*10u);
    int target = -1;
    /* Source2F:BA08..BD16 and C2E4..C5FE replace every party command
       with ordinary attack at the first signed-positive enemy record.
       Previously reserved items remain consumed, even for skipped actors.
       Source39:5936 then skips fallen/sleeping/confused records normally. */
    for (unsigned enemy = 0; enemy < world->battle_enemy_count; ++enemy)
        if ((int16_t)world->battle_enemy_hps[enemy] > 0) {
            target = (int)enemy; break;
        }
    if (target < 0) return -1;
    world->battle_pending_mask = 0;
    memset(world->battle_commands, 0, sizeof(world->battle_commands));
    memset(world->battle_item_reservations, 0, sizeof(world->battle_item_reservations));
    for (unsigned member = 0; member < 3; ++member)
        if (world->party_active[member]) {
            world->battle_commands[member] =
                (NpeBattleCommand){COMMAND_ATTACK, (uint8_t)target, 0};
            world->battle_native_targets[member]=world_native_target(world,(unsigned)target,0);
            world->battle_native_kinds[member]=world->battle_menu_previous_kind=0;
            world->battle_pending_mask |= (uint8_t)(1u << member);
        }
    world->battle_target = (uint8_t)target;
    if(random_prompt && world->battle_animation_observer)
        world->battle_animation_observer(world->battle_animation_context,world,
            NPE_BATTLE_ANIMATION_FAILED_FLEE,0,NPE_BATTLE_ANIMATION_BEGIN);
    world->battle_action_ready=1; /*2F:BDA6/C624, after the failure prompt. */
    /*2F:C63C and CA70 reset/scanning also apply to failed escape. */
    int first=world_command_party_from(world,0);
    world->battle_actor=(uint8_t)(first>=0?first:(int)world_record_party_count(world));
    world->battle_execution_slot=(uint8_t)(world_record_party_count(world)+1);
    return world_run_round(world);
}

static int world_finish_party_action(NpeWorld *world) {
    /* Effects settle only inside the round executor. */
    return world->battle_executing ? NPE_BATTLE_CONTINUE : -1;
}

static int world_prepare_shared_attack(NpeWorld *world,int combine) {
    if(!world || world->game_mode!=3 || world->battle_executing)return -1;
    unsigned party=world_record_party_count(world),target=0;
    if(party>3 || (!combine && world->battle_formation_count>3))return -1;
    if(!combine) {
        /*44:8B94..8C3A scans physical HP for nonzero, not signed-positive.
          Even count0 examines the first slot; an all-zero prefix retains
          its last target. This preparation does not execute the command. */
        uint8_t record[148];
        for(;;) {
            if(npe_world_battle_record_image(world,party+target,record))return -1;
            if(record[80] || record[81] || target+1>=world->battle_formation_count)break;
            ++target;
        }
        world->battle_display_target=(uint8_t)(party+target+1);
    }
    world->battle_pending_mask=0;
    /*44:8C44..8E66 and49:8192..8330 write every prefix record. Fallen,
      sleeping and confused members are skipped later by the executor. */
    for(unsigned member=0;member<party;++member) {
        uint8_t record[148],selectors[7];
        if(npe_world_battle_record_image(world,member,record))return -1;
        memcpy(selectors,record+126,sizeof(selectors));
        selectors[0]=(uint8_t)(party+target+1);selectors[1]=(uint8_t)(combine?3:4);
        selectors[4]=1;selectors[5]=(uint8_t)(combine!=0);selectors[6]=0;
        /* Combined entry retains64D0/D1; group entry clears both. */
        if(!combine)selectors[2]=selectors[3]=0;
        if(npe_world_record_write(world,1,member,126,selectors,sizeof(selectors)))return -1;
        world->battle_commands[member]=(NpeBattleCommand){combine?COMMAND_COMBINE:COMMAND_GROUP,(uint8_t)target,0};
        world->battle_pending_mask|=(uint8_t)(1u<<member);
    }
    uint8_t first[148];if(npe_world_battle_record_image(world,0,first))return -1;
    world->battle_menu_previous_kind=first[127];
    if(combine)world->battle_display_target=first[126]; /*49:8354. */
    else {
        /*44:8E80..8EAE zero-extends bytes from the first physical record,
          also when the party prefix is empty. Combine leaves these words. */
        world->battle_selection_magic_type=first[128];
        world->battle_selection_item_type=first[129];
        world->battle_selection_type_copy=first[130];
        world->battle_actor=(uint8_t)(party-1); /*44:8EC2, including count0. */
    }
    world->battle_target=(uint8_t)target;
    return 0;
}

int npe_world_battle_prepare_group(NpeWorld *world) {
    return world_prepare_shared_attack(world,0);
}

int npe_world_battle_prepare_combine(NpeWorld *world) {
    return world_prepare_shared_attack(world,1);
}

int npe_world_battle_group_attack(NpeWorld *world) {
    if(!world || world->battle_lost || npe_world_battle_prepare_group(world) ||
       !world->battle_pending_mask)return -1;
    world->battle_action_ready=1;
    world->battle_actor=0; /*2F:C63C, after group preparation. */
    world->battle_menu_phase=(uint8_t)(world->battle_menu_phase/10u*10u);
    return world_run_round(world);
}

int npe_world_battle_combine_attack(NpeWorld *world) {
    unsigned members = 0, living = 0, last_member = 0;
    uint16_t attack_sum = 0;
    int damage = 0;
    if (!world || world->game_mode != 3 || world->battle_lost ||
        !world->battle_enemy_count) return -1;
    for (unsigned member = 0; member < 3; ++member) {
        if (!world->party_active[member]) continue;
        ++members;
        last_member = member;
        living += (int16_t)world->hp[member] > 0;
        /* Source2D:4CC6..4D2E includes every in-party attack word,
           including fallen and sleeping records, retaining each low sum. */
        attack_sum = (uint16_t)(attack_sum + world_battle_stat(
            world->attack[member], world->battle_skill_attack_bonus[member]));
    }
    if (!world->battle_executing) {
        if (living < 2) return -1;
        if(npe_world_battle_prepare_combine(world))return -1;
        world->battle_action_ready=1; /*2F:C6BC confirms type3. */
        world->battle_actor=0; /*2F:C6D4, before the physical executor. */
        world->battle_menu_phase=10; /*2F:C6C8, before entering the executor. */
        return world_run_round(world);
    }
    if (!members || npe_world_battle_action_actor(world) >= 3) return -1;
    /*39:5FC0..5FEE uses the first physical command byte. This cursor is
      also consumed by the victory refund loop; it is not the menu actor. */
    if(world->battle_native_kinds[0]==3 && world->battle_execution_slot<=members)
        world->battle_execution_slot=(uint8_t)members;
    if(world->battle_animation_observer)
        world->battle_animation_observer(world->battle_animation_context,world,
            NPE_BATTLE_ANIMATION_COMBINE,world->battle_target,NPE_BATTLE_ANIMATION_SETTLE);
    /* Source31:78DA..7A52 moves each positive hostile group target left
       two pixels and clears its 40x50 inclusive animation rectangle.
       Restoring the actor does not restore the erased scenery. */
    for (unsigned enemy = 0; enemy < world->battle_enemy_count; ++enemy)
        if ((int16_t)world->battle_enemy_hps[enemy] > 0)
            world->battle_enemy_shifted_mask |= (uint8_t)(1u << enemy);
    if ((int16_t)attack_sum > 999) attack_sum = 999;
    for (unsigned enemy = 0; enemy < world->battle_enemy_count; ++enemy) {
        int difference = (int16_t)(uint16_t)(attack_sum - world->battle_enemy_defenses[enemy]);
        damage = difference / 3;
        int floor = world->level[last_member] / 5 + 4;
        /* Source2D:4D56..4E5E divides the low difference by three, then
           uses the last party record's level floor and the same variance.
           No attribute multiplier or final 999 damage cap is applied. */
        if (damage <= floor) damage = floor;
        damage += (int)world->battle_variance_roll - 3;
        world->battle_enemy_damage_values[enemy] = (uint16_t)damage;
    }
    /* Float text and pose restoration see the records before group HP
       commit. A newly killed enemy is still drawn throughout this phase. */
    if (world->battle_animation_observer)
        world->battle_animation_observer(world->battle_animation_context,
            world, NPE_BATTLE_ANIMATION_COMBINE, world->battle_target,
            NPE_BATTLE_ANIMATION_DAMAGE);
    /* The original group commit consumes shared word7926, left holding
       the last physical enemy's prepared value, for every enemy slot. */
    for (unsigned enemy = 0; enemy < world->battle_enemy_count; ++enemy)
        world->battle_enemy_hps[enemy] = world_subtract_scalar(
            world->battle_enemy_hps[enemy], (uint16_t)damage);
    return world_battle_damage_enemy(world, world->battle_target, 0);
}

int npe_world_battle_defend(NpeWorld *world) {
    unsigned defense;
    if (!world || world->game_mode != 3 || world->battle_lost ||
        !world->battle_enemy_count || npe_world_battle_action_actor(world) >= 3 ||
        !world->party_active[npe_world_battle_action_actor(world)] ||
        !world->hp[npe_world_battle_action_actor(world)]) return -1;
    if (!world->battle_executing)
        return world_queue_command(world, COMMAND_DEFEND, npe_world_battle_action_actor(world), 0);
    /* Action 5, source 2D:4F44..507A, increases the acting battle
       record's defense by 100 and caps that value at 999. */
    defense = world_positive_stat_effect(world->defense[npe_world_battle_action_actor(world)], 100);
    world->battle_party_defenses[npe_world_battle_action_actor(world)] = (uint16_t)defense;
    /* The native action writes base defense +100 to the current defense
       word, replacing any earlier spell defense effect. */
    world->battle_skill_defense_bonus[npe_world_battle_action_actor(world)] = 0;
    /* Source 2D:5036..507A writes two to this actor's defense counter.
       The round-end update consumes one before the next party input. */
    world->battle_defense_turns[npe_world_battle_action_actor(world)] = 2;
    return world_finish_party_action(world);
}

int npe_world_battle_flee(NpeWorld *world) {
    if (!world || world->game_mode != 3 || world->battle_lost ||
        world->battle_actor >= 3 ||
        !world->party_active[world->battle_actor] ||
        !world->hp[world->battle_actor]) return -1;
    /* Source 4E:8BCC marks scripted battle with 7A9A = 1. Action 6 then
       branches at 2F:B9E0 to 2F:C256, with no random draw or mode-1
       write; the failed branch replaces party commands with attacks. */
    if (world->story_event_id) return world_failed_flee_round(world,0);
    /* Source 2F:C11C calls the reward service while exiting, but
       44:6B08 gives no reward without the victory flag 7AC8 == 0x3A.
       Source 2F:B9D0 checks action 6 and 7A9A == 0 (random battle).
       2F:B9E4..B9FA draws random % 100: <10 takes the failed branch,
       while >=10 reaches 2F:C14A, which writes mode 1. The original
       native generator is used with a host-selected starting state. */
    if (world_random15(world) % 100u < 10u)
        return world_failed_flee_round(world,1);
    if (world->battle_animation_observer)
        world->battle_animation_observer(world->battle_animation_context, world,
            NPE_BATTLE_ANIMATION_FLEE, 0, NPE_BATTLE_ANIMATION_BEGIN);
    /*2F:C0DC..C104 releases after the fleeing actors have been drawn. */
    if(npe_world_battle_resources(world->battle_file,world,0))return -1;
    world->battle_action_ready=0; /*2F:C13E. */
    /* Source2F:C12E/C132 clears the LCD before mode1 redraws seven
       tile rows. The four-row margins retain white (palette index0). */
    world->map_margin_color = 0;
    world->game_mode = 1;
    world->battle_enemy_count = 0;
    world->battle_enemy_id = 0;
    world->battle_enemy_hp = 0;
    world->battle_target = 0;
    world->battle_actor = 0;
    world_finish_battle_state(world, 0);
    return NPE_BATTLE_FLED;
}

int npe_world_battle_move_icon(const NpeWorld *world, unsigned icon, unsigned direction) {
    unsigned candidate = icon, living = 0;
    if (!world || world->game_mode != 3 || icon >= 4 || direction >= 4) return -1;
    if (direction == 3 && (icon & 1u)) --candidate;
    else if (direction == 1 && !(icon & 1u)) ++candidate;
    else if (direction == 0 && icon >= 2) candidate -= 2;
    else if (direction == 2 && icon < 2) candidate += 2;
    if (candidate == 3) {
        /* Native base-icon navigation does not wrap at an edge. The
           fourth icon needs two living members; sleep does not disable it. */
        for (unsigned i = 0; i < 3; ++i)
            living += world->party_active[i] && (int16_t)world->hp[i] > 0;
        if (living < 2) candidate = icon;
    }
    return (int)candidate;
}

int npe_world_battle_cycle_target(NpeWorld *world, int direction) {
    unsigned step, target;
    if (!world || world->game_mode != 3 || !world->battle_enemy_count ||
        (direction != -1 && direction != 1)) return -1;
    target = world->battle_target;
    for (step = 0; step < world->battle_enemy_count; ++step) {
        target = direction > 0 ?
                 (target + 1) % world->battle_enemy_count :
                 (target + world->battle_enemy_count - 1) %
                 world->battle_enemy_count;
        /* Target UI/source40:1944 and1BDE requires signed-positive HP.
           Execution retarget40:176E deliberately tests only zero. */
        if ((int16_t)world->battle_enemy_hps[target] > 0) {
            world->battle_target = (uint8_t)target;
            if(!world->battle_executing && world->battle_actor<3)
                (void)npe_world_battle_select_target(world,target,0);
            world->battle_display_target=world_native_target(world,target,0);
            world->battle_enemy_id = world->battle_enemy_ids[target];
            world->battle_enemy_hp = world->battle_enemy_hps[target];
            return (int)target;
        }
    }
    return -1;
}

static int world_battle_damage_enemy(NpeWorld *world, unsigned target,
                                     unsigned damage) {
    unsigned member, living = 0, reward_experience = 0;
    world->battle_target = (uint8_t)target;
    /* Source2A:107A..1166 permits nonzero negative targets. Apply a
       signed-positive effect, subtract its low word, then clamp negative. */
    if (damage > 0)
        world->battle_enemy_hps[target] = world_subtract_scalar(
            world->battle_enemy_hps[target], (uint16_t)damage);
    world->battle_enemy_hp = world->battle_enemy_hps[target];
    for (member = 0; member < world->battle_enemy_count; ++member)
        living += world->battle_enemy_hps[member] != 0;
    if (!living) {
        world->battle_menu_phase=58; /*33:C128 victory stage, retained on exit. */
        /*33:C13E..C23A returns the resident item IDs from 7ACC-1 to
          the last party record before rewards, including the killing
          actor. 39:5D12 makes 7ACC one-based; combined attack replaces
          it with the party count at5FEE. This loop is by record index,
          not by which queued actions have already executed. */
        unsigned first=(uint8_t)(world->battle_execution_slot-1);
        for(unsigned i=first;i<3;++i)if(world->party_active[i]){
            world_refund_battle_item(world,world->battle_item_ids[i]);
        }
        /*33:C23E..C266 follows refunds and precedes reward displays. */
        if(npe_world_battle_resources(world->battle_file,world,0))return -1;
        /* Source33:C27E enters the reward display after the defeated
           enemy scene is cleared, while all battle records still exist. */
        if(world->battle_animation_observer)
            world->battle_animation_observer(world->battle_animation_context,world,
                NPE_BATTLE_ANIMATION_VICTORY,target,NPE_BATTLE_ANIMATION_BEGIN);
        uint64_t total_gold = world->money;
        for (member = 0; member < world->battle_enemy_count; ++member) {
            total_gold += world->battle_enemy_gold_rewards[member];
            reward_experience +=
                world->battle_enemy_experience_rewards[member];
        }
        /* Victory settlement source 44:6DA6..6E00 caps 52:799E/79A0
           at 0x000F423F. */
        world->money = (uint32_t)(total_gold > 999999u ? 999999u :
                                  total_gold);
        /* Source 44:6CDA..6D92 awards the battle experience to every
           surviving participant, in party record order. */
        for (member = 0; member < 3; ++member) {
            unsigned total;
            if (!world->party_active[member] || (int16_t)world->hp[member] <= 0) continue;
            total = world->experience[member] + reward_experience;
            /* Source 44:6D5E..6D78 uses one 16-bit ADD and stores the
               low word; experience wraps rather than saturating. */
            world->experience[member] = (uint16_t)total;
        }
        /* Victory follow-up 1C:40AE (source 39:40AE) visits each enemy
           record. 64CA is the first one-based item ID; zero skips the
           whole enemy. 64CB is a second candidate. The source sums 64AA
           (third secondary stat) from the initial positive-HP party
           prefix, draws once
           per eligible enemy, and performs 16-bit UDIV after UMUL. */
        {
            unsigned party_stat = 0;
            uint8_t awarded[3] = {0};
            memset(world->battle_drop_names,0,sizeof(world->battle_drop_names));
            /* Source39:41D4..4218 tests the next record before 417A.
               The loop ends at the first nonpositive HP; it does not
               skip that record and continue summing later survivors. */
            for (member = 0; member < 3; ++member) {
                if (!world->party_active[member]) continue;
                if ((int16_t)world->hp[member] <= 0) break;
                party_stat = (uint16_t)(party_stat + world->other_stats[member][2]);
            }
            for (member = 0; member < world->battle_enemy_count; ++member) {
                unsigned roll, adjusted, slot;
                if (!world->battle_enemy_drop_ids[member][0]) continue;
                roll = world_random15(world) % 100u;
                adjusted = (uint16_t)(roll *
                    (uint16_t)(5000u - party_stat)) / 5000u;
                for (slot = 0; slot < 2; ++slot) {
                    unsigned id = world->battle_enemy_drop_ids[member][slot];
                    if ((slot == 0 ? adjusted >= 30u : adjusted >= 60u) &&
                        !world->story_event_id) continue;
                    if (id) {
                        NpeItem item;
                        if(npe_world_item(world->battle_file,world,id,&item))return -1;
                        memcpy(world->battle_drop_names[member][slot],item.name,item.name_size);
                        uint16_t count = (uint16_t)(npe_world_inventory_word(world,id) + 1u);
                        npe_world_set_inventory_word(world,id,(int16_t)count>99?99:count);
                        awarded[member] |= (uint8_t)(1u << slot);
                    }
                }
            }
            /* Source39:4718..4890 displays collected names only after
               every enemy's probability and inventory update. */
            if(world->battle_animation_observer)
                for(unsigned enemy=0;enemy<world->battle_enemy_count;++enemy)
                    for(unsigned slot=0;slot<2;++slot)if(awarded[enemy]&(1u<<slot))
                        world->battle_animation_observer(world->battle_animation_context,world,
                            NPE_BATTLE_ANIMATION_DROP,enemy*2+slot,NPE_BATTLE_ANIMATION_BEGIN);
        }
        /* Victory follow-up 33:C27E calls runtime 12:A4BC (source
           2F:A4BC). The 2F:C11C reward call is in the successful flee
           path; reward source 44:6B08 gates that call on 7AC8 == 0x3A.
           The level-up routine tests each
           surviving participant at 2F:A4F4/B7AE. It may level more than
           once, subtracting the old threshold before setting the new one
           from the incremented level at 2F:A8B8..A900. */
        unsigned level_panel = 0;
        for (member = 0; member < 3; ++member) {
            if (!world->party_active[member] || (int16_t)world->hp[member] <= 0) continue;
            if(world->level[member]>=99)continue;
            while (world->experience_to_next[member] &&
                   world->experience[member] >=
                       world->experience_to_next[member]) {
                unsigned level, stat;
                world->experience[member] = (uint16_t)(
                    world->experience[member] -
                    world->experience_to_next[member]);
                /* Source2F:A562 subtracts XP before A61C's level cap.
                   A participant reaching99 in this loop can consume
                   further thresholds without displaying another level. */
                if(world->level[member]>=99)continue;
                unsigned display_target=member|(level_panel<<8);
                if(world->battle_animation_observer)
                    world->battle_animation_observer(world->battle_animation_context,world,
                        NPE_BATTLE_ANIMATION_LEVEL,display_target,NPE_BATTLE_ANIMATION_BEGIN);
                level = ++world->level[member];
                world->damage_floor_stat[member] = (uint8_t)level;
                world->experience_to_next[member] = (uint16_t)(
                    3u * (level * level + 5u * level) - 8u);
                stat = world->max_hp[member] + world->growth[member][0];
                world->max_hp[member] = (int16_t)(uint16_t)stat > 999 ? 999 : (uint16_t)stat;
                world->hp[member] = world->max_hp[member];
                stat = world->max_qi[member] + world->growth[member][1];
                world->max_qi[member] = (int16_t)(uint16_t)stat > 999 ? 999 : (uint16_t)stat;
                world->qi[member] = world->max_qi[member];
                stat = world->attack[member] + world->growth[member][2];
                world->attack[member] = (int16_t)(uint16_t)stat > 999 ? 999 : (uint16_t)stat;
                stat = world->defense[member] + world->growth[member][3];
                world->defense[member] = (int16_t)(uint16_t)stat > 999 ? 999 : (uint16_t)stat;
                for (unsigned index = 0; index < 3; ++index) {
                    stat = world->other_stats[member][index] +
                           world->growth[member][index + 4];
                    world->other_stats[member][index] =
                        (int16_t)(uint16_t)stat > 999 ? 999 : (uint16_t)stat;
                }
                if(world->battle_animation_observer){
                    world->battle_animation_observer(world->battle_animation_context,world,
                        NPE_BATTLE_ANIMATION_LEVEL,display_target,NPE_BATTLE_ANIMATION_DAMAGE);
                }
                /*43:C214 loads before testing the prefix bound, including
                  zero IDs. C36A loads the displayed ID again; C41A loads
                  the next ID even when its level ends the display loop. */
                uint8_t index=0;unsigned id;NpeSkill skill;
                for(;;){
                    id=world_learning_record_byte(world,member,index);
                    if(npe_world_skill(world->battle_file,world,id,&skill))return -1;
                    if(index++>=18 || skill.required_level>=world->level[member])break;
                }
                uint8_t first=(uint8_t)(index-1);
                while(skill.required_level==world->level[member]){
                    if(npe_world_skill(world->battle_file,world,id,&skill))return -1;
                    if(world->battle_animation_observer)
                        world->battle_animation_observer(world->battle_animation_context,world,
                            NPE_BATTLE_ANIMATION_LEARN,id|(member<<8)|((unsigned)(uint16_t)((int)(uint8_t)(index-1)-(int)first)<<16),NPE_BATTLE_ANIMATION_BEGIN);
                    id=world_learning_record_byte(world,member,index);
                    ++index;
                    if(npe_world_skill(world->battle_file,world,id,&skill))return -1;
                }
            }
            ++level_panel;
        }
        if(world->story_event_id){
            NpeMapBattleEvent event;
            if(npe_world_battle_event(world->battle_file,world,world->story_event_id,&event)!=1)return -1;
            world->battle_win_mode=event.win_mode;world->battle_win_event_id=event.win_event_id;
        }
        world->battle_execution_slot=(uint8_t)(world_record_party_count(world)+world->battle_enemy_count);
        world->battle_action_ready=0; /*33:C4F2/C4FE, after the reward panels. */
        world->map_init_pending=1; /*33:C58C..C594 requests native mode initialization. */
        world->game_mode = world->story_event_id ?
                           world->battle_win_mode : 1;
        /* Source 33:C59C..C5DE pauses the contacted chase slot for five
           updates after an event-zero victory. */
        if (!world->story_event_id && world->battle_chase_slot &&
            world->battle_chase_slot <= 2)
            world->chase_cooldown[world->battle_chase_slot - 1] = 5;
        world->battle_chase_slot = 0;
        if (world->story_event_id)
            world->story_event_id = world->battle_win_event_id;
        world->battle_enemy_count = 0;
        world->battle_enemy_id = 0;
        world->battle_target = 0;
        world->battle_actor = 0;
        world_reset_victory_records(world);
        world->battle_display_target=0;
        /* 33:C2E4..C328 runs after experience/level-up. Signed
           nonpositive HP is restored to one without granting fallen XP. */
        for (member = 0; member < 3; ++member) {
            if (!world->party_active[member]) continue;
            if ((int16_t)world->hp[member] <= 0) world->hp[member] = 1;
            else {
                /* Source33:C32E..C4B4 computes signed maxHP/10 and
                   stat/(stat+20) separately, multiplies the low word,
                   adds to HP, then only applies a signed maximum clamp. */
                uint16_t stat = world->other_stats[member][0];
                int maximum_part = world_divide_signed_word(world->max_hp[member], 10);
                int factor = world_divide_signed_word(stat, (uint16_t)(stat + 20));
                uint16_t value = (uint16_t)(world->hp[member] +
                    (uint16_t)(maximum_part * factor));
                world->hp[member] = (int16_t)value > (int16_t)world->max_hp[member] ?
                                    world->max_hp[member] : value;
            }
        }
        world_finish_battle_state(world, 1);
        if (world->game_mode == 14)
            npe_world_apply_story_event(world, world->story_event_id);
        else
            world_resolve_simple_mode(world);
        if (!world->game_mode) world->game_mode = 1;
        return NPE_BATTLE_WON;
    }
    if (world_finish_party_action(world) == NPE_BATTLE_LOST)
        return NPE_BATTLE_LOST;
    if (!world->battle_enemy_hps[target])
        npe_world_battle_cycle_target(world, 1);
    return NPE_BATTLE_CONTINUE;
}

int npe_world_battle_attack(NpeWorld *world, unsigned target) {
    unsigned damage;
    if (!world || world->game_mode != 3 || world->battle_lost ||
        !world->battle_enemy_count || target >= world->battle_enemy_count ||
        !world->battle_enemy_hps[target] || npe_world_battle_action_actor(world) >= 3 ||
        !world->party_active[npe_world_battle_action_actor(world)] ||
        !world->hp[npe_world_battle_action_actor(world)]) return -1;
    if (!world->battle_executing)
        return world_queue_command(world, COMMAND_ATTACK, target, 0);
    damage = world_normal_hit(world,
                              world_battle_stat(
                                  world->attack[npe_world_battle_action_actor(world)],
                                  world->battle_skill_attack_bonus[
                                      npe_world_battle_action_actor(world)]),
                              world->battle_enemy_defenses[target],
                              world->level[npe_world_battle_action_actor(world)],
                              world->other_stats[npe_world_battle_action_actor(world)][2],
                              world->battle_enemy_lucks[target]);
    memset(world->battle_enemy_damage_values,0,sizeof(world->battle_enemy_damage_values));
    /*40:1B44 supplies647C to64D3. Nonzero numeric dispatch applies
      one prepared7926 to all hostile records; visual dispatch tests1. */
    unsigned group=world->battle_item_groups[npe_world_battle_action_actor(world)]!=0;
    unsigned visual_group=world->battle_item_groups[npe_world_battle_action_actor(world)]==1;
    /*2D:3A58..3ADA clears six words at64B6 in the acting record;
      3B28..3BA8 writes the selected physical target, while3BF8..3C9E
      fills the enemy prefix for group1. Prepare before the first pose. */
    uint8_t prepared[12]={0};
    unsigned party_count=world_record_party_count(world);
    for(unsigned enemy=0;enemy<world->battle_enemy_count&&party_count+enemy<6;++enemy)
        if(visual_group || (!group&&enemy==target))
            record_word(prepared,2*(party_count+enemy),damage);
    (void)npe_world_record_write(world,1,npe_world_battle_action_actor(world),
                               102,prepared,sizeof(prepared));
    for(unsigned enemy=0;enemy<world->battle_enemy_count;++enemy)
        if(visual_group || (!group&&enemy==target))
            world->battle_enemy_damage_values[enemy]=(uint16_t)damage;
    if(world->battle_animation_observer){
        world->battle_animation_observer(world->battle_animation_context,world,
            NPE_BATTLE_ANIMATION_ATTACK,target,NPE_BATTLE_ANIMATION_BEGIN);
        world->battle_animation_observer(world->battle_animation_context,world,
            NPE_BATTLE_ANIMATION_ATTACK,target,NPE_BATTLE_ANIMATION_DAMAGE);
    }
    for(unsigned enemy=0;enemy<world->battle_enemy_count;++enemy)
        if((visual_group||enemy==target)&&(int16_t)world->battle_enemy_hps[enemy]>0)
            world->battle_enemy_shifted_mask|=(uint8_t)(1u<<enemy);
    if(group){
        for(unsigned enemy=0;enemy<world->battle_enemy_count;++enemy)
            world->battle_enemy_hps[enemy]=world_subtract_scalar(world->battle_enemy_hps[enemy],(uint16_t)damage);
        return world_battle_damage_enemy(world,target,0);
    }
    return world_battle_damage_enemy(world, target, damage);
}

static int world_reserve_battle_item(const NpeFile *file, NpeWorld *world,
                                     unsigned item_id, unsigned side) {
    NpeItem item;
    uint16_t quantity=npe_world_inventory_word(world,item_id);
    if (!file || !world || world->game_mode != 3 || world->battle_lost ||
        world->battle_executing || !world->battle_enemy_count || world->battle_actor >= 3 ||
        !world_party_can_act(world, world->battle_actor) || !item_id ||
        item_id>255 ||
        world->battle_item_reservations[world->battle_actor] ||
        (item_id != 255 && (int16_t)quantity <= 0) ||
        npe_world_item(file, world, item_id, &item)) return -1;
    /* Source38:47E0..4850 rejects the wrong side, not an effect type.
       Both use/throw modes display the same complete inventory list. */
    if (item.target_side != side) return -2;
    if (side == 1 && item.effect_type >= 16 && item.effect_type <= 19 && item.group) {
        unsigned fallen = 0;
        for (unsigned i = 0; i < 3; ++i)
            fallen += world->party_active[i] && (int16_t)world->hp[i] <= 0;
        if (!fallen) return -2;
    }
    unsigned actor=world->battle_actor;
    world->battle_commands[actor]=(NpeBattleCommand){
        (uint8_t)(side?COMMAND_ITEM:COMMAND_THROW),(uint8_t)actor,(uint8_t)item_id};
    world->battle_native_kinds[actor]=world->battle_menu_previous_kind=2;
    world->battle_item_types[actor]=item.effect_type;
    /*38:49DA..49FC clears64D0 for an item;64D6 is untouched. */
    world->battle_magic_types[actor]=0;
    world->battle_item_type_copies[actor]=item.effect_type;
    world->battle_item_ids[actor]=(uint8_t)item_id;
    world->battle_item_converted_mask&=(uint8_t)~(1u<<actor);
    world->battle_item_groups[actor]=item.group;world->battle_item_sides[actor]=item.target_side;
    (void)npe_world_battle_select_target(world,side?actor:0,side!=0);
    world->battle_display_target=world->battle_native_targets[actor]; /*38:4C50. */
    world->battle_selection_item_type=world->battle_item_types[actor]; /*38:4CE6. */
    npe_world_set_inventory_word(world,item_id,(uint16_t)(quantity-1u));
    world->battle_item_reservations[world->battle_actor] = (uint8_t)item_id;
    return 0;
}

int npe_world_battle_reserve_throw_item(const NpeFile *file, NpeWorld *world,
                                        unsigned item_id) {
    return world_reserve_battle_item(file, world, item_id, 0);
}

int npe_world_battle_reserve_use_item(const NpeFile *file, NpeWorld *world,
                                      unsigned item_id) {
    return world_reserve_battle_item(file, world, item_id, 1);
}

int npe_world_battle_throw_item(const NpeFile *file, NpeWorld *world,
                                unsigned item_id, unsigned target) {
    NpeItem item;uint8_t raw[41];
    uint16_t quantity=npe_world_inventory_word(world,item_id);
    if (!file || !world || world->game_mode != 3 || world->battle_lost ||
        !world->battle_enemy_count || target >= world->battle_enemy_count ||
        !world->battle_enemy_hps[target] || npe_world_battle_action_actor(world) >= 3 ||
        !world->party_active[npe_world_battle_action_actor(world)] ||
        !world->hp[npe_world_battle_action_actor(world)] || !item_id ||
        item_id>255 ||
        (!world->battle_executing && !quantity && item_id != 255 &&
         world->battle_item_reservations[npe_world_battle_action_actor(world)] != item_id) ||
        npe_world_item(file, world, item_id, &item)) return -1;
    if(world->battle_executing && npe_world_battle_item_view(world,npe_world_battle_action_actor(world),raw,&item))return -1;
    if (item.target_side != 0) return -2;
    if (!world->battle_executing) {
        unsigned reserved = world->battle_item_reservations[npe_world_battle_action_actor(world)];
        if (reserved && reserved != item_id) return -1;
        if (!reserved && npe_world_battle_reserve_throw_item(file, world, item_id)) return -1;
        world->battle_file = file;
        return world_queue_command(world, COMMAND_THROW, target, item_id);
    }
    /* The native selection already debited this item. Round execution
       consumes its reservation even when the last inventory word is zero. */
    if (world->battle_item_reservations[npe_world_battle_action_actor(world)] == item_id)
        world->battle_item_reservations[npe_world_battle_action_actor(world)] = 0;
    uint16_t display_hp=item.effect_type==31?(uint16_t)item.attack_bonus:item.hp_restore;
    int16_t display_attack=item.effect_type==31?0:item.attack_bonus;
    unsigned display_value=item.effect_type==2?1:
        (item.effect_type!=1 && item.effect_type!=31)?0:
        item.defense_bonus?(uint16_t)item.defense_bonus:
        display_attack?(uint16_t)display_attack:
        item.qi_restore?item.qi_restore:display_hp;
    memset(world->battle_party_damage_values,0,sizeof(world->battle_party_damage_values));
    memset(world->battle_enemy_damage_values,0,sizeof(world->battle_enemy_damage_values));
    for(unsigned enemy=0;enemy<world->battle_enemy_count;++enemy)
        if(item.group==1 || enemy==target)
            world->battle_enemy_damage_values[enemy]=(uint16_t)display_value;
    /* Use and throw share native action2 and the character's casting
       group; item effects commit after the three floating positions. */
    if(world->battle_animation_observer){
        world->battle_animation_observer(world->battle_animation_context,world,
            NPE_BATTLE_ANIMATION_ITEM,target,NPE_BATTLE_ANIMATION_BEGIN);
    }
    /* Casting finishes before2D:45B6/4606 changes the resident types.
      This also runs when no renderer observes the action. */
    if(item.effect_type==31){
        world->battle_item_types[npe_world_battle_action_actor(world)]=1;
        world->battle_item_type_copies[npe_world_battle_action_actor(world)]=1;
        world->battle_item_converted_mask|=(uint8_t)(1u<<npe_world_battle_action_actor(world));
    }
    world_prepare_effect_record(world,npe_world_battle_action_actor(world),
        target,0,item.group,(item.effect_type==1||item.effect_type==31)?display_value:0,
        1,item.effect_type==1||item.effect_type==31);
    if(world->battle_animation_observer){
        world->battle_animation_observer(world->battle_animation_context,world,
            NPE_BATTLE_ANIMATION_ITEM,target,NPE_BATTLE_ANIMATION_DAMAGE);
    }
    /* Source 2D:4584..4644 routes type 31 to an enemy and replaces the
       HP effect word at 52:7926 with the item's +17 attack word. Source
       2A:14D0..15C0 subtracts positive 7926 from current HP at 64A0
       and clamps negative results to zero. Source 2D:47F6..49E0 adds
       the signed status words at 7420 onward to the target record at
       648E onward. Type1/31 then use the four signed-positive scalar
       services, including independent enemy attack/defense counters. */
    if (item.effect_type == 2) {
        world_apply_battle_status_effects(world, 0, item.group == 1, target, item.status_effects);
        return world_finish_party_action(world);
    }
    if (item.effect_type == 1 || item.effect_type == 31) {
        unsigned last = target;
        for (unsigned enemy = 0; enemy < world->battle_enemy_count; ++enemy) {
            if (item.group != 1 && enemy != target) continue;
            world_apply_enemy_scalar_effects(world, enemy,
                item.effect_type == 31 ? (uint16_t)item.attack_bonus : item.hp_restore,
                item.qi_restore, item.effect_type == 31 ? 0 : (uint16_t)item.attack_bonus,
                (uint16_t)item.defense_bonus);
            last = enemy;
        }
        return world_battle_damage_enemy(world, last, 0);
    }
    return world_finish_party_action(world);
}

int npe_world_battle_use_item(const NpeFile *file, NpeWorld *world,
                              unsigned item_id, unsigned character) {
    NpeItem item;uint8_t raw[41];
    uint16_t quantity=npe_world_inventory_word(world,item_id);
    if (!file || !world || world->game_mode != 3 || !world->battle_enemy_count ||
        world->battle_lost || npe_world_battle_action_actor(world) >= 3 ||
        !world->party_active[npe_world_battle_action_actor(world)] ||
        !world->hp[npe_world_battle_action_actor(world)] || !item_id || item_id>255 ||
        character >= 3 || !world->party_active[character] ||
        (!world->battle_executing && !quantity && item_id != 255 &&
         world->battle_item_reservations[npe_world_battle_action_actor(world)] != item_id) ||
        npe_world_item(file, world, item_id, &item)) return -1;
    if(world->battle_executing && npe_world_battle_item_view(world,npe_world_battle_action_actor(world),raw,&item))return -1;
    if (item.target_side != 1) return -2;
    unsigned revival = item.effect_type >= 16 && item.effect_type <= 19;
    if (!world->battle_executing) {
        unsigned reserved = world->battle_item_reservations[npe_world_battle_action_actor(world)];
        if (reserved && reserved != item_id) return -1;
        if (!reserved) {
            int result = npe_world_battle_reserve_use_item(file, world, item_id);
            if (result) return result;
        }
        world->battle_file = file;
        return world_queue_command(world, COMMAND_ITEM, character, item_id);
    }
    if (world->battle_item_reservations[npe_world_battle_action_actor(world)] == item_id)
        world->battle_item_reservations[npe_world_battle_action_actor(world)] = 0;
    if (!item.group && !revival && (int16_t)world->hp[character] <= 0)
        return world_finish_party_action(world);
    unsigned display_value=item.effect_type==2 || revival?1:item.effect_type!=1?0:
        item.defense_bonus?(uint16_t)item.defense_bonus:
        item.attack_bonus?(uint16_t)item.attack_bonus:
        item.qi_restore?item.qi_restore:item.hp_restore;
    memset(world->battle_party_damage_values,0,sizeof(world->battle_party_damage_values));
    memset(world->battle_enemy_damage_values,0,sizeof(world->battle_enemy_damage_values));
    for(unsigned member=0;member<3;++member)
        if(world->party_active[member] && (item.group==1 || member==character))
            world->battle_party_damage_values[member]=(uint16_t)display_value;
    /* Native action2 selects the current character's casting group at
       Source2F:D296..D532 and displays before committing item effects. */
    if(world->battle_animation_observer){
        world->battle_animation_observer(world->battle_animation_context,world,
            NPE_BATTLE_ANIMATION_ITEM,character,NPE_BATTLE_ANIMATION_BEGIN);
    }
    world_prepare_effect_record(world,npe_world_battle_action_actor(world),
        character,1,item.group,item.effect_type==1?display_value:0,1,item.effect_type==1);
    if(world->battle_animation_observer){
        world->battle_animation_observer(world->battle_animation_context,world,
            NPE_BATTLE_ANIMATION_ITEM,character,NPE_BATTLE_ANIMATION_DAMAGE);
    }
    if (item.effect_type == 2) {
        world_apply_battle_status_effects(world, 1, item.group == 1, character, item.status_effects);
        return world_finish_party_action(world);
    }
    /* Item action2 shares the source HP/effect services, but its flags
       are at +36/+37 rather than the skill record's +37/+38. */
    for (unsigned member = 0; member < 3; ++member) {
        if (!world->party_active[member] || (!item.group && member != character)) continue;
        if (revival) {
            if ((int16_t)world->hp[member] <= 0)
                world->hp[member] = npe_battle_revival_hp(item.effect_type, world->max_hp[member]);
            continue;
        }
        if (item.effect_type == 1)
            world_apply_party_scalar_effects(world, member, item.hp_restore, item.qi_restore,
                (uint16_t)item.attack_bonus, (uint16_t)item.defense_bonus);
    }
    return world_finish_party_action(world);
}

int npe_world_battle_cast_skill(const NpeFile *file, NpeWorld *world,
                                unsigned skill_id, unsigned target) {
    NpeSkill skill;
    if (!file || !skill_id || npe_skill(file, skill_id - 1, &skill)) return -1;
    if (skill.raw[14] != 1 || skill.raw[38] != 1 ||
        !(skill.parameter_a || skill.parameter_b)) return -2;
    return npe_world_battle_cast_magic(file, world, skill_id, target);
}

int npe_world_menu_cast_skill(const NpeFile *file, NpeWorld *world,
                              unsigned caster, unsigned target,
                              unsigned skill_id) {
    NpeSkill skill;
    if (!file || !world || world->game_mode != 6 ||
        caster >= 3 || target >= 3 || !world->party_active[caster] ||
        !world->party_active[target] || !skill_id ||
        npe_world_skill(file,world,skill_id,&skill)) return -1;
    /* Source 37:D1E4 gates map casting by file +38 (7384), not by ID,
       description or the unused +12 metadata. Source 30:9444 loads
       HP/QI only for magic; ATK/DEF/other stat deltas are zero here.
       Types other than1/2 fall through without an attribute effect here.
       Type 2 adds the four map state words without rebuilding equipment. */
    if (!skill.raw[38]) return -2;
    /*30:9670/9F1E applies attributes only for types1/2, then returns
      throughA106 for every other type. The caller still pays at37:D2E6
      orD4B6; it does not reject default types before the QI gate. */
    /* The list controller already applied the shared level gate. Do not
       reread resource levels here: same-ID resident metadata may differ. */
    unsigned known=0;for(unsigned i=0;i<18 && world->skill_slots[caster][i];++i)
        if(world->skill_slots[caster][i]==skill_id){known=1;break;}
    if(!known || world->level[caster]<skill.required_level)return -4;
    if ((int16_t)world->qi[caster] < (int16_t)skill.qi_cost) return -3;
    if (skill.raw[14] == 1) {
        uint16_t qi_gain = (uint16_t)((unsigned)skill.raw[17] << 8 | skill.raw[18]);
        for (unsigned member = 0; member < 3; ++member) {
            if (!world->party_active[member] ||
                (skill.raw[37] != 1 && member != target)) continue;
            uint16_t hp = (uint16_t)(world->hp[member] + skill.hp_effect);
            uint16_t qi = (uint16_t)(world->qi[member] + qi_gain);
            world->hp[member] = (int16_t)hp > (int16_t)world->max_hp[member] ? world->max_hp[member] : hp;
            world->qi[member] = (int16_t)qi > (int16_t)world->max_qi[member] ? world->max_qi[member] : qi;
        }
    }
    if (skill.raw[14] == 2) {
        if (skill.raw[37] == 1) {
            /* Source37:D280..D298 passes target -1 for group magic.
               Source30:9FEA..A102 uses the active member index for both
               the state offset and effect, repeating it four times.
               6A04 - 148 aliases inventory word 212, not a party record.
               Preserve this firmware behavior, including signed wraps. */
            for (unsigned member = 0; member < 3; ++member)
                if (world->party_active[member] == 1)
                    world->condition_words[212 + member] = (uint16_t)(
                        world->condition_words[212 + member] +
                        4u * (uint16_t)npe_skill_status_effect(&skill, member));
        } else {
            for (unsigned status = 0; status < 4; ++status)
                world->status_guard[target][status] = (uint16_t)(
                    world->status_guard[target][status] +
                    (uint16_t)npe_skill_status_effect(&skill, status));
        }
    }
    /* Source 37:D2E6/D4B6 subtracts cost after applying the target effect,
       so self/group QI recovery is capped before the caster pays. */
    world->qi[caster] = (uint16_t)(world->qi[caster] - skill.qi_cost);
    return 1;
}

int npe_world_battle_cast_heal_skill(const NpeFile *file, NpeWorld *world,
                                     unsigned skill_id, unsigned target) {
    NpeSkill skill;
    if (!file || !skill_id || npe_skill(file, skill_id - 1, &skill)) return -1;
    if (skill.raw[14] != 1 || skill.raw[38] != 1 || !skill.hp_effect ||
        skill.parameter_a || skill.parameter_b) return -2;
    return npe_world_battle_cast_magic(file, world, skill_id, target);
}

int npe_world_battle_cast_attack_skill(const NpeFile *file, NpeWorld *world,
                                       unsigned skill_id, unsigned target) {
    NpeSkill skill;
    if (!file || !skill_id || npe_skill(file, skill_id - 1, &skill)) return -1;
    if (skill.raw[14] != 1 || skill.raw[38] != 0) return -2;
    return npe_world_battle_cast_magic(file, world, skill_id, target);
}

int npe_world_battle_select_magic(const NpeFile *file,NpeWorld *world,unsigned id) {
    NpeSkill skill;unsigned actor;
    if(!file || !world || world->game_mode!=3 || (actor=world->battle_actor)>=3 ||
       !world->party_active[actor] || npe_world_skill(file,world,id,&skill))return -1;
    if(!id || !skill.required_level || skill.required_level>world->level[actor])return -4;
    if((int16_t)world->qi[actor]<(int16_t)skill.qi_cost)return -3;
    /*3A:07B4..08AC confirms the map list's selected record against
      battle level/QI, without rescanning the battle skill prefix. */
    world->battle_commands[actor]=(NpeBattleCommand){COMMAND_MAGIC,(uint8_t)actor,(uint16_t)id};
    /*3A:0A92..0AB6 writes the independent byte before the target selector. */
    world->battle_skill_ids[actor]=(uint8_t)id;
    world->battle_native_kinds[actor]=world->battle_menu_previous_kind=1;
    world->battle_magic_types[actor]=skill.raw[14];
    world->battle_magic_groups[actor]=skill.raw[37];
    world->battle_magic_sides[actor]=skill.raw[38];
    /*3A:0A2C copies the magic effect into64D2;0AE2 clears64D1.
      Shared command flags replace previous item metadata, not64D5. */
    world->battle_item_types[actor]=0;world->battle_item_type_copies[actor]=skill.raw[14];
    world->battle_item_groups[actor]=skill.raw[37];world->battle_item_sides[actor]=skill.raw[38];
    (void)npe_world_battle_select_target(world,skill.raw[38]?actor:0,skill.raw[38]!=0);
    world->battle_display_target=world->battle_native_targets[actor]; /*3A:0CFA. */
    return 1;
}

int npe_world_battle_cast_magic(const NpeFile *file, NpeWorld *world,
                                unsigned skill_id, unsigned target) {
    NpeSkill skill;
    uint8_t raw[42];
    unsigned actor, group, own_side, kind;
    if (!file || !world || world->game_mode != 3 || world->battle_lost ||
        !world->battle_enemy_count || npe_world_battle_action_actor(world) >= 3 ||
        !world->party_active[npe_world_battle_action_actor(world)] || !world->hp[npe_world_battle_action_actor(world)] ||
        !skill_id || npe_world_skill(file,world,skill_id,&skill)) return -1;
    if(world->battle_executing && world->battle_commands[npe_world_battle_action_actor(world)].kind==COMMAND_MAGIC &&
       npe_world_battle_magic_view(world,npe_world_battle_action_actor(world),raw,&skill))return -1;
    kind = (unsigned)npe_skill_battle_effect(&skill);
    if (!kind) return -2;
    own_side = skill.raw[38] == 1;
    group = skill.raw[37] == 1;
    if (own_side ? target >= 3 || !world->party_active[target] :
                   target >= world->battle_enemy_count)
        return -1;
    if (kind >= 16 && !group && !world->battle_executing) {
        /*2F:B8D0..B988 rejects a signed-positive selected record for
          single revival, regardless of side. Group spells skip this gate. */
        if ((int16_t)(own_side ? world->hp[target] : world->battle_enemy_hps[target]) > 0)
            return -1;
    }
    actor = npe_world_battle_action_actor(world);
    /* Native seal gate is at list entry, not an extra execution service:
       a queued spell can still execute if sealed later in the round. */
    if (!world->battle_executing && world->battle_party_status[actor][3] < 0) return -5;
    /* Source3A:0834..08AC compares signed QI/fee at selection only.
       Source2D:4488..44A4 subtracts the low word without rechecking:
       other actions may drain QI after this spell has been queued. */
    if (!world->battle_executing &&
        (int16_t)world->qi[actor] < (int16_t)skill.qi_cost) return -3;
    if (!world->battle_executing) {
        int selected=npe_world_battle_select_magic(file,world,skill_id);
        if(selected<0)return selected;
        world->battle_file = file;
        return world_queue_command(world, COMMAND_MAGIC, target, skill_id);
    }
    {
        /* Source35:EF88..F198 /F6B8..F96C selects the first nonzero
           DEF/ATK/QI/HP display word. Composite healing displays QI. */
        unsigned value=kind==NPE_BATTLE_EFFECT_ANIMATION_ONLY?0:kind==2 || kind>=16?1:skill.parameter_b?skill.parameter_b:
            skill.parameter_a?skill.parameter_a:skill.qi_effect?skill.qi_effect:own_side?skill.hp_effect:
            npe_battle_party_spell_damage(skill.hp_effect,
                world_battle_stat(world->attack[actor],world->battle_skill_attack_bonus[actor]));
        memset(world->battle_party_damage_values,0,sizeof(world->battle_party_damage_values));
        memset(world->battle_enemy_damage_values,0,sizeof(world->battle_enemy_damage_values));
        for(unsigned i=0;i<(own_side?3:world->battle_enemy_count);++i)
            if(group || i==target){
                if(own_side)world->battle_party_damage_values[i]=(uint16_t)value;
                else world->battle_enemy_damage_values[i]=(uint16_t)value;
            }
        /* Source39:5D76..605C displays both animation groups and three
           number positions before the scalar/status services commit HP/QI. */
        if(world->battle_animation_observer){
            world->battle_animation_observer(world->battle_animation_context,world,
                NPE_BATTLE_ANIMATION_PARTY_SPELL,target,NPE_BATTLE_ANIMATION_BEGIN);
        }
        world_prepare_effect_record(world,actor,target,own_side,skill.raw[37],
                                    kind==1?value:0,0,kind==1);
        if(world->battle_animation_observer){
            world->battle_animation_observer(world->battle_animation_context,world,
                NPE_BATTLE_ANIMATION_PARTY_SPELL,target,NPE_BATTLE_ANIMATION_DAMAGE);
        }
    }
    world->qi[actor] = (uint16_t)(world->qi[actor] - skill.qi_cost);
    if (kind == NPE_BATTLE_EFFECT_ANIMATION_ONLY)
        return world_finish_party_action(world);
    if (kind >= 16) {
        /* Source40:1562..15AA excludes types16..19 from dead-target
           replacement. Source2A:0C4C..0F4A revives fallen party records
           only, leaving QI, statuses, bonuses and acted flags untouched. */
        for (unsigned member = 0; own_side && member < 3; ++member)
            if (world->party_active[member] && (int16_t)world->hp[member] <= 0 &&
                (group || member == target))
                world->hp[member] = npe_battle_revival_hp(kind, world->max_hp[member]);
        return world_finish_party_action(world);
    }
    if (kind == 2) {
        int16_t effects[4];
        for (unsigned status = 0; status < 4; ++status)
            effects[status] = npe_skill_status_effect(&skill, status);
        world_apply_battle_status_effects(world, own_side, group, target, effects);
        return world_finish_party_action(world);
    }
    if (own_side) {
        for (unsigned member = 0; member < 3; ++member) {
            if (!world->party_active[member] || (!group && member != target)) continue;
            world_apply_party_scalar_effects(world, member, skill.hp_effect, skill.qi_effect,
                skill.parameter_a, skill.parameter_b);
        }
        return world_finish_party_action(world);
    }
    /* One HP effect, one QI charge, no per-target RNG. */
    unsigned damage = npe_battle_party_spell_damage(skill.hp_effect,
        world_battle_stat(world->attack[actor], world->battle_skill_attack_bonus[actor]));
    unsigned last = target;
    for (unsigned enemy = 0; enemy < world->battle_enemy_count; ++enemy) {
        /* Native group scalar services include fallen enemy records;
           their QI/stats change, but dead-record timers do not advance. */
        if (!group && enemy != target) continue;
        world_apply_enemy_scalar_effects(world, enemy, (uint16_t)damage, skill.qi_effect,
            skill.parameter_a, skill.parameter_b);
        last = enemy;
    }
    return world_battle_damage_enemy(world, last, 0);
}

static unsigned world_roll100(NpeWorld *world) {
    return world_random15(world) % 100u;
}

/* Source 39:B170..B5F2. Indices in 52:792E/7930/7932 are chosen before
   the source replaces them with IDs from 52:7916. The second and third
   choices can deliberately repeat an earlier enemy type. */
static void world_form_encounter(NpeWorld *world, unsigned eligible,
                                 unsigned indices[3], unsigned *formation) {
    unsigned roll, first, second = 0, third = 0, count;
    if (eligible == 1) {
        indices[0] = 0;
        *formation = 1;
        return;
    }
    roll = world_roll100(world);
    count = roll < 10 ? 3 : roll < 30 ? 2 : 1;
    first = world_roll100(world) % eligible;
    indices[0] = first;
    if (count >= 2) {
        roll = world_roll100(world);
        /* 39:B282..B294 divides the constant 100 by twice the eligible
           type count, then compares the fresh 0..99 roll with that limit. */
        if (roll < 100 / (2 * eligible)) {
            second = first;
        } else {
            second = roll % (eligible - 1);
            if (second >= first) second++;
        }
        indices[1] = second;
    }
    if (count >= 3) {
        roll = world_roll100(world);
        if (first == second) {
            /*39:B398..B3B8 multiplies3 by the formation count (3),
              not the number of eligible enemy types. */
            if (roll < 100 / (3 * count)) {
                third = first;
            } else {
                third = roll % 6;
                if (third >= first) third++;
            }
        } else if (roll < 7) {
            third = first;
        } else if (roll < 14) {
            third = second;
        } else {
            third = roll % 5;
            if (third >= first && third >= second) third += 2;
            else if (third >= first || third >= second) third++;
        }
        if (second >= eligible) second = world_roll100(world) % eligible;
        if (third >= eligible) third = world_roll100(world) % eligible;
        indices[1] = second;
        indices[2] = third;
    }
    *formation = count;
}

static int world_prepare_ordinary_battle(const NpeFile *file,NpeWorld *world) {
    unsigned formation;
    world->battle_scripted_entry=0;
    int selection=npe_world_select_battle_enemies(file,world);if(selection)return selection;
    formation=world->battle_formation_count;
    if(formation>3)return -2; /*Counts beyond the physical slots still need native RAM alias handling. */
    world_clear_enemy_spell_cache(world);
    /*4E:7F30..7F70 allocates and loads packed images on ordinary entry. */
    if(npe_world_battle_resources(file,world,1))return -1;
    int loaded=npe_world_load_battle_enemies(file,world);if(loaded)return loaded;
    world->battle_enemy_count = (uint8_t)formation;
    memcpy(world->battle_party_defenses, world->defense,
           sizeof(world->battle_party_defenses));
    memset(world->battle_defense_turns, 0,
           sizeof(world->battle_defense_turns));
    memset(world->battle_skill_attack_bonus, 0,
           sizeof(world->battle_skill_attack_bonus));
    memset(world->battle_skill_defense_bonus, 0,
           sizeof(world->battle_skill_defense_bonus));
    memset(world->battle_attack_turns, 0,
           sizeof(world->battle_attack_turns));
    world->battle_enemy_id = world->battle_enemy_ids[0];
    world->battle_enemy_hp = world->battle_enemy_hps[0];
    world->battle_target = 0;
    /* Source49:8DF0..8E10 starts the command cursor at the first record,
       even when that member's HP is zero. Entry does not heal fallen HP. */
    world->battle_actor = 0;
    world->battle_lost = 0;
    world->battle_win_mode = world->battle_lose_mode = 0;
    world->battle_win_event_id = world->battle_lose_event_id = 0;
    world->story_event_id = 0;
    world->game_mode = 3;
    world->battle_chase_slot = 0;
    world->battle_file = file;
    world->battle_pending_mask = world->battle_executing = 0;
    memset(world->battle_party_status, 0, sizeof(world->battle_party_status));
    memset(world->battle_commands, 0, sizeof(world->battle_commands));
    memset(world->battle_item_reservations, 0, sizeof(world->battle_item_reservations));
    int party_loaded=world_refresh_entry_equipment(file, world);if(party_loaded)return party_loaded;
    npe_world_sound_battle_start(world);
    return 1;
}

static int world_random_encounter(const NpeFile *file, NpeWorld *world,
                                  const NpeMap *map) {
    if (map->tail_size < 652) return 0;
    if(!world->encounters_enabled||world->exit_choice)return 0;
    /* Source37:17FE..18CE: counter7A02 rises after an eligible attempt,
       including collisions. The threshold comparison is signed16-bit;
       03:4124 supplies a random 16-bit value, divided by 1000. A remainder
       below the counter enters mode 3; reaching 1000 enters unconditionally. */
    if ((int16_t)world->encounter_steps < 1000) {
        world->encounter_steps++;
        if (world_random15(world) % 1000 >= world->encounter_steps) return 0;
    }
    world->encounter_steps = 0;
    /*37:185A..1886 /188E..18C6 enter mode3 before the entry service
      examines enemy IDs. Unsupported entry data must not erase that step. */
    world->game_mode=3;world->story_event_id=0;world->event_context_id=0;
    world->map_init_pending=1;
    return world_prepare_ordinary_battle(file,world);
}

static int chase_active(const NpeMapChase *chase, unsigned progress) {
    return chase->enemy_id &&
        ((!chase->min_progress && !chase->max_progress) ||
         (progress >= chase->min_progress && progress <= chase->max_progress));
}

static int world_npc_can_move(const NpeMap *map, const NpeWorld *world,
                               unsigned slot, unsigned x, unsigned y) {
    unsigned index = y * map->width + x;
    /* Source40:6E76..7040 compares all eight object positions, all
       other twenty NPC positions and all twelve exits. It does not filter
       these blockers by ID, collection flag or active story range. */
    for (unsigned i = 0; i < 8; ++i) {
        NpeMapObject object;
        if (npe_world_object(world, map, i, &object)) return 0;
        if (index == (unsigned)object.y * map->width + object.x) return 0;
    }
    for (unsigned i = 0; i < 20; ++i) if (i != slot) {
        const uint8_t *npc;
        unsigned nx, ny;
        if (npe_world_npc(world, map, i, &npc)) return 0;
        if (npe_world_npc_position(world, npc, i, &nx, &ny)) return 0;
        if (index == ny * map->width + nx) return 0;
    }
    for (unsigned i = 0; i < 12; ++i) {
        NpeMapExit exit;
        if (npe_world_exit(world, map, i, &exit)) return 0;
        if (index == (unsigned)exit.y * map->width + exit.x) return 0;
    }
    /* The native player collision uses the displayed position plus scroll,
       including the one-row offset possible at lower-edge spawns. */
    if (index == (world->view.offset_y + world->view.screen_y) * map->width +
                 world->view.offset_x + world->view.screen_x) return 0;
    /* Source40:70A0..70CC tests the flattened tile index against 11.
       Right-edge X==width can address the next row. Reject a read beyond
       the allocated tile grid rather than inventing native heap contents. */
    if (index >= (unsigned)map->width * map->height) return 0;
    return npe_map_tile(map, index % map->width, index / map->width) <= 11;
}

int npe_world_update_npcs(const NpeFile *file, NpeWorld *world) {
    NpeMap map;
    int changed = 0;
    if (!file || !world || !world->map_number ||
        npe_map(file, world->map_number - 1, &map) || map.tail_size < 592) return -1;
    world_bind_tiles(world, &map);
    /* Source42:8392..8778 processes all twenty slots in order, including
       off-screen/inactive NPCs, when ID!=0 and the moving byte equals 1. */
    for (unsigned slot = 0; slot < 20; ++slot) {
        const uint8_t *npc;
        unsigned x, y, nx, ny, direction, old_direction;
        if (npe_world_npc(world, &map, slot, &npc)) return -1;
        if ((!npc[0] && !npc[1]) || npc[7] != 1) continue;
        if (npe_world_npc_position(world, npc, slot, &x, &y)) return -1;
        old_direction = world->npc_override_direction[slot] <= 3 ?
                        world->npc_override_direction[slot] : npc[6];
        direction = world_random15(world) & 3u;
        nx = x; ny = y;
        if (direction == 0 && y < map.height) ++ny;
        else if (direction == 1 && x < map.width) ++nx;
        else if (direction == 2 && y > 0) --ny;
        else if (direction == 3 && x > 0) --nx;
        if (world_npc_can_move(&map, world, slot, nx, ny)) {
            world->npc_override_x[slot] = (uint8_t)nx;
            world->npc_override_y[slot] = (uint8_t)ny;
        } else {
            /* Source42:869A..874E repeatedly draws until the final facing
               differs from the attempted direction, without retrying a step. */
            unsigned blocked_direction = direction;
            do { direction = world_random15(world) & 3u; }
            while (direction == blocked_direction);
            nx = x; ny = y;
        }
        world->npc_override_direction[slot] = (uint8_t)direction;
        changed |= nx != x || ny != y || direction != old_direction;
    }
    return changed;
}

static int chase_touches_player(const NpeWorld *world, unsigned slot) {
    int cx=(int16_t)(world->chase_x[slot]-world->view.offset_x-1);
    int cy=(int16_t)(world->chase_y[slot]-world->view.offset_y-1);
    int hx=(int)world->view.screen_x,hy=(int)world->view.screen_y;
    /* Preserve the literal third branch at source3A:5B26..5B58. */
    return (cx==hx-2 && cy>=hy-2 && cy<=hy+2) ||
           (cx==hx+1 && cy>=hy-2 && cy<=hy+2) ||
           (cx==hx && (cy>=hy-1 || cx<=hy+1));
}

int npe_world_update_chase(const NpeFile *file, NpeWorld *world) {
    NpeMap map;
    unsigned slot;
    if (!file || !world || !world->map_number ||
        npe_map(file, world->map_number - 1, &map)) return -1;
    for (slot = 0; slot < 2; ++slot) {
        NpeMapChase chase;
        int found = npe_world_chase(world, &map, slot, &chase);
        int cx, cy, hx = (int)world->view.screen_x,
                    hy = (int)world->view.screen_y;
        if (found < 0) return -1;
        if (!found || !chase.enemy_id) return 0;
        if (!chase_active(&chase, world->story_progress)) continue;
        if (world->chase_cooldown[slot]) {
            --world->chase_cooldown[slot];
            return 0; /* 3A:5A10 returns before the second slot. */
        }
        cx = (int16_t)(world->chase_x[slot] - world->view.offset_x - 1);
        cy = (int16_t)(world->chase_y[slot] - world->view.offset_y - 1);
        /* Preserve the literal third branch at 3A:5B26..5B58: it jumps
           to contact on cy >= hy-1, otherwise compares cx with hy+1. */
        if (chase_touches_player(world,slot)) {
            world_restore_enemy_record_views(world);
            world_clear_enemy_spell_cache(world);
            memset(world->battle_enemy_hps,0,sizeof(world->battle_enemy_hps));
            world->battle_selected_ids[0]=chase.enemy_id;
            world->battle_selected_ids[1]=world->battle_selected_ids[2]=0;
            world->battle_formation_count=1;world->battle_scripted_entry=0;
            if(npe_world_battle_resources(file,world,1))return -1;
            int loaded=npe_world_load_battle_enemies(file,world);if(loaded)return loaded;
            world->battle_enemy_count=1;
            world->battle_enemy_id=world->battle_enemy_ids[0];
            world->battle_enemy_hp=world->battle_enemy_hps[0];
            memcpy(world->battle_party_defenses, world->defense, sizeof(world->defense));
            world_clear_battle_effects(world);
            int party_loaded=world_refresh_entry_equipment(file, world);if(party_loaded)return party_loaded;
            world->battle_actor = 0;
            world->battle_target = world->battle_lost = 0;
            world->battle_win_mode = world->battle_lose_mode = 0;
            world->battle_win_event_id = world->battle_lose_event_id = 0;
            world->encounter_steps = 0;
            world->story_event_id = 0;
            world->battle_chase_slot = (uint8_t)(slot + 1);
            world->game_mode = 3;
            npe_world_sound_battle_start(world);
            return 1;
        }
        /* Source 3A:5BB0..5D66 changes both axes without testing tiles. */
        if (cx > hx) --world->chase_x[slot];
        else if (cx < hx) ++world->chase_x[slot];
        if (cy > hy) --world->chase_y[slot];
        else if (cy < hy) ++world->chase_y[slot];
    }
    return 0;
}

int npe_world_select_battle_enemies(const NpeFile *file,NpeWorld *world){
    if(!file||!world)return -1;
    world_restore_enemy_record_views(world);
    /*39:AE9E..AEF8 clears all three enemy HP words before selecting. */
    memset(world->battle_enemy_hps,0,sizeof(world->battle_enemy_hps));
    if(world->battle_scripted_entry==1){
        NpeMap map;NpeMapBattleEvent event;
        world->battle_formation_count=3;
        if(!world->map_number||npe_map(file,world->map_number-1,&map)||
           npe_world_battle_event(file,world,world->story_event_id,&event)!=1)return -2;
        for(unsigned i=0;i<4;++i){NpeMapMonster monster;
            if(npe_world_monster(world,&map,i,&monster))return -1;
            if(monster.enemy_id==event.formation_key){
                /*Matched formation bytes are zero extended, unlike random IDs. */
                world->battle_selected_ids[0]=monster.left_enemy_id;
                world->battle_selected_ids[1]=monster.enemy_id;
                world->battle_selected_ids[2]=monster.right_enemy_id;return 0;
            }
        }
    }
    if((int16_t)world->event_context_id>0){
        world->battle_formation_count=1;
        uint32_t address=UINT32_C(0x527840)+(world->event_context_id-1u)*14u;
        uint8_t raw[2];
        for(unsigned i=0;i<2;++i){
            if(!npe_world_resident_read(world,address+i,raw+i,1))continue;
            if(npe_rpg_runtime_owner(file)!=world||npe_rpg_runtime_read(file,address+i,raw+i,1))return -2;
        }
        world->battle_selected_ids[0]=(uint16_t)(((unsigned)raw[0]<<8)|raw[1]);
        world->battle_selected_ids[1]=world->battle_selected_ids[2]=0;return 0;
    }
    unsigned eligible=0,indices[3]={0},formation=0;
    while(eligible<8&&world->random_enemy_records[eligible])++eligible;
    if(!eligible)return 0; /*39:B162 preserves all words and the current NUM. */
    world_form_encounter(world,eligible,indices,&formation);
    world->battle_formation_count=(uint8_t)formation;
    for(unsigned i=0;i<formation;++i){
        unsigned id=world->random_enemy_records[indices[i]];
        world->battle_selected_ids[i]=(uint16_t)(id<128?id:id|0xff00u);
    }
    return 0;
}

int npe_world_prepare_scripted_battle(const NpeFile *file, NpeWorld *world) {
    NpeMap map;
    NpeMapBattleEvent event;
    unsigned enemy_count;
    int result;
    if (!file || !world || world->game_mode != 3 ||
        !world->story_event_id || world->battle_enemy_count ||
        !world->map_number || npe_map(file, world->map_number - 1, &map) ||
        map.tail_size < 652) return -1;
    result = npe_world_battle_event(file,world,world->story_event_id,&event);
    if (result != 1)
        return -1;
    world->battle_scripted_entry=1;
    result=npe_world_select_battle_enemies(file,world);if(result)return result;
    enemy_count=world->battle_formation_count;if(enemy_count>3)return -2;
    world_clear_enemy_spell_cache(world);
    /*4E:8B80..8BC0 allocates and loads packed images on scripted entry. */
    if(npe_world_battle_resources(file,world,1))return -1;
    result=npe_world_load_battle_enemies(file,world);if(result)return result;
    /* Original entry copies map HP verbatim, including zero/negative
       words. Recovery belongs to the service that changes map HP. */
    world->battle_enemy_count = (uint8_t)enemy_count;
    memcpy(world->battle_party_defenses, world->defense,
           sizeof(world->battle_party_defenses));
    memset(world->battle_defense_turns, 0,
           sizeof(world->battle_defense_turns));
    memset(world->battle_skill_attack_bonus, 0,
           sizeof(world->battle_skill_attack_bonus));
    memset(world->battle_skill_defense_bonus, 0,
           sizeof(world->battle_skill_defense_bonus));
    memset(world->battle_attack_turns, 0,
           sizeof(world->battle_attack_turns));
    /* Scripted entry starts on the middle record, including an empty
       formation; native public entry does not scan for a living target. */
    world->battle_target = 1;
    world->battle_enemy_id = world->battle_enemy_ids[world->battle_target];
    world->battle_enemy_hp = world->battle_enemy_hps[world->battle_target];
    /* Source49:8DF0..8E10 selects record0, including a fallen member. */
    world->battle_actor = 0;
    world->battle_lost = 0;
    world->battle_win_mode = event.win_mode;
    world->battle_win_event_id = event.win_event_id;
    world->battle_lose_mode = event.lose_mode;
    world->battle_lose_event_id = event.lose_event_id;
    world->battle_file = file;
    world->battle_pending_mask = world->battle_executing = 0;
    memset(world->battle_party_status, 0, sizeof(world->battle_party_status));
    memset(world->battle_commands, 0, sizeof(world->battle_commands));
    memset(world->battle_item_reservations, 0, sizeof(world->battle_item_reservations));
    result=world_refresh_entry_equipment(file, world);if(result)return result;
    world_initialize_enemy_immunity(world);
    npe_world_sound_battle_start(world);
    return 1;
}

int npe_world_prepare_battle(const NpeFile *file,NpeWorld *world) {
    if(!file||!world||world->game_mode!=3||world->battle_enemy_count||
       world->battle_party_flags_saved||!world->map_number)return -1;
    /* Original4F:BBC4..BBDA dispatches event0 to31:7F18, otherwise
       to31:8B6A. A party/story followup can enter either service without
       an eligible map movement or a new encounter threshold roll. */
    return world->story_event_id?npe_world_prepare_scripted_battle(file,world):
                                world_prepare_ordinary_battle(file,world);
}

int npe_world_npc_action(const NpeFile *file, NpeWorld *world,
                         unsigned npc_id, NpeNpcAction *action,
                         unsigned *selected_case) {
    unsigned index;
    if (!world || !npc_id || npc_id >= sizeof(world->npc_state) ||
        npe_npc_action_for_state(file, npc_id, world->npc_state[npc_id],
                                 action, &index)) return -1;
    /* PRG source 35:4452..4492 writes the clamp back to saved state. */
    world->npc_state[npc_id] = (uint8_t)index;
    if (selected_case) *selected_case = index;
    return 0;
}

int npe_world_npc_commit(const NpeFile *file, NpeWorld *world,
                         unsigned npc_id, int action_succeeded) {
    NpeNpcAction action;
    if (npe_world_npc_action(file, world, npc_id, &action, NULL)) return -1;
    if (action_succeeded) world->npc_state[npc_id]++;
    return 0;
}


int npe_world_shop_items(const NpeFile *file,const NpeWorld *world,uint8_t ids[254],unsigned *count) {
    if(!world || !ids || !count)return -1;
    if(!world->shop_transaction_sell){
        if(!world->shop_catalog_ready)return npe_shop_items(file,world->event_context_id,ids,count);
        unsigned n=0;
        for(unsigned id=1;id<255;++id)if(world->shop_catalog_present[id])ids[n++]=(uint8_t)id;
        *count=n;return 0;
    }
    unsigned n=0;
    /*3D:F140..F1D0: display rows require positive signed inventory,
      but only a nonzero resource price. This is not the cached price
      gate used by the selection and movement services. */
    unsigned first=1,last=254;
    if(world->shop_list_ready){
        if(world->shop_selected_item==0xffff){*count=0;return 0;}
        /*3D:F0FC..F10E/F2BE..F2CC reads only the retained window.
          Unrelated owned records must not cause resource reads or errors. */
        first=(uint8_t)world->shop_first_item;
        last=(uint8_t)world->shop_last_item;
        if(first<1)first=1;
        if(last>254)last=254;
    }
    for(unsigned id=first;id<=last;++id){NpeItem item;
        if((int16_t)world->condition_words[id]<=0)continue;
        if(npe_native_item(file,id,&item))return -1;
        if(item.price)ids[n++]=(uint8_t)id;
    }
    *count=n;return 0;
}

static void shop_sync_indices(NpeWorld *world,const uint8_t *ids,unsigned count) {
    world->shop_first=world->shop_selection=255;
    for(unsigned i=0;i<count;++i){
        if(ids[i]==world->shop_first_item)world->shop_first=(uint8_t)i;
        if(ids[i]==world->shop_selected_item)world->shop_selection=(uint8_t)i;
    }
}
static void shop_list_row(NpeWorld *world,const uint8_t *ids,unsigned count) {
    /*3D:F104..F10A/F1DA..F1E8/F388: the sell list redraw derives
      the cursor from rendered rows; a selected zero-price resource
      can have no displayed row and leaves the local row zero. The
      FFFF early return preserves the resident row. */
    if(world->shop_selected_item==0xffff)return;
    world->shop_row=0;
    unsigned row=0;
    for(unsigned i=0;i<count;++i){
        if(ids[i]<world->shop_first_item || ids[i]>world->shop_last_item)continue;
        if(ids[i]==world->shop_selected_item)world->shop_row=(uint8_t)row;
        ++row;
    }
}
static int shop_stock(const NpeWorld *world,unsigned id,uint16_t *stock){
    uint8_t high,low;uint32_t address=UINT32_C(0x5267c8)+id*2u;
    if(world_service_byte_read(world,address,&high)||world_service_byte_read(world,address+1,&low))return -1;
    *stock=(uint16_t)((unsigned)high<<8|low);return 0;
}
static int shop_find_next(const NpeFile *file,NpeWorld *world,unsigned id,int direction) {
    /*4B:E53C consumes the starting byte and scans1..254. Buy candidates
      use the catalog's presence flags; sell candidates load each positive
      inventory record and test the cached signed price. */
    for(int candidate=(int)(uint8_t)id+direction;
        candidate>=world->shop_scan_first&&candidate<=world->shop_scan_last;candidate+=direction){
        if(world->shop_inventory_only){
            uint16_t stock;if(shop_stock(world,(unsigned)candidate,&stock))return -1;
            if((int16_t)stock>0)return candidate;
        }else
        if(world->shop_transaction_sell){NpeItem item;
            uint16_t stock;if(shop_stock(world,(unsigned)candidate,&stock))return -1;
            if((int16_t)stock<=0)continue;
            if(npe_world_item(file,world,(unsigned)candidate,&item))return -1;
            if((int16_t)item.price>0)return candidate;
        }else{uint8_t present;
            if(world_service_byte_read(world,UINT32_C(0x526dba)+(unsigned)candidate,&present))return -1;
            if(present)return candidate;
        }
    }
    return 0xffff;
}
int npe_world_shop_mode_begin(const NpeFile *file,NpeWorld *world) {
    if(!file || !world || world->game_mode!=4 || world->story_event_id==2)return -1;
    if(!world->map_init_pending)return 0;
    /*32:4440..4466 consumes7AB4 even when22:B4BA rejects7938.
      22:B50E..B530 precedes the signed context check at B53C/B726. */
    world->lcd_background=0;
    world->shop_inventory_only=0;world->shop_scan_first=1;world->shop_scan_last=254;
    world->map_init_pending=0;
    if((int16_t)world->event_context_id<10){
        /* B740..B744 returns without changing the resident UI, selection,
           bounds or catalog. These host flags route its subsequent input;
           list_ready prevents an input service from retrying initialization. */
        world->shop_ready=1;world->shop_list_ready=1;
        return 0;
    }
    world->shop_kind=world->shop_ready=world->event_context_id==10?2:1;
    return npe_world_shop_begin(file,world)?-1:1;
}
int npe_world_shop_return(NpeWorld *world) {
    if(!world || world->game_mode!=4 || world->story_event_id==2)return -1;
    /*32:1E6E..1EF0: every non-list UI restores rectangle29,4,211,111.
      Only a second F4 on UI1 enters mode1;7936 remains resident. */
    if(world->shop_ui!=1){world->shop_ui=1;return 0;}
    world->game_mode=1;world->map_init_pending=1;world->shop_ready=0;
    return 1;
}
int npe_world_shop_begin(const NpeFile *file,NpeWorld *world) {
    uint8_t ids[254];unsigned count;
    if(!world)return -1;
    world->shop_page_pending=world->shop_page_backfill_count=world->shop_navigation_page=0;
    /*3F:B4C2..B4CE calls03:41BC with foreground15/background0.
      This graphics state remains resident after the shop closes. */
    world->lcd_background=0;
    /*3F:B50E..B530 resets these controls for both buy and sell entry. */
    world->shop_inventory_only=0;world->shop_scan_first=1;world->shop_scan_last=254;
    if(world->shop_kind==1){NpeShopCatalog catalog;
        if(npe_shop_catalog(file,world->event_context_id,&catalog))return -1;
        memset(world->shop_catalog_present,0,255);
        for(unsigned i=0;i<catalog.count;++i)
            if(catalog.items[i]>=1&&catalog.items[i]<=254)world->shop_catalog_present[catalog.items[i]]=1;
        world->shop_catalog_ready=1;
    }
    /*3F:B754 resets only selection. The bound writes at B7F2/B816 and
      B916/B93A occur only for an eligible item, leaving empty bounds resident. */
    world->shop_selected_item=0xffff;
    if(world->shop_kind==1)world->shop_transaction_sell=0;
    else if(world->shop_kind==2)world->shop_transaction_sell=1;
    if(world->shop_kind==2){
        unsigned found=0;
        /*3F:B8DE..B94A stops after seven accepted entries, so it must
          not load every owned item simply to build a host array. */
        for(unsigned id=1;id<255;++id){NpeItem item;
            if((int16_t)world->condition_words[id]<=0)continue;
            if(npe_world_item(file,world,id,&item))return -1;
            if((int16_t)item.price<=0)continue;
            if(!found)world->shop_first_item=world->shop_selected_item=(uint16_t)id;
            world->shop_last_item=(uint16_t)id;
            if(++found==7)break;
        }
    }else if(world->shop_kind==1){
        if(npe_world_shop_items(file,world,ids,&count))return -1;
        if(count){world->shop_selected_item=world->shop_first_item=ids[0];world->shop_last_item=ids[count<7?count-1:6];}
    }
    world->shop_row=0;world->shop_list_ready=1;world->shop_ui=1;
    if(npe_world_shop_items(file,world,ids,&count))return -1;
    shop_sync_indices(world,ids,count);shop_list_row(world,ids,count);return 0;
}
static int shop_finish_navigation(const NpeFile *file,NpeWorld *world) {
    uint8_t ids[254];unsigned count;
    if(npe_world_shop_items(file,world,ids,&count))return -1;
    shop_sync_indices(world,ids,count);
    /*3F:DBE0 with clear=0 preserves the buy cursor row.3D:EE72
      derives the sell row from displayed entries instead. */
    if(world->shop_navigation_page && world->shop_transaction_sell)shop_list_row(world,ids,count);
    return 1;
}
int npe_world_shop_resume(const NpeFile *file,NpeWorld *world) {
    if(!world)return -1;
    if(!world->shop_page_pending)return 0;
    /*40:C59E..C61E rereads last and the live upper bound. If the guard
      stops before six entries, native code remains in that loop, without
      submitting LCD or processing another menu key. Yield to the host;
      a later live bound change resumes the same invocation. */
    while((int16_t)world->shop_last_item<world->shop_scan_last){
        int following=shop_find_next(file,world,world->shop_last_item,1);
        if(following<0)return -1;
        world->shop_last_item=world->shop_selected_item=(uint16_t)following;
        if(++world->shop_page_backfill_count==6){
            world->shop_page_pending=0;world->shop_row=6;
            return shop_finish_navigation(file,world);
        }
    }
    return NPE_SHOP_MOVE_PENDING;
}
int npe_world_shop_move(const NpeFile *file,NpeWorld *world,int direction) {
    if(!world || (direction!=1 && direction!=-1))return -1;
    if(world->shop_page_pending)return npe_world_shop_resume(file,world);
    if(!world->shop_list_ready && npe_world_shop_begin(file,world))return -1;
    world->shop_navigation_page=0;
    if(world->shop_selected_item==0xffff)return 0;
    int next=shop_find_next(file,world,world->shop_selected_item,direction);
    if(next<0)return -1;
    int page=(direction>0?(int16_t)world->shop_selected_item>=(int16_t)world->shop_last_item:
                           (int16_t)world->shop_selected_item<=(int16_t)world->shop_first_item);
    /*40:C3D8/42:5CAC stores FFFF within existing bounds and still
      draws the cursor. Only a failed search across the page is ignored. */
    if(next==0xffff&&page)return 0;
    world->shop_navigation_page=(uint8_t)page;
    if(direction>0 && page){
        world->shop_first_item=world->shop_last_item=world->shop_selected_item=(uint16_t)next;
        world->shop_row=0;
        for(unsigned i=0;i<6 && (int16_t)world->shop_last_item<world->shop_scan_last;++i){int following=shop_find_next(file,world,(unsigned)next,1);
            if(following<0)return -1;
            if(following==0xffff)break;
            next=following;world->shop_last_item=(uint16_t)next;
        }
    }else if(direction<0 && page){
        world->shop_first_item=world->shop_last_item=world->shop_selected_item=(uint16_t)next;
        unsigned found=0;
        while((int16_t)next>world->shop_scan_first&&found<6){int previous=shop_find_next(file,world,(unsigned)next,-1);
            if(previous<0)return -1;
            if(previous==0xffff){
                world->shop_page_pending=1;world->shop_page_backfill_count=(uint8_t)found;
                return npe_world_shop_resume(file,world);
            }
            next=previous;world->shop_first_item=(uint16_t)next;++found;
        }
        /*40:C64C reaches the lower bound without entering backfill. */
        world->shop_row=6;
    }else{
        world->shop_selected_item=(uint16_t)next;
        world->shop_row=(uint8_t)(world->shop_row+direction);
    }
    return shop_finish_navigation(file,world);
}

int npe_world_shop_quantity(NpeWorld *world,unsigned item_id,int direction) {
    if(!world || !item_id || item_id>254 || (direction!=1 && direction!=-1))return -1;
    uint16_t stock=world->condition_words[item_id];
    uint8_t quantity=(uint8_t)(world->shop_quantity+direction);
    /*40:C6D8..C76C,42:5F24..5FB2: byte wrap and signed word comparisons. */
    if(direction>0){
        if(world->shop_transaction_sell ? (int16_t)quantity>(int16_t)stock :
                                 (int16_t)(uint16_t)(quantity+stock)>99)quantity=1;
    }else if(!quantity)quantity=(uint8_t)(world->shop_transaction_sell?stock:99-stock);
    world->shop_quantity=quantity;return 0;
}

int npe_world_trade_item(const NpeFile *file,NpeWorld *world,
                         unsigned item_id,uint8_t quantity,int selling) {
    NpeItem item;
    if(!world || !item_id || item_id>=255 || npe_world_item(file,world,item_id,&item))return -1;
    /*3C:4A74..4B16 sign-extends741E, multiplies by the quantity byte,
      and tests the signed32-bit subtraction. There is no999999 cap.
      4B50..4B6E/4BE2..4C00 stores the low16-bit inventory result. */
    int32_t price=(int16_t)item.price;
    if(selling)price/=2; /*4C0A..4C3E divides before multiplying. */
    uint32_t amount=(uint32_t)(price*(int32_t)quantity);
    if(!selling && (int32_t)(world->money-amount)<0)return 0;
    world->condition_words[item_id]=(uint16_t)(world->condition_words[item_id]+
        (selling?-(int)quantity:(int)quantity));
    world->money=selling?world->money+amount:world->money-amount;
    return 1;
}

int npe_world_shop_trade(const NpeFile *file,NpeWorld *world) {
    if(!world || (!world->shop_list_ready && npe_world_shop_begin(file,world)))return -1;
    unsigned selected=world->shop_selected_item;
    int result=npe_world_trade_item(file,world,selected,world->shop_quantity,world->shop_transaction_sell!=0);
    if(result<=0 || !world->shop_transaction_sell || (int16_t)world->condition_words[selected]>0)return result;
    uint8_t ids[254];unsigned count;
    /*3C:4C82..4EA6 preserves the page bounds as IDs, not seven-entry
      block numbers. A sold item can shift one bound outside the old page. */
    if(selected==world->shop_first_item){
        int previous=shop_find_next(file,world,selected,-1);if(previous<0)return -1;
        if(previous!=0xffff){
            if(selected==world->shop_last_item)world->shop_last_item=(uint16_t)previous;
            world->shop_first_item=world->shop_selected_item=(uint16_t)previous;
        }else{
            int next=shop_find_next(file,world,selected,1);if(next<0)return -1;
            world->shop_first_item=world->shop_selected_item=(uint16_t)next;
            int following=shop_find_next(file,world,world->shop_last_item,1);if(following<0)return -1;
            if(following!=0xffff)world->shop_last_item=(uint16_t)following;
        }
    }else if(selected==world->shop_last_item){
        int next=shop_find_next(file,world,selected,1);if(next<0)return -1;
        if(next==0xffff){next=shop_find_next(file,world,world->shop_last_item,-1);if(next<0)return -1;--world->shop_row;}
        world->shop_last_item=world->shop_selected_item=(uint16_t)next;
    }else{
        int next=shop_find_next(file,world,selected,1);if(next<0)return -1;world->shop_selected_item=(uint16_t)next;
        int previous=shop_find_next(file,world,world->shop_first_item,-1);if(previous<0)return -1;
        if(previous!=0xffff)world->shop_first_item=(uint16_t)previous;
        else{next=shop_find_next(file,world,world->shop_last_item,1);if(next<0)return -1;if(next!=0xffff)world->shop_last_item=(uint16_t)next;}
    }
    if(npe_world_shop_items(file,world,ids,&count))return -1;
    shop_sync_indices(world,ids,count);shop_list_row(world,ids,count);return result;
}

int npe_world_npc_event(const NpeFile *file,NpeWorld *world,unsigned npc_id,NpeNpcEvent *event){
    if(!file||!world||!event)return -1;
    if(!file->rpg_runtime&&npe_world_resources_open((NpeFile *)file,world))return -1;
    if(npe_rpg_runtime_owner(file)!=world)return -1;
    int result=npe_rpg_runtime_npc_event(file,(uint16_t)npc_id,event);
    if(result)return result;
    world->mode2_dialogue_offset=world->text_file_position-file->sections[5].offset;
    world->story_control_ready=0;world->mode2_ready=1;
    return 0;
}

static int world_finish_text_followup(NpeWorld *world,unsigned mode,unsigned id) {
    /*43:6970..698C and3B:7728..7748 restore the raw continuation.
      Display dispatch4A:F498/F4A0 handles0/>24. Numeric callers include
      that normalization, retaining the complete event word. */
    world->game_mode=(uint8_t)mode;
    world->story_event_id=(uint16_t)id;
    if(mode==14)return npe_world_apply_story_event(world,id);
    if(!mode || mode>24){
        if(!world->defer_story_events)world->game_mode=1;
        return 1;
    }
    world_resolve_simple_mode(world);
    return 1;
}

int npe_world_finish_npc_event(NpeWorld *world, const NpeNpcEvent *event) {
    if (!world || !event) return -1;
    world->mode2_ready = 0;
    if(world->mode2_context>=10){
        world->game_mode=4;world->story_event_id=1;world->event_context_id=world->mode2_context;
        world->shop_kind=world->shop_ready=world->mode2_context>10?1:2;world->shop_selection=world->shop_first=0;world->shop_ui=1;world->shop_list_ready=0;
        return 1;
    }
    /* Source 43:6956..698C restores 7A0A/7A82 after the text closes.
       The dispatcher source 4A:F47C..F566 sends mode 14 to the map event
       table; the original Yang trace advances progress 3->4 through it. */
    return world_finish_text_followup(world,event->followup_mode,event->followup_event_id);
}

int npe_world_prepare_mode2(const NpeFile *file, NpeWorld *world) {
    NpeNpcEvent event;
    int result;
    if (!file || !world || world->game_mode != 2) return -1;
    if (world->mode2_ready) return 1;
    /* Fresh native mode 2 (source 3B:71BE..723A) calls runtime 18:4360
       at 3B:7212 before reading 797A/797C. That is the NPC loader at
       source 35:4360, which selects by 7936 and the saved NPC state.
       Money services set the fresh-entry flag at source 46:4BAC..4BB4. */
    result = npe_world_npc_event(file, world, world->story_event_id, &event);
    return result ? result : 1;
}

int npe_world_finish_mode2(NpeWorld *world) {
    if (!world || world->game_mode != 2) return -1;
    if (!world->mode2_ready) return 0;
    world->mode2_ready = 0;
    /* NPC controller3B:76EC..7720 takes the selected NPC's extra byte,
      before the cached followup. Window stories use the separate43 path. */
    if(world->mode2_context>=10){
        world->game_mode=4;world->story_event_id=1;world->event_context_id=world->mode2_context;
        world->shop_kind=world->shop_ready=world->mode2_context>10?1:2;world->shop_selection=world->shop_first=0;world->shop_ui=1;world->shop_list_ready=0;
        return 1;
    }
    return world_finish_text_followup(world,world->mode2_followup_mode,world->mode2_followup_event_id);
}

int npe_world_load_story_event_cache(NpeWorld *world,const NpeMap *map) {
    NpeMapStoryEvent events[15];unsigned count;
    if(!world || !map)return -1;
    if(map->tail_size<703){world->story_event_count=0;return 0;}
    if(npe_map_story_events(map,events,&count))return -1;
    /*29:EAA2..EBD2 visits15 slots, reading only those below group11's
      count. The remaining resident records are not cleared. */
    for(unsigned i=0;i<count;++i)world->story_events[i]=events[i];
    world->story_event_count=(uint8_t)count;
    return 0;
}

int npe_world_story_event(const NpeWorld *world,unsigned id,NpeMapStoryEvent *event) {
    if(!world || !event)return -1;
    id=(uint8_t)id;
    if(!id || id>15)return 0;
    *event=world->story_events[id-1];
    /* File ID and source pointer are metadata, absent from74F4's cache. */
    event->id=(uint16_t)id;event->raw=NULL;
    return 1;
}

int npe_world_apply_story_event(NpeWorld *world, unsigned event_id) {
    if (!world) return -1;
    if (world->defer_story_events) {
        /* The original callers restore mode/event to7934/7936, then return
           to4A:F562. Its mode14 handler commits the OLD progress first.
           Queue the complete word; only4C:4C6E truncates its slot index. */
        world->game_mode = 14;
        world->story_event_id = (uint16_t)event_id;
        return 1;
    }
    return npe_world_execute_story_event(world,event_id);
}

int npe_world_execute_story_event(NpeWorld *world, unsigned event_id) {
    NpeMapStoryEvent event;
    if (!world) return -1;
    /*4C:4C6E truncates to a byte. Invalid slots set mode0, leaving the
      progress and event words alone; the display dispatcher returns to1. */
    int result=npe_world_story_event(world,event_id,&event);
    if(result<0)return -1;
    if(!result){world->game_mode=0;return 0;}
    world->story_progress = event.story_progress;
    if (npe_world_update_tiles(world) < 0) return -1;
    world->game_mode = event.mode;
    world->story_event_id = event.next_id;
    world_resolve_simple_mode(world);
    return 1;
}

int npe_world_prepare_story(const NpeFile *file,NpeWorld *world) {
    NpeStoryRecord story;
    if (!file || !world || world->game_mode != 5 ||
        !world->story_event_id ||
        npe_story_record(file, world->story_event_id - 1, &story)) return -1;
    /* Source 43:667C/66F2 replaces the retained text pointer with a
       section-10 story pointer, which is not a section-5 NPC dialogue. */
    world->mode2_ready = 0;
    /*43:6796..67E0 loads the shared resident continuation once on entry. */
    world->mode2_followup_mode=story.control_byte;
    world->mode2_followup_event_id=story.control_word;
    world->text_style=(uint8_t)story.header_value;
    world->text_rect[0]=0;world->text_rect[1]=world->text_style==1?12:84;
    world->text_rect[2]=240;world->text_rect[3]=world->text_style==1?131:118;
    world->text_file_position=(uint32_t)(story.text-file->bytes);
    world->text_offset=0;world->text_more=1;
    world->story_control_ready=1;
    return 1;
}

int npe_world_finish_story(const NpeFile *file,NpeWorld *world) {
    if(!file || !world || world->game_mode!=5)return -1;
    if(!world->story_control_ready && npe_world_prepare_story(file,world)!=1)return -1;
    world->story_control_ready=0;
    return world_finish_text_followup(world,world->mode2_followup_mode,world->mode2_followup_event_id);
}

int npe_world_select_choice(const NpeFile *file, NpeWorld *world,
                            unsigned choice_index) {
    NpeMapChoiceEvent choice;
    int result;
    if (!file || !world || world->game_mode != 17 ||
        choice_index > 1) return -1;
    result = npe_world_choice_event(file, world, world->story_event_id, &choice);
    if (result <= 0) return result;
    world->game_mode = choice.mode[choice_index];
    world->story_event_id = choice.next_id[choice_index];
    world->event_context_id = 0;
    world->map_init_pending = 1;
    world->quiz_ready = 0;
    world_resolve_simple_mode(world);
    /* Dispatcher source 4A:F492..F602 maps modes outside 1..24 back
       to map mode 1 after this handler returns. */
    if (!world->game_mode || world->game_mode > 24)
        world->game_mode = 1;
    return 1;
}

void npe_render_choice_event_move(uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    if(!pixels)return;
    /*45:D32E..D368 toggles both inclusive LCD rectangles. */
    for(unsigned y=51;y<=67;++y)for(unsigned x=76;x<=159;++x)
        if(x<=95||x>=140)pixels[y*NPE_WIDTH+x]=15-pixels[y*NPE_WIDTH+x];
}
int npe_render_choice_event(const NpeFile *file,
                            uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    if(!file||!pixels)return -1;
    /*45:D23A..D2B8: keep the resident LCD, paint the right resource,
      invert its rectangle, then paint the left resource. */
    if(npe_render_indexed_resource(file,npe_image_group_index(file,7,8),134,45,0,pixels))return -1;
    for(unsigned y=51;y<=67;++y)for(unsigned x=140;x<=159;++x)
        pixels[y*NPE_WIDTH+x]=15-pixels[y*NPE_WIDTH+x];
    return npe_render_indexed_resource(file,npe_image_group_index(file,7,7),70,45,0,pixels);
}

int npe_world_load_money_events(NpeWorld *world,const NpeMap *map) {
    NpeMapMoneyEvent loaded[5];
    if(!world||!map)return -1;
    if(map->tail_size<703)return 0;
    memcpy(loaded,world->money_events,sizeof(loaded));
    for(unsigned i=0;i<5;++i){
        NpeMapMoneyEvent event;int result=npe_map_money_event(map,i+1,&event);
        if(result<0)return -1;
        if(!result)break;
        event.id=(uint16_t)(i+1);event.raw=NULL;loaded[i]=event;
    }
    memcpy(world->money_events,loaded,sizeof(loaded));return 0;
}

int npe_world_money_event(const NpeWorld *world,unsigned id,NpeMapMoneyEvent *event) {
    if(!world||!event)return -1;
    id&=255u;
    if(!id||id>5)return 0;
    *event=world->money_events[id-1];event->id=(uint16_t)id;event->raw=NULL;
    return 1;
}

int npe_world_change_money(NpeWorld *world,const NpeMapMoneyEvent *event,uint16_t *notice_amount) {
    uint16_t amount;
    if(!world||!event||!notice_amount)return -1;
    amount=event->amount;
    /*46:4C8A..4CF6 sign-extends the16-bit operand, wraps the addition,
      then compares the result as a signed32-bit value to999999. */
    if(event->add_money){
        world->money+=(uint32_t)(int32_t)(int16_t)amount;
        if((int32_t)world->money>999999)world->money=999999;
    }else{
        /*46:4D28..4D6C clips the arithmetic operand to the old low
          money word on a signed comparison; subtraction sign-extends it. */
        if((int32_t)world->money<(int16_t)amount)amount=(uint16_t)world->money;
        world->money-=(uint32_t)(int32_t)(int16_t)amount;
    }
    /*46:4C5A formats the requested amount BEFORE that subtraction clip. */
    *notice_amount=event->amount;return 1;
}

int npe_world_finish_money_event(NpeWorld *world,const NpeMapMoneyEvent *event) {
    if(!world||!event)return -1;
    world->game_mode = event->next_mode;
    world->story_event_id = event->next_id;
    if (world->game_mode == 14)
        npe_world_apply_story_event(world, world->story_event_id);
    else
        world_resolve_simple_mode(world);
    return 1;
}

int npe_world_apply_money_event(const NpeFile *file,NpeWorld *world) {
    NpeMapMoneyEvent event;uint16_t amount;int result;
    if(!file||!world||world->game_mode!=11||!world->map_number)return -1;
    world->map_init_pending=1;
    result=npe_world_money_event(world,world->story_event_id,&event);
    if(result<=0){if(!result)world->game_mode=0;return result;}
    if(npe_world_change_money(world,&event,&amount)<0)return -1;
    return npe_world_finish_money_event(world,&event);
}

int npe_world_load_item_events(NpeWorld *world,const NpeMap *map) {
    NpeMapItemEvent loaded[5];
    if(!world||!map)return -1;
    if(map->tail_size<703)return 0;
    memcpy(loaded,world->item_events,sizeof(loaded));
    for(unsigned i=0;i<5;++i){
        NpeMapItemEvent event;int result=npe_map_item_event(map,i+1,&event);
        if(result<0)return -1;
        if(!result)break;
        event.raw=NULL;loaded[i]=event;
    }
    memcpy(world->item_events,loaded,sizeof(loaded));return 0;
}

int npe_world_item_event(const NpeWorld *world,unsigned id,NpeMapItemEvent *event) {
    if(!world||!event)return -1;
    id&=255u;if(!id||id>5)return 0;
    *event=world->item_events[id-1];event->raw=NULL;return 1;
}

int npe_world_change_item(NpeWorld *world,const NpeMapItemEvent *event) {
    uint16_t current,updated;
    if(!world||!event)return -1;
    /* Source 44:5B54..5CBA indexes 52:67C8 by the full word, including0.
       The signed full-word address may alias other resident records.
       Addition wraps at 16 bits; subtraction stores the result and then
       clears it when that signed 16-bit result is negative. */
    uint16_t index=event->item_index;
    int32_t signed_index=index<32768u?(int32_t)index:(int32_t)index-65536;
    uint32_t address=(uint32_t)(INT32_C(0x5267c8)+signed_index*2);uint8_t bytes[2];
    const NpeFile *file=world->battle_file;
    if(npe_world_resident_read(world,address,bytes,2)){
        if(!file||npe_rpg_runtime_owner(file)!=world||npe_rpg_runtime_read(file,address,bytes,2))return -2;
    }
    current=(uint16_t)(((unsigned)bytes[0]<<8)|bytes[1]);
    updated = event->add_item ? (uint16_t)(current + event->quantity) :
                               (uint16_t)(current - event->quantity);
    if (!event->add_item && (int16_t)updated < 0) updated = 0;
    bytes[0]=(uint8_t)(updated>>8);bytes[1]=(uint8_t)updated;
    if(npe_world_resident_write(world,address,bytes,2)){
        if(!file||npe_rpg_runtime_owner(file)!=world||npe_rpg_runtime_write(file,address,bytes,2))return -2;
    }
    return 1;
}

int npe_world_finish_item_event(NpeWorld *world,const NpeMapItemEvent *event) {
    if(!world||!event)return -1;
    world->game_mode = event->next_mode;
    world->story_event_id = event->next_id;
    if (world->game_mode == 14)
        npe_world_apply_story_event(world, world->story_event_id);
    else
        world_resolve_simple_mode(world);
    return 1;
}

int npe_world_apply_item_event(const NpeFile *file,NpeWorld *world) {
    NpeMapItemEvent event;NpeItem item;int result;
    if(!file||!world||world->game_mode!=12||!world->map_number)return -1;
    unsigned event_id=world->story_event_id;
    world->map_init_pending=1;result=npe_world_item_event(world,event_id,&event);
    if(result<=0){if(!result)world->game_mode=0;return result;}
    if(npe_world_item(file,world,event.item_index,&item)||npe_world_change_item(world,&event)<0)return -1;
    if(npe_world_item_event(world,event_id,&event)!=1)return -1;
    return npe_world_finish_item_event(world,&event);
}

int npe_world_apply_move_event(const NpeFile *file, NpeWorld *world) {
    NpeMapMoveTrigger event;
    if (!file || !world || world->game_mode != 20 || !world->map_number) return -1;
    /*4D:AC0E..AC16 precedes the low-byte range check, including invalid IDs. */
    world->map_init_pending=1;
    int result=npe_world_move_event(world,world->story_event_id,&event);
    if(result<=0){if(!result)world->game_mode=0;return result;}
    world->game_mode = event.mode;
    world->story_event_id = event.event_id;
    if (world->game_mode == 14)
        npe_world_apply_story_event(world, world->story_event_id);
    else
        world_resolve_simple_mode(world);
    return 1;
}

int npe_world_finish_game(NpeWorld *world) {
    if (!world || world->game_mode != 21) return -1;
    /*4A:F59A..F5D2 releases the map without stopping audio or changing
      the battle resource flag, then requests the title initializer. */
    const NpeFile *file=world->battle_file;
    if(file && file->rpg_runtime){
        if(npe_rpg_runtime_owner(file)!=world || npe_rpg_runtime_finish_game(file))return -1;
    }else{
        world->game_mode=7;
        world->map_init_pending=1;
    }
    return 1;
}

int npe_world_close_menu(NpeWorld *world) {
    if (!world || world->game_mode != 4) return -1;
    /* Mode 4: source 4F:43F6 calls 4C:6FBA for key handling. Its 0x8E
       branch calls 4F:1E6E, which writes mode 1 to 52:7934 and leaves
       the current event number unchanged. */
    world->game_mode = 1;
    return 1;
}

int npe_world_equipment_description_page(const NpeFile *file,NpeWorld *world,int direction) {
    if(!file || !world || world->game_mode!=6 || world->property_page!=10 ||
       (direction!=1 && direction!=-1))return -1;
    /*47:572E/59A2 keep icons/name; the signed left guard precedes loading. */
    if(direction>0){
        if(world->shop_description_end==(uint16_t)(world->item_description_length-1u))return 0;
    }else if(!world->shop_description_start || world->shop_description_start>=0x8000u)return 0;
    if(!file->rpg_runtime && npe_world_resources_open((NpeFile *)file,world))return -1;
    if(npe_rpg_runtime_owner(file)!=world)return -1;
    NpeItem item;
    if(npe_world_item(file,world,world->equipment_description_item,&item))return -1;
    world->lcd_background=0;
    uint16_t start,result,count;
    if(direction>0){
        start=world->shop_description_start=(uint16_t)(world->shop_description_end+1u);
        count=66;
    }else{
        start=world->shop_description_start;
        world->shop_description_end=(uint16_t)(start-1u);
        count=(uint16_t)(0u-66u);
    }
    /* Preserve the actual terminated caller-local bytes. Reconstructing a
       source span from indices loses empty pages, clamp/parity and word wrap. */
    if(npe_rpg_runtime_text_slice(file,UINT32_C(0x5273ab),start,count,
        world->equipment_description_local,sizeof(world->equipment_description_local),&result))return -1;
    if(direction>0)world->shop_description_end=(uint16_t)(world->shop_description_end+result);
    else world->shop_description_start=(uint16_t)(start-result);
    world->equipment_description_local_ready=1;
    ++world->equipment_description_revision;
    return 1;
}

int npe_world_open_system_menu(NpeWorld *world) {
    if (!world || world->game_mode != 1) return -1;
    /* Source 37:137E..1392: the map controller handles 0x8E by
       selecting mode 6 and setting the menu redraw flag. */
    world->game_mode = 6;
    /*49:0080..008A sets the graphics background to0.03:4214 uses
      that resident color later, including NPC redraws after menu exit. */
    world->lcd_background = 0;
    world->property_page=world->property_member=world->equipment_view_slot=0;
    /*49:0022..0070 starts at1 and counts only exact flag1 in slots1/2.
      Property navigation retains this byte until the next main menu entry. */
    world->menu_party_count=(uint8_t)(1+(world->party_active[1]==1)+(world->party_active[2]==1));
    return 1;
}

int npe_world_close_system_menu(NpeWorld *world) {
    if (!world || world->game_mode != 6) return -1;
    /* Source 4B:B6F4..B704 calls 33:15A8 for 0x8E. The latter writes
       mode 1 to 52:7934 and does not touch 52:7936. */
    world->game_mode = 1;
    return 1;
}

typedef struct {
    NpeMapNpcMoveEvent event;
    unsigned slot,x,y,direction;
    int steps,step_x,step_y;
} NpcScriptMove;

static int npc_move_signed_word(unsigned value) {
    value &= 65535u;
    return value < 32768u ? (int)value : (int)value - 65536;
}

static int npc_move_abs_word(int value) {
    /*33:1724 returns a word; negating -32768 still yields signed -32768. */
    return value == -32768 ? value : abs(value);
}

static int prepare_npc_script_move(const NpeFile *file,NpeWorld *world,
                                    NpcScriptMove *move) {
    NpeMap map;
    NpeMapNpcMoveEvent event;
    const uint8_t *npc;
    unsigned slot, start_x, start_y;
    int dx, dy;
    int result;
    if (!file || !world || world->game_mode != 18 ||
        !world->map_number || npe_map(file, world->map_number - 1, &map))
        return -1;
    result = npe_world_npc_move_event(world, world->story_event_id, &event);
    if (result < 0) return -1;
    if (!result) {
        world->game_mode = 1;
        return 0;
    }
    /*3C:8D78..8E0A uses target coordinates directly. Scripted NPC
      motion can leave the map; walk/collision bounds do not apply. */
    for (slot = 0; slot < 20; ++slot) {
        if (npe_world_npc(world, &map, slot, &npc)) return -1;
        if (((unsigned)npc[0] << 8 | npc[1]) == event.npc_id) break;
    }
    if (slot == 20) return -1;
    if (npe_world_npc_position(world, npc, slot, &start_x, &start_y)) return -1;
    dx = npc_move_signed_word(event.x - start_x);
    dy = npc_move_signed_word(event.y - start_y);
    move->event=event;move->slot=slot;move->x=start_x;move->y=start_y;
    move->steps=npc_move_abs_word(dx)>npc_move_abs_word(dy)?npc_move_abs_word(dx):npc_move_abs_word(dy);
    /* DIV by zero sets V (CN1916884A p28); the original does not trap.
       With zero steps the loop at3C:90B6 skips all quotient uses, but
       still writes the direction and publishes the cached followup. */
    /* Source3C:8DE6..8E0A divides once before the loop. Signed DIV
       truncates toward zero; the shorter diagonal axis can stay still. */
    move->step_x=move->steps?dx/(int)move->steps:0;
    move->step_y=move->steps?dy/(int)move->steps:0;
    if (npc_move_abs_word(dx) > npc_move_abs_word(dy))
        move->direction=dx>0?1u:3u;
    else
        move->direction=dy>=0?0u:2u;
    return 1;
}

static void world_finish_script_followup(NpeWorld *world,unsigned mode,unsigned id,
                                         int display_dispatch) {
    world->game_mode = (uint8_t)mode;
    world->story_event_id = (uint16_t)id;
    if (world->game_mode == 14 && !display_dispatch)
        npe_world_apply_story_event(world, world->story_event_id);
    else
        world_resolve_simple_mode(world);
    if (!world->game_mode || world->game_mode > 24) world->game_mode = 1;
}

int npe_world_apply_npc_move_event(const NpeFile *file,NpeWorld *world) {
    NpcScriptMove move;
    int result=prepare_npc_script_move(file,world,&move);
    if(result<=0)return result;
    /* The native loop index is a byte. Counts >=255 never reach an exit;
       a numeric end-state request cannot synthesize a successful followup. */
    if(move.steps>=255)return NPE_NPC_MOVE_NONTERMINATING;
    int steps=move.steps>0?move.steps:0;
    world->npc_override_x[move.slot]=(uint8_t)((int)move.x+move.step_x*steps);
    world->npc_override_y[move.slot]=(uint8_t)((int)move.y+move.step_y*steps);
    world->npc_override_direction[move.slot]=(uint8_t)move.direction;
    world_finish_script_followup(world,move.event.next_mode,move.event.next_id,0);
    return 1;
}

static void world_swap_party_slots(NpeWorld *world, unsigned a, unsigned b) {
#define SWAP_PARTY_FIELD(field) do { \
    uint8_t temporary[sizeof(world->field[0])]; \
    memcpy(temporary, &world->field[a], sizeof(temporary)); \
    memcpy(&world->field[a], &world->field[b], sizeof(temporary)); \
    memcpy(&world->field[b], temporary, sizeof(temporary)); \
} while (0)
    if (a == b) return;
    unsigned direction_a=a?world->companion_directions[a-1]:world->direction;
    unsigned direction_b=b?world->companion_directions[b-1]:world->direction;
    if(a)world->companion_directions[a-1]=(uint8_t)direction_b;else world->direction=direction_b;
    if(b)world->companion_directions[b-1]=(uint8_t)direction_a;else world->direction=direction_a;
    SWAP_PARTY_FIELD(party_ids);
    SWAP_PARTY_FIELD(party_active);
    SWAP_PARTY_FIELD(party_attack_groups);
    SWAP_PARTY_FIELD(party_characters);
    SWAP_PARTY_FIELD(party_names);
    SWAP_PARTY_FIELD(party_record_residue);
    SWAP_PARTY_FIELD(skill_slots);
    SWAP_PARTY_FIELD(learned_skill_count);
    SWAP_PARTY_FIELD(equipped_items);
    SWAP_PARTY_FIELD(hp); SWAP_PARTY_FIELD(max_hp);
    SWAP_PARTY_FIELD(qi); SWAP_PARTY_FIELD(max_qi);
    SWAP_PARTY_FIELD(attack); SWAP_PARTY_FIELD(defense);
    SWAP_PARTY_FIELD(experience); SWAP_PARTY_FIELD(experience_to_next);
    SWAP_PARTY_FIELD(level); SWAP_PARTY_FIELD(growth);
    SWAP_PARTY_FIELD(other_stats); SWAP_PARTY_FIELD(damage_floor_stat);
    SWAP_PARTY_FIELD(status_guard);
#undef SWAP_PARTY_FIELD
}

int npe_world_load_script_walk_events(NpeWorld *world,const NpeMap *map) {
    NpeScriptWalkCache loaded[4];
    if(!world || !map)return -1;
    if(map->tail_size<703)return 0;
    memcpy(loaded,world->script_walk_events,sizeof(loaded));
    /*29:E5D0..E7DE overwrites present records only; the file ID and
      reserved byte+3 are discarded. Resident coordinates are words. */
    for(unsigned i=0;i<4;++i){
        NpeMapScriptWalkEvent event;
        int result=npe_map_script_walk_event(map,i+1,&event);
        if(result<0)return -1;
        if(!result)break;
        loaded[i]=(NpeScriptWalkCache){.x=event.x,.y=event.y,.marker=event.marker,
            .next_mode=event.next_mode,.next_id=event.next_id};
    }
    memcpy(world->script_walk_events,loaded,sizeof(loaded));
    return 0;
}

int npe_world_script_walk_event(const NpeWorld *world,unsigned id,NpeScriptWalkCache *event) {
    if(!world || !event)return -1;
    id=(uint8_t)id;
    if(!id || id>4)return 0;
    *event=world->script_walk_events[id-1];
    return 1;
}

int npe_world_load_npc_move_events(NpeWorld *world, const NpeMap *map) {
    NpeNpcMoveCache loaded[5];
    if (!world || !map) return -1;
    if (map->tail_size < 703) return 0;
    memcpy(loaded, world->npc_move_events, sizeof(loaded));
    /*29:D8BC..DAE0 only replaces records below group2's count. File ID
      and the reserved byte are consumed but are not cached service data. */
    for (unsigned i = 0; i < 5; ++i) {
        NpeMapNpcMoveEvent event;
        int result = npe_map_npc_move_event(map, i + 1, &event);
        if (result < 0) return -1;
        if (!result) break;
        loaded[i] = (NpeNpcMoveCache){.npc_id=event.npc_id,
            .next_id=event.next_id,.x=event.x,.y=event.y,.next_mode=event.next_mode};
    }
    memcpy(world->npc_move_events, loaded, sizeof(loaded));
    return 0;
}

int npe_world_npc_move_event(const NpeWorld *world, unsigned id,
                              NpeMapNpcMoveEvent *event) {
    id = (uint8_t)id;
    if (!world || !event) return -1;
    if (!id || id > 5) return 0;
    /*3C:8BAE..8BC8 selects a fixed slot using the low event byte,
      including slots absent from the current NPE map's directory. */
    const NpeNpcMoveCache *cached = &world->npc_move_events[id - 1];
    *event = (NpeMapNpcMoveEvent){.id=(uint16_t)id,.npc_id=cached->npc_id,
        .next_id=cached->next_id,.x=cached->x,.y=cached->y,.next_mode=cached->next_mode};
    return 1;
}

int npe_world_load_party_events(NpeWorld *world, const NpeMap *map) {
    NpeMapPartyEvent loaded[3];
    unsigned i;
    if (!world || !map) return -1;
    /* Existing fixed-tail maps have no indexed script directory. */
    if (map->tail_size < 703) return 0;
    memcpy(loaded, world->party_events, sizeof(loaded));
    /*29:DFB8..E12E reads only indices below group6's count. Remaining
      resident records survive map changes and successful warm loads. */
    for (i = 0; i < 3; ++i) {
        NpeMapPartyEvent event;
        int result = npe_map_party_event(map, i + 1, &event);
        if (result < 0) return -1;
        if (!result) break;
        loaded[i] = event;
    }
    memcpy(world->party_events, loaded, sizeof(loaded));
    return 0;
}

int npe_world_party_event(const NpeWorld *world, unsigned id,
                          NpeMapPartyEvent *event) {
    id = (uint8_t)id;
    if (!world || !event) return -1;
    if (!id || id > 3) return 0;
    /*3F:0F82..1466 uses the fixed resident table, including unused slots. */
    *event = world->party_events[id - 1];
    return 1;
}

int npe_world_change_party(NpeWorld *world, const NpeMapPartyEvent *event) {
    unsigned slot;
    if (!world || !event) return -1;
    /* Source3F:1038..1228 tests all three flags against exactly one.
       The remaining add branch selects slot1 by ID, otherwise slot2;
       it never moves or activates slot0. */
    if (event->add_member) {
        for (slot = 0; slot < 3; ++slot)
            if (world->party_ids[slot] == event->character_id &&
                world->party_active[slot] == 1) return 0;
        if (world->party_ids[1] == event->character_id) slot = 1;
        else if (world->party_active[1] == 1) slot = 2;
        else {
            world_swap_party_slots(world, 1, 2);
            slot = 1;
        }
        world->party_active[slot] = 1;
    } else {
        /* Source3F:122C..13DA compacts only the two companion records.
           An inactive record remains available for a later add event. */
        if (world->party_ids[1] == event->character_id &&
            world->party_active[1] == 1) {
            slot = 1;
            if (world->party_active[2] == 1) {
                world_swap_party_slots(world, 1, 2);
                slot = 2;
            }
        } else if (world->party_ids[2] == event->character_id &&
                   world->party_active[2] == 1) slot = 2;
        else return 0;
        world->party_active[slot] = 0;
    }
    return (int)slot + 1; /* Changed record supplies the original notice name. */
}

int npe_world_finish_party_event(NpeWorld *world, const NpeMapPartyEvent *event) {
    if (!world || !event) return -1;
    /* The followup is assigned after the timed notice,3F:13DC..1466. */
    world->game_mode = event->next_mode;
    world->story_event_id = event->next_id;
    if (world->game_mode == 14)
        npe_world_apply_story_event(world, world->story_event_id);
    else
        world_resolve_simple_mode(world);
    return 1;
}

int npe_world_apply_party_event(const NpeFile *file, NpeWorld *world) {
    NpeMapPartyEvent event;
    int result;
    if (!file || !world || world->game_mode != 10 ||
        !world->map_number)
        return -1;
    world->map_init_pending=1; /*22:0F6E..0F76 precedes the event range check. */
    result = npe_world_party_event(world, world->story_event_id, &event);
    if (result < 0) return -1;
    if (!result) { world->game_mode = 0; return 0; }
    if (npe_world_change_party(world, &event) < 0) return -1;
    return npe_world_finish_party_event(world, &event);
}

int npe_world_load_skill_events(NpeWorld *world,const NpeMap *map) {
    NpeMapSkillEvent loaded[2];
    if(!world||!map)return -1;
    if(map->tail_size<703)return 0;
    memcpy(loaded,world->skill_events,sizeof(loaded));
    for(unsigned i=0;i<2;++i){NpeMapSkillEvent e;int found=npe_map_skill_event(map,i+1,&e);
        if(found<0)return -1;
        if(!found)break;
        e.id=0;e.raw=NULL;loaded[i]=e;}
    memcpy(world->skill_events,loaded,sizeof(loaded));return 0;
}

int npe_world_skill_event(const NpeWorld *world,unsigned id,NpeMapSkillEvent *event) {
    if(!world||!event)return -1;
    id&=255u;if(!id||id>2)return 0;
    *event=world->skill_events[id-1];event->raw=NULL;return 1;
}

int npe_world_change_skill(NpeWorld *world,const NpeMapSkillEvent *event,unsigned *member) {
    unsigned slot;
    if(!world||!event||!member)return -1;
    for (slot = 0; slot < 3; ++slot)
        if (world->party_ids[slot] == event->character_id) break;
    /*40:9FF0 resets the candidate to0 after each failed comparison.
      All three character records are searched, including inactive ones. */
    if (slot == 3) slot=0;
    /* Source 40:A0C6..A1EA inserts the skill byte at 52:69DF and
       shifts the following 17 bytes of this character's skill area. */
    memmove(world->skill_slots[slot] + 1, world->skill_slots[slot], 17);
    world->skill_slots[slot][0] = event->skill_id;
    if (world->learned_skill_count[slot] < 18)
        world->learned_skill_count[slot]++;
    *member=slot;return 1;
}

int npe_world_finish_skill_event(NpeWorld *world,const NpeMapSkillEvent *event) {
    if(!world||!event)return -1;
    world->game_mode = event->next_mode;
    world->story_event_id = event->next_id;
    if (world->game_mode == 14)
        npe_world_apply_story_event(world, world->story_event_id);
    else
        world_resolve_simple_mode(world);
    return 1;
}

int npe_world_apply_skill_event(const NpeFile *file,NpeWorld *world) {
    NpeMapSkillEvent event;NpeSkill skill;unsigned member;int result;
    if(!file||!world||world->game_mode!=9||!world->map_number)return -1;
    world->map_init_pending=1;
    result=npe_world_skill_event(world,world->story_event_id,&event);
    if(result<=0){if(!result)world->game_mode=0;return result;}
    if(npe_world_change_skill(world,&event,&member)<0)return -1;
    if(npe_world_skill(file,world,event.skill_id,&skill))return -1;
    return npe_world_finish_skill_event(world,&event);
}

int npe_world_load_party_adjust_event(NpeWorld *world,const NpeMap *map) {
    NpeMapPartyAdjustEvent event;int result;
    if(!world||!map)return -1;
    if(map->tail_size<703)return 0;
    result=npe_map_party_adjust_event(map,1,&event);
    if(result<0)return -1;
    if(result){event.raw=NULL;world->party_adjust_event=event;}
    return 0;
}

int npe_world_party_adjust_event(const NpeWorld *world,unsigned id,NpeMapPartyAdjustEvent *event) {
    if(!world||!event)return -1;
    if((id&255u)!=1)return 0;
    *event=world->party_adjust_event;event->raw=NULL;return 1;
}

int npe_world_change_party_leader(NpeWorld *world,const NpeMapPartyAdjustEvent *event) {
    unsigned slot;
    if(!world||!event)return -1;
    for (slot = 1; slot < 3; ++slot)
        if (world->party_ids[slot] == event->character_id &&
            world->party_active[slot]==1) break;
    if (slot == 3) {
        world->game_mode = 0;
        return 0;
    }
    /* Source 46:F09A..F164 copies 0x94 bytes from slot 0 to a temporary
       buffer, selected slot to slot 0, then the buffer to selected slot. */
    world_swap_party_slots(world, 0, slot);
    /*46:F16A..F182 restores first-slot screen coordinates after copying
      all records. The world viewport remains at its original position. */
    return 1;
}

int npe_world_finish_party_adjust_event(NpeWorld *world,const NpeMapPartyAdjustEvent *event) {
    if(!world||!event)return -1;
    world->game_mode = event->next_mode;
    world->story_event_id = event->next_id;
    if (world->game_mode == 14)
        npe_world_apply_story_event(world, world->story_event_id);
    else
        world_resolve_simple_mode(world);
    return 1;
}

int npe_world_apply_mode13_event(const NpeFile *file,NpeWorld *world) {
    NpeMapPartyAdjustEvent event;int result;
    if(!file||!world||world->game_mode!=13||!world->map_number)return -1;
    world->map_init_pending=1;
    result=npe_world_party_adjust_event(world,world->story_event_id,&event);
    if(result<=0){if(!result)world->game_mode=0;return result;}
    result=npe_world_change_party_leader(world,&event);
    return result>0?npe_world_finish_party_adjust_event(world,&event):result;
}

int npe_world_load_teleport_events(NpeWorld *world,const NpeMap *map) {
    NpeMapTeleportEvent loaded[2];
    if(!world||!map)return -1;
    if(map->tail_size<703)return 0;
    memcpy(loaded,world->teleport_events,sizeof(loaded));
    for(unsigned i=0;i<2;++i){
        NpeMapTeleportEvent event;int result=npe_map_teleport_event(map,i+1,&event);
        if(result<0)return -1;
        if(!result)break;
        /*29:DDA0..DFAC discards the file ID; +2/+3 resident bytes are
          preserved, unused by36:839C. Only effective fields are modeled. */
        event.id=0;event.raw=NULL;loaded[i]=event;
    }
    memcpy(world->teleport_events,loaded,sizeof(loaded));return 0;
}

int npe_world_teleport_event(const NpeWorld *world,unsigned id,NpeMapTeleportEvent *event) {
    if(!world||!event)return -1;
    id&=255u;if(!id||id>2)return 0;
    *event=world->teleport_events[id-1];event->raw=NULL;return 1;
}

int npe_world_apply_teleport_event(const NpeFile *file, NpeWorld *world) {
    NpeMap destination;
    NpeMapTeleportEvent event;
    NpeViewport view;
    int result;
    if (!file || !world || world->game_mode != 16 ||
        !world->map_number)
        return -1;
    result = npe_world_teleport_event(world, world->story_event_id, &event);
    if (result < 0) return -1;
    if (!result) {
        world->game_mode = 0;
        return 0;
    }
    if (!event.destination_map ||
        npe_map(file, event.destination_map - 1, &destination) ||
        npe_map_spawn_viewport(&destination, event.x, event.y, &view))
        return -1;
    /*36:854C/8592 assigns the followup before48:21E0 loads the target.
      Mode14 runs after that partial cache load. A missing target slot
      retains its old source record; a provided target slot replaces it. */
    world->game_mode = event.next_mode;
    world->story_event_id = event.next_id;
    if (world->game_mode != 14)
        world_resolve_simple_mode(world);
    world->map_number = event.destination_map;
    world->x = event.x;
    world->y = event.y;
    world->view = view;
    if (event.destination_map < sizeof(world->condition5_bytes))
        world->condition5_bytes[event.destination_map] = 1;
    if (world_load_story_events(world, &destination)) return -1;
    npe_world_sound_map_loaded(world);
    memset(world->npc_override_x, 255, sizeof(world->npc_override_x));
    memset(world->npc_override_y, 255, sizeof(world->npc_override_y));
    memset(world->npc_override_direction, 255,
           sizeof(world->npc_override_direction));
    if(world->game_mode==14)
        npe_world_apply_story_event(world,world->story_event_id);
    /*48:2404..240C requests the map-controller redraw after the name
      notice. Preserve0/25 until the actual dispatcher normalizes them. */
    world->map_init_pending=1;
    return 1;
}

static int prepare_script_walk(const NpeFile *file,const NpeWorld *world,
                                NpeScriptWalkCache *event,int *steps,unsigned *direction) {
    NpeMap map;
    if(!file || !world || world->game_mode!=19 || !world->map_number ||
       npe_map(file,world->map_number-1,&map))return -1;
    int result=npe_world_script_walk_event(world,world->story_event_id,event);
    if(result<=0)return result;
    int dx=npc_move_signed_word(event->x-world->view.offset_x-world->view.screen_x);
    int dy=npc_move_signed_word(event->y-world->view.offset_y-world->view.screen_y+1);
    if(npc_move_abs_word(dx)>npc_move_abs_word(dy)){
        *steps=npc_move_abs_word(dx);*direction=dx>0?1u:3u;
    }else{
        *steps=npc_move_abs_word(dy);*direction=dy>0?0u:2u;
    }
    return 1;
}

int npe_world_apply_script_walk_event(const NpeFile *file,NpeWorld *world) {
    NpeScriptWalkCache event;int steps;unsigned direction;
    int result=prepare_script_walk(file,world,&event,&steps,&direction);
    if(result<0)return result;
    if(!result){world->game_mode=1;return 0;}
    /*47:03D4/043A starts a byte counter at0; count255 terminates,
      count256 wraps forever. A numeric request must not invent its end. */
    if(steps>=256)return NPE_SCRIPT_WALK_NONTERMINATING;
    NpeWorld moved=*world;moved.direction=(uint8_t)direction;
    for(int i=0;i<steps;++i)
        if(world_step_service(file,&moved,direction,NULL,NULL,NULL,1)<0)return -1;
    world_finish_script_followup(&moved,event.next_mode,event.next_id,0);
    *world=moved;
    return 1;
}

int npe_world_script_walk_with_display(const NpeFile *file,NpeWorld *world,
                                       uint8_t *pixels,NpeMapDisplayCommit commit,
                                       void *context) {
    if(!pixels)return npe_world_apply_script_walk_event(file,world);
    if(!world || world->game_mode!=19 || npe_render_world_load(file,world,pixels))return -1;
    /*47:0298 commits the entry map, then029C..02A4 sets7AB4. */
    if(commit && commit(context,pixels,0))return -2;
    world->map_init_pending=1;
    NpeScriptWalkCache event;int steps;unsigned direction;
    int result=prepare_script_walk(file,world,&event,&steps,&direction);
    if(result<0)return result;
    if(!result){world->game_mode=0;return 0;}
    world->direction=(uint8_t)direction;
    for(unsigned i=0;(int)i<steps;i=(uint8_t)(i+1)){
        result=world_step_service(file,world,direction,pixels,commit,context,1);
        if(result<0)return result;
        /*47:03E2/0448 waits15 after the complete step, including a
          blocked attempt. Waiting does not submit another LCD frame. */
        if(commit && commit(context,NULL,15))return -2;
    }
    world_finish_script_followup(world,event.next_mode,event.next_id,1);
    return 1;
}

int npe_world_finish_animation_event(const NpeFile *file, NpeWorld *world) {
    NpeMapAnimationEvent event;
    int result;
    if (!file || !world || world->game_mode != 15 ||
        !world->map_number)
        return -1;
    world->map_init_pending=1;
    result = npe_world_scene_event(world, world->story_event_id, &event);
    if (result < 0) return -1;
    if (!result) {
        world->game_mode = 0;
        return 0;
    }
    /* Source 4A:3628..37E0 invokes a scene-animation service, then
       copies RAM 52:7554/754E (file bytes +10/+11) to mode/event.
       SDL draws the resource frames separately. The frame compositor and
       scene audio are verified independently; this reducer follows events. */
    world->game_mode = event.next_mode;
    world->story_event_id = event.next_id;
    if (world->game_mode == 14)
        npe_world_apply_story_event(world, world->story_event_id);
    else
        world_resolve_simple_mode(world);
    if (!world->game_mode || world->game_mode > 24) world->game_mode = 1;
    return 1;
}

int npe_world_prepare_question(const NpeFile *file, NpeWorld *world,
                               NpeQuizQuestion *question) {
    unsigned first, count, selected;
    uint16_t event_id;
    if (!file || !world || !question || world->game_mode != 8)
        return -1;
    if (world->quiz_ready)
        return npe_quiz_question(file, world->quiz_question_index, question);
    /* Source 2C:BF14/BF8A replaces 797A/797C with question text. */
    world->mode2_ready = 0;
    event_id = world->story_event_id;
    if (event_id < 10) {
        if (npe_quiz_category_range(file, event_id, &first, &count) ||
            !count) return -2;
        /* Source 2C:BEB2..BECC retries the draw until it differs from
           52:7ADE. A one-question category cannot yield another ID. */
        do {
            selected = first + world_random15(world) % count;
        } while (count > 1 && selected == world->quiz_previous_index);
    } else selected = event_id;
    if (npe_quiz_question(file, selected, question)) return -1;
    world->quiz_category = event_id;
    world->quiz_input_type = question->input_type;
    world->quiz_question_index = (uint16_t)selected;
    if (event_id < 10) world->quiz_previous_index = (uint16_t)selected;
    /* Original 2C:C752/C7A8 initializes the grid or ASCII input. */
    world->event_context_id = 0;
    world->quiz_input_length = 0;
    world->quiz_input[0] = 0;
    if (question->input_type) world->quiz_previous_index = 0;
    world->lcd_background = 0;
    world->story_event_id = (uint16_t)selected;
    world->quiz_ready = 1;
    return 0;
}

int npe_world_finish_quiz(NpeWorld *world, uint16_t *award_out) {
    if (!world || world->game_mode != 8 || !world->quiz_ready ||
        world->quiz_category >= 10) return -1;
    /* Original 4A:1EF8 uses the positive branch even for a wrapped score. */
    uint16_t score = (uint16_t)(world->quiz_answer_count -
                               2u * world->quiz_wrong_count);
    uint16_t award = (uint16_t)(score * (world->quiz_category + 3u));
    uint64_t money = (uint64_t)world->money + award;
    world->money = money > 999999u ? 999999u : money;
    if (award_out) *award_out = award;
    world->quiz_answer_count = 0;
    world->quiz_wrong_count = 0;
    world->game_mode = 1;
    world->story_event_id = 0;
    world->quiz_ready = 0;
    return 0;
}

static int world_quiz_result(NpeWorld *world, const NpeQuizQuestion *question,
                             unsigned result) {
    if (world->quiz_category < 10) {
        uint16_t *counter = result == 0 ?
            &world->quiz_category_correct[world->quiz_category] :
            &world->quiz_category_wrong[world->quiz_category];
        if (*counter != UINT16_MAX) ++*counter;
        ++world->quiz_answer_count;
        if (result != 0) ++world->quiz_wrong_count;
        if (world->quiz_wrong_count >= 3) {
            npe_world_finish_quiz(world, NULL);
        } else {
            world->game_mode = 8;
            world->story_event_id = world->quiz_category;
        }
    } else {
        uint16_t *counter = result == 0 ?
            &world->quiz_fixed_correct : &world->quiz_fixed_wrong;
        if (*counter != UINT16_MAX) ++*counter;
        /* Fixed questions update 76AC/76AE, never the session counter. */
        world->game_mode = question->followup_mode[result];
        world->story_event_id = question->followup_event_id[result];
    }
    world->quiz_ready = 0;
    world_resolve_simple_mode(world);
    if (!world->game_mode || world->game_mode > 24)
        world->game_mode = 1;
    return result == 0 ? 1 : 0;
}

int npe_world_answer_question(const NpeFile *file, NpeWorld *world,
                              unsigned answer_index) {
    NpeQuizQuestion question;
    if (!file || !world || world->game_mode != 8 ||
        answer_index > 5 || !world->quiz_ready ||
        npe_quiz_question(file, world->quiz_question_index, &question))
        return -1;
    return world_quiz_result(world, &question,
                            answer_index == question.correct_answer ? 0 : 1);
}

int npe_world_answer_text(const NpeFile *file, NpeWorld *world) {
    NpeQuizQuestion question;
    if (!file || !world || world->game_mode != 8 || !world->quiz_ready ||
        npe_quiz_question(file, world->quiz_question_index, &question) ||
        !question.input_type) return -1;
    /* Original selects A when correct=0, B otherwise; strcmp is exact. */
    unsigned answer = question.correct_answer ? 2 : 1;
    size_t length = strlen(world->quiz_input);
    unsigned wrong = length != question.text_size[answer] ||
        memcmp(world->quiz_input, question.text[answer], length) != 0;
    return world_quiz_result(world, &question, wrong);
}

int npe_world_quiz_key(const NpeFile *file, NpeWorld *world, unsigned key) {
    NpeQuizQuestion question;
    if (!file || !world || world->game_mode != 8 || !world->quiz_ready ||
        npe_quiz_question(file, world->quiz_question_index, &question)) return -1;
    if (question.input_type) {
        int ascii = (key >= 0x28 && key <= 0x39 && key != 0x2c) ||
                    (key >= 'a' && key <= 'z') ||
                    (key >= 'A' && key <= 'Z') || key == ' ';
        if (ascii && world->quiz_input_length < 32) {
            world->quiz_input[world->quiz_input_length++] = (char)key;
            world->quiz_input[world->quiz_input_length] = 0;
            return 1;
        }
        if (key == 0x89 && world->quiz_input_length) {
            world->quiz_input[--world->quiz_input_length] = 0;
            return 1;
        }
        if (key == 0x86) return 2;
    } else {
        unsigned selected = world->event_context_id;
        switch (key) {
        case 0x80: case 'j': case 'J': if (selected & 1) --selected; break;
        case 0x81: case 'l': case 'L': if (!(selected & 1)) ++selected; break;
        case 0x82: case 'i': case 'I': if (selected >= 2) selected -= 2; break;
        case 0x83: case 'm': case 'M': if (selected < 2) selected += 2; break;
        case 0x86: case 'k': case 'K': return 2;
        default: selected = UINT_MAX; break;
        }
        if (selected != UINT_MAX) { world->event_context_id = (uint16_t)selected; return 1; }
    }
    return key == 0x8e && world->quiz_category < 10 ? 3 : 0;
}

static uint16_t level_stat_add(uint16_t value, uint8_t growth) {
    /*44:C9BA..CBB8 first wraps each ADD word, then uses signed SLE. */
    uint16_t total=(uint16_t)(value+growth);
    return (int16_t)total>999 ? 999 : total;
}

int npe_world_apply_experience_event(NpeWorld *world) {
    unsigned gain, total, threshold, level;
    if (!world || world->game_mode != 23) return -1;
    threshold = world->experience_to_next[0];
    /* 44:C8DC UMUL writes R6:R7, then C8E2 uses the 16-bit UDIV form
       (opcode C3), which divides only the low product word in R7. */
    gain = (uint16_t)(threshold * world->story_event_id) / 100u;
    /*C8E4 ADD is16-bit; only that wrapped result is widened at C8E8. */
    total = (uint16_t)(world->experience[0] + gain);
    world->experience[0] = (uint16_t)total;
    level = world->level[0];
    if (total >= threshold && level < 99) {
        world->experience[0] = (uint16_t)(world->experience[0] - threshold);
        world->experience_to_next[0] =
            (uint16_t)(3u * (level * level + 5u * level) - 8u);
        world->level[0] = (uint8_t)(level + 1);
        world->damage_floor_stat[0] = world->level[0];
        world->max_hp[0] = level_stat_add(world->max_hp[0], world->growth[0][0]);
        world->hp[0] = world->max_hp[0];
        world->max_qi[0] = level_stat_add(world->max_qi[0], world->growth[0][1]);
        world->qi[0] = world->max_qi[0];
        world->attack[0] = level_stat_add(world->attack[0], world->growth[0][2]);
        world->defense[0] = level_stat_add(world->defense[0], world->growth[0][3]);
        if ((int16_t)(uint16_t)(world->other_stats[0][0] +
            world->growth[0][4]) > 999)
            world->other_stats[0][0] = 999;
        else
            /* Source 44:CB16..CB2C reads 6A24 on the uncapped path. */
            world->other_stats[0][0] = world->defense[0];
        world->other_stats[0][1] = level_stat_add(world->other_stats[0][1],
                                                  world->growth[0][5]);
        world->other_stats[0][2] = level_stat_add(world->other_stats[0][2],
                                                  world->growth[0][6]);
    }
    world->game_mode = 1;
    world->map_init_pending = 1; /*44:CBD0, including the no-upgrade path. */
    return level > 0 && world->level[0] != level ? 1 : 0;
}

static int world_walk_old_display(const NpeFile *file,const NpeWorld *world,
                                   int blocked,uint8_t *pixels,
                                   NpeMapDisplayCommit commit,void *context) {
    if(!pixels)return 0;
    int immediate=blocked ? npe_render_world_walk_blocked(file,world,pixels) : 0;
    if(immediate<0)return -1;
    if(immediate)return commit && commit(context,pixels,0) ? -2 : 0;
    for(unsigned pose=0;pose<4;++pose){
        if(npe_render_world_walk_pose(file,world,pose,pixels))return -1;
        if(commit && commit(context,pixels,10))return -2;
    }
    return 0;
}

static void world_walk_viewport(const NpeMap *map,NpeWorld *world,unsigned direction) {
    NpeViewport *view=&world->view;
    /* Source3B:1682..18B6 updates the existing viewport by one cell.
       Map-entry centering (48:2290) is a different service. */
    if(direction==0){
        if(view->screen_y<4)++view->screen_y;
        else if(view->offset_y+7<map->height)++view->offset_y;
        else if(view->screen_y+1<7)++view->screen_y;
    }else if(direction==1){
        if(view->screen_x<7)++view->screen_x;
        else if(view->offset_x+15<map->width)++view->offset_x;
        else if(view->screen_x+1<15)++view->screen_x;
    }else if(direction==2){
        if(view->screen_y>4)--view->screen_y;
        else if(view->offset_y>0)--view->offset_y;
        /* Native top guard tests screen_y-1 >0, so row1 stays row1. */
        else if(view->screen_y>1)--view->screen_y;
    }else{
        if(view->screen_x>7)--view->screen_x;
        else if(view->offset_x>0)--view->offset_x;
        else if(view->screen_x>0)--view->screen_x;
    }
    world->x=view->offset_x+view->screen_x;
    world->y=view->offset_y+view->screen_y;
}

static int world_monster_in_story(const NpeMapMonster *monster,unsigned progress) {
    return (!monster->min_progress && !monster->max_progress) ||
           (progress>=monster->min_progress && progress<=monster->max_progress);
}

static int world_exit_load(const NpeFile *file,NpeWorld *world,const NpeMapExit *exit,
                           int second,int followup) {
    NpeMap target;NpeViewport view;
    unsigned number=second ? exit->second_map : exit->destination_map;
    unsigned x=second ? exit->second_x : exit->destination_x;
    unsigned y=second ? exit->second_y : exit->destination_y;
    if(!number || npe_map(file,number-1,&target) || npe_map_spawn_viewport(&target,x,y,&view))return -1;
    world->map_number=(uint16_t)number;world->x=(uint16_t)x;world->y=(uint16_t)y;world->view=view;
    if(number<sizeof(world->condition5_bytes))world->condition5_bytes[number]=1;
    if(world_load_story_events(world,&target))return -1;
    npe_world_sound_map_loaded(world);
    /* Automatic36:8736/8CDA assigns mode/event; confirmation37:19EA
       selects map and spawn only, retaining mode/event and7AC4. */
    if(followup){world->game_mode=second ? exit->second_mode : exit->next_mode;
        world->story_event_id=second ? exit->second_id : exit->next_id;
        if(!world->game_mode)world->game_mode=1;}
    memset(world->npc_override_x,255,sizeof(world->npc_override_x));
    memset(world->npc_override_y,255,sizeof(world->npc_override_y));
    memset(world->npc_override_direction,255,sizeof(world->npc_override_direction));
    return NPE_MOVE_EXIT;
}

int npe_world_select_exit(NpeWorld *world,unsigned choice) {
    if(!world || world->game_mode!=1 || !world->exit_choice || choice<1 || choice>2)return -1;
    world->exit_choice=(uint8_t)choice;return 0;
}

int npe_world_confirm_exit(const NpeFile *file,NpeWorld *world) {
    NpeMap map;NpeMapExit exit;
    if(!world || world->game_mode!=1 || !world->exit_choice || !world->map_number ||
       npe_map(file,world->map_number-1,&map) || npe_world_exit(world,&map,world->exit_slot,&exit))return -1;
    return world_exit_load(file,world,&exit,world->exit_choice!=1,0);
}

static int world_step_service(const NpeFile *file,NpeWorld *world,
                                unsigned direction,uint8_t *pixels,
                                NpeMapDisplayCommit commit,void *context,int scripted) {
    NpeMap map;
    NpeMapMoveTrigger trigger;
    int x, y, triggered,display_result,move_result=NPE_MOVE_OK;
    NpeWorld before;
    unsigned i;
    if (!world || direction > 3 || !world->map_number ||
        npe_map(file, world->map_number - 1, &map)) return -1;
    if (!scripted && (world->game_mode != 1 || world->exit_choice)) return NPE_MOVE_BLOCKED;
    if (!scripted && npe_world_update_tiles(world) < 0) return -1;
    world_bind_tiles(world, &map);
    world->direction = direction;
    /* Source3A:001C..0044 derives the collision origin from the viewport,
       including the bottom-edge adjustment made on map entry. */
    x = (int)(world->view.offset_x+world->view.screen_x) + (direction == 1) - (direction == 3);
    y = (int)(world->view.offset_y+world->view.screen_y) + (direction == 0) - (direction == 2);
    if (x < 0 || y < 0 || x >= map.width || y >= map.height)
        goto blocked;
    /* Source3A:0106..037A: object positions are tested first without
       an ID/taken gate, then active NPCs. Both precede exits and events.
       Compare the native flattened position, as the original does. */
    for (i = 0; i < 8; ++i) {
        NpeMapObject object;
        if (npe_world_object(world, &map, i, &object)) return -1;
        if ((unsigned)y * map.width + (unsigned)x ==
            (unsigned)object.y * map.width + object.x)
            goto blocked;
    }
    for (i = 0; i < 20; ++i) {
        const uint8_t *npc;
        unsigned npc_x, npc_y;
        if (npe_world_npc(world, &map, i, &npc)) return -1;
        if (!npe_map_npc_active(npc, world->story_progress)) continue;
        if (npe_world_npc_position(world, npc, i, &npc_x, &npc_y)) return -1;
        if ((unsigned)y * map.width + (unsigned)x == npc_y * map.width + npc_x)
            goto blocked;
    }
    for (i = 0; i < 12; ++i) {
        NpeMapExit exit;
        if (npe_world_exit(world, &map, i, &exit)) break;
        /* Source3A:03C2..03FC compares flattened positions for all slots,
           including a zero destination. The controller tests progress next. */
        if ((unsigned)y*map.width+(unsigned)x!=(unsigned)exit.y*map.width+exit.x) continue;
        if(scripted){
            /*3B:1656 moves for nonzero collision kinds, including exits;
              the map controller37:1778 is not called by mode19. */
            world->exit_slot=(uint8_t)i;
            goto walk_unblocked;
        }
        display_result=world_walk_old_display(file,world,1,pixels,commit,context);
        if(display_result)return display_result;
        /* Source 37:1778..17C6 compares 7A66 (story progress) with the
           exit's 6B88 word before loading the destination map. */
        world->exit_slot=(uint8_t)i;
        if (world->story_progress < exit.min_progress){
            /*37:17CE adds a static player submission on denied progress. */
            if(pixels){
                if(npe_render_world_static_player(file,world,pixels))return -1;
                if(commit && commit(context,pixels,0))return -2;
            }
            return NPE_MOVE_EXIT_DENIED;
        }
        world->encounter_steps=0;world->exit_choice=0;
        if(exit.kind && world->story_progress>=exit.second_min_progress){
            world->exit_choice=1;return NPE_MOVE_EXIT_CHOICE;
        }
        if(world_exit_load(file,world,&exit,0,1)<0)return -1;
        if(pixels){
            if(npe_render_world_load(file,world,pixels))return -1;
            if(commit && commit(context,pixels,0))return -2;
        }
        return NPE_MOVE_EXIT;
    }
    for (i = 0; i < 4; ++i) {
        NpeMapMonster monster;
        if (map.tail_size < 644) break;
        if (npe_world_monster(world, &map, i, &monster)) return -1;
        /* Source3A:045C..05EE gates collision on NPC ID and story range,
           independently of graphic or enemy ID. */
        if (monster.npc_id && world_monster_in_story(&monster,world->story_progress) &&
            (unsigned)monster.y * map.width + monster.x ==
                (unsigned)y * map.width + (unsigned)x)
            goto blocked;
    }
    /* Source3A:0602..0760 reaches triggers after fixed monsters, and
       only falls through to the terrain service when no trigger matches. */
    triggered = npe_world_move_trigger(world,&map,(unsigned)x,(unsigned)y,&trigger);
    if (triggered < 0) return -1;
    if (triggered) {
        display_result=world_walk_old_display(file,world,1,pixels,commit,context);
        if(display_result)return display_result;
        world->game_mode = trigger.mode;
        world->story_event_id = trigger.event_id;
        /* Collision3A:073A..0742 requests fresh initialization even when
           the trigger enters mode2 after a previous dialogue has closed. */
        world->map_init_pending=1;
        /* Collision3A:0698..0748 leaves kind0. The caller still runs
           encounter/chase before dispatching the resulting mode, which
           may have been replaced by an ordinary battle. */
        move_result=NPE_MOVE_EVENT;
        goto continuation;
    }
    if (!npe_tile_base_passable(npe_map_tile(&map, (unsigned)x, (unsigned)y)))
        goto blocked;
walk_unblocked:
    display_result=world_walk_old_display(file,world,0,pixels,commit,context);
    if(display_result)return display_result;
    before=*world;
    world_walk_viewport(&map,world,direction);
    if(pixels){
        display_result=npe_render_world_walk_finish(file,&before,world,pixels,commit,context);
        if(display_result)return display_result;
    }
continuation:
    if(scripted)return move_result;
    /* Source37:176E tests exit kind3. Both ordinary kind1 and blocked
       kind0 fall through encounter-first, then chase, after walking LCD
       commits. Accepted/denied exits return through their own controller. */
    triggered = world_random_encounter(file, world, &map);
    if (triggered < 0) return -1;
    if (triggered) return NPE_MOVE_EVENT;
    before=*world;
    triggered = npe_world_update_chase(file, world);
    if (triggered < 0) return -1;
    if(pixels && render_world_chase_only(file,&before,world,pixels))return -1;
    if (triggered) return NPE_MOVE_EVENT;
    if(move_result==NPE_MOVE_EVENT){
        /* Source4A:F48E..F4A0/F5EE..F602 normalizes unhandled mode
           bytes after the encounter/chase continuation, not at collision. */
        if(!world->game_mode || world->game_mode>24)world->game_mode=1;
        if(world->game_mode==14)npe_world_apply_story_event(world,world->story_event_id);
        else world_resolve_simple_mode(world);
    }
    return move_result;
blocked:
    display_result=world_walk_old_display(file,world,1,pixels,commit,context);
    if(display_result)return display_result;
    move_result=NPE_MOVE_BLOCKED;
    goto continuation;
}

int npe_world_step_with_display(const NpeFile *file,NpeWorld *world,
                                unsigned direction,uint8_t *pixels,
                                NpeMapDisplayCommit commit,void *context) {
    return world_step_service(file,world,direction,pixels,commit,context,0);
}

int npe_world_step(const NpeFile *file,NpeWorld *world,unsigned direction) {
    return npe_world_step_with_display(file,world,direction,NULL,NULL,NULL);
}

static int world_facing_npc(const NpeFile *file,const NpeWorld *world,
                            unsigned *npc_id,uint8_t *context) {
    NpeMap map;
    int x, y;
    unsigned i;
    if (!world || !npc_id || !world->map_number ||
        npe_map(file, world->map_number - 1, &map)) return -1;
    /*33:37F0/37F6 and3F10/3F16 skip every slot for directions outside
      0..3 and finally returnFFFF. It is a no-target result, not an error. */
    if (world->direction > 3) return 0;
    /*33:3772..3796 uses the current viewport, including the adjusted
      bottom row after loading a map, rather than its requested spawn. */
    x = (int)(world->view.offset_x+world->view.screen_x) + (world->direction == 1) -
        (world->direction == 3);
    y = (int)(world->view.offset_y+world->view.screen_y) + (world->direction == 0) -
        (world->direction == 2);
    for (i = 0; i < 20; ++i) {
        const uint8_t *npc;
        unsigned id;
        unsigned npc_x, npc_y;
        if (npe_world_npc(world, &map, i, &npc)) break;
        if (!npe_map_npc_active(npc, world->story_progress)) continue;
        id = (unsigned)npc[0] << 8 | npc[1];
        if (npe_world_npc_position(world, npc, i, &npc_x, &npc_y)) return -1;
        if (npc_x == (unsigned)x && npc_y == (unsigned)y) {
            *npc_id = id;
            if(context)*context=npc[12];
            return 1;
        }
    }
    for (i = 0; i < 4; ++i) {
        NpeMapMonster monster;
        if (map.tail_size < 644) break;
        if (npe_world_monster(world, &map, i, &monster)) return -1;
        if (npe_map_monster_active(&monster, world->story_progress) &&
            monster.x == (unsigned)x && monster.y == (unsigned)y) {
            *npc_id = monster.npc_id;
            if(context)*context=0; /*33:4064/41CC/4334/449C clears7A68. */
            return 1;
        }
    }

    return 0;
}

int npe_world_facing_npc(const NpeFile *file,const NpeWorld *world,unsigned *npc_id) {
    return world_facing_npc(file,world,npc_id,NULL);
}

int npe_world_begin_talk(const NpeFile *file,NpeWorld *world) {
    unsigned id;
    uint8_t context=0;
    if(!world || world->game_mode!=1)return -1;
    int found=world_facing_npc(file,world,&id,&context);
    if(found!=1)return found;
    /*37:139A..13E8 runs before the7AC4 direction/confirmation gates. */
    world->game_mode=2;world->story_event_id=(uint16_t)id;world->mode2_ready=0;
    world->map_init_pending=1; /*37:13E0..13E8 enters the NPC controller afresh. */
    world->mode2_context=context;
    return 1;
}

int npe_world_object_action(const NpeFile *file, NpeWorld *world,
                            NpeMapObject *object) {
    return npe_world_object_action_with_display(file,world,object,NULL,NULL);
}

int npe_world_object_action_with_display(const NpeFile *file,NpeWorld *world,
                                         NpeMapObject *object,NpeMapDisplayCommit commit,
                                         void *context) {
    NpeMap map;
    int x, y;
    unsigned i;
    if (!world || !object || !world->map_number ||
        npe_map(file, world->map_number - 1, &map)) return -1;
    /*46:CEAE/CEB4 skips the object slots and returnsFFFF for >3. */
    if (world->direction > 3) return 0;
    /*46:CE3A..CE62 selects from the actual view. A requested spawn can
      differ from that position after the bottom-edge map adjustment. */
    x = (int)(world->view.offset_x + world->view.screen_x) + (world->direction == 1) -
        (world->direction == 3);
    y = (int)(world->view.offset_y + world->view.screen_y) + (world->direction == 0) -
        (world->direction == 2);
    for (i = 0; i < 8; ++i) {
        NpeMapObject candidate;
        if (npe_world_object(world, &map, i, &candidate)) return -1;
        /* Source46:CE9C returns FFFF at the first zero ID; it does not
           continue to later slots. Mismatched positions use D094's loop. */
        if (!candidate.id) return 0;
        if (candidate.x != x || candidate.y != y) continue;
        if (candidate.item_index >= sizeof(world->condition_words) /
                                    sizeof(world->condition_words[0])) return -2;
        int flag=npe_world_object_flag(world,candidate.id);
        if(flag<0)return flag;
        if(flag)return 0;
        /*37:14F0 commits the flag,25:3082 replaces the cell with53.
          37:156A then compares the signed word against99; negatives
          increment with16-bit wrap, and capped pickups still disappear. */
        if(world_object_flag_set(world,candidate.id))return -2;
        if(npe_rpg_runtime_owner(file)==world &&
           npe_rpg_runtime_map_current(file,world->map_number)==1){
            /* The flag write can alias this record's coordinates. Native
               type1 reads the live slot and width after that write. */
            uint8_t coordinates[2],width;
            if(npe_rpg_runtime_read(file,UINT32_C(0x5275a4)+8u*i,coordinates,2) ||
               npe_rpg_runtime_read(file,UINT32_C(0x527a56),&width,1))return -1;
            uint16_t index=(uint16_t)(coordinates[1]*width+coordinates[0]);
            if(npe_rpg_runtime_map_objects_clear(file,(uint16_t)i)<0 ||
               world_record_tile(world,index,53))return -1;
        }else if(world_write_tile(world, (uint16_t)(candidate.y * map.width + candidate.x), 53))return -1;
        *object = candidate;
        if(commit){
            uint8_t pixels[NPE_WIDTH*NPE_HEIGHT];
            if(npe_render_world_load(file,world,pixels))return -1;
            if(commit(context,pixels,0))return -2;
        }
        if ((int16_t)world->condition_words[candidate.item_index] < 99)
            world->condition_words[candidate.item_index]++;
        return 1;
    }
    return 0;
}

int npe_world_use_item(const NpeFile *file, NpeWorld *world,
                       unsigned item_id, unsigned character) {
    NpeItem item;
    if (!file || !world || !item_id ||
        item_id >= sizeof(world->condition_words) /
                   sizeof(world->condition_words[0]) ||
        character >= 3 || !world->party_active[character] ||
        (int16_t)world->condition_words[item_id] <= 0 ||
        npe_world_item(file, world, item_id, &item)) return -1;
    /* Source2F:15F6/1654/19A8: map use rejects equipment, handles
       type24 through the separate map quest table, then gates side==1.
       Source30:9444 is the common map service, not the battle service. */
    if (item.effect_type >= 25) return 0;
    if (item.effect_type == 24) {
        NpeMap map;
        if(npe_map(file,world->map_number-1,&map))return -1;
        for(unsigned slot=0;slot<5;++slot){
            NpeMapQuestItem event;int result=npe_map_quest_item(&map,slot,&event);
            if(result<0)return -1;
            if(!result)break;
            if(event.item_id!=item_id || event.min_progress>world->story_progress)continue;
            /* Both zero disables the coordinate check. A single zero
               is a literal coordinate, not a wildcard. */
            if((event.x || event.y) && (event.x!=world->x || event.y!=world->y))continue;
            if(event.npc_id){
                unsigned npc_id=0;
                result=npe_world_facing_npc(file,world,&npc_id);
                if(result<0)return -1;
                if(!result || npc_id!=event.npc_id)continue;
            }
            world->condition_words[item_id]--;
            world->game_mode=event.next_mode;world->story_event_id=event.next_id;
            world->event_context_id=event.npc_id;
            world->event_blank_background=1;
            world->mode2_ready=0;
            if(world->game_mode==14)
                return npe_world_apply_story_event(world,world->story_event_id)==1?1:-1;
            if(!world->game_mode)world->game_mode=1;
            world_resolve_simple_mode(world);
            return 1;
        }
        return -3;
    }
    if (item.target_side != 1) return -2;
    if (item.effect_type == 1) {
        /* Single and group paths add low words, cap only by signed
           upper comparison, and include fallen active members. The
           three other stats change; prepared ATK/DEF words do not. */
        for (unsigned member = 0; member < 3; ++member) {
            if (world->party_active[member] != 1 ||
                (!item.group && member != character)) continue;
            uint16_t hp = (uint16_t)(world->hp[member] + item.hp_restore);
            uint16_t qi = (uint16_t)(world->qi[member] + item.qi_restore);
            world->hp[member] = (int16_t)hp > (int16_t)world->max_hp[member] ? world->max_hp[member] : hp;
            world->qi[member] = (int16_t)qi > (int16_t)world->max_qi[member] ? world->max_qi[member] : qi;
            for (unsigned stat = 0; stat < 3; ++stat) {
                unsigned offset = 21 + 2 * stat;
                uint16_t delta = (uint16_t)((unsigned)item.raw[offset] << 8 | item.raw[offset+1]);
                world->other_stats[member][stat] = (uint16_t)(world->other_stats[member][stat] + delta);
            }
        }
    } else if (item.effect_type == 2) {
        if (item.group) {
            /* Original target=-1 group service aliases inventory 212,
               uses member index as state/effect index and repeats four
               times. Preserve the same bug as group map spells. */
            for (unsigned member = 0; member < 3; ++member)
                if (world->party_active[member] == 1)
                    world->condition_words[212+member] = (uint16_t)(
                        world->condition_words[212+member] +
                        4u * (uint16_t)item.status_effects[member]);
        } else {
            for (unsigned status = 0; status < 4; ++status)
                world->status_guard[character][status] = (uint16_t)(
                    world->status_guard[character][status] +
                    (uint16_t)item.status_effects[status]);
        }
    }
    /* Other friendly kinds below24 fall through the map service and
       are still consumed by its caller, including types16..19. */
    world->condition_words[item_id]--;
    return 1;
}

/* Source 2F:1D70..2A50 maps effect types 25..30 to the six bytes at
   52:69D8..69DD in this order. The item record's +17/+19 signed words
   are the attack/defense modifiers shown by Tianjiang's equipment data. */
static int world_rebuild_equipment_status(const NpeFile *file, NpeWorld *world,
                                           unsigned character) {
    /* Source39:1974..20A2 scans 69D8,69DB,69D9,69DA,69DC,69DD.
       Positive signed item state words replace the map state in that
       order. Zero/negative words leave it intact; removing an item does
       not clear its previous state. This is separate from stat deltas. */
    static const uint8_t order[6] = {0,3,1,2,4,5};
    for (unsigned i = 0; i < 6; ++i) {
        unsigned id = world->battle_party_flags_saved ?
            world->battle_map_records[character][18+order[i]] : world->equipped_items[character][order[i]];
        NpeItem item;
        /* Original loads all six slots, including ID0 cache clears. */
        if (npe_world_item(file,world,id,&item)) return -1;
        for (unsigned status = 0; status < 4; ++status)
            if (item.status_effects[status] > 0)
                world->status_guard[character][status] = (uint16_t)item.status_effects[status];
    }
    return 0;
}

static int world_apply_equipment_attributes(const NpeFile *file, NpeWorld *world,
                                              unsigned character,
                                              unsigned item_id, int factor) {
    NpeItem loaded;
    if(npe_world_item(file,world,item_id,&loaded))return -1;
    const NpeItem *item=&loaded;
    /* Source30:9562..9602 prepares all seven item words times +/-1.
       A106..A5DA adds low words, caps HP/QI by signed upper comparison
       only, and leaves ATK/DEF/three other stats unbounded. Maxima do
       not change. Each removal/application is a separate service call. */
    uint16_t hp = (uint16_t)(world->hp[character] + factor * (int32_t)item->hp_restore);
    uint16_t qi = (uint16_t)(world->qi[character] + factor * (int32_t)item->qi_restore);
    world->hp[character] = (int16_t)hp > (int16_t)world->max_hp[character] ? world->max_hp[character] : hp;
    world->qi[character] = (int16_t)qi > (int16_t)world->max_qi[character] ? world->max_qi[character] : qi;
    world->attack[character] = (uint16_t)(world->attack[character] + factor * (int32_t)item->attack_bonus);
    world->defense[character] = (uint16_t)(world->defense[character] + factor * (int32_t)item->defense_bonus);
    for (unsigned stat = 0; stat < 3; ++stat) {
        unsigned offset = 21 + 2 * stat;
        uint16_t value = (uint16_t)((unsigned)item->raw[offset] << 8 | item->raw[offset + 1]);
        world->other_stats[character][stat] = (uint16_t)(
            world->other_stats[character][stat] + factor * (int32_t)value);
    }
    return world_rebuild_equipment_status(file, world, character);
}

int npe_world_equip_item(const NpeFile *file, NpeWorld *world,
                         unsigned item_id, unsigned character) {
    static const uint8_t slots[6] = {0, 2, 4, 1, 5, 3};
    NpeItem item, old_item;
    unsigned slot, old_id;
    if (!file || !world || character >= 3 || !world->party_active[character] ||
        !item_id || item_id >= sizeof(world->condition_words) /
                               sizeof(world->condition_words[0]) ||
        (int16_t)world->condition_words[item_id] <= 0 ||
        npe_world_item(file,world,item_id,&item)) return -1;
    /*2F:1AF4..1BD4 gates the character sex byte before type dispatch.
      This byte is streamed from NPE character+12 to resident69D7. */
    if(item.flags==1 || item.flags==2){
        unsigned sex=world->party_characters[character].sex;
        if((item.flags==1 && sex==2) || (item.flags==2 && sex==1))return -2;
    }
    if(item.effect_type==31)return -3;
    if (item.effect_type < 25 || item.effect_type > 30) return 0;
    unsigned type=item.effect_type;
    slot = slots[item.effect_type - 25];
    old_id = world->equipped_items[character][slot];
    /* Same-ID jumps through the remaining type tests to2F:2A92. */
    if(old_id==item_id)return 0;
    /* Source2F:1D70..2A50 refunds and removes the old effect while its
       slot still contains the old ID. That call rebuilds states before
       the slot changes; the caller then applies the newly equipped ID. */
    if(old_id!=item_id){
        if(npe_world_item(file,world,old_id,&old_item))return -1;
        if(old_item.effect_type==type){
            world->condition_words[old_id]++;
            if(world_apply_equipment_attributes(file,world,character,old_id,-1))return -1;
        }
        world->condition_words[item_id]--;
        world->equipped_items[character][slot]=(uint8_t)item_id;
    }
    /*2F:2A52 applies+1 after a slot replacement. */
    if(world_apply_equipment_attributes(file,world,character,item_id,1))return -1;
    return 1;
}

static int world_unequip_id(const NpeFile *file,NpeWorld *world,
                            unsigned character,unsigned item_id) {
    if(!file || !world || character>=3 || item_id>255)return -1;
    if (!item_id) return 0;
    /*3F:8380..83CE refunds one word, without a slot/type gate.
      8418..87B4 searches in display order; each successful branch jumps
      to87B6. Clear the first match before13:9444 removes its effect. */
    if(npe_world_set_inventory_word(world,item_id,
        (uint16_t)(npe_world_inventory_word(world,item_id)+1u)))return -1;
    static const uint8_t order[6]={0,2,3,4,1,5};
    for(unsigned i=0;i<6;++i)
        if(world->equipped_items[character][order[i]]==item_id){
            world->equipped_items[character][order[i]]=0;
            break;
        }
    if(world_apply_equipment_attributes(file,world,character,item_id,-1))return -1;
    return 1;
}

int npe_world_unequip_item(const NpeFile *file,NpeWorld *world,
                           unsigned character,unsigned slot) {
    if(!file || !world || character>=3 || slot>=6 ||
       !world->party_active[character])return -1;
    return world_unequip_id(file,world,character,world->equipped_items[character][slot]);
}

int npe_world_unequip_selected(const NpeFile *file,NpeWorld *world) {
    if(!world || world->game_mode!=6 || world->property_page!=10 ||
       world->property_member>=3 || world->equipment_view_slot>=6)return -1;
    /* The native input consumes7AB2, not a fresh read of the slot.
       Empty selections leave the LCD, loader and page indices intact. */
    int result=world_unequip_id(file,world,world->property_member,
                                world->equipment_description_item);
    if(result>0)world->equipment_description_item=0;
    return result;
}

/* Rendering needs the resource index, not a host interpretation of its
   dimensions. Keep public bitmap inspection separate from these descriptors. */
static int indexed_draw_descriptor(uint16_t index,NpeIndexedBitmap *bitmap) {
    memset(bitmap,0,sizeof(*bitmap));
    bitmap->resource_index=index;bitmap->indexed_resource=1;
    return 0;
}
static int actor_draw_bitmap(const NpeFile *file,unsigned actor,unsigned direction,
                              unsigned frame,NpeIndexedBitmap *bitmap) {
    if(file && file->rpg_runtime){
        if(!bitmap || actor>254 || direction>255 || frame>2)return -1;
        return indexed_draw_descriptor(npe_image_group_index(file,3,
            (uint16_t)(1+actor*12+direction*3+frame)),bitmap);
    }
    return npe_actor_bitmap(file,actor,direction,frame,bitmap);
}
static int npc_draw_bitmap(const NpeFile *file,unsigned graphic,unsigned direction,
                            NpeIndexedBitmap *bitmap) {
    if(file && file->rpg_runtime){
        if(!bitmap || direction>255)return -1;
        return indexed_draw_descriptor(npe_image_group_index(file,1,
            (uint16_t)(1+graphic*4+direction)),bitmap);
    }
    return npe_npc_bitmap(file,graphic,direction,bitmap);
}
static int enemy_draw_bitmap(const NpeFile *file,const NpeEnemy *enemy,
                              NpeIndexedBitmap *bitmap) {
    if(file && file->rpg_runtime){
        if(!enemy || !bitmap)return -1;
        return indexed_draw_descriptor(npe_image_group_index(file,4,
            (uint16_t)enemy->graphic),bitmap);
    }
    return npe_enemy_bitmap(file,enemy,bitmap);
}

static int world_enemy_draw_bitmap(const NpeFile *file,const NpeWorld *world,
                                   unsigned member,unsigned lookup,NpeIndexedBitmap *bitmap){
    if(world&&world->battle_enemy_records_loaded){
        uint8_t record[148];unsigned slot=world_record_party_count(world)+member;
        if(npe_world_battle_record_image(world,slot,record))return -1;
        uint16_t pose=(uint16_t)((unsigned)record[46]*256+record[47]);
        uint16_t index=npe_image_group_index(file,4,pose);
        if(file->rpg_runtime)return indexed_draw_descriptor(index,bitmap);
        return npe_indexed_bitmap(file,index,bitmap);
    }
    NpeEnemy enemy;
    return !lookup||npe_enemy(file,lookup-1,&enemy)?-1:enemy_draw_bitmap(file,&enemy,bitmap);
}

static int blit_actor(const NpeFile *file,const NpeIndexedBitmap *actor, int left, int top,
                       uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    if(file->rpg_runtime&&actor->indexed_resource){
        int result=npe_rpg_runtime_draw_indexed(file,(int16_t)actor->resource_index,
                                               (int16_t)left,(int16_t)top,2,pixels);
        return result==-2?-1:0;
    }
    unsigned x, y;
    for (y = 0; y < actor->height; ++y) {
        int py = top + (int)y;
        if (py < 0 || py >= NPE_HEIGHT) continue;
        for (x = 0; x < actor->width; ++x) {
            int px = left + (int)x;
            uint8_t value;
            if (px < 0 || px >= NPE_WIDTH) continue;
            value = npe_indexed_pixel(actor, x, y);
            if (value) pixels[(unsigned)py * NPE_WIDTH + (unsigned)px] = value;
        }
    }
    return 0;
}

static int blit_battle_actor(const NpeFile *file,const NpeIndexedBitmap *actor, int left, int top,
                               uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    if(file->rpg_runtime&&actor->indexed_resource){
        int result=npe_rpg_runtime_draw_indexed(file,(int16_t)actor->resource_index,
                                               (int16_t)left,(int16_t)top,0,pixels);
        return result==-2?-1:0;
    }
    /* Battle renderers clear the actor rectangle before drawing; zero
       pixels cover scenery and previously drawn actors. Map actors differ. */
    for (unsigned y = 0; y < actor->height; ++y)
        for (unsigned x = 0; x < actor->width; ++x) {
            int px = left + (int)x, py = top + (int)y;
            if (px >= 0 && px < NPE_WIDTH && py >= 0 && py < NPE_HEIGHT)
                pixels[(unsigned)py * NPE_WIDTH + (unsigned)px] = npe_indexed_pixel(actor,x,y);
        }
    return 0;
}

int npe_render_world_frame(const NpeFile *file, const NpeWorld *world,
                           unsigned actor_frame,
                           uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    NpeMap map;
    NpeIndexedBitmap actor;
    unsigned i, pass;
    if (!world || actor_frame > 2 ||
        npe_map(file, world->map_number - 1, &map)) return -1;
    world_bind_tiles(world, &map);
    /* Source2E:7404..741A stores the one-based record ordinal in69D2;
       Source46:BD8A..BD94 adds twelve images per ordinal to the actor base.
       The raw file byte12 is not that runtime identifier. */
    if(!world->party_ids[0])return -1;
    if (npe_render_map(file, &map, world->view.offset_x,
                       world->view.offset_y, pixels) ||
        actor_draw_bitmap(file, world->party_ids[0]-1, world->direction, actor_frame, &actor))
        return -1;
    for(unsigned row=0;row<4;++row){
        memset(pixels+row*NPE_WIDTH,world->map_margin_color,NPE_WIDTH);
        memset(pixels+(NPE_HEIGHT-1-row)*NPE_WIDTH,world->map_margin_color,NPE_WIDTH);
    }
    for (pass = 0; pass < 2; ++pass) {
        if (pass == 1)
            if(blit_actor(file,&actor, (int)world->view.screen_x * 16,
                       (int)world->view.screen_y * 16 - 12, pixels))return -1;
        for (i = 0; i < 20; ++i) {
            const uint8_t *npc;
            NpeIndexedBitmap image;
            int left, top;
            unsigned npc_x, npc_y;
            if (npe_world_npc(world, &map, i, &npc)) break;
            if (!npe_map_npc_active(npc, world->story_progress)) continue;
            if (npe_world_npc_position(world, npc, i, &npc_x, &npc_y)) return -1;
            if ((npc_y > world->y) != (pass == 1) ||
                /* Source4B:25A2..25CC subtracts one from the NPE group.
                   npe_npc_bitmap takes a zero-based group, as for monsters. */
                npc_draw_bitmap(file, ((unsigned)npc[4] << 8 | npc[5]) - 1u,
                               world->npc_override_direction[i] <= 3 ?
                               world->npc_override_direction[i] : npc[6],
                               &image)) continue;
            left = ((int)npc_x - (int)world->view.offset_x) * 16;
            top = ((int)npc_y - (int)world->view.offset_y) * 16 - 12;
            if(blit_actor(file,&image, left, top, pixels))return -1;
        }
        for (i = 0; i < 4; ++i) {
            NpeMapMonster monster;
            NpeIndexedBitmap image;
            int left, top;
            if (map.tail_size < 644) break;
            if (npe_world_monster(world, &map, i, &monster)) return -1;
            if (!npe_map_monster_active(&monster, world->story_progress) ||
                (monster.y > world->y) != (pass == 1) ||
                npc_draw_bitmap(file, monster.graphic - 1, 0, &image))
                continue;
            left = ((int)monster.x - (int)world->view.offset_x) * 16;
            top = ((int)monster.y - (int)world->view.offset_y) * 16 - 12;
            if(blit_actor(file,&image, left, top, pixels))return -1;
        }
    }
    /* 31:E9A2..E9C2 reads header vector 2 into 52:7AEE; the chase
       renderer 3A:5E64..5F26 adds the group's one-based graphic. */
    for (i = 0; i < 2; ++i) {
        NpeMapChase chase;
        NpeIndexedBitmap image;
        int found = npe_world_chase(world, &map, i, &chase);
        int x = (int)world->chase_x[i] - (int)world->view.offset_x;
        int y = (int)world->chase_y[i] - (int)world->view.offset_y;
        if (found < 0) return -1;
        if (!found || !chase.enemy_id) break;
        if (!chase_active(&chase, world->story_progress) ||
            world->chase_cooldown[i] || x <= 0 || x >= 14 || y <= 0 || y >= 6)
            continue;
        if(file->rpg_runtime){
            indexed_draw_descriptor(npe_image_group_index(file,2,chase.graphic),&image);
        }else if (!chase.graphic || file->vectors[2] + chase.graphic > file->vectors[3] ||
            npe_indexed_bitmap(file, file->vectors[2] + chase.graphic, &image))return -1;
        if(blit_actor(file,&image, x * 16, y * 16 - 12, pixels))return -1;
    }
    return 0;
}

int npe_render_world(const NpeFile *file, const NpeWorld *world,
                     uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    return npe_render_world_frame(file, world, 1, pixels);
}

/* Source4B:79FA uses ID alone, including NPCs outside the story range.
   Source3C:D5F6 stops at the first adjacent slot: one decision controls
   the player's position relative to the entire NPC drawing pass. */
static unsigned npc_player_adjacency(const NpeWorld *world,
                                      const uint8_t *npc, unsigned slot) {
    unsigned x, y;
    unsigned hx = (uint8_t)(world->view.offset_x + world->view.screen_x);
    unsigned hy = (uint8_t)(world->view.offset_y + world->view.screen_y);
    if (!npc[0] && !npc[1]) return 0;
    if (npe_world_npc_position(world, npc, slot, &x, &y)) return 0;
    if (x == hx && y == hy + 1) return 1;
    if (x == hx && (int)y == (int)hy - 1) return 2;
    return 0;
}

static int erase_npc_tile(const NpeFile *file, const NpeMap *map,
                           const NpeWorld *world, int sx, int sy,
                           uint8_t *pixels) {
    int left = sx * 16, top = sy * 16 + 4;
    NpeIndexedBitmap image;
    /* Native bitmap coordinates are unsigned. A negative origin does not
       clip into the LCD. Opaque mode0 can overwrite the LCD margins. */
    if (left < 0 || left >= NPE_WIDTH || top < 0 || top >= NPE_HEIGHT) return 0;
    /*4A:9144 and3F:D832 read the live105-word cache. A valid viewport
      can cross a logical row or the grid's end; do not rebuild that word
      from bounded map coordinates. The following words alias visited flags. */
    int index = sy * 15 + sx;
    unsigned tile;
    if(map->resident_file==file && npe_rpg_runtime_map_current(file,map->number)){
        uint8_t bytes[2];
        if(index<0 || npe_rpg_runtime_read(file,UINT32_C(0x5270b4)+2u*(unsigned)index,bytes,2))return -1;
        tile=(unsigned)bytes[0]*256+bytes[1];
    }else if(index>=105 && (unsigned)(index-105)<sizeof(world->condition5_bytes)/2){
        /* 52:7186 follows the 105 viewport words at70B4. Preserve the
           native alias into the visited-map bytes, rather than inventing
           an eighth row of map tiles or reading past a C allocation. */
        unsigned offset=(unsigned)(index-105)*2;
        tile=(unsigned)world->condition5_bytes[offset]*256+world->condition5_bytes[offset+1];
    }else{
        if(index<0 || index>=105)return -1;
        unsigned x=world->view.offset_x+(unsigned)(index%15);
        unsigned y=world->view.offset_y+(unsigned)(index/15);
        if(x>=map->width || y>=map->height)return -1;
        tile=npe_map_tile(map,x,y);
    }
    /* Out-of-cache aliases can select a sprite or an invalid image ID.
       The original caller ignores the bitmap service's error return. */
    if(file->rpg_runtime){
        int result=npe_rpg_runtime_draw_indexed(file,(int16_t)tile,
                                               (int16_t)left,(int16_t)top,0,pixels);
        return result==-2?-1:0;
    }
    if(npe_indexed_bitmap(file,tile,&image))return 0;
    if(index<105 && (image.width!=16 || image.height!=16))return -1;
    for (unsigned iy=0; iy<image.height; ++iy)
        for (unsigned ix=0; ix<image.width; ++ix) {
            int px=left+(int)ix, py=top+(int)iy;
            if (px<NPE_WIDTH && py>=0 && py<NPE_HEIGHT)
                pixels[py*NPE_WIDTH+px]=npe_indexed_pixel(&image,ix,iy);
        }
    return 0;
}

static int npc_periodic_visible(const NpeWorld *world, const uint8_t *npc,
                                 unsigned slot) {
    unsigned x,y;
    if (npe_world_npc_position(world,npc,slot,&x,&y)) return 0;
    return npe_map_npc_active(npc,world->story_progress) &&
           x>=world->view.offset_x && x<=world->view.offset_x+14 &&
           y>=world->view.offset_y && y<=world->view.offset_y+7;
}

static int blit_periodic_npc(const NpeFile *file,const NpeIndexedBitmap *image,int left,int top,
                               uint8_t *pixels) {
    if(file->rpg_runtime && image->indexed_resource){
        int result=npe_rpg_runtime_draw_indexed(file,(int16_t)image->resource_index,
                                               (int16_t)left,(int16_t)top,2,pixels);
        return result==-2?-1:0;
    }
    if(left<0 || top<0)return 0;
    for(unsigned y=0;y<image->height;++y)for(unsigned x=0;x<image->width;++x){
        int px=left+(int)x,py=top+(int)y;
        /* Source2A:8232 clips mode2 against the full 240x120 LCD.
           The NPC service restores its margins afterwards, rather than
           imposing a smaller clip rectangle on every actor bitmap. */
        if(px>=NPE_WIDTH || py<0 || py>=NPE_HEIGHT)continue;
        uint8_t value=npe_indexed_pixel(image,x,y);
        if(value)pixels[py*NPE_WIDTH+px]=value;
    }
    return 0;
}

static int draw_world_npc_slot(const NpeFile *file,const NpeMap *map,
                               const NpeWorld *world,unsigned slot,uint8_t *pixels) {
    const uint8_t *npc;NpeIndexedBitmap image;unsigned x,y;
    if(npe_world_npc(world,map,slot,&npc))return -1;
    if(!npc[0] && !npc[1])return 0;
    if(npc_periodic_visible(world,npc,slot)){
        if(npc_draw_bitmap(file,((unsigned)npc[4]<<8 | npc[5])-1u,
            world->npc_override_direction[slot]<=3 ? world->npc_override_direction[slot] : npc[6],&image))return -1;
        if (npe_world_npc_position(world,npc,slot,&x,&y)) return -1;
        if(blit_periodic_npc(file,&image,((int)x-(int)world->view.offset_x)*16,
                          ((int)y-(int)world->view.offset_y)*16-12,pixels))return -1;
    }
    /* Source4B:268E..26BE, including failed visibility checks. */
    memset(pixels,world->lcd_background,4*NPE_WIDTH);
    memset(pixels+(NPE_HEIGHT-4)*NPE_WIDTH,world->lcd_background,4*NPE_WIDTH);
    return 0;
}

int npe_render_world_walk_blocked(const NpeFile *file,const NpeWorld *world,
                                  uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    NpeMap map;NpeIndexedBitmap player;
    if(!file || !world || !pixels || !world->party_ids[0] ||
       npe_map(file,world->map_number-1,&map) ||
       actor_draw_bitmap(file,world->party_ids[0]-1,world->direction,1,&player))return -1;
    world_bind_tiles(world,&map);
    unsigned hx=(uint8_t)(world->view.offset_x+world->view.screen_x);
    unsigned hy=(uint8_t)(world->view.offset_y+world->view.screen_y);
    /* Source3B:1480..1534 draws before testing adjacency, and stops
       after the first vertical neighbour. This differs from a timer. */
    for(unsigned slot=0;slot<24;++slot){
        unsigned adjacent=0;const uint8_t *npc=NULL;
        if(slot<20){
            if(npe_world_npc(world,&map,slot,&npc))return -1;
            if(!npc[0] && !npc[1])continue;
            if(draw_world_npc_slot(file,&map,world,slot,pixels))return -1;
            adjacent=npc_player_adjacency(world,npc,slot);
        }else{
            NpeMapMonster monster;
            if(npe_world_monster(world,&map,slot-20,&monster))return -1;
            if(!monster.npc_id)continue;
            if(draw_world_monster_slot(file,&map,world,slot-20,pixels))return -1;
            if(monster.x==hx && monster.y==hy+1)adjacent=1;
            if(monster.x==hx && (int)monster.y==(int)hy-1)adjacent=2;
            /* The lower-neighbour branch redraws this monster below. */
            if(adjacent==1){
                if(erase_npc_tile(file,&map,world,world->view.screen_x,world->view.screen_y,pixels) ||
                   erase_npc_tile(file,&map,world,world->view.screen_x,(int)world->view.screen_y-1,pixels))return -1;
                if(blit_periodic_npc(file,&player,(int)world->view.screen_x*16,(int)world->view.screen_y*16-12,pixels))return -1;
                if(draw_world_monster_slot(file,&map,world,slot-20,pixels))return -1;
                return 1;
            }
        }
        if(!adjacent)continue;
        if(adjacent==1 &&
           (erase_npc_tile(file,&map,world,world->view.screen_x,world->view.screen_y,pixels) ||
            erase_npc_tile(file,&map,world,world->view.screen_x,(int)world->view.screen_y-1,pixels)))return -1;
        if(blit_periodic_npc(file,&player,(int)world->view.screen_x*16,(int)world->view.screen_y*16-12,pixels))return -1;
        if(adjacent==1 && draw_world_npc_slot(file,&map,world,slot,pixels))return -1;
        return 1;
    }
    /* Source3B:163E..1646 erases both tiles even when the following
       walking service protects a lower tile for a zero-ID NPC record. */
    if(erase_npc_tile(file,&map,world,world->view.screen_x,world->view.screen_y,pixels) ||
       erase_npc_tile(file,&map,world,world->view.screen_x,(int)world->view.screen_y-1,pixels))return -1;
    return 0;
}

int npe_render_world_static_player(const NpeFile *file,const NpeWorld *world,uint8_t *pixels) {
    NpeIndexedBitmap player;
    if(!world || !pixels || !world->party_ids[0] ||
       actor_draw_bitmap(file,world->party_ids[0]-1,world->direction,1,&player))return -1;
    if(blit_periodic_npc(file,&player,(int)world->view.screen_x*16,
                     (int)world->view.screen_y*16-12,pixels))return -1;
    return 0;
}

int npe_render_world_walk_pose(const NpeFile *file,const NpeWorld *world,
                               unsigned step,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    NpeMap map;NpeIndexedBitmap player;unsigned below=0;
    if(!file || !world || !pixels || step>=4 || !world->party_ids[0] ||
       npe_map(file,world->map_number-1,&map) ||
       actor_draw_bitmap(file,world->party_ids[0]-1,world->direction,step%3,&player))return -1;
    world_bind_tiles(world,&map);
    /* Source46:BC52..BD0A scans coordinates without checking NPC IDs.
       For the first three poses a record directly below the player
       protects the lower tile; the fourth pose erases both tiles. */
    unsigned hx=world->view.offset_x+world->view.screen_x;
    unsigned hy=world->view.offset_y+world->view.screen_y;
    for(unsigned slot=0;slot<20;++slot){
        const uint8_t *npc;unsigned x,y;
        if(npe_world_npc(world,&map,slot,&npc))return -1;
        if (npe_world_npc_position(world,npc,slot,&x,&y)) return -1;
        if(x==hx && y==hy+1){below=1;break;}
    }
    if((!below || step==3) &&
       erase_npc_tile(file,&map,world,world->view.screen_x,world->view.screen_y,pixels))return -1;
    if(erase_npc_tile(file,&map,world,world->view.screen_x,(int)world->view.screen_y-1,pixels))return -1;
    if(blit_periodic_npc(file,&player,(int)world->view.screen_x*16,
                     (int)world->view.screen_y*16-12,pixels))return -1;
    return 0;
}

static int draw_world_monster_slot(const NpeFile *file,const NpeMap *map,
                                   const NpeWorld *world,unsigned slot,
                                   uint8_t *pixels) {
    NpeMapMonster monster;NpeIndexedBitmap image;
    if(npe_world_monster(world,map,slot,&monster))return -1;
    /* Source47:F862 checks viewport and story only; Source4B:B2C4 then
       tests the signed graphic word. ID gates belong to each caller.
       The visible Y range is offset+1 ..offset+6, without an NPC footer. */
    if(!monster.graphic || monster.graphic>INT16_MAX ||
       !world_monster_in_story(&monster,world->story_progress) ||
       monster.x<world->view.offset_x || monster.x>world->view.offset_x+14 ||
       monster.y<world->view.offset_y+1 || monster.y>world->view.offset_y+6)return 0;
    if(npc_draw_bitmap(file,monster.graphic-1,0,&image))return -1;
    if(blit_periodic_npc(file,&image,((int)monster.x-(int)world->view.offset_x)*16,
        ((int)monster.y-(int)world->view.offset_y)*16-12,pixels))return -1;
    return 0;
}

int npe_render_world_load(const NpeFile *file,const NpeWorld *world,
                           uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    NpeMap map;NpeIndexedBitmap player;
    if(!file || !world || !pixels || !world->party_ids[0] ||
       npe_map(file,world->map_number-1,&map))return -1;
    world_bind_tiles(world,&map);
    if(npe_render_map(file,&map,world->view.offset_x,world->view.offset_y,pixels) ||
       actor_draw_bitmap(file,world->party_ids[0]-1,world->direction,1,&player))return -1;
    if(blit_periodic_npc(file,&player,(int)world->view.screen_x*16,
                     (int)world->view.screen_y*16-12,pixels))return -1;
    /* Source4C:6AB8..6BF8 has no adjacency test or Y sort. */
    for(unsigned slot=0;slot<20;++slot)
        if(draw_world_npc_slot(file,&map,world,slot,pixels))return -1;
    for(unsigned slot=0;slot<4;++slot){
        NpeMapMonster monster;if(npe_world_monster(world,&map,slot,&monster))return -1;
        /* Source4C:6B4E..6BB4 uses the enemy byte on map loading. */
        if(monster.enemy_id && draw_world_monster_slot(file,&map,world,slot,pixels))return -1;
    }
    memset(pixels,world->lcd_background,4*NPE_WIDTH);
    memset(pixels+(NPE_HEIGHT-4)*NPE_WIDTH,world->lcd_background,4*NPE_WIDTH);
    return 0;
}

int npe_world_npc_move_with_display(const NpeFile *file,NpeWorld *world,
                                    uint8_t *pixels,NpeMapDisplayCommit commit,
                                    void *context) {
    NpcScriptMove move;NpeMap map;
    if(!pixels)return npe_world_apply_npc_move_event(file,world);
    if(!world || world->game_mode!=18)return -1;
    if(npe_render_world_load(file,world,pixels))return -1;
    /* Source3C:8B9E redraws the map before validating the event. */
    if(commit && commit(context,pixels,0))return -2;
    /*3C:8BA2..8BAA sets7AB4 after the entry redraw, before validation. */
    world->map_init_pending=1;
    int result=prepare_npc_script_move(file,world,&move);
    if(result<=0)return result;
    if(npe_map(file,world->map_number-1,&map))return -1;
    world_bind_tiles(world,&map);
    world->npc_override_direction[move.slot]=(uint8_t)move.direction;
    uint8_t background[NPE_WIDTH*NPE_HEIGHT];
    /*3C:90AA..90C2 stores the increment back to a byte before comparing
      against the signed step word. At255 it wraps to0 and keeps running. */
    for(unsigned step=1;(int)step<=move.steps;step=(uint8_t)(step+1)){
        unsigned x,y;const uint8_t *npc;
        if(npe_world_npc(world,&map,move.slot,&npc))return -1;
        if (npe_world_npc_position(world,npc,move.slot,&x,&y)) return -1;
        int sx=(int)x-(int)world->view.offset_x,sy=(int)y-(int)world->view.offset_y;
        int left=sx*16-4,top=sy*16-16,right=sx*16+20,bottom=sy*16+24;
        if(step==1){
            /* Source44:E84C erases lower then upper tile and restores an
               adjacent player. There is no movement-flag test here. */
            if(npc_periodic_visible(world,npc,move.slot)){
                if(erase_npc_tile(file,&map,world,sx,sy,pixels) ||
                   erase_npc_tile(file,&map,world,sx,sy-1,pixels))return -1;
                if(npc_player_adjacency(world,npc,move.slot) &&
                   npe_render_world_static_player(file,world,pixels))return -1;
            }
            memcpy(background,pixels,sizeof(background));
        }else{
            /* Source3C:8FA4..8FC8 restores the old-coordinate rectangle
               from the retained drawing plane before moving the NPC. */
            for(int py=top;py<=bottom;++py)if(py>=0&&py<NPE_HEIGHT)
                for(int px=left;px<=right;++px)if(px>=0&&px<NPE_WIDTH)
                    pixels[py*NPE_WIDTH+px]=background[py*NPE_WIDTH+px];
        }
        if(left<NPE_WIDTH && right>=0 && top<NPE_HEIGHT && bottom>=0 &&
           commit && commit(context,pixels,0))return -2;
        world->npc_override_x[move.slot]=(uint8_t)((int)move.x+move.step_x*(int)step);
        world->npc_override_y[move.slot]=(uint8_t)((int)move.y+move.step_y*(int)step);
        if(draw_world_npc_slot(file,&map,world,move.slot,pixels))return -1;
        if(commit && commit(context,pixels,15))return -2;
    }
    world_finish_script_followup(world,move.event.next_mode,move.event.next_id,1);
    return 1;
}

int npe_render_world_walk_finish(const NpeFile *file,const NpeWorld *before,
                                 const NpeWorld *after,uint8_t *pixels,
                                 NpeMapDisplayCommit commit,void *context) {
    NpeMap map;NpeIndexedBitmap player;
    if(!file || !before || !after || !pixels ||
       before->map_number!=after->map_number || !after->party_ids[0] ||
       npe_map(file,after->map_number-1,&map) ||
       actor_draw_bitmap(file,after->party_ids[0]-1,after->direction,1,&player))return -1;
    world_bind_tiles(before,&map);
    /* The old-coordinate erasure still reads the pre-scroll cache even
       though the host has already updated the live World viewport. */
    if(npe_rpg_runtime_owner(file)==after && npe_rpg_runtime_map_current(file,map.number))
        map.resident_file=file;
    /* Source3B:166C erases both old-position cache tiles before scrolling. */
    if(erase_npc_tile(file,&map,before,before->view.screen_x,before->view.screen_y,pixels) ||
       erase_npc_tile(file,&map,before,before->view.screen_x,(int)before->view.screen_y-1,pixels))return -1;
    int dx=(int)after->view.offset_x-(int)before->view.offset_x;
    int dy=(int)after->view.offset_y-(int)before->view.offset_y;
    if(dx<-1 || dx>1 || dy<-1 || dy>1 || (dx && dy))return -1;
    world_bind_tiles(after,&map);
    /*3B:18BA refreshes after old-position erasure and before3F:D700
      scrolls/draws its edge, including moves with no viewport change. */
    if(map.resident_file==file){
        uint16_t cached[105];
        if(npe_rpg_runtime_map_cache(file,map.number,after->view.offset_x,
                                     after->view.offset_y,cached)!=1)return -1;
    }
    if(dx || dy){
        /* Source3F:D756..D776 scrolls only the inclusive LCD rectangle
           (0,4)..(239,115), by16 pixels. New viewport edge tiles follow
           in opaque mode0; the four-row LCD margins remain retained. */
        uint8_t old[NPE_WIDTH*NPE_HEIGHT];memcpy(old,pixels,sizeof(old));
        for(int y=4;y<116;++y)for(int x=0;x<NPE_WIDTH;++x){
            int sx=x+dx*16,sy=y+dy*16;
            if(sx>=0 && sx<NPE_WIDTH && sy>=4 && sy<116)
                pixels[y*NPE_WIDTH+x]=old[sy*NPE_WIDTH+sx];
        }
        if(dx)for(int y=0;y<7;++y){
            if(erase_npc_tile(file,&map,after,dx>0?14:0,y,pixels))return -1;
        }
        if(dy)for(int x=0;x<15;++x){
            if(erase_npc_tile(file,&map,after,x,dy>0?6:0,pixels))return -1;
        }
    }
    unsigned npc_slot,monster_slot,hx=after->view.offset_x+after->view.screen_x;
    unsigned hy=after->view.offset_y+after->view.screen_y;
    for(npc_slot=0;npc_slot<20;++npc_slot){
        const uint8_t *npc;if(npe_world_npc(after,&map,npc_slot,&npc))return -1;
        if(!npc[0] && !npc[1])continue;
        if(draw_world_npc_slot(file,&map,after,npc_slot,pixels))return -1;
        unsigned adjacent=npc_player_adjacency(after,npc,npc_slot);
        if(!adjacent)continue;
        if(adjacent==1 &&
           (erase_npc_tile(file,&map,after,after->view.screen_x,after->view.screen_y,pixels) ||
            erase_npc_tile(file,&map,after,after->view.screen_x,(int)after->view.screen_y-1,pixels)))return -1;
        if(blit_periodic_npc(file,&player,(int)after->view.screen_x*16,(int)after->view.screen_y*16-12,pixels))return -1;
        if(adjacent==1 && draw_world_npc_slot(file,&map,after,npc_slot,pixels))return -1;
        if(commit && commit(context,pixels,0))return -2;
        break;
    }
    for(monster_slot=0;monster_slot<4;++monster_slot){
        NpeMapMonster monster;if(npe_world_monster(after,&map,monster_slot,&monster))return -1;
        /* Source3B:1A2C and4B:6B7A use the NPC word, including hidden
           formations and zero-enemy formations, for walk adjacency. */
        if(!monster.npc_id)continue;
        if(draw_world_monster_slot(file,&map,after,monster_slot,pixels))return -1;
        unsigned adjacent=monster.x==hx && monster.y==hy+1 ? 1 :
            monster.x==hx && (int)monster.y==(int)hy-1 ? 2 : 0;
        if(!adjacent)continue;
        if(adjacent==1 &&
           (erase_npc_tile(file,&map,after,after->view.screen_x,after->view.screen_y,pixels) ||
            erase_npc_tile(file,&map,after,after->view.screen_x,(int)after->view.screen_y-1,pixels)))return -1;
        if(blit_periodic_npc(file,&player,(int)after->view.screen_x*16,(int)after->view.screen_y*16-12,pixels))return -1;
        if(adjacent==1 && draw_world_monster_slot(file,&map,after,monster_slot,pixels))return -1;
        break;
    }
    if(npc_slot==20 && monster_slot==4)
        if(blit_periodic_npc(file,&player,(int)after->view.screen_x*16,(int)after->view.screen_y*16-12,pixels))return -1;
    return commit && commit(context,pixels,0) ? -2 : 0;
}

int npe_render_npc_periodic(const NpeFile *file, const NpeWorld *before,
                             const NpeWorld *after,
                             uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    NpeMap map;
    NpeIndexedBitmap player;
    unsigned adjacent = 0;
    if (!file || !before || !after || !pixels ||
        before->map_number != after->map_number ||
        memcmp(&before->view, &after->view, sizeof(before->view)) ||
        !after->party_ids[0] ||
        npe_map(file, after->map_number - 1, &map) ||
        actor_draw_bitmap(file, after->party_ids[0]-1, after->direction, 1, &player))
        return -1;
    world_bind_tiles(before, &map);
    for (unsigned slot = 0; slot < 20; ++slot) {
        const uint8_t *npc;
        unsigned x, y;
        if (npe_world_npc(before, &map, slot, &npc)) return -1;
        /* Erasure happens even when the attempted step is blocked. */
        if (npc[7] != 1 || !npc_periodic_visible(before,npc,slot)) continue;
        if (npe_world_npc_position(before, npc, slot, &x, &y)) return -1;
        int sx = (int)x - (int)before->view.offset_x;
        int sy = (int)y - (int)before->view.offset_y;
        /* Source44:E84C draws the lower tile, then the upper tile. */
        if (erase_npc_tile(file, &map, before, sx, sy, pixels) ||
            erase_npc_tile(file, &map, before, sx, sy-1, pixels)) return -1;
        if (npc_player_adjacency(before, npc, slot))
            if(blit_periodic_npc(file,&player, (int)after->view.screen_x*16,
                       (int)after->view.screen_y*16-12, pixels))return -1;
    }
    for (unsigned slot = 0; slot < 20 && !adjacent; ++slot) {
        const uint8_t *npc;
        if (npe_world_npc(after, &map, slot, &npc)) return -1;
        adjacent = npc_player_adjacency(after, npc, slot);
    }
    if (adjacent != 2)
        if(blit_periodic_npc(file,&player, (int)after->view.screen_x*16,
                   (int)after->view.screen_y*16-12, pixels))return -1;
    for (unsigned slot = 0; slot < 20; ++slot) {
        if(draw_world_npc_slot(file,&map,after,slot,pixels))return -1;
    }
    if (adjacent == 2)
        if(blit_periodic_npc(file,&player, (int)after->view.screen_x*16,
                   (int)after->view.screen_y*16-12, pixels))return -1;
    return 0;
}

static int render_world_chase_only(const NpeFile *file,const NpeWorld *before,
                               const NpeWorld *after,
                               uint8_t *pixels) {
    NpeMap map;
    if(!file || !before || !after || !pixels ||
       before->map_number!=after->map_number ||
       memcmp(&before->view,&after->view,sizeof(before->view)) ||
       npe_map(file,before->map_number-1,&map))return -1;
    world_bind_tiles(before,&map);
    for(unsigned slot=0;slot<2;++slot){
        NpeMapChase chase;NpeIndexedBitmap image;
        int found=npe_world_chase(before,&map,slot,&chase);
        if(found<0)return -1;
        if(!found || !chase.enemy_id)break;
        if(!chase_active(&chase,before->story_progress))continue;
        /* Cooldown and contact both return from the complete chase service.
           Neither erases/draws this slot, or processes any later slot. */
        if(before->chase_cooldown[slot] || chase_touches_player(before,slot))break;
        int sx=(int)before->chase_x[slot]-(int)before->view.offset_x;
        int sy=(int)before->chase_y[slot]-(int)before->view.offset_y;
        /* Source3F:4F38,4FCE,50AE,5144: lower-left, upper-left,
           lower-right, upper-right, before this slot changes position. */
        if(erase_npc_tile(file,&map,before,sx,sy,pixels) ||
           erase_npc_tile(file,&map,before,sx,sy-1,pixels) ||
           erase_npc_tile(file,&map,before,sx+1,sy,pixels) ||
           erase_npc_tile(file,&map,before,sx+1,sy-1,pixels))return -1;
        sx=(int)after->chase_x[slot]-(int)after->view.offset_x;
        sy=(int)after->chase_y[slot]-(int)after->view.offset_y;
        if(sx<=0 || sx>=14 || sy<=0 || sy>=6)continue;
        if(file->rpg_runtime){
            indexed_draw_descriptor(npe_image_group_index(file,2,chase.graphic),&image);
        }else if(!chase.graphic || file->vectors[2]+chase.graphic>file->vectors[3] ||
           npe_indexed_bitmap(file,file->vectors[2]+chase.graphic,&image))return -1;
        if(blit_periodic_npc(file,&image,sx*16,sy*16-12,pixels))return -1;
    }
    return 0;
}

int npe_render_world_periodic(const NpeFile *file,const NpeWorld *before,
                               const NpeWorld *after,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    if(render_world_chase_only(file,before,after,pixels))return -1;
    /* Source3C:D5E8 runs this even after chase selected battle mode. */
    return npe_render_npc_periodic(file,before,after,pixels);
}

int npe_world_battle_coordinate(const NpeWorld *w,unsigned enemy,unsigned member,unsigned axis){
    static const uint8_t positions[2][6][2]={
        {{168,51},{136,75},{168,51},{136,75},{168,51},{200,28}},
        {{42,38},{82,16},{42,38},{82,16},{42,38},{2,54}}
    };
    if(!w||enemy>1||member>=3||axis>1)return -1;
    if(w->battle_party_flags_saved||w->battle_enemy_records_loaded){
        unsigned prefix=world_record_party_count(w),slot=enemy?prefix+member:member;
        if(prefix>3||slot>=6)return -1;
        /* Source31:7C7A/7CBC and2F:BE80/BECC zero-extend64CC/64CD.
           Coordinates survive entry/exit copies independently of map scroll. */
        return w->battle_record_residue[slot][124+axis];
    }
    /* Resource inspection and unprepared numeric scenes have no resident
       battle records. Keep their original initializer layout. Production
       battle entry always installs the physical records before drawing. */
    unsigned count=enemy?w->battle_enemy_count:0,compact=member;
    if(!enemy){compact=0;for(unsigned i=0;i<3;++i)if(w->party_active[i]){
        if(i<member)++compact;
        ++count;
    }}
    if(!count||count>3||compact>=count)return -1;
    return positions[enemy][count*(count-1)/2+compact][axis];
}

static void render_battle_status(const int16_t states[4], int left, int top,
                                 unsigned width, unsigned height,
                                 uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    /* Source38:8296..8AF6 draws engine text (runtime31:DB9C..DBBA).
       Signed-negative words alone display. These glyphs are firmware UI;
       the actor images and their pose indices still come from the NPE. */
    for (unsigned state = 0; state < 4; ++state) {
        if (states[state] >= 0) continue;
        int x0 = left + ((state == 1 || state == 2) ? (int)width - 13 : 0);
        int y0 = top + (state >= 2 ? (int)height - 13 : 0);
        for (unsigned y = 0; y < 12; ++y) {
            unsigned bits = (unsigned)battle_status_glyphs[state][2*y] << 8 |
                            battle_status_glyphs[state][2*y+1];
            for (unsigned x = 0; x < 12; ++x) {
                int px = x0 + (int)x, py = y0 + (int)y;
                if (px >= 0 && px < NPE_WIDTH && py >= 0 && py < NPE_HEIGHT)
                    pixels[(unsigned)py*NPE_WIDTH+(unsigned)px] =
                        bits & (0x8000u >> x) ? 15 : 0;
            }
        }
    }
}

void npe_render_battle_status_refresh(const NpeWorld *world,
                                       uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    unsigned slots[3],count=0;
    if (!world || !pixels ||
        !world->battle_enemy_count || world->battle_enemy_count>3) return;
    for (unsigned i=0;i<3;++i)
        if (world->party_active[i]) slots[count++]=i;
    for (unsigned enemy=0;enemy<world->battle_enemy_count;++enemy)
        if ((int16_t)world->battle_enemy_hps[enemy]>0)
            render_battle_status(world->battle_enemy_status[enemy],
                npe_world_battle_coordinate(world,1,enemy,0),
                npe_world_battle_coordinate(world,1,enemy,1),40,50,pixels);
    for (unsigned compact=0;compact<count;++compact) {
        unsigned slot=slots[compact];
        if ((int16_t)world->hp[slot]>0)
            render_battle_status(world->battle_party_status[slot],
                npe_world_battle_coordinate(world,0,slot,0),npe_world_battle_coordinate(world,0,slot,1),32,40,pixels);
    }
}

void npe_render_battle_status_overlay(const NpeWorld *world,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    if(world && world->battle_scene_refreshed)npe_render_battle_status_refresh(world,pixels);
}

static int battle_draw_resource(const NpeFile *file,const NpeWorld *world,
                                uint16_t number,int x,int y,uint8_t kind,uint8_t *pixels){
    if(!file||!pixels)return -1;
    if(file->rpg_runtime){
        if(world&&npe_rpg_runtime_owner(file)!=world)return -1;
        return npe_rpg_runtime_draw(file,number,(int16_t)x,(int16_t)y,kind,pixels);
    }
    /* Standalone resource inspection APIs without an attached runtime. */
    NpeIndexedBitmap bitmap;
    if(!number||npe_battle_bitmap(file,number-1,&bitmap))return -1;
    if(kind==0)return blit_battle_actor(file,&bitmap,x,y,pixels);
    if(kind>=2)return blit_actor(file,&bitmap,x,y,pixels);
    return -1;
}
static int render_battle_formation(const NpeFile *file,
                                   const uint8_t enemy_ids[3],
                                   const uint16_t *enemy_hps,
                                   unsigned enemy_count, const NpeWorld *world,
                                   int draw_overlays, unsigned omitted_party, unsigned party_kind,
                                   uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    int inspect=world==NULL;
    NpeWorld inspection={0};
    inspection.party_active[0]=1;inspection.battle_enemy_count=(uint8_t)enemy_count;
    const NpeWorld *layout=world?world:&inspection;
    NpeIndexedBitmap bitmap;
    unsigned member;
    if (!file || !pixels || !enemy_ids || enemy_count > 3) return -1;
    memset(pixels, 0, NPE_WIDTH * NPE_HEIGHT);
    /* Native Tianjiang battle LCD: top-left and bottom-right scenery,
       32x32 command controls, 40x50 enemies and 32x40 party actors. The
       enemy coordinates follow source31:D146..D716 and the original
       RAM records64CC/64CD. */
    if (battle_draw_resource(file,inspect?NULL:world,1,0,0,2,pixels)||
        battle_draw_resource(file,inspect?NULL:world,26,197,87,2,pixels))return -1;
    /* Preserve the background erased by the hostile target's leftward
       animation; ordinary round redraws alone do not erase this area. */
    if (world)
        for (member = 0; member < enemy_count; ++member) {
            if (!(world->battle_enemy_shifted_mask & (1u << member))) continue;
            int left = npe_world_battle_coordinate(layout,1,member,0) - 2;
            int top = npe_world_battle_coordinate(layout,1,member,1);
            for (int y = top; y <= top+50 && y < NPE_HEIGHT; ++y)
                for (int x = left; x <= left+40 && x < NPE_WIDTH; ++x)
                    if (x >= 0) pixels[(unsigned)y*NPE_WIDTH+(unsigned)x] = 0;
        }
    for (member = 0; member < enemy_count; ++member) {
        if (enemy_hps && (int16_t)enemy_hps[member] <= 0) continue;
        if (world_enemy_draw_bitmap(file,world,member,enemy_ids[member],&bitmap)) return -1;
        if(blit_battle_actor(file,&bitmap, npe_world_battle_coordinate(layout,1,member,0),
                   npe_world_battle_coordinate(layout,1,member,1), pixels))return -1;
    }
    if (world && world->battle_scene_refreshed) {
        /* Source43:D9E0..DAFE clears signed-nonpositive enemy records,
           including unloaded physical slots, after their neighbours were
           drawn. Inclusive endpoints can erase the neighbour's border. */
        for (member = 0; member < enemy_count; ++member) {
            if ((int16_t)enemy_hps[member] > 0) continue;
            int left = npe_world_battle_coordinate(layout,1,member,0);
            int top = npe_world_battle_coordinate(layout,1,member,1);
            for (int y = top; y <= top+50 && y < NPE_HEIGHT; ++y)
                for (int x = left; x <= left+40 && x < NPE_WIDTH; ++x)
                    pixels[(unsigned)y*NPE_WIDTH+(unsigned)x] = 0;
        }
    }
    if (draw_overlays) {
        if (battle_draw_resource(file,inspect?NULL:world,4,3,88,0,pixels))return -1;
    }
    {
        unsigned slots[3] = {0,0,0}, count = 0, actor_slot = 0;
        if (world) {
            for (member = 0; member < 3; ++member)
                if (world->party_active[member]) {
                    if (member == world->battle_actor) actor_slot = count;
                    slots[count++] = member;
                }
        } else count = 1;
        if (!count) return -1;
        /* 4C:0560 initializes 7700..770B; 38:DFD8..E0A2 selects the
           triangular count offset. 31:7BEA draws compact slots in reverse.
           2E:764E..78CC loads eight NPE19..34 one-based pose images.
           31:7D9A selects pose2 at <=maxHP/5; fallen pose3 is retained
           by the refresh path that skips them at 31:7D14..7D3E. */
        for (unsigned compact = count; compact-- > 0;) {
            NpeCharacter character;
            unsigned slot = slots[compact], pose = 0, image;
            if(omitted_party&(1u<<slot))continue;
            if (inspect && npe_character(file,0,&character)) return -1;
            if (world) {
                if ((int16_t)world->hp[slot] <= 0) pose = 3;
                else if ((int16_t)world->hp[slot] <= (int16_t)world->max_hp[slot] / 5) pose = 2;
            }
            image = !inspect ? npe_world_battle_pose(world,slot,pose) :
                ((unsigned)character.raw[19+pose*2]<<8 | character.raw[20+pose*2]);
            /* Source41:43DE..4508 selectively clears the final 32x40
               pose rectangle, including its right/bottom endpoints. Clearing
               every high-HP nondefending neighbour erases cached fallen pixels. */
            if (world && world->battle_scene_refreshed &&
                (world->battle_party_refresh_mask & (1u << slot)))
                for (int y = npe_world_battle_coordinate(layout,0,slot,1); y <= npe_world_battle_coordinate(layout,0,slot,1)+40; ++y)
                    for (int x = npe_world_battle_coordinate(layout,0,slot,0); x <= npe_world_battle_coordinate(layout,0,slot,0)+32; ++x)
                        if (x < NPE_WIDTH && y < NPE_HEIGHT)
                            pixels[(unsigned)y*NPE_WIDTH+(unsigned)x] = 0;
            if(battle_draw_resource(file,inspect?NULL:world,(uint16_t)image,npe_world_battle_coordinate(layout,0,slot,0),
                                    npe_world_battle_coordinate(layout,0,slot,1),
                                    world&&(int16_t)world->hp[slot]<=0?0:(uint8_t)party_kind,pixels))return -1;
        }
        if (draw_overlays) {
            npe_render_battle_status_overlay(world, pixels);
            if(battle_draw_resource(file,inspect?NULL:world,5,npe_world_battle_coordinate(layout,0,slots[actor_slot],0)+12+
                   (int)((3 * ((2 * actor_slot + 1) % 4) + 1 + 2 * actor_slot) / 2),
                   npe_world_battle_coordinate(layout,0,slots[actor_slot],1)-12,2,pixels))return -1;
        }
    }
    return 0;
}

int npe_render_battle_formation_scene(const NpeFile *file,
                                      const uint8_t enemy_ids[3],
                                      unsigned enemy_count,
                                      uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    return render_battle_formation(file, enemy_ids, NULL, enemy_count, NULL, 1, 0, 0,
                                   pixels);
}

int npe_render_battle_world_scene(const NpeFile *file, const NpeWorld *world,
                                  uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    if (!world) return -1;
    return render_battle_formation(file, world->battle_enemy_ids,
                                   world->battle_enemy_hps,
                                   world->battle_enemy_count,
                                   world, 1, 0, 0, pixels);
}

int npe_render_battle_actors_scene(const NpeFile *file, const NpeWorld *world,
                                   uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    if (!world) return -1;
    return render_battle_formation(file,world->battle_enemy_ids,
        world->battle_enemy_hps,world->battle_enemy_count,world,0,0,0,pixels);
}

int npe_render_battle_controls(const NpeFile *file, unsigned icon,
                                uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    if (battle_draw_resource(file,NULL,4,3,88,0,pixels)) return -1;
    return npe_render_battle_icon_selection(icon,pixels);
}

int npe_render_battle_round_restore(const NpeFile *file, const NpeWorld *world,
                                     unsigned selective,
                                     uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    unsigned slots[3],count=0;
    if (!file || !world || !pixels || !world->battle_enemy_count ||
        world->battle_enemy_count>3 || selective>1) return -1;
    for (unsigned i=0;i<3;++i) if(world->party_active[i]) slots[count++]=i;
    if (!count) return -1;
    /* Source43:D98E and41:4216 mutate the cached display. Clearing a dead
       enemy at its original position retains its displaced left edge. */
    for (unsigned i=0;i<world->battle_enemy_count;++i) {
        if ((int16_t)world->battle_enemy_hps[i]>0) continue;
        int left=npe_world_battle_coordinate(world,1,i,0);
        int top=npe_world_battle_coordinate(world,1,i,1);
        for (int y=top;y<=top+50 && y<NPE_HEIGHT;++y)
            for (int x=left;x<=left+40 && x<NPE_WIDTH;++x)
                pixels[(unsigned)y*NPE_WIDTH+(unsigned)x]=0;
    }
    for (unsigned compact=count;compact-- >0;) {

        unsigned slot=slots[compact],pose=0,id;
        if (selective && !(world->battle_party_refresh_mask&(1u<<slot))) continue;
        int left=npe_world_battle_coordinate(world,0,slot,0),top=npe_world_battle_coordinate(world,0,slot,1);
        if (selective)
            for (int y=top;y<=top+40 && y<NPE_HEIGHT;++y)
                for (int x=left;x<=left+32 && x<NPE_WIDTH;++x)
                    pixels[(unsigned)y*NPE_WIDTH+(unsigned)x]=0;
        if ((int16_t)world->hp[slot]<=0) pose=3;
        else if ((int16_t)world->hp[slot]<=(int16_t)world->max_hp[slot]/5) pose=2;
        id=npe_world_battle_pose(world,slot,pose);
        if(battle_draw_resource(file,world,(uint16_t)id,left,top,0,pixels))return -1;
    }
    return 0;
}

int npe_render_battle_flee_stage(const NpeFile *file,const NpeWorld *world,
                                 unsigned member,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    if(!file||!world||!pixels||member>=3||!world->party_active[member])return -1;
    /* Source2F:BE30 marks successive positive records as action6.
       Source37:8C6E clears/rebuilds scenery and enemies, then9050..957E
       draws remaining party records in reverse.37:94F0 uses transparent
       kind2 for positive HP;91FC uses kind0 for fallen records. */
    unsigned omitted=0;
    for(unsigned slot=0;slot<=member;++slot)
        if(world->party_active[slot]&&(int16_t)world->hp[slot]>0)omitted|=1u<<slot;
    return render_battle_formation(file,world->battle_enemy_ids,world->battle_enemy_hps,
        world->battle_enemy_count,world,0,omitted,2,pixels);
}

int npe_render_battle_cursor(const NpeFile *file, const NpeWorld *world,
                              uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    unsigned count=0,actor=0;
    if (!world || !pixels) return -1;
    for (unsigned i=0;i<3;++i) if (world->party_active[i]) {
        if (i==world->battle_actor) actor=count;
        ++count;
    }
    if(!count)return -1;
    if(battle_draw_resource(file,world,5,npe_world_battle_coordinate(world,0,world->battle_actor,0)+12+
        (int)((3*((2*actor+1)%4)+1+2*actor)/2),npe_world_battle_coordinate(world,0,world->battle_actor,1)-12,2,pixels))return -1;
    return 0;
}

int npe_render_battle_combine_prepare(const NpeFile *file, const NpeWorld *world,
                                      uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    if (npe_render_battle_actors_scene(file,world,pixels)) return -1;
    return npe_render_battle_combine_stage(file,world,pixels);
}

int npe_render_battle_combine_stage(const NpeFile *file, const NpeWorld *world,
                                      uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    NpeIndexedBitmap image;
    unsigned slots[3],count=0;
    if (!file || !world || !pixels || !world->battle_enemy_count ||
        world->battle_enemy_count>3) return -1;
    for (unsigned i=0;i<3;++i) if (world->party_active[i]) slots[count++]=i;
    if (!count) return -1;
    for (unsigned i=0;i<world->battle_enemy_count;++i) {
        if ((int16_t)world->battle_enemy_hps[i]<=0) continue;
        if (world_enemy_draw_bitmap(file,world,i,world->battle_enemy_ids[i],&image)) return -1;
        int left=npe_world_battle_coordinate(world,1,i,0);
        int top=npe_world_battle_coordinate(world,1,i,1);
        /* Source32:E5E4..E7BC: clear the original inclusive rectangle,
           then draw at x-2 with zero transparency, in physical order. */
        for (int y=top;y<=top+50 && y<NPE_HEIGHT;++y)
            for (int x=left;x<=left+40 && x<NPE_WIDTH;++x)
                pixels[(unsigned)y*NPE_WIDTH+(unsigned)x]=0;
        if(blit_actor(file,&image,left-2,
            npe_world_battle_coordinate(world,1,i,1),pixels))return -1;
    }
    /* Source32:EC8A..EDE2 moves every positive party record to (142+25*i,59),
       clearing its original inclusive rectangle. Dead records skip movement. */
    for (unsigned compact=count;compact-- >0;) {

        unsigned slot=slots[compact],pose,id;
        if ((int16_t)world->hp[slot]<=0) continue;
        int left=npe_world_battle_coordinate(world,0,slot,0),top=npe_world_battle_coordinate(world,0,slot,1);
        for (int y=top;y<=top+40 && y<NPE_HEIGHT;++y)
            for (int x=left;x<=left+32 && x<NPE_WIDTH;++x)
                pixels[(unsigned)y*NPE_WIDTH+(unsigned)x]=0;
        pose=(int16_t)world->hp[slot]<=(int16_t)world->max_hp[slot]/5 ? 2 : 0;
        id=npe_world_battle_pose(world,slot,pose);
        if(battle_draw_resource(file,world,(uint16_t)id,142+25*(int)compact,59,0,pixels))return -1;
    }
    return 0;
}

/* Native type0 without an effect group uses character poses5 and6.
   Stages: approach, strike, return; all work on the current scene cache. */
int npe_render_battle_attack_stage(const NpeFile *file, const NpeWorld *world,
                                    unsigned target, unsigned stage,
                                    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    unsigned slot;
    NpeIndexedBitmap image;
    if(!file||!world||!pixels||stage>2||!world->battle_enemy_count||
        world->battle_enemy_count>3||target>=world->battle_enemy_count||
        npe_world_battle_action_actor(world)>=3||!world->party_active[npe_world_battle_action_actor(world)])return -1;
    slot=npe_world_battle_action_actor(world);
    /*31:79FE /2F:D23C test exactly1 for the group pose, whereas
      the numeric HP dispatch also accepts other nonzero group values. */
    unsigned group=world->battle_item_groups[slot]==1;
    int ex=npe_world_battle_coordinate(world,1,target,0);
    int ey=npe_world_battle_coordinate(world,1,target,1);
    for(unsigned receiver=0;stage!=1 && receiver<world->battle_enemy_count;++receiver){
        if((!group&&receiver!=target)||(int16_t)world->battle_enemy_hps[receiver]<=0)continue;
        int rx=npe_world_battle_coordinate(world,1,receiver,0);
        int ry=npe_world_battle_coordinate(world,1,receiver,1);
        int left=stage?rx-2:rx;
        for(int y=ry;y<=ry+50&&y<NPE_HEIGHT;++y)
            for(int x=left;x<=left+40&&x<NPE_WIDTH;++x)
                if(x>=0)pixels[(unsigned)y*NPE_WIDTH+(unsigned)x]=0;
        if(world_enemy_draw_bitmap(file,world,receiver,world->battle_enemy_ids[receiver],&image))return -1;
        if(stage){if(blit_battle_actor(file,&image,rx,ry,pixels))return -1;}
        else {if(blit_actor(file,&image,rx-2,ry,pixels))return -1;}
    }
    int left=stage?ex+40:npe_world_battle_coordinate(world,0,slot,0);
    int top=stage?ey+10:npe_world_battle_coordinate(world,0,slot,1);
    for(int y=top;y<=top+40&&y<NPE_HEIGHT;++y)
        for(int x=left;x<=left+32&&x<NPE_WIDTH;++x)
            if(x>=0&&y>=0)pixels[(unsigned)y*NPE_WIDTH+(unsigned)x]=0;
    /* Source31:7DFA..7E20 selects acting type0/type4, then 7EBC
       overrides the low-HP pose with pose0 for its return. */
    unsigned pose=stage==0?5:stage==1?6:0;
    unsigned id=npe_world_battle_pose(world,slot,pose);
    left=stage==2?npe_world_battle_coordinate(world,0,slot,0):ex+(stage?24:40);
    top=stage==2?npe_world_battle_coordinate(world,0,slot,1):ey+10;
    if(battle_draw_resource(file,world,(uint16_t)id,left,top,stage==1?2:0,pixels))return -1;
    /* Source31:849C..869A redraws the preceding physical enemy record
       after a party attacker returns, if that record has signed HP > 0.
       The strike rectangle can overlap that neighbour's cached bitmap. */
    if(stage==2 && !group && target>0 && (int16_t)world->battle_enemy_hps[target-1]>0){
        if(world_enemy_draw_bitmap(file,world,target-1,world->battle_enemy_ids[target-1],&image))return -1;
        if(blit_battle_actor(file,&image,npe_world_battle_coordinate(world,1,target-1,0),
                          npe_world_battle_coordinate(world,1,target-1,1),pixels))return -1;
    }
    return 0;
}

typedef struct {
    unsigned kind,own,group;
    uint16_t attack,defense;
    int16_t statuses[4];
} BattleDisplayEffect;

static int battle_display_effect(const NpeFile *file,const NpeWorld *world,
                                 BattleDisplayEffect *effect){
    if(!file||!world||!effect||npe_world_battle_action_actor(world)>=3)return -1;
    const NpeBattleCommand *command=&world->battle_commands[npe_world_battle_action_actor(world)];
    if(command->kind==COMMAND_ITEM || command->kind==COMMAND_THROW){
        NpeItem item;uint8_t raw[41];
        if(npe_world_battle_item_view(world,npe_world_battle_action_actor(world),raw,&item))return -1;
        effect->kind=item.effect_type;effect->own=item.target_side==1;
        effect->group=item.group==1;effect->attack=(uint16_t)item.attack_bonus;
        /* Source2D:4584 converts throwing type31 to scalar type1 with
           its attack word moved to HP, so no attack label is drawn. */
        if(command->kind==COMMAND_THROW && item.effect_type==31){
            effect->kind=1;effect->attack=0;
        }
        effect->defense=(uint16_t)item.defense_bonus;
        memcpy(effect->statuses,item.status_effects,sizeof(effect->statuses));
    }else{
        NpeSkill skill;uint8_t raw[42];
        if(!command->resource_id||npe_world_battle_magic_view(world,npe_world_battle_action_actor(world),raw,&skill))return -1;
        effect->kind=skill.raw[14];effect->own=skill.raw[38]==1;
        effect->group=skill.raw[37]==1;effect->attack=skill.parameter_a;
        effect->defense=skill.parameter_b;
        for(unsigned status=0;status<4;++status)
            effect->statuses[status]=npe_skill_status_effect(&skill,status);
    }
    return 0;
}

int npe_render_battle_party_spell_stage(const NpeFile *file,const NpeWorld *world,
                                      unsigned target,unsigned stage,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    unsigned slots[3],count=0,actor=world?npe_world_battle_action_actor(world):0;
    BattleDisplayEffect effect;NpeIndexedBitmap image;
    if(!file||!world||!pixels||stage>1||actor>=3||!world->party_active[actor]||
       !world->battle_enemy_count||world->battle_enemy_count>3||
       battle_display_effect(file,world,&effect))return -1;
    unsigned own=effect.own,group=effect.group;
    if(own?(target>=3||!world->party_active[target]):target>=world->battle_enemy_count)return -1;
    for(unsigned i=0;i<3;++i)if(world->party_active[i])slots[count++]=i;
    /* Source32:E552..E7BC /31:7976..7BBE: hostile receivers move
       left2 in physical order; return clears that displaced rectangle. */
    if(!own)for(unsigned i=0;i<world->battle_enemy_count;++i){
        if((!group&&i!=target)||(int16_t)world->battle_enemy_hps[i]<=0)continue;
        int x=npe_world_battle_coordinate(world,1,i,0),y=npe_world_battle_coordinate(world,1,i,1),left=x-(stage?2:0);
        for(int py=y;py<=y+50&&py<NPE_HEIGHT;++py)
            for(int px=left;px<=left+40&&px<NPE_WIDTH;++px)
                if(px>=0)pixels[(unsigned)py*NPE_WIDTH+(unsigned)px]=0;
        if(world_enemy_draw_bitmap(file,world,i,world->battle_enemy_ids[i],&image))return -1;
        if(stage){if(blit_battle_actor(file,&image,x,y,pixels))return -1;}
        else {if(blit_actor(file,&image,x-2,y,pixels))return -1;}
    }
    for(unsigned compact=count;compact-- >0;){
        unsigned slot=slots[compact];
        if(slot!=actor&&!(own&&slot==target))continue;
        if((int16_t)world->hp[slot]<=0)continue;
        /*32:EE48..EE6E reads this physical party record's queued flags,
          including a receiver whose command differs from the acting spell.
          Own-side group records retain their pose during preparation. */
        if(!stage && world->battle_item_groups[slot]==1 &&
           world->battle_item_sides[slot]==1)continue;
        int x=npe_world_battle_coordinate(world,0,slot,0),y=npe_world_battle_coordinate(world,0,slot,1);
        /* Source31:7EC2..7F40 clears the selected receiver at x+2
           before restoring its original pose, also when an own group
           uses the caster as its selected record. */
        int clear_left=x+(stage&&own&&slot==target?2:0);
        for(int py=y;py<=y+40&&py<NPE_HEIGHT;++py)
            for(int px=clear_left;px<=clear_left+32&&px<NPE_WIDTH;++px)pixels[(unsigned)py*NPE_WIDTH+(unsigned)px]=0;
        unsigned pose=world->battle_commands[slot].kind==COMMAND_DEFEND?4:
            (int16_t)world->hp[slot]<=(int16_t)world->max_hp[slot]/5?2:0;
        unsigned id=npe_world_battle_pose(world,slot,pose);
        if(battle_draw_resource(file,world,(uint16_t)id,x,y,0,pixels))return -1;
    }
    /* Source31:849C..869A also redraws the physical enemy preceding
       a hostile selected target. Its border can be erased by the return. */
    if(stage&&!own&&target>0&&(int16_t)world->battle_enemy_hps[target-1]>0){
        if(world_enemy_draw_bitmap(file,world,target-1,world->battle_enemy_ids[target-1],&image))return -1;
        if(blit_battle_actor(file,&image,npe_world_battle_coordinate(world,1,target-1,0),npe_world_battle_coordinate(world,1,target-1,1),pixels))return -1;
    }
    return 0;
}

static int render_battle_enemy_party_stage(const NpeFile *file,const NpeWorld *world,
                                         unsigned target,unsigned stage,unsigned spell,
                                         uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    unsigned enemy=world?world->battle_animation_enemy:0;
    NpeIndexedBitmap image;
    if(!file||!world||!pixels||stage>1||target>=3||!world->party_active[target]||
       !world->battle_enemy_count||world->battle_enemy_count>3||enemy>=world->battle_enemy_count)return -1;
    int px=npe_world_battle_coordinate(world,0,target,0),py=npe_world_battle_coordinate(world,0,target,1);
    unsigned group=0;
    if(spell){
        NpeSkill skill;uint8_t raw[42];
        if(world->skill_record_id!=world->battle_enemy_last_skill[enemy] ||
           npe_world_skill_view(world,raw,&skill))return -1;
        group=skill.raw[37]==1;
    }
    /* Source31:79FE..7A52 restores every positive enemy for hostile
       group effects, including unshifted receivers on the caster's side. */
    for(unsigned i=0;i<world->battle_enemy_count;++i){
        if(i!=enemy && !(spell && stage && group))continue;
        if((int16_t)world->battle_enemy_hps[i]<=0)continue;
        int ex=npe_world_battle_coordinate(world,1,i,0),ey=npe_world_battle_coordinate(world,1,i,1);
        int left=stage?(spell?ex+(i==enemy?4:0):px-40):ex;
        int top=stage?(spell?ey:py-10):ey;
        for(int y=top;y<=top+50&&y<NPE_HEIGHT;++y)
            for(int x=left;x<=left+40&&x<NPE_WIDTH;++x)
                if(x>=0&&y>=0)pixels[(unsigned)y*NPE_WIDTH+(unsigned)x]=0;
        if(world_enemy_draw_bitmap(file,world,i,world->battle_enemy_ids[i],&image))return -1;
        if(stage){if(blit_battle_actor(file,&image,ex,ey,pixels))return -1;}
        else if(file->rpg_runtime){
            int result=npe_rpg_runtime_draw_indexed(file,(int16_t)image.resource_index,
                (int16_t)(spell?ex+4:px-40),(int16_t)(spell?ey:py-10),1,pixels);
            if(result==-2)return -1;
        }else {
            /* Source48:01DA..0200 selects firmware03:42C4 (mode1).
               Ordinary approach and in-place spell preparation retain occupied pixels. */
            for(unsigned y=0;y<image.height;++y)for(unsigned x=0;x<image.width;++x){
                int dx=(spell?ex+4:px-40)+(int)x,dy=(spell?ey:py-10)+(int)y;
                if(dx>=0&&dx<NPE_WIDTH&&dy>=0&&dy<NPE_HEIGHT){
                    unsigned offset=(unsigned)dy*NPE_WIDTH+(unsigned)dx;
                    if(!pixels[offset])pixels[offset]=npe_indexed_pixel(&image,x,y);
                }
            }
        }
    }
    for(unsigned i=0;i<3;++i){
        if(!world->party_active[i])continue;
        px=npe_world_battle_coordinate(world,0,i,0);py=npe_world_battle_coordinate(world,0,i,1);
        if(i!=target)continue;
        if((int16_t)world->hp[i]<=0)continue;
        /*32:EE48..EE6E also applies when this record receives an enemy
          attack or spell: test its retained command flags, not the enemy's. */
        if(!stage && world->battle_item_groups[i]==1 &&
           world->battle_item_sides[i]==1)continue;
        int left=px+(stage?2:0);
        for(int y=py;y<=py+40&&y<NPE_HEIGHT;++y)
            for(int x=left;x<=left+32&&x<NPE_WIDTH;++x)
                pixels[(unsigned)y*NPE_WIDTH+(unsigned)x]=0;
        /* Source32:EC22..EC2E / EF78 receiving pose and queued defense;
           Source31:80F0..810A restores only the selected party record. */
        unsigned pose=world->battle_commands[i].kind==COMMAND_DEFEND?4:
            !stage?1:(int16_t)world->hp[i]<=(int16_t)world->max_hp[i]/5?2:0;
        unsigned id=npe_world_battle_pose(world,i,pose);
        if(battle_draw_resource(file,world,(uint16_t)id,px+(stage?0:2),py,0,pixels))return -1;
    }
    return 0;
}

int npe_render_battle_enemy_attack_stage(const NpeFile *file,const NpeWorld *world,
                                       unsigned target,unsigned stage,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    int result=render_battle_enemy_party_stage(file,world,target,stage,0,pixels);
    if(result || !stage || !target)return result;
    /*31:82A0..8498 restores the physical party record preceding a
      hostile receiver after the enemy returns. Unlike the receiver,
      this neighbouring pose ignores the queued defense command. */
    unsigned previous=target-1;
    unsigned pose=(int16_t)world->hp[previous]<=0?3:
        (int16_t)world->hp[previous]<=(int16_t)world->max_hp[previous]/5?2:0;
    return battle_draw_resource(file,world,(uint16_t)npe_world_battle_pose(world,previous,pose),
        npe_world_battle_coordinate(world,0,previous,0),npe_world_battle_coordinate(world,0,previous,1),0,pixels);
}

int npe_render_battle_enemy_spell_stage(const NpeFile *file,const NpeWorld *world,
                                      unsigned target,unsigned stage,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    NpeSkill skill;uint8_t raw[42];
    if(!file||!world||world->battle_animation_enemy>=world->battle_enemy_count)return -1;
    unsigned id=world->battle_enemy_last_skill[world->battle_animation_enemy];
    if(!id||world->skill_record_id!=id||npe_world_skill_view(world,raw,&skill)||skill.raw[38]==1||
       (skill.raw[14]!=1 && skill.raw[14]!=2))return -1;
    /* Source32:E4E2..E54E leaves the caster in its formation, shifted right4. */
    return render_battle_enemy_party_stage(file,world,target,stage,1,pixels);
}

int npe_render_battle_enemy_recovery_stage(const NpeFile *file,const NpeWorld *world,
                                         unsigned target,unsigned stage,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    NpeSkill skill;uint8_t raw[42];
    if(!file||!world||!pixels||stage>1||!world->battle_enemy_count||world->battle_enemy_count>3||
       target>=world->battle_enemy_count||world->battle_animation_enemy>=world->battle_enemy_count)return -1;
    unsigned actor=world->battle_animation_enemy,id=world->battle_enemy_last_skill[actor];
    if(!id||world->skill_record_id!=id||npe_world_skill_view(world,raw,&skill)||skill.raw[38]!=1||
       (skill.raw[14]!=1 && skill.raw[14]!=2))return -1;
    /* Source32:E4E2..E54E moves an enemy caster right4. Only the selected
       receiver moves left2, including group healing; page31 restores them. */
    for(unsigned i=0;i<world->battle_enemy_count;++i){
        if((int16_t)world->battle_enemy_hps[i]<=0 ||
           (i!=actor && i!=target))continue;
        int x=npe_world_battle_coordinate(world,1,i,0),y=npe_world_battle_coordinate(world,1,i,1);
        int shift=i==actor?4:-2,left=x+(stage?shift:0);
        for(int py=y;py<=y+50&&py<NPE_HEIGHT;++py)
            for(int px=left;px<=left+40&&px<NPE_WIDTH;++px)
                if(px>=0&&py>=0)pixels[(unsigned)py*NPE_WIDTH+(unsigned)px]=0;
        NpeIndexedBitmap image;
        if(world_enemy_draw_bitmap(file,world,i,world->battle_enemy_ids[i],&image))return -1;
        if(stage){if(blit_battle_actor(file,&image,x,y,pixels))return -1;}
        else if(file->rpg_runtime){
            int result=npe_rpg_runtime_draw_indexed(file,(int16_t)image.resource_index,
                                                   (int16_t)(x+shift),(int16_t)y,1,pixels);
            if(result==-2)return -1;
        }
        else for(unsigned py=0;py<image.height;++py)for(unsigned px=0;px<image.width;++px){
            int dx=x+shift+(int)px,dy=y+(int)py;
            if(dx>=0&&dx<NPE_WIDTH&&dy>=0&&dy<NPE_HEIGHT){
                unsigned offset=(unsigned)dy*NPE_WIDTH+(unsigned)dx;
                if(!pixels[offset])pixels[offset]=npe_indexed_pixel(&image,px,py);
            }
        }
    }
    return 0;
}

int npe_render_battle_poison_numbers(const NpeWorld *world,unsigned step,
                                    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    unsigned count=0;
    if(!world||!pixels||step>=3||!world->battle_enemy_count||world->battle_enemy_count>3)return -1;
    for(unsigned i=0;i<3;++i)count+=world->party_active[i]!=0;
    if(!count)return -1;
    /* Source36:73EE/7436 and758C/75D4: (x+20,y+6), up7 per step.
       Each side displays its own amount, even though HP uses a shared word. */
    for(unsigned i=0;i<6;++i){
        unsigned enemy=i>=3,slot=enemy?i-3:i;
        int x,y;
        if(enemy){
            if(slot>=world->battle_enemy_count)continue;
            x=npe_world_battle_coordinate(world,1,slot,0);y=npe_world_battle_coordinate(world,1,slot,1);
            if((int16_t)world->battle_enemy_hps[slot]<=0||world->battle_enemy_status[slot][0]>=0)continue;
        }else {
            if(!world->party_active[slot])continue;
            x=npe_world_battle_coordinate(world,0,slot,0);y=npe_world_battle_coordinate(world,0,slot,1);
            if((int16_t)world->hp[slot]<=0||world->battle_party_status[slot][0]>=0)continue;
        }
        x+=20;y+=6-7*(int)step;
        unsigned damage=enemy?world->battle_enemy_damage_values[slot]:world->battle_party_damage_values[slot];
        char number[16];snprintf(number,sizeof(number),"-%u",damage);
        for(unsigned n=0;number[n];++n){
            const uint8_t *glyph=npe_native_counter_glyph(number[n]);if(!glyph)return -1;
            for(unsigned gy=0;gy<10;++gy)for(unsigned gx=0;gx<5;++gx){
                int px=x+6*(int)n+(int)gx,py=y+(int)gy;
                if(px>=0&&px<NPE_WIDTH&&py>=0&&py<NPE_HEIGHT&&(glyph[gy]&(1u<<(4-gx))))
                    pixels[(unsigned)py*NPE_WIDTH+(unsigned)px]=15;
            }
        }
    }
    return 0;
}

static int render_battle_party_numbers(const NpeWorld *world,unsigned step,unsigned recovery,
                                      uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    unsigned count=0;
    if(!world||!pixels||step>=3)return -1;
    for(unsigned i=0;i<3;++i)count+=world->party_active[i]!=0;
    if(!count)return -1;
    /* Source30:16E4/1728: original party (x+10,y+4), up7 per step. */
    for(unsigned i=0;i<3;++i){
        if(!world->party_active[i])continue;
        int damage=(int16_t)world->battle_party_damage_values[i];
        int x=npe_world_battle_coordinate(world,0,i,0)+10,y=npe_world_battle_coordinate(world,0,i,1)+4-7*(int)step;
        if((int16_t)world->hp[i]<=0||damage<=0)continue;
        char number[16];snprintf(number,sizeof(number),"%c%d",recovery?'+':'-',damage);
        for(unsigned n=0;number[n];++n){
            const uint8_t *glyph=npe_native_counter_glyph(number[n]);if(!glyph)return -1;
            for(unsigned gy=0;gy<10;++gy)for(unsigned gx=0;gx<5;++gx){
                int px=x+6*(int)n+(int)gx,py=y+(int)gy;
                if(px>=0&&px<NPE_WIDTH&&py>=0&&py<NPE_HEIGHT&&(glyph[gy]&(1u<<(4-gx))))pixels[(unsigned)py*NPE_WIDTH+(unsigned)px]=15;
            }
        }
    }
    return 0;
}

static int render_battle_enemy_effect_numbers(const NpeWorld *world,unsigned step,
                                             uint8_t *pixels);

int npe_render_battle_party_damage_numbers(const NpeWorld *world,unsigned step,
                                         uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    if(world&&world->battle_executing&&
       npe_world_battle_action_actor(world)>=world_record_party_count(world))
        return render_battle_enemy_effect_numbers(world,step,pixels);
    return render_battle_party_numbers(world,step,0,pixels);
}

static int render_battle_effect_numbers(const NpeWorld *world,unsigned step,
                                        unsigned own,unsigned group,
                                        unsigned party_receiver,uint8_t *pixels){
    uint8_t actor[148];
    if(npe_world_battle_record_image(world,npe_world_battle_action_actor(world),actor))return -1;
    unsigned count=world_record_party_count(world)+world->battle_enemy_count;
    for(unsigned slot=0;slot<count&&slot<6;++slot){
        /*30:1614..162C limits a single action to its saved target. */
        if(!group&&slot+1!=actor[126])continue;
        uint8_t record[148];
        if(npe_world_battle_record_image(world,slot,record))return -1;
        int value=resident_record_signed(actor,102+2*slot);
        /*30:1648..165A chooses the coordinate/activity branch from
          the selected receiver's physical side, independently of
          the effect sign. Groups retain the other prefix's words. */
        if(value<=0 || (party_receiver&&!record[43]) || resident_record_signed(record,80)<=0)continue;
        int x=record[124]+10,y=record[125]+(party_receiver?4:6)-7*(int)step;
        char number[16];snprintf(number,sizeof(number),"%c%d",own?'+':'-',value);
        for(unsigned n=0;number[n];++n){
            const uint8_t *glyph=npe_native_counter_glyph(number[n]);if(!glyph)return -1;
            for(unsigned gy=0;gy<10;++gy)for(unsigned gx=0;gx<5;++gx){
                int px=x+6*(int)n+(int)gx,py=y+(int)gy;
                if(px>=0&&px<NPE_WIDTH&&py>=0&&py<NPE_HEIGHT&&(glyph[gy]&(1u<<(4-gx))))
                    pixels[(unsigned)py*NPE_WIDTH+(unsigned)px]=15;
            }
        }
    }
    return 0;
}

static int render_battle_enemy_effect_numbers(const NpeWorld *world,unsigned step,
                                             uint8_t *pixels){
    if(!world||!pixels||step>=3||!world->battle_file)return -1;
    uint8_t actor[148];
    if(npe_world_battle_record_image(world,npe_world_battle_action_actor(world),actor))return -1;
    unsigned party_count=world_record_party_count(world);
    unsigned party_receiver=world->battle_display_target<=party_count;
    if(actor[130]==1)
        return render_battle_effect_numbers(world,step,actor[132]==1,actor[131],party_receiver,pixels);
    if(actor[130]!=2)return 0;
    uint8_t raw[42];NpeSkill skill;
    if(npe_world_skill_view(world,raw,&skill))return -1;
    int16_t effects[4];for(unsigned s=0;s<4;++s)effects[s]=npe_skill_status_effect(&skill,s);
    uint8_t target_record[148];
    if(!actor[126]||npe_world_battle_record_image(world,actor[126]-1,target_record))return -1;
    for(unsigned slot=0;slot<party_count+world->battle_enemy_count&&slot<6;++slot){
        if(!actor[131]&&slot+1!=actor[126])continue;
        uint8_t record[148];if(npe_world_battle_record_image(world,slot,record))return -1;
        /*30:2522..2554 requires a positive prepared word even for
          labels. Single enemy status spells do not set a marker. */
        if(resident_record_signed(actor,102+2*slot)<=0 ||
           (party_receiver&&!record[43]) || resident_record_signed(record,80)<=0)continue;
        unsigned label[2]={0,0},length=0;
        /*30:20DA..2120 falls through after the all-positive label;
          the first positive status replaces it. Negative labels use
          the saved selected target, including a group display. */
        for(unsigned s=0;s<4;++s){
            if(!effects[s])continue;
            if(effects[s]>0){label[0]=4;label[1]=s;length=2;}
            else{
                int16_t changed=(int16_t)(uint16_t)((uint16_t)resident_record_signed(target_record,62+2*s)+(uint16_t)effects[s]);
                if(changed<0){label[0]=s;length=1;}
                else{label[0]=6;label[1]=7;length=2;}
            }
            break;
        }
        int x=record[124]+10,y=record[125]+(party_receiver?4:6)-7*(int)step;
        for(unsigned n=0;n<length;++n){
            const uint8_t *glyph=label[n]<4?battle_status_glyphs[label[n]]:battle_spell_glyphs[label[n]-4];
            for(unsigned gy=0;gy<12;++gy){unsigned bits=(unsigned)glyph[2*gy]<<8|glyph[2*gy+1];
                for(unsigned gx=0;gx<12;++gx){int px=x+12*(int)n+(int)gx,py=y+(int)gy;
                    if(px>=0&&px<NPE_WIDTH&&py>=0&&py<NPE_HEIGHT&&(bits&(0x8000u>>gx)))
                        pixels[(unsigned)py*NPE_WIDTH+(unsigned)px]=15;
                }
            }
        }
    }
    return 0;
}

static int render_battle_party_effect_labels(const NpeWorld *world,unsigned step,
                                             const BattleDisplayEffect *effect,
                                             uint8_t *pixels){
    uint8_t actor[148],target[148];
    if(npe_world_battle_record_image(world,npe_world_battle_action_actor(world),actor) ||
       !actor[126] || npe_world_battle_record_image(world,actor[126]-1,target))return -1;
    unsigned kind=actor[130],revival=kind>=16&&kind<=19;
    unsigned label[2]={0,0},length=0;
    if(revival){label[0]=10;label[1]=11;length=2;}
    else if(kind==1){label[0]=(int16_t)effect->defense>0?9:8;length=1;}
    else if(kind==2){
        const int16_t *effects=effect->statuses;
        /*30:1BC4..1BE2 jumps directly to drawing the party's all-positive
          immunity label. Unlike the enemy path, it does not fall through. */
        if(effects[0]>0&&effects[1]>0&&effects[2]>0&&effects[3]>0){
            label[0]=5;label[1]=4;length=2;
        }else for(unsigned s=0;s<4;++s){
            if(!effects[s])continue;
            if(effects[s]>0){label[0]=4;label[1]=s;length=2;}
            else{
                /*30:1C40..1FDC reads the saved selected target for all
                  group labels. C commits the status after this callback. */
                int16_t changed=(int16_t)(uint16_t)((uint16_t)resident_record_signed(target,62+2*s)+(uint16_t)effects[s]);
                if(changed<0){label[0]=s;length=1;}
                else{label[0]=6;label[1]=7;length=2;}
            }
            break;
        }
    }else return 0;
    unsigned party_count=world_record_party_count(world);
    unsigned party_receiver=world->battle_display_target<=party_count;
    /*30:15AE..1610 replaces each prepared display value for party revival
      with half the selected maximum: signed low-word (5 * max) / 10.
      This gate is independent of the HP actually restored by the item. */
    int revival_value=(int16_t)(uint16_t)(5u*(uint16_t)resident_record_signed(target,78))/10;
    for(unsigned slot=0;slot<party_count+world->battle_enemy_count&&slot<6;++slot){
        if(!actor[131]&&slot+1!=actor[126])continue;
        uint8_t record[148];if(npe_world_battle_record_image(world,slot,record))return -1;
        int value=revival?revival_value:resident_record_signed(actor,102+2*slot);
        int hp=resident_record_signed(record,80);
        /*30:1648..169E chooses activity/coordinates from the selected side;
          1850..187C reverses the HP gate for revival, 2522 tests the value. */
        if(value<=0 || (party_receiver&&!record[43]) || (revival?hp>0:hp<=0))continue;
        int x=record[124]+10,y=record[125]+(party_receiver?4:6)-7*(int)step;
        for(unsigned n=0;n<length;++n){
            const uint8_t *glyph=label[n]<4?battle_status_glyphs[label[n]]:battle_spell_glyphs[label[n]-4];
            for(unsigned gy=0;gy<12;++gy){unsigned bits=(unsigned)glyph[2*gy]<<8|glyph[2*gy+1];
                for(unsigned gx=0;gx<12;++gx){int px=x+12*(int)n+(int)gx,py=y+(int)gy;
                    if(px>=0&&px<NPE_WIDTH&&py>=0&&py<NPE_HEIGHT&&(bits&(0x8000u>>gx)))
                        pixels[(unsigned)py*NPE_WIDTH+(unsigned)px]=15;
                }
            }
        }
    }
    return 0;
}

int npe_render_battle_party_spell_numbers(const NpeWorld *world,unsigned step,
                                        uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    BattleDisplayEffect effect;
    if(!world||!pixels||step>=3||npe_world_battle_action_actor(world)>=3||!world->battle_file||
       battle_display_effect(world->battle_file,world,&effect))return -1;
    /* Source30:1880..2522 constructs engine labels before drawing at
       the same three floating positions. Status services have already
       added the effect words for this decision; do not mutate live HP/QI. */
    unsigned revival=effect.kind>=16 && effect.kind<=19;
    if(revival||effect.kind==2||(effect.kind==1 &&
       ((int16_t)effect.attack>0||(int16_t)effect.defense>0))){
        if(world->battle_executing)
            return render_battle_party_effect_labels(world,step,&effect,pixels);
        unsigned own=effect.own;
        const int16_t *effects=effect.statuses;
        for(unsigned i=0;i<(own?3:world->battle_enemy_count);++i){
            if(own&&!world->party_active[i])continue;
            int x=own?npe_world_battle_coordinate(world,0,i,0)+10:npe_world_battle_coordinate(world,1,i,0)+10;
            int y=(own?npe_world_battle_coordinate(world,0,i,1)+4:npe_world_battle_coordinate(world,1,i,1)+6)-7*(int)step;
            int16_t hp=(int16_t)(own?world->hp[i]:world->battle_enemy_hps[i]);
            /* Source30:1850..187C reverses the signed HP gate for
               revival labels: only records still fallen at display time. */
            if((revival?hp>0:hp<=0)||
               !(own?world->battle_party_damage_values[i]:world->battle_enemy_damage_values[i]))continue;
            unsigned label[2]={0,0},length=0;
            if(revival){label[0]=10;label[1]=11;length=2;}
            else if(effect.kind!=2){label[0]=(int16_t)effect.defense>0?9:8;length=1;}
            else if(effects[0]>0&&effects[1]>0&&effects[2]>0&&effects[3]>0){label[0]=5;label[1]=4;length=2;}
            else for(unsigned s=0;s<4;++s){
                if(!effects[s])continue;
                if(effects[s]>0){label[0]=4;label[1]=s;length=2;}
                else {
                    int16_t old=own?world->battle_party_status[i][s]:world->battle_enemy_status[i][s];
                    int16_t changed=(int16_t)(uint16_t)((uint16_t)old+(uint16_t)effects[s]);
                    if(changed<0){label[0]=s;length=1;}
                    else {label[0]=6;label[1]=7;length=2;}
                }
                break;
            }
            for(unsigned n=0;n<length;++n){
                const uint8_t *glyph=label[n]<4?battle_status_glyphs[label[n]]:battle_spell_glyphs[label[n]-4];
                for(unsigned gy=0;gy<12;++gy){unsigned bits=(unsigned)glyph[2*gy]<<8|glyph[2*gy+1];
                    for(unsigned gx=0;gx<12;++gx){int px=x+12*(int)n+(int)gx,py=y+(int)gy;
                        if(px>=0&&px<NPE_WIDTH&&py>=0&&py<NPE_HEIGHT&&(bits&(0x8000u>>gx)))pixels[(unsigned)py*NPE_WIDTH+(unsigned)px]=15;
                    }
                }
            }
        }
        return 0;
    }
    if(effect.kind!=1)return 0;
    if(world->battle_executing)
        return render_battle_effect_numbers(world,step,effect.own,effect.group,
            world->battle_display_target<=world_record_party_count(world),pixels);
    return effect.own?render_battle_party_numbers(world,step,1,pixels):
        npe_render_battle_damage_numbers(world,step,pixels);
}

int npe_render_battle_defend_stage(const NpeFile *file, const NpeWorld *world,
                                    uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]) {
    unsigned count=0,slots[3],slot;
    if(!file||!world||!pixels||npe_world_battle_action_actor(world)>=3||
        !world->party_active[npe_world_battle_action_actor(world)])return -1;
    slot=npe_world_battle_action_actor(world);
    for(unsigned i=0;i<3;++i)if(world->party_active[i])slots[count++]=i;
    /* Native type5 also redraws the first compact party record. Pose
       selection is per record (source32:EF78/31:8200), not shared with
       the acting defender; three defenders redraw first+current only. */
    for(unsigned compact=count;compact-- >0;){
        unsigned member=slots[compact];
        if(member!=slot&&compact!=0)continue;
        if((int16_t)world->hp[member]<=0)continue;
        int x=npe_world_battle_coordinate(world,0,member,0),y=npe_world_battle_coordinate(world,0,member,1);
        for(int py=y;py<=y+40&&py<NPE_HEIGHT;++py)
            for(int px=x;px<=x+32&&px<NPE_WIDTH;++px)
                pixels[(unsigned)py*NPE_WIDTH+(unsigned)px]=0;
        unsigned pose=member==slot||world->battle_commands[member].kind==COMMAND_DEFEND?4:
            (int16_t)world->hp[member]<=(int16_t)world->max_hp[member]/5?2:0;
        unsigned id=npe_world_battle_pose(world,member,pose);
        if(battle_draw_resource(file,world,(uint16_t)id,x,y,0,pixels))return -1;
    }
    return 0;
}

int npe_render_battle_defend_restore(const NpeFile *file,const NpeWorld *world,
                                      uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    unsigned actor=world?npe_world_battle_action_actor(world):0,count=0,slots[3];
    if(!file||!world||!pixels||actor>=3||!world->party_active[actor])return -1;
    for(unsigned slot=0;slot<3;++slot)if(world->party_active[slot])slots[count++]=slot;
    /*39:604A calls31:7654 after the temporary defense LCD. Its party
      branch31:80F6..810A uses the actual actor and saved physical target,
      rather than the initial display's first-record argument. */
    for(unsigned compact=count;compact-- >0;){
        unsigned slot=slots[compact];
        if(slot!=actor&&compact+1!=world->battle_display_target)continue;
        if((int16_t)world->hp[slot]<=0)continue;
        int x=npe_world_battle_coordinate(world,0,slot,0),y=npe_world_battle_coordinate(world,0,slot,1);
        /*31:7EC2..7F40 shifts only the clear rectangle of the saved
          receiver by two pixels.813E..81BE reloads its original drawing
          coordinates; reverse record order preserves neighbour clipping. */
        int clear_x=x+(compact+1==world->battle_display_target?2:0);
        for(int py=y;py<=y+40&&py<NPE_HEIGHT;++py)
            for(int px=clear_x;px<=clear_x+32&&px<NPE_WIDTH;++px)
                if(px>=0&&py>=0)pixels[(unsigned)py*NPE_WIDTH+(unsigned)px]=0;
        unsigned pose=(slot==actor||world->battle_commands[slot].kind==COMMAND_DEFEND)?4:
            (int16_t)world->hp[slot]<=(int16_t)world->max_hp[slot]/5?2:0;
        if(battle_draw_resource(file,world,(uint16_t)npe_world_battle_pose(world,slot,pose),x,y,0,pixels))return -1;
    }
    return 0;
}

int npe_render_battle_animation_frame(const NpeFile *file, unsigned group,
                                      unsigned frame, int left, int top,
                                      uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    NpeIndexedBitmap image;
    if (!pixels || npe_animation_frame(file,group,frame,&image)) return -1;
    if(blit_actor(file,&image,left,top,pixels))return -1;
    return 0;
}

int npe_render_battle_combine_restore(const NpeFile *file, const NpeWorld *world,
                                      uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    NpeIndexedBitmap image;
    unsigned slots[3],count=0;
    if (!file || !world || !pixels || !world->battle_enemy_count ||
        world->battle_enemy_count>3) return -1;
    for (unsigned i=0;i<3;++i) if (world->party_active[i]) slots[count++]=i;
    if (!count) return -1;
    /* Source31:7976..7BBE restores the original coordinates after clearing
       the displaced inclusive rectangle. Preserve neighbouring cached edges. */
    for (unsigned i=0;i<world->battle_enemy_count;++i) {
        if ((int16_t)world->battle_enemy_hps[i]<=0) continue;
        int left=npe_world_battle_coordinate(world,1,i,0);
        int top=npe_world_battle_coordinate(world,1,i,1);
        for (int y=top;y<=top+50 && y<NPE_HEIGHT;++y)
            for (int x=left-2;x<=left+38 && x<NPE_WIDTH;++x)
                if(x>=0)pixels[(unsigned)y*NPE_WIDTH+(unsigned)x]=0;
        if (world_enemy_draw_bitmap(file,world,i,world->battle_enemy_ids[i],&image)) return -1;
        if(blit_battle_actor(file,&image,left,top,pixels))return -1;
    }
    /* Source31:7F82..80EC clears the temporary group position in reverse
       record order. It may erase pixels from a neighbour already restored. */
    for (unsigned compact=count;compact-- >0;) {

        unsigned slot=slots[compact],pose,id;
        if ((int16_t)world->hp[slot]<=0) continue;
        int left=142+25*(int)compact,top=59;
        for (int y=top;y<=top+40 && y<NPE_HEIGHT;++y)
            for (int x=left;x<=left+32 && x<NPE_WIDTH;++x)
                pixels[(unsigned)y*NPE_WIDTH+(unsigned)x]=0;
        pose=(int16_t)world->hp[slot]<=(int16_t)world->max_hp[slot]/5 ? 2 : 0;
        id=npe_world_battle_pose(world,slot,pose);
        if(battle_draw_resource(file,world,(uint16_t)id,npe_world_battle_coordinate(world,0,slot,0),
            npe_world_battle_coordinate(world,0,slot,1),0,pixels))return -1;
    }
    return 0;
}

static int render_battle_enemy_numbers(const NpeWorld *world, unsigned step, unsigned recovery,
                                     uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    if (!world || !pixels || !world->battle_enemy_count ||
        world->battle_enemy_count>3 || step>=3) return -1;
    /* Source30:1772..184C and2522..25C0: three transparent text positions,
       at each record's original (x+10,y+6), moving up7 each iteration. */
    for (unsigned i=0;i<world->battle_enemy_count;++i) {
        int damage=(int16_t)world->battle_enemy_damage_values[i];
        if ((int16_t)world->battle_enemy_hps[i]<=0 || damage<=0) continue;
        char number[16];
        snprintf(number,sizeof(number),"%c%d",recovery?'+':'-',damage);
        int left=npe_world_battle_coordinate(world,1,i,0)+10;
        int top=npe_world_battle_coordinate(world,1,i,1)+6-7*(int)step;
        for (unsigned n=0;number[n];++n) {
            const uint8_t *glyph=npe_native_counter_glyph(number[n]);
            if (!glyph) return -1;
            for (unsigned y=0;y<10;++y)
                for (unsigned x=0;x<5;++x) {
                    int px=left+6*(int)n+(int)x,py=top+(int)y;
                    if(px>=0 && px<NPE_WIDTH && py>=0 && py<NPE_HEIGHT &&
                        (glyph[y]&(1u<<(4-x))))
                        pixels[(unsigned)py*NPE_WIDTH+(unsigned)px]=15;
                }
        }
    }
    return 0;
}

int npe_render_battle_damage_numbers(const NpeWorld *world,unsigned step,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    return render_battle_enemy_numbers(world,step,0,pixels);
}
int npe_render_battle_enemy_recovery_numbers(const NpeWorld *world,unsigned step,uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    if(world&&world->battle_executing&&
       npe_world_battle_action_actor(world)>=world_record_party_count(world))
        return render_battle_enemy_effect_numbers(world,step,pixels);
    return render_battle_enemy_numbers(world,step,1,pixels);
}

static int battle_party_target_position(const NpeWorld *world, unsigned target,
                                        int *x, int *y) {
    unsigned count = 0, compact = 0;
    if (!world || target >= 3 || !world->party_active[target]) return -1;
    for (unsigned i = 0; i < 3; ++i)
        if (world->party_active[i]) {
            if (i == target) compact = count;
            ++count;
        }
    *x = npe_world_battle_coordinate(world,0,target,0) + 12 +
         (int)((3 * ((2 * compact + 1) % 4) + 1 + 2 * compact) / 2);
    *y = npe_world_battle_coordinate(world,0,target,1) - 8;
    return 0;
}

int npe_render_battle_party_target(const NpeFile *file, const NpeWorld *world,
                                   unsigned target,
                                   uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    int x, y;
    if (!pixels || battle_party_target_position(world, target, &x, &y) ||
        npe_render_battle_world_scene(file, world, pixels)) return -1;
    if(battle_draw_resource(file,world,7,x,y,2,pixels))return -1;
    return 0;
}

int npe_draw_battle_party_target_pointer(const NpeFile *file,const NpeWorld *world,
                                         unsigned target,uint8_t *pixels) {
    int x,y;
    if(!pixels || battle_party_target_position(world,target,&x,&y))return -1;
    return battle_draw_resource(file,world,7,x,y,2,pixels);
}

int npe_clear_battle_party_target(const NpeWorld *world,unsigned target,uint8_t *pixels) {
    int left,top;if(!pixels || battle_party_target_position(world,target,&left,&top))return -1;
    for(int y=top;y<=top+4;++y)for(int x=left&~3;x<=left+8;++x)
        if(x>=0&&x<NPE_WIDTH&&y>=0&&y<NPE_HEIGHT)pixels[(unsigned)y*NPE_WIDTH+(unsigned)x]=0;
    return 0;
}

int npe_update_battle_party_target(const NpeFile *file, const NpeWorld *world,
                                   unsigned previous, unsigned target,
                                   uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    int left, top;
    if (!pixels || battle_party_target_position(world, previous, &left, &top)) return -1;
    if (previous == target) return 0;
    /* Preserve the scene while clearing the native old-pointer rectangle. */
    for (int y = top; y <= top + 4; ++y)
        for (int x = left & ~3; x <= left + 8; ++x)
            if(x>=0&&x<NPE_WIDTH&&y>=0&&y<NPE_HEIGHT)pixels[(unsigned)y * NPE_WIDTH + (unsigned)x] = 0;
    /* Counter updates clear nine 6x12 cells. Longer signed values may
       extend past this area, so their trailing cells remain on screen. */
    for (unsigned line = 0; line < 2; ++line)
        for (unsigned y = 88 + line * 16; y < 100 + line * 16; ++y)
            memset(pixels + y * NPE_WIDTH + 82, 0, 54);
    if (battle_party_target_position(world, target, &left, &top)) return -1;
    if(battle_draw_resource(file,world,7,left,top,2,pixels))return -1;
    return 0;
}

static int battle_enemy_target_position(const NpeWorld *world, unsigned target,
                                         int *left, int *top) {
    if (!world || !world->battle_enemy_count || world->battle_enemy_count > 3 ||
        target >= world->battle_enemy_count) return -1;
    *left = npe_world_battle_coordinate(world,1,target,0) + 16;
    *top = npe_world_battle_coordinate(world,1,target,1) - 4;
    return 0;
}

int npe_clear_battle_enemy_target(const NpeWorld *world, unsigned target,
                                  uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    int left,top;
    if (!pixels || battle_enemy_target_position(world,target,&left,&top)) return -1;
    for (int y=top;y<=top+4;++y)
        for (int x=left&~3;x<=left+8;++x)
            if(x>=0&&x<NPE_WIDTH&&y>=0&&y<NPE_HEIGHT)pixels[(unsigned)y*NPE_WIDTH+(unsigned)x]=0;
    return 0;
}

int npe_draw_battle_enemy_target_pointer(const NpeFile *file,const NpeWorld *world,
                                         unsigned target,uint8_t *pixels) {
    int x,y;
    if(!pixels || battle_enemy_target_position(world,target,&x,&y))return -1;
    return battle_draw_resource(file,world,6,x,y,2,pixels);
}

int npe_update_battle_enemy_target(const NpeFile *file, const NpeWorld *world,
                                    unsigned previous,
                                    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    int left, top;
    if (!pixels || battle_enemy_target_position(world, previous, &left, &top)) return -1;
    if (previous == world->battle_target) return 0;
    /* Native inclusive rectangle clear also erases the enemy's first
       scanline below the old pointer; preserve that partial redraw. */
    if (npe_clear_battle_enemy_target(world,previous,pixels)) return -1;
    if (battle_enemy_target_position(world, world->battle_target, &left, &top)) return -1;
    if(battle_draw_resource(file,world,6,left,top,2,pixels))return -1;
    return 0;
}

int npe_render_battle_enemy_target(const NpeFile *file, const NpeWorld *world,
                                    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    int left, top;
    if (!world || !pixels) return -1;
    if (battle_enemy_target_position(world, world->battle_target, &left, &top) ||
        npe_render_battle_world_scene(file, world, pixels)) return -1;
    if(battle_draw_resource(file,world,6,left,top,2,pixels))return -1;
    return 0;
}

int npe_render_battle_icon_selection(unsigned icon,
                                    uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    unsigned x, y;
    unsigned left, top;
    if (!pixels || icon >= 4) return -1;
    left = 5 + (icon & 1u) * 16;
    top = 90 + (icon >> 1) * 16;
    for (y = top; y < top + 14; ++y)
        for (x = left; x < left + 14; ++x)
            pixels[y * NPE_WIDTH + x] = 15 - pixels[y * NPE_WIDTH + x];
    return 0;
}

int npe_render_battle_command_menu(const NpeFile *file, unsigned menu,
                                   unsigned selection,
                                   uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    NpeIndexedBitmap bitmap;
    unsigned x, y, left, top, count;
    if (!file || !pixels || (menu != 1 && menu != 2)) return -1;
    count = menu == 1 ? 5 : 2;
    if (selection >= count)return -1;
    left = menu == 1 ? 38 : 86;
    top = menu == 1 ? 10 : 26;
    if(file->rpg_runtime){
        if(battle_draw_resource(file,NULL,(uint16_t)(menu+1),(int)left,(int)top,0,pixels))return -1;
    }else{
    if(npe_battle_bitmap(file,menu,&bitmap)||bitmap.width!=48||bitmap.height!=(menu==1?93:47))return -1;
    for (y = 0; y < bitmap.height; ++y)
        for (x = 0; x < bitmap.width; ++x)
            pixels[(top + y) * NPE_WIDTH + left + x] =
                npe_indexed_pixel(&bitmap, x, y);
    }
    for (y = (menu == 1 ? 11u : 10u) + selection * 15;
         y < (menu == 1 ? 26u : 25u) + selection * 15; ++y)
        for (x = menu == 1 ? 8u : 9u;
             x < (menu == 1 ? 42u : 43u); ++x)
            pixels[(top + y) * NPE_WIDTH + left + x] =
                15 - pixels[(top + y) * NPE_WIDTH + left + x];
    return 0;
}

int npe_render_battle_scene(const NpeFile *file, unsigned enemy_id,
                            uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    uint8_t ids[3] = {(uint8_t)enemy_id, 0, 0};
    if (!enemy_id || enemy_id > UINT8_MAX) return -1;
    return npe_render_battle_formation_scene(file, ids, 1, pixels);
}

int npe_render_battle_reward_panel(const NpeFile *file,uint16_t gold,uint16_t experience,
                                   uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    uint16_t panel=npe_image_group_index(file,6,30);
    uint16_t totals[2]={gold,experience};
    if(!file||!pixels)return -1;
    /* Source44:6B28..6BCA sums the physical enemy records in words;
       6C1A..6CCA draws the current NPE UI base+30 at (60,14), then
       formats signed decimal gold and XP at (105,24)/(105,39). */
    if(npe_render_indexed_resource(file,panel,60,14,0,pixels))return -1;
    for(unsigned line=0;line<2;++line){
        char number[8];snprintf(number,sizeof(number),"%d",(int)(int16_t)totals[line]);
        for(unsigned i=0;number[i];++i){
            const uint8_t *glyph=npe_native_counter_glyph(number[i]);
            if(!glyph)return -1;
            /* Native reward text clears eleven rows; the panel's
               separator immediately below each number remains intact. */
            for(unsigned y=0;y<11;++y)for(unsigned x=0;x<6;++x)
                pixels[(24+15*line+y)*NPE_WIDTH+105+6*i+x]=
                    y<10&&x<5&&(glyph[y]&(0x10u>>x))?15:0;
        }
    }
    return 0;
}

int npe_render_battle_rewards(const NpeFile *file,const NpeWorld *world,
                              uint8_t pixels[NPE_WIDTH*NPE_HEIGHT]){
    uint16_t totals[2]={0,0};
    if(!world||!world->battle_enemy_count||world->battle_enemy_count>3)return -1;
    for(unsigned enemy=0;enemy<world->battle_enemy_count;++enemy){
        totals[0]=(uint16_t)(totals[0]+world->battle_enemy_gold_rewards[enemy]);
        totals[1]=(uint16_t)(totals[1]+world->battle_enemy_experience_rewards[enemy]);
    }
    return npe_render_battle_reward_panel(file,totals[0],totals[1],pixels);
}

void npe_render_system_menu_overlay(const NpeFile *file,
                                const NpeWorld *world,
                                unsigned selection,
                                uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    unsigned x, y;
    if (!world || !pixels || world->game_mode != 6 || selection >= 4)
        return;
    if(!file)return;
    if(npe_render_indexed_resource(file,npe_image_group_index(file,7,2),0,37,0,pixels) ||
       npe_render_indexed_resource(file,npe_image_group_index(file,7,1),0,7,0,pixels))return;
    /* main.c uses the original compact12 money renderer after this
       resource overlay; no synthetic font is prepainted here. */
    /* Source49:0170..01FE uses shared inclusive row boundaries,
       not a constant pitch between the four highlight rectangles. */
    static const unsigned first_y[4]={45,62,78,93};
    static const unsigned last_y[4]={62,78,93,110};
    for (y = first_y[selection]; y <= last_y[selection]; ++y)
        for (x = 8; x < 43; ++x)
            pixels[y * NPE_WIDTH + x] =
                15 - pixels[y * NPE_WIDTH + x];
}

void npe_render_system_submenu_overlay(const NpeFile *file,
                                   const NpeWorld *world,
                                   unsigned selection,
                                   uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    unsigned x, y;
    if (!world || !pixels || world->game_mode != 6 || selection >= 4)
        return;
    if(!file)return;
    /* Native system submenu overlaps the parent at (42,37).
       Source36:4B62..4C78 uses inclusive, nonuniform rectangles. */
    if(npe_render_indexed_resource(file,npe_image_group_index(file,7,5),42,37,0,pixels))return;
    static const unsigned first_y[4]={45,63,78,93};
    static const unsigned last_y[4]={63,78,93,110};
    for (y = first_y[selection]; y <= last_y[selection]; ++y)
        for (x = 50; x <= 105; ++x)
            pixels[y * NPE_WIDTH + x] =
                15 - pixels[y * NPE_WIDTH + x];
}

int npe_render_music_menu_overlay(const NpeFile *file, unsigned selection,
                                   uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    unsigned top, bottom;
    /* Source4F:1FFA..207C draws the current game's UI resource at
       (105,70), then inverts inclusive (113,78)..(155,95) or
       (113,95)..(155,111). The two ranges share row 95. */
    if (!file || !pixels || selection > 1) return -1;
    if(npe_render_indexed_resource(file,npe_image_group_index(file,7,6),105,70,0,pixels))return -1;
    top = selection ? 95 : 78;
    bottom = selection ? 111 : 95;
    for (unsigned y = top; y <= bottom; ++y)
        for (unsigned x = 113; x <= 155; ++x)
            pixels[y*NPE_WIDTH+x] = 15-pixels[y*NPE_WIDTH+x];
    return 0;
}

void npe_render_property_submenu_overlay(const NpeFile *file,
                                         const NpeWorld *world,
                                         unsigned selection,
                                         uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    unsigned x, y;
    if (!file || !world || !pixels || world->game_mode != 6 ||
        selection >= 2) return;
    if(npe_render_indexed_resource(file,npe_image_group_index(file,7,3),42,37,0,pixels))return;
    /* Original LCD status/wear selections invert local x=8..40,
       y=8..25 or y=25..41, respectively. */
    for (y = selection ? 25 : 8;
         y <= (selection ? 41u : 25u); ++y)
        for (x = 8; x <= 40; ++x)
            pixels[(37 + y) * NPE_WIDTH + 42 + x] =
                15 - pixels[(37 + y) * NPE_WIDTH + 42 + x];
}

void npe_render_item_submenu_overlay(const NpeFile *file,
                                     const NpeWorld *world,
                                     unsigned selection,
                                     uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    unsigned x, y;
    if (!file || !world || !pixels || world->game_mode != 6 ||
        selection >= 2) return;
    if(npe_render_indexed_resource(file,npe_image_group_index(file,7,4),42,70,0,pixels))return;
    for (y = selection ? 25 : 8;
         y <= (selection ? 41u : 25u); ++y)
        for (x = 8; x <= 41; ++x)
            pixels[(70 + y) * NPE_WIDTH + 42 + x] =
                15 - pixels[(70 + y) * NPE_WIDTH + 42 + x];
}

const uint8_t *npe_native_counter_glyph(char ch) {
    /* 5x10 counter cells from the original battle LCD captures. */
    static const uint8_t digits[10][10] = {
        {0,14,17,17,17,17,17,17,17,14},
        {0,4,12,4,4,4,4,4,4,14},
        {0,14,17,17,1,2,4,8,17,31},
        {0,14,17,1,1,6,1,1,17,14},
        {0,2,6,6,10,10,18,31,2,2},
        {0,31,16,16,30,1,1,1,17,14},
        {0,7,8,16,22,25,17,17,17,14},
        {0,31,17,1,2,2,2,4,4,4},
        {0,14,17,17,17,14,17,17,17,14},
        {0,14,17,17,17,17,15,1,2,28}
    };
    static const uint8_t slash[10] = {2,2,4,4,4,8,8,8,16,16};
    static const uint8_t colon[10] = {0,0,0,0,0,6,6,0,6,6};
    static const uint8_t minus[10] = {0,0,0,0,0,30,0,0,0,0};
    static const uint8_t plus[10] = {0,0,4,4,4,31,4,4,4,0};
    if (ch >= '0' && ch <= '9') return digits[ch - '0'];
    if (ch == '/') return slash;
    if (ch == ':') return colon;
    if (ch == '-') return minus;
    if (ch == '+') return plus;
    return NULL;
}

int npe_render_save_slots_overlay(const NpeFile *file, unsigned left,
                                  unsigned top, const unsigned levels[3],
                                  unsigned selection, const uint8_t *title_font16,
                                  uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    static const unsigned number_tops[3] = {15, 31, 49};
    static const unsigned title_number_tops[3] = {12, 28, 46};
    unsigned x, y, row;
    if (!file || !levels || !pixels || selection >= 3 ||
        left > NPE_WIDTH-72 || top > NPE_HEIGHT-74) return -1;
    if(npe_render_indexed_resource(file,npe_image_group_index(file,7,12),(int16_t)left,(int16_t)top,0,pixels))return -1;
    for (row = 0; row < 3; ++row) {
        char number[16];
        unsigned pos;
        snprintf(number, sizeof(number), "%u", levels[row]);
        for (pos = 0; number[pos]; ++pos) {
            if (title_font16) {
                const uint8_t *bits;
                if (memcmp(title_font16, "NPEF16V3", 8) ||
                    number[pos] < '0' || number[pos] > '9' ||
                    30 + pos * 8 + 8 > 64) return -1;
                bits = title_font16 + 8 +
                       (size_t)(number[pos] - 0x20) * 32;
                for (y = 0; y < 16; ++y) {
                    unsigned mask = (unsigned)bits[y * 2] << 8 |
                                    bits[y * 2 + 1];
                    for (x = 0; x < 8; ++x)
                        if (mask & (0x8000u >> x))
                            pixels[(top + title_number_tops[row] + y) *
                                       NPE_WIDTH + left + 30 + pos * 8 + x] = 15;
                }
            } else {
                const uint8_t *bits = npe_native_counter_glyph(number[pos]);
                if (!bits || 30 + pos * 6 + 5 > 64) return -1;
                for (y = 0; y < 10; ++y)
                    for (x = 0; x < 5; ++x)
                        if (bits[y] & (1u << (4 - x)))
                            pixels[(top + number_tops[row] + y) * NPE_WIDTH +
                                   left + 30 + pos * 6 + x] = 15;
            }
        }
    }
    for (y = 11 + selection * 17; y < 29 + selection * 17; ++y)
        for (x = 8; x < 64; ++x)
            pixels[(top + y) * NPE_WIDTH + left + x] =
                15 - pixels[(top + y) * NPE_WIDTH + left + x];
    if (title_font16)
        for (x = 30; x < 38; ++x)
            pixels[(top + 28) * NPE_WIDTH + left + x] = 15;
    return 0;
}

static void draw_native_battle_counter(const char *line, unsigned top,
                                       uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    unsigned i, x, y;
    for (i = 0; line[i] && 82 + i * 6 + 6 <= NPE_WIDTH; ++i) {
        const uint8_t *glyph = npe_native_counter_glyph(line[i]);
        if (!glyph) continue;
        /* Native 6x12 text cells have an opaque white background, including
           the trailing column and bottom two rows. Long values erase sprites. */
        for (y = 0; y < 12 && top + y < NPE_HEIGHT; ++y)
            for (x = 0; x < 6; ++x)
                pixels[(top + y) * NPE_WIDTH + 82 + i * 6 + x] = 0;
        for (y = 0; y < 10; ++y)
            for (x = 0; x < 5; ++x)
                if (glyph[y] & (1u << (4 - x)))
                    pixels[(top + y) * NPE_WIDTH + 82 + i * 6 + x] = 15;
    }
}

void npe_render_battle_hud_values(unsigned hp, unsigned max_hp, unsigned qi,
                                  unsigned max_qi,
                                  uint8_t pixels[NPE_WIDTH * NPE_HEIGHT]) {
    char line[48];
    if (!pixels) return;
    snprintf(line, sizeof(line), ":%d/%d", (int16_t)hp, (int16_t)max_hp);
    draw_native_battle_counter(line, 88, pixels);
    snprintf(line, sizeof(line), ":%d/%d", (int16_t)qi, (int16_t)max_qi);
    draw_native_battle_counter(line, 104, pixels);
}

#include "world_view.inc"
