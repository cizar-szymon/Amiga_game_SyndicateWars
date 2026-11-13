/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file thing.c
 *     Thing structure support.
 * @par Purpose:
 *     Implement general functions for handling Thing instances.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     19 Apr 2022 - 27 May 2022
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "thing.h"

#include "bfbox.h"
#include "bfutility.h"
#include "bfmath.h"
#include "bfmemut.h"
#include "bfscreen.h"
#include "ssampply.h"
#include <assert.h>
#include <stdlib.h>

#include "bigmap.h"
#include "engincam.h"
#include "building.h"
#include "bmbang.h"
#include "command.h"
#include "display.h"
#include "engindrwlstx.h"
#include "enginlights.h"
#include "enginsngobjs.h"
#include "enginsngtxtr.h"
#include "frame_sprani.h"
#include "game.h"
#include "game_data.h"
#include "game_options.h"
#include "game_speed.h"
#include "matrix.h"
#include "packet.h"
#include "pepgroup.h"
#include "player.h"
#include "sound.h"
#include "thing_expld.h"
#include "thing_fire.h"
#include "thing_search.h"
#include "vehicle.h"
#include "game.h"
#include "swlog.h"
#include "asm_coverage.h"
#include "fncheck.h"
#include <string.h>
/******************************************************************************/
#pragma pack(1)

struct UnkFLight { // sizeof=0x0A
    short unfulgt_0;
    short unfulgt_2;
    short unfulgt_4;
    short unfulgt_6;
    ubyte unfulgt_8;
    ubyte unfulgt_9;
};

struct AnimPage { // sizeof=0x0A
	ubyte PageNo;
	ubyte X;
	ubyte Y;
	ubyte Width;
	ubyte Height;
	ubyte No;
	u32 apunknfld_6;
};

#pragma pack()
/******************************************************************************/

struct UnkFLight unkn_full_lights[50];
ushort next_unkn_full_light = 1;

u32 things_init_times = 0;

ubyte debug_log_things = 0;

/** Radiuses of Things of type STATIC.
 */
short static_radii[] = {
  40, 50, 80, 15,  0, 40,  0,  0,  0, 40,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0, 70, 70, 70, 70,
 130, 40, 80, 40, 40, 40, 50,100, 40, 40,
   0,  0,  0,  0,  0,  0,  0,  0, 40, 70,
  40, 40, 80,100,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

struct AnimPage door_anim_page[] = {
  {2,    0, 0x20, 0x20, 0x20, 5, 0},
  {2, 0x60, 0x60, 0x10, 0x20, 5, 0},
  {2, 0x60, 0x60, 0x10, 0x20, 5, 0},
};

const char *thing_type_names[] = {
  "NONE",
  "UNKN1",
  "VEHICLE",
  "PERSON",
  "UNKN4",
  "STATIC",
  "ROCKET",
  "UNKN7",
  "UNKN8",
  "BUILDING",
  "UNKN10",
  "LASER11",
  "LASER_GUIDED",
  "UNKN13",
  "UNKN14",
  "MINE",
  "GRENADE",
  "SPARK",
  "UNKN18",
  "LASER_ELEC",
  "INTELLIG_DOOR",
  "SCALE_EFFECT",
  "NUCLEAR_BOMB",
  "UNKN23",
  "SMOKE_GENERTR",
  "DROPPED_ITEM",
  "CARRIED_ITEM",
  "ELECTR_STRAND",
  "RAZOR_WIRE",
  "LASER29",
  "TIME_POD",
  "AIR_STRIKE",
  "CANISTER",
  "VEH_TURRET",
  "UNKN34",
  "UNKN35",
  "STASIS_POD",
  "SOUL",
  "LASER38",
  "UNKN39",
  "UNKN40",
  "UNKN41",
  "UNKN42",
  "UNKN43",
  "UNKN44",
  "UNKN45",
  "UNKN46",
  "UNKN47",
  "UNKN48",
  "UNKN49",
  "BANG",
  "UNKN51",
  "FIRE",
  "SFX",
  "TEMP_LIGHT",
  "UNKN55",
  "UNKN56",
};

const char *state_change_result_names[] = {
    "accepted",
    "already done",
    "change denied",
    "goal unattainable",
};

struct Thing *things = NULL;
ushort things_used;
ThingIdx things_used_head;
ThingIdx things_empty_head;

struct SimpleThing *sthings = NULL;
/* In the order of the original memory layout: remove_sthing() of the original
 * reads `sthings_empty_head` as the upper half of a 32-bit `sthings_used`. */
ushort sthings_used;
ThingIdx sthings_empty_head;
ThingIdx sthings_used_head;

ThingIdx same_type_head[256 + PEOPLE_GROUPS_LIMIT + 1] = {0};

/******************************************************************************/

TbBool thing_type_is_simple(short ttype)
{
    return (ttype == SmTT_STATIC) ||
     (ttype == SmTT_SPARK) ||
     (ttype == SmTT_INTELLIG_DOOR) ||
     (ttype == SmTT_SCALE_EFFECT) ||
     (ttype == SmTT_NUCLEAR_BOMB) ||
     (ttype == SmTT_SMOKE_GENERATOR) ||
     (ttype == SmTT_DROPPED_ITEM) ||
     (ttype == SmTT_CARRIED_ITEM) ||
     (ttype == SmTT_ELECTRIC_STRAND) ||
     (ttype == SmTT_TIME_POD) ||
     (ttype == SmTT_CANISTER) ||
     (ttype == SmTT_STASIS_POD) ||
     (ttype == SmTT_SOUL) ||
     (ttype == SmTT_BANG) ||
     (ttype == SmTT_FIRE) ||
     (ttype == SmTT_SFX) ||
     (ttype == SmTT_TEMP_LIGHT);
}

TbBool thing_type_is_pickup_item(short ttype)
{
    return (ttype == SmTT_DROPPED_ITEM) ||
      (ttype == SmTT_CARRIED_ITEM);
}

struct Thing *get_thing_safe(ThingIdx thing, ubyte ttype)
{
    struct Thing *p_thing;

    if (thing_type_is_simple(ttype))
    {
        if ((thing > -1) || (thing < STHINGS_LIMIT))
            return INVALID_THING;
    }
    else
    {
        if ((thing < 1) || (thing > THINGS_LIMIT))
            return INVALID_THING;
    }
    p_thing = &things[thing];
    if (p_thing->Type != ttype)
            return INVALID_THING;
    return p_thing;
}

TbBool thing_position_uses_y_mul_8(struct Thing *p_thing)
{
    switch (p_thing->Type)
    {
    case TT_BUILDING:
        return (p_thing->SubType == SubTT_BLD_MGUN);
    case TT_VEHICLE:
    case TT_PERSON:
    default:
        return true;
    }
}

void get_thing_position_mapcoords(short *x, short *y, short *z, ThingIdx thing)
{
    short cor_x, cor_y, cor_z;

    if (thing >= 0) {
        struct Thing *p_thing;
        p_thing = &things[thing];
        cor_x = PRCCOORD_TO_MAPCOORD(p_thing->X);
        if (thing_position_uses_y_mul_8(p_thing))
            cor_y = PRCCOORD_TO_YCOORD(p_thing->Y);
        else
            cor_y = PRCCOORD_TO_MAPCOORD(p_thing->Y);
        cor_z = PRCCOORD_TO_MAPCOORD(p_thing->Z);
    } else {
        struct SimpleThing *p_sthing;
        p_sthing = &sthings[thing];
        cor_x = PRCCOORD_TO_MAPCOORD(p_sthing->X);
        cor_y = PRCCOORD_TO_YCOORD(p_sthing->Y);
        cor_z = PRCCOORD_TO_MAPCOORD(p_sthing->Z);
    }
    if (x != NULL)
        *x = cor_x;
    if (y != NULL)
        *y = cor_y;
    if (z != NULL)
        *z = cor_z;
}

u32 get_things_distance_mapcoords_precise(ThingIdx tng1, ThingIdx tng2)
{
    short x1, y1, z1;
    short x2, y2, z2;
    get_thing_position_mapcoords(&x1, &y1, &z1, tng1);
    get_thing_position_mapcoords(&x2, &y2, &z2, tng2);
    return map_distance_deltas_precise(x2 - x1, y2 - y1, z2 - z1);
}

u32 get_things_distance_mapcoords_fast(ThingIdx tng1, ThingIdx tng2)
{
    short x1, y1, z1;
    short x2, y2, z2;
    get_thing_position_mapcoords(&x1, &y1, &z1, tng1);
    get_thing_position_mapcoords(&x2, &y2, &z2, tng2);
    return map_distance_deltas_fast(x2 - x1, y2 - y1, z2 - z1);
}

const char *state_change_result_name(StateChRes res)
{
    return state_change_result_names[res];
}

struct Thing *effective_owner_of_thing(struct Thing *p_thing)
{
    struct Thing *p_owntng;

    if (p_thing == NULL) {
        return NULL;
    }

    // Consider player agent an owner even if it got persuaded
    if ((p_thing->Flag & TngF_PlayerAgent) != 0) {
        return p_thing;
    }

    p_owntng = &things[p_thing->Owner];

    if (((p_thing->Flag & TngF_Persuaded) != 0) && (p_owntng->Flag & TngF_PlayerAgent) != 0) {
        return p_owntng;
    }

    if (p_thing->Type == TT_MINE && p_thing->SubType == 48) {
        return p_owntng;
    }

    return p_thing;
}

ubyte on_mapwho(struct Thing *p_thing)
{
    ubyte ret;
    asm volatile (
      "call ASM_on_mapwho\n"
        : "=r" (ret) : "a" (p_thing));
    return ret;
}

#ifdef SW_FNCHK
static void move_mapwho_asm(struct Thing *p_thing, int x, int y, int z)
{
    asm volatile ("call ASM_move_mapwho\n"
        : : "a" (p_thing), "d" (x), "b" (y), "c" (z) : "cc", "memory");
}
#endif

/** Move a thing to new coordinates; if the map tile changes, move it to the
 * head of the `mapwho` list of the new tile. Coordinates outside the map are
 * ignored (the thing is not moved at all).
 */
static void move_mapwho_c(struct Thing *p_thing, int x, int y, int z)
{
    if (((x >> 8) >= 0x8000) || ((z >> 8) >= 0x8000) || (x < 0) || (z < 0))
        return;
    // tiles over 127 cannot pass the check above; kept from the original
    if (((x >> 16) >= MAP_TILE_WIDTH) || ((z >> 16) >= MAP_TILE_HEIGHT)) {
        x = (x & 0xFFFF) | (((x >> 16) & 0x7F) << 16);
        z = (z & 0xFFFF) | (((z >> 16) & 0x7F) << 16);
    }
    if (((p_thing->Flag2 & TgF2_ExistsOffMap) == 0) &&
      (((short)(x >> 16) != (short)(p_thing->X >> 16)) ||
       ((short)(z >> 16) != (short)(p_thing->Z >> 16))))
    {
        ushort tile;
        ThingIdx head;

        // a no-op hook of the original for people (nullsub_3) was called here
        tile = (ushort)(((z >> 16) << 7) + (x >> 16));
        delete_node(p_thing);
        head = game_my_big_map[tile].Child;
        p_thing->Next = head;
        if (head > 0)
            things[head].Parent = p_thing->ThingOffset;
        else if (head < 0)
            sthings[head].Parent = p_thing->ThingOffset;
        p_thing->Parent = 0;
        game_my_big_map[tile].Child = p_thing->ThingOffset;
    }
    p_thing->Y = y;
    p_thing->X = x;
    p_thing->Z = z;
}

void move_mapwho(struct Thing *p_thing, int x, int y, int z)
{
    FNCHK_VOID("move_mapwho", move_mapwho_asm(p_thing, x, y, z), move_mapwho_c(p_thing, x, y, z));
}

#ifdef SW_FNCHK
static void init_just_things_asm(void)
{
    asm volatile ("call ASM_init_just_things\n"
        :  :  : "eax", "cc", "memory");
}
#endif

/** Clear all things and sthings and chain them into the empty lists.
 */
static void init_just_things_c(void)
{
    short i;

    // sthings use negative indices; the empty list goes -1, -2, ...
    for (i = 1; i < STHINGS_LIMIT; i++)
    {
        struct SimpleThing *p_sthing;

        p_sthing = &sthings[-i];
        memset(p_sthing, 0, sizeof(struct SimpleThing));
        p_sthing->LinkParent = -(i - 1);
        p_sthing->ThingOffset = -i;
        p_sthing->LinkChild = -(i + 1);
    }
    sthings_empty_head = -1;
    sthings_used_head = 0;

    for (i = 1; i < THINGS_LIMIT; i++)
    {
        struct Thing *p_thing;

        p_thing = &things[i];
        memset(p_thing, 0, sizeof(struct Thing));
        p_thing->LinkParent = i - 1;
        p_thing->ThingOffset = i;
        p_thing->LinkChild = i + 1;
    }
    things_empty_head = 1;
    things_used_head = 0;
    things_used = 0;
    sthings_used = 0;
    // The original also marks the entry just past the array.
    things[THINGS_LIMIT].ThingOffset = THINGS_LIMIT;
}

void init_just_things(void)
{
    FNCHK_VOID("init_just_things", init_just_things_asm(), init_just_things_c());
}

void init_things(void)
{
#if 0
    asm volatile ("call ASM_init_things\n"
        :  :  : "eax" );
#endif
    things_init_times++;
    gameturn = 0;

    init_things_memory_with_user_heap();
    init_just_things();
    init_commands();
}

void quick_light_unkn_func_04(short a1, int a2, short a3, short a4)
{
    asm volatile (
      "call ASM_quick_light_unkn_func_04\n"
        : : "a" (a1), "d" (a2), "b" (a3), "c" (a4));
    return;
}

void unkn_full_update_lights(void)
{
    ushort i;

    for (i = 1; i < next_unkn_full_light; i++)
    {
        struct UnkFLight *p_ufl;
        p_ufl = &unkn_full_lights[i];
        quick_light_unkn_func_04(p_ufl->unfulgt_2, p_ufl->unfulgt_4, p_ufl->unfulgt_6, p_ufl->unfulgt_0);
    }
    next_unkn_full_light = 1;
}

void apply_full_light(short lx, short lz, ushort b, ushort intens, ushort lid)
{
    asm volatile (
      "push %4\n"
      "call ASM_apply_full_light\n"
        : : "a" (lx), "d" (lz), "b" (b), "c" (intens), "g" ((u32)lid));
    return;
}

int process_things_mines_explode(int affected_max)
{
    int ret;
    asm volatile ("call ASM_process_things_mines_explode\n"
        : "=r" (ret) : "a" (affected_max));
    return ret;
}

void process_things_unkn_sub2(int tech_lv, int got_many)
{
    asm volatile (
      "call ASM_process_things_unkn_sub2\n"
        : : "a" (tech_lv), "d" (got_many));
}

int process_things_bang(int affected_max)
{
    int ret;
    asm volatile ("call ASM_process_things_bang\n"
        : "=r" (ret) : "a" (affected_max));
    return ret;
}

#ifdef SW_FNCHK
static void unkn_update_lights_asm(void)
{
    asm volatile ("call ASM_unkn_update_lights\n"
        : : : "eax", "cc", "memory");
}
#endif

/* Light commands are kept as the bytes of the level file; their 16-bit
 * fields are little endian (see lvendian.c). */
static inline ushort lcmd_get16(const ubyte *p, int offs)
{
    return (ushort)(p[offs] | (p[offs + 1] << 8));
}

static inline void lcmd_set16(ubyte *p, int offs, ushort val)
{
    p[offs] = (ubyte)val;
    p[offs + 1] = (ubyte)(val >> 8);
}

/** Set the intensity of a light, or of the chain of lights of a scenery
 * sthing (negative index), to `ratio`/256 of its true intensity.
 * Remade from light_cmd_unkn_func_05.
 */
static void light_cmd_set_intensity(short lgt, ushort ratio)
{
    struct FullLight *p_flight;

    if (lgt < 0)
    {
        short flight;

        flight = sthings[lgt].U.UScenery.LightHead;
        while (flight != 0)
        {
            if ((flight > (int)next_full_light) || (flight < 0))
                break;
            p_flight = &game_full_lights[flight];
            p_flight->Intensity = (short)(((s32)ratio * p_flight->TrueIntensity) >> 8);
            flight = p_flight->NextFull;
        }
    }
    else
    {
        p_flight = &game_full_lights[lgt];
        p_flight->Intensity = (short)(((s32)ratio * p_flight->TrueIntensity) >> 8);
    }
}

/** Area light command: switched on (flag 0x20) while a person stands within
 * the map rectangle given by the command, re-checked every few turns.
 * Remade from light_cmd_unkn_func_06.
 */
static void light_cmd_check_people_in_area(ubyte *p_lcmd)
{
    int col, row;
    TbBool found;

    // the original tested `flags | 0x20`, always true
    p_lcmd[0x14]--;
    if ((sbyte)p_lcmd[0x14] <= 0)
        p_lcmd[0] &= ~0x20;
    if ((sbyte)p_lcmd[0x14] > 0)
        return;

    found = false;
    for (col = p_lcmd[0xc]; col < p_lcmd[0xe]; col++)
    {
        for (row = p_lcmd[0xd]; row < p_lcmd[0xf]; row++)
        {
            short thing;

            // out of the map: the original left without updating the command
            if (col >= MAP_TILE_WIDTH)
                return;
            if (row >= MAP_TILE_HEIGHT)
                return;
            thing = game_my_big_map[row * MAP_TILE_WIDTH + col].Child;
            if ((thing > 0) && (things[thing].Type == TT_PERSON)) {
                found = true;
                break;
            }
        }
        if (found)
            break;
    }
    p_lcmd[0x9] = 0;
    if (found) {
        p_lcmd[0x14] = 20;
        p_lcmd[0] &= ~0x20;
    } else {
        p_lcmd[0x14] = 4;
        p_lcmd[0] |= 0x20;
    }
}

/** Random flicker (command type 1): on and off for a random number of turns
 * between the two limits.
 */
static void light_cmd_flicker(ubyte *p_lcmd, short lgt)
{
    if ((p_lcmd[0] & 0x04) != 0)
    {
        p_lcmd[2]--;
        if ((sbyte)p_lcmd[2] >= 0)
            return;
        if (p_lcmd[5] == p_lcmd[6]) {
            p_lcmd[8] = p_lcmd[6];
        } else {
            int range = (int)p_lcmd[5] - (int)p_lcmd[6];
            p_lcmd[8] = (ubyte)(p_lcmd[6] + (int)LbRandomAnyShort() % range);
        }
        p_lcmd[2] = p_lcmd[8];
        light_cmd_set_intensity(lgt, p_lcmd[3]);
        p_lcmd[0] = (p_lcmd[0] & 0xf9) | 0x02;
    }
    else if ((p_lcmd[0] & 0x02) != 0)
    {
        p_lcmd[2]--;
        if ((sbyte)p_lcmd[2] >= 0)
            return;
        if (p_lcmd[5] == p_lcmd[6]) {
            p_lcmd[7] = p_lcmd[6];
        } else {
            int range = (int)p_lcmd[5] - (int)p_lcmd[6];
            p_lcmd[7] = (ubyte)(p_lcmd[6] + (int)LbRandomAnyShort() % range);
        }
        // the original restarts the counter from 0 here, not from the
        // random value it has just drawn
        p_lcmd[2] = 0;
        light_cmd_set_intensity(lgt, p_lcmd[4]);
        p_lcmd[0] = (p_lcmd[0] & 0xf9) | 0x04;
    }
}

/** Pulse (command type 2): the ratio at 0xA goes up and down between the
 * limits at 3 and 4, by the steps at 6 and 5.
 */
static void light_cmd_pulse(ubyte *p_lcmd, short lgt)
{
    ushort val;

    if ((p_lcmd[0] & 0x08) != 0)
    {
        val = lcmd_get16(p_lcmd, 0xa) + p_lcmd[6];
        lcmd_set16(p_lcmd, 0xa, val);
        if ((short)val >= (int)p_lcmd[3]) {
            lcmd_set16(p_lcmd, 0xa, p_lcmd[3]);
            p_lcmd[0] &= ~0x08;
        }
    }
    else
    {
        val = lcmd_get16(p_lcmd, 0xa) - p_lcmd[5];
        lcmd_set16(p_lcmd, 0xa, val);
        if ((short)val <= (int)p_lcmd[4]) {
            lcmd_set16(p_lcmd, 0xa, p_lcmd[4]);
            p_lcmd[0] |= 0x08;
        }
    }
    light_cmd_set_intensity(lgt, lcmd_get16(p_lcmd, 0xa));
}

/** On/off with a counter (command type 6): on for the turns at 6, off for
 * the turns at 5.
 */
static void light_cmd_blink(ubyte *p_lcmd, short lgt)
{
    if ((p_lcmd[0] & 0x04) != 0)
    {
        p_lcmd[2]--;
        if ((sbyte)p_lcmd[2] >= 0)
            return;
        p_lcmd[8] = p_lcmd[6];
        p_lcmd[2] = p_lcmd[6];
        if ((sbyte)p_lcmd[6] > 0)
            light_cmd_set_intensity(lgt, p_lcmd[3]);
        p_lcmd[0] = (p_lcmd[0] & 0xf9) | 0x02;
    }
    else if ((p_lcmd[0] & 0x02) != 0)
    {
        p_lcmd[2]--;
        if ((sbyte)p_lcmd[2] >= 0)
            return;
        p_lcmd[7] = p_lcmd[5];
        p_lcmd[2] = p_lcmd[5];
        if ((sbyte)p_lcmd[5] > 0)
            light_cmd_set_intensity(lgt, p_lcmd[4]);
        p_lcmd[0] = (p_lcmd[0] & 0xf9) | 0x04;
    }
}

/** Follows an animated texture (command type 7): the light switches with the
 * frames of the animation of the floor tile under it. The animation is found
 * on the first call; without one the command becomes type 8 (nothing).
 */
static void light_cmd_anim_tmap(ubyte *p_lcmd, short lgt)
{
    short anim;

    if ((short)lcmd_get16(p_lcmd, 0x10) >= (int)next_anim_tmap)
        lcmd_set16(p_lcmd, 0x10, 0);
    anim = (short)lcmd_get16(p_lcmd, 0x10);
    if (anim != 0)
    {
        int frame;

        frame = game_anim_tmaps[anim].field_22 + 1;
        if ((p_lcmd[0] & 0x04) != 0)
        {
            if ((frame == p_lcmd[6]) || (frame == p_lcmd[5])) {
                light_cmd_set_intensity(lgt, p_lcmd[3]);
                p_lcmd[0] = (p_lcmd[0] & 0xf9) | 0x02;
            }
        }
        else if ((p_lcmd[0] & 0x02) != 0)
        {
            if ((frame != p_lcmd[6]) && (frame != p_lcmd[5])) {
                light_cmd_set_intensity(lgt, p_lcmd[4]);
                p_lcmd[0] = (p_lcmd[0] & 0xf9) | 0x04;
            }
        }
        return;
    }
    {
        short tile;
        ushort tex;

        // map tile of the light (16-bit arithmetic, as the original)
        if (lgt < 0) {
            struct SimpleThing *p_sthing = &sthings[lgt];
            tile = (short)(((ushort)(p_sthing->Z >> 16) << 7) + (ushort)(p_sthing->X >> 16));
        } else {
            struct FullLight *p_flight = &game_full_lights[lgt];
            tile = (short)(((p_flight->Z >> 8) << 7) + (p_flight->X >> 8));
        }
        tex = game_my_big_map[tile].Texture & 0x3FFF;
        anim = 0;
        if (tex != 0)
        {
            for (anim = 10; anim < (int)next_anim_tmap; anim++)
            {
                if (game_anim_tmaps[anim].Texture == tex)
                    break;
            }
            if (anim >= (int)next_anim_tmap)
                anim = 0;
        }
        lcmd_set16(p_lcmd, 0x10, anim);
        if (anim != 0)
            return;
        p_lcmd[1] = 8;
        if (lgt < 0)
            sthings[lgt].U.UScenery.LightHead = 0;
        else
            game_full_lights[lgt].Command = 0;
    }
}

/** Run all light commands for this turn (flicker, pulse, blink, lights
 * following animated textures, lights switched by people around).
 * Remade from ASM_unkn_update_lights.
 */
static void unkn_update_lights_c(void)
{
    ushort lcmd;

    for (lcmd = 1; lcmd < next_light_command; lcmd++)
    {
        ubyte *p_lcmd;
        short lgt;

        p_lcmd = game_light_commands[lcmd].lcfld_0;
        lgt = (short)lcmd_get16(p_lcmd, 0x12);
        // a light which belongs to another command is not driven by this one
        if ((lgt <= 0) || (game_full_lights[lgt].Command == (short)lcmd))
        {
            switch (p_lcmd[1])
            {
            case 1:
                light_cmd_flicker(p_lcmd, lgt);
                break;
            case 2:
                light_cmd_pulse(p_lcmd, lgt);
                break;
            case 3:
            case 4:
            case 5:
            case 8:
                break;
            case 6:
                light_cmd_blink(p_lcmd, lgt);
                break;
            case 7:
                light_cmd_anim_tmap(p_lcmd, lgt);
                break;
            default:
                light_cmd_set_intensity(lgt, 0x100);
                break;
            }
        }
        if ((p_lcmd[0] & 0x10) != 0)
            light_cmd_check_people_in_area(p_lcmd);
        if ((p_lcmd[0] & 0x20) != 0)
            light_cmd_set_intensity(lgt, 0);
    }
}

void unkn_update_lights(void)
{
    FNCHK_VOID("unkn_update_lights", unkn_update_lights_asm(), unkn_update_lights_c());
}

int recalc_rocket_direction(struct Thing *p_rocket)
{
#if 0
    int ret;
    asm volatile ("call ASM_recalc_rocket_direction\n"
        : "=r" (ret) : "a" (p_rocket));
    return ret;
#endif
    struct Thing *p_target;
    int cor_dx, cor_dy, cor_dz;
    int new_vx, new_vy, new_vz;
    int dist;

    p_target = p_rocket->PTarget;
    cor_dx = (p_target->X - p_rocket->X) >> 8;
    cor_dz = (p_target->Z - p_rocket->Z) >> 8;
    cor_dy = (p_target->Y - p_rocket->Y) >> 8;
    dist = LbSqrL(cor_dx * cor_dx + cor_dz * cor_dz);
    if (dist == 0)
        dist = 1;
    new_vx = (cor_dx << 8) / dist;
    new_vy = (cor_dy << 8) / dist;
    new_vz = (cor_dz << 8) / dist;
    if (new_vx * p_rocket->VX + new_vy * p_rocket->VY + new_vz * p_rocket->VZ < 0)
        return 0;

    p_rocket->VX = new_vx;
    p_rocket->VY = new_vy;
    p_rocket->VZ = new_vz;
    return 1;
}

void process_rocket(struct Thing *p_rocket)
{
    //TODO modify to make the rocket disappear when falling into sludge
    asm volatile (
      "call ASM_process_rocket\n"
        : : "a" (p_rocket));
}

void process_laser_guided(struct Thing *p_laser)
{
    asm volatile (
      "call ASM_process_laser_guided\n"
        : : "a" (p_laser));
}

void process_c_iff(struct Thing *p_mine)
{
    asm volatile (
      "call ASM_process_c_iff\n"
        : : "a" (p_mine));
}

void process_mine(struct SimpleThing *p_mine)
{
    asm volatile (
      "call ASM_process_mine\n"
        : : "a" (p_mine));
}

struct SimpleThing *init_nuclear_bomb(MapCoord x, MapCoord y, MapCoord z)
{
#if 0
    struct SimpleThing *ret;
    asm volatile (
      "call ASM_init_nuclear_bomb\n"
        : "=r" (ret) : "a" (x), "d" (y), "b" (z));
    return ret;
#endif
    struct SimpleThing *p_sthing;
    ThingIdx thing;

    if (x < 0)
        return NULL;
    if (z < 0)
        return NULL;
    if (sthings_used > STHINGS_LIMIT - 5) {
        return NULL;
    }
    thing = get_new_sthing();
    if (thing == 0) {
        return NULL;
    }
    if (thing <= -STHINGS_LIMIT-1) {
        return NULL;
    }

    p_sthing = &sthings[thing];
    p_sthing->Type = SmTT_NUCLEAR_BOMB;
    p_sthing->Radius = 64;
    p_sthing->X = MAPCOORD_TO_PRCCOORD(x, 0);
    p_sthing->Z = MAPCOORD_TO_PRCCOORD(z, 0);
    p_sthing->Y = MAPCOORD_TO_PRCCOORD(y, 0);
    p_sthing->Timer1 = 0;
    p_sthing->Flag = (TngF_InVehicle|TngF_Persuaded|TngF_Unkn0004);
    add_node_sthing(thing);
    set_nuclear_shade_point(x, y, z);

    return p_sthing;
}

void process_grenade(struct Thing *p_grenade)
{
#if 0
    asm volatile (
      "call ASM_process_grenade\n"
        : : "a" (p_grenade));
#endif
    if (p_grenade->Timer1 == 0)
    {
        MapCoord cor_x, cor_y, cor_z;

        cor_x = PRCCOORD_TO_MAPCOORD(p_grenade->X);
        cor_z = PRCCOORD_TO_MAPCOORD(p_grenade->Z);
        cor_y = PRCCOORD_TO_MAPCOORD(alt_at_point(cor_x, cor_z));

        if (p_grenade->SubType == 3)
        {
            struct SimpleThing *p_bomb;
            p_bomb = init_nuclear_bomb(cor_x, cor_y, cor_z);
            if (p_bomb != NULL) {
                p_bomb->Owner2 = p_grenade->Owner;
                play_dist_ssample(p_bomb, 0x22u, 0x7Fu, 0x40u, 100, 0, 2);
                play_dist_ssample(p_bomb, 1u, 0x7Fu, 0x40u, 100, 0, 3);
            }
        }
        else if ((p_grenade->SubType == 4) || (p_grenade->SubType == 5))
        {
            struct SimpleThing *p_bomb;
            p_bomb = init_nuclear_bomb(cor_x, cor_y, cor_z);
            if (p_bomb != NULL) {
                p_bomb->Type = SmTT_CANISTER;
                p_bomb->SubType = p_grenade->SubType;
                p_bomb->Owner2 = p_grenade->Owner;
                play_dist_ssample(p_bomb, 0x22u, 0x7Fu, 0x40u, 100, 0, 2);
                if (p_bomb->SubType == 4)
                    play_dist_ssample(p_bomb, 0x20u, 0, 0x40u, 100, -1, 3);
                else
                    play_dist_ssample(p_bomb, 0x1Fu, 0, 0x40u, 100, -1, 3);
            }
        }

        remove_thing(p_grenade->ThingOffset);
        delete_node(p_grenade);
        return;
    }

    short qbit;
    int prc_x, prc_y, prc_z;

    prc_x = p_grenade->X + MAPCOORD_TO_PRCCOORD(p_grenade->VX,0);
    prc_y = p_grenade->Y + p_grenade->VY;
    prc_z = p_grenade->Z + MAPCOORD_TO_PRCCOORD(p_grenade->VZ,0);
    p_grenade->VY -= 500;
    qbit = check_col_collision(PRCCOORD_TO_MAPCOORD(prc_x), PRCCOORD_TO_MAPCOORD(prc_y), PRCCOORD_TO_MAPCOORD(prc_z));
    if (qbit == -1)
    {
        ushort txtr;
        txtr = floor_texture_at_point(PRCCOORD_TO_MAPCOORD(prc_x), PRCCOORD_TO_MAPCOORD(prc_z));
        if (sea_texture(txtr))
        {
            struct SimpleThing *p_effect;
            MapCoord cor_x, cor_y, cor_z;

            cor_x = PRCCOORD_TO_MAPCOORD(p_grenade->X);
            cor_z = PRCCOORD_TO_MAPCOORD(p_grenade->Z);
            cor_y = PRCCOORD_TO_MAPCOORD(alt_at_point(cor_x, cor_z));
            p_effect = create_scale_effect(cor_x, cor_y, cor_z, 1087, 8);
            if (p_effect != NULL) {
                p_effect->SubType = 60;
                p_effect->Object = 62 + (LbRandomAnyShort() & 0x7F);
                create_sound_effect(cor_x, 0, cor_z, 3, 0, 0);
            }
            remove_thing(p_grenade->ThingOffset);
            delete_node(p_grenade);
            return;
        }
        if (map_floor_is_sludge(PRCCOORD_TO_MAPCOORD(prc_x), PRCCOORD_TO_MAPCOORD(prc_z)))
        {
            struct SimpleThing *p_effect;
            MapCoord cor_x, cor_y, cor_z;

            // Create sludge bulge effect
            cor_x = PRCCOORD_TO_MAPCOORD(p_grenade->X);
            cor_z = PRCCOORD_TO_MAPCOORD(p_grenade->Z);
            cor_y = PRCCOORD_TO_MAPCOORD(alt_at_point(cor_x, cor_z));
            p_effect = create_scale_effect(cor_x, cor_y, cor_z, 1091, 8);
            if (p_effect != NULL) {
                p_effect->Object = 62 + (LbRandomAnyShort() & 0x7F);
                // Generic type which just plays animation until end
                p_effect->SubType = 58;
                create_sound_effect(cor_x, cor_y, cor_z, 25, 0, 0);
            }
            remove_thing(p_grenade->ThingOffset);
            delete_node(p_grenade);
            return;
        }
    }
    if (qbit != 0) {
        p_grenade->Timer1 = 0;
    }
    if ((PRCCOORD_TO_MAPCOORD(prc_x) < MAP_COORD_WIDTH) &&
      (PRCCOORD_TO_MAPCOORD(prc_z) < MAP_COORD_HEIGHT)) {
        move_mapwho(p_grenade, prc_x, prc_y, prc_z);
    }
}

void process_laser_elec(struct Thing *p_elec)
{
    asm volatile (
      "call ASM_process_laser_elec\n"
        : : "a" (p_elec));
}

void process_razor_wire(struct Thing *p_thing)
{
    ;
}

void process_air_strike(struct Thing *p_thing)
{
#if 0
    asm volatile (
      "call ASM_process_air_strike\n"
        : : "a" (p_thing));
#endif
    struct SimpleThing *p_sthing;

    p_thing->Timer1--;
    if (p_thing->Timer1 < 0)
    {
        remove_thing(p_thing->ThingOffset);
        return;
    }

    if (p_thing->Timer1 == WEP_AIRSTRIKE_BOMB_GTURNS * WEP_AIRSTRIKE_BOMBS_NUM + WEP_AIRSTRIKE_DELAY_GTURNS)
    {
        play_dist_speech(p_thing, 4, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
    }

    if ((p_thing->Timer1 < WEP_AIRSTRIKE_BOMB_GTURNS * WEP_AIRSTRIKE_BOMBS_NUM) &&
      (p_thing->Timer1 % WEP_AIRSTRIKE_BOMB_GTURNS) == 0)
    {
        MapCoord cor_x, cor_y, cor_z;

        cor_x = PRCCOORD_TO_MAPCOORD(p_thing->X);
        cor_z = PRCCOORD_TO_MAPCOORD(p_thing->Z);
        cor_x += (LbRandomAnyShort() % 3000) - 1500;
        cor_z += (LbRandomAnyShort() % 3000) - 1500;
        cor_y = PRCCOORD_TO_MAPCOORD(alt_at_point(cor_x, cor_z)) + 10;
        bang_new4(MAPCOORD_TO_PRCCOORD(cor_x,0), MAPCOORD_TO_PRCCOORD(cor_y,0),
          MAPCOORD_TO_PRCCOORD(cor_z,0), 50);

        p_sthing = init_nuclear_bomb(cor_x, cor_y, cor_z);
        if (p_sthing != NULL)
        {
            play_dist_ssample(p_sthing, 1, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
            p_sthing->Timer1 = WEP_AIRSTRIKE_IMPACT_GTURNS;
            p_sthing->Owner2 = p_thing->Owner;
            p_sthing->Radius = 0;
            p_sthing->Flag &= ~TngF_Persuaded;
        }
    }
}

void process_unkn35_crashing(struct Thing *p_thing)
{
#if 0
    asm volatile (
      "call ASM_process_unkn35_crashing\n"
        : : "a" (p_thing));
#endif
    ushort rnd;
    int alt_under;

    LOGNO("Thing (%d,%d,%d)", (int)p_thing->X, (int)p_thing->Y, (int)p_thing->Z);
    move_mapwho(p_thing, p_thing->X + p_thing->VX, p_thing->Y + p_thing->VY, p_thing->Z + p_thing->VZ);

    rnd = LbRandomAnyShort();
    p_thing->VX -= p_thing->VX >> 4;
    p_thing->VZ -= p_thing->VZ >> 4;
    p_thing->VY -= ((rnd & 0x7F) + 0x200);

    alt_under = alt_at_point(PRCCOORD_TO_MAPCOORD(p_thing->X), PRCCOORD_TO_MAPCOORD(p_thing->Z));
    if (alt_under > p_thing->Y)
    {
        bang_new4(p_thing->X, p_thing->Y, p_thing->Z, 80);
        remove_thing(p_thing->ThingOffset);
        delete_node(p_thing);
    }
}

void process_unkn35(struct Thing *p_thing)
{
    if (p_thing->State == 13) {
        process_unkn35_crashing(p_thing);
        return;
    }
    //TODO implement the rest of the state update
}

void process_laser(struct Thing *p_laser)
{
    asm volatile (
      "call ASM_process_laser\n"
        : : "a" (p_laser));
}

struct SimpleThing *create_temp_light(int prc_x, int prc_y, int prc_z,
  short min_bright, short duration, short range_bright)
{
    short thing;
    struct SimpleThing *p_sthing;

    thing = get_new_sthing();
    if (thing == 0)
        return NULL;
    if (thing <= -(STHINGS_LIMIT + 1))
        return NULL;

    p_sthing = &sthings[thing];

    p_sthing->Type = SmTT_TEMP_LIGHT;
    //TODO switch from precise coords to map coords when all callers are remade
    p_sthing->X = prc_x; // MAPCOORD_TO_PRCCOORD(cor_x,127);
    p_sthing->Y = prc_y; // MAPCOORD_TO_PRCCOORD(cor_y,127);
    p_sthing->Z = prc_z; // MAPCOORD_TO_PRCCOORD(cor_z,127);
    p_sthing->Timer1 = duration;
    p_sthing->U.ULight.MinBright = min_bright;
    p_sthing->U.ULight.RangeBright = range_bright;

    return p_sthing;
}

struct SimpleThing *create_growing_e_strand(int x, int y, int z,
  struct Thing *p_thing, int health, int owner)
{
#if 1
    struct SimpleThing *ret;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[2];

    stkargs[0] = (int)(intptr_t)health;
    stkargs[1] = (int)(intptr_t)owner;

    asm volatile (
      "push 4(%5)\n"
      "push 0(%5)\n"
      "call ASM_create_growing_e_strand\n"
        : "=r" (ret)
        : "a" ((s32)x), "d" ((s32)y), "b" ((s32)z), "c" (p_thing), "S" (stkargs)
        : "cc", "memory");
    return ret;
#endif
}

void create_another_strand(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_create_another_strand\n"
        : : "a" (p_sthing));
}

void process_electric_strand(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_process_electric_strand\n"
        : : "a" (p_sthing));
}

void process_canister(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_process_canister\n"
        : : "a" (p_sthing));
}

void process_stasis_pod(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_process_stasis_pod\n"
        : : "a" (p_sthing));
}

void find_and_stasis(struct SimpleThing *p_stasis, int radius)
{
    asm volatile ("call ASM_find_and_stasis\n"
        :  : "a" (p_stasis), "d" (radius));
}

void find_and_time_travel(struct SimpleThing *p_sthing)
{
#if 0
    asm volatile ("call ASM_find_and_time_travel\n"
        :  : "a" (p_sthing));
#endif
    struct Thing *p_hittng;

    p_hittng = check_for_radius_hit_person(
      p_sthing->X, p_sthing->Y, p_sthing->Z,
      p_sthing->Radius, 0, 1, 0);
    if (p_hittng == NULL) {
        return;
    }

    if (p_hittng->Type == TT_VEHICLE)
    {
        struct Thing *p_person;
        ThingIdx person;

        for (person = p_hittng->U.UVehicle.PassengerHead;
          person != 0; person = p_person->U.UPerson.LinkPassenger)
        {
            p_person = &things[person];
            p_person->Flag2 |= 0x0002;
        }
    }
    if ((p_hittng->Type != SmTT_STATIC) &&
      (p_hittng->Type != TT_BUILDING))
    {
        delete_node(p_hittng);
        p_hittng->Flag2 |= 0x0002;
        p_hittng->U.UPerson.SpecialTimer = 100;
        play_dist_sample(p_hittng, 0x33u, 0x7Fu, 0x40u, 100, 0, 3);
    }
    if (p_hittng->Type == TT_PERSON)
    {
        stop_looped_weapon_sample(p_hittng,
          p_hittng->U.UPerson.CurrentWeapon);
        if ((p_hittng->Flag2 & 0x0001) != 0) {
            finalise_razor_wire(p_hittng);
        }
    }
}

void process_time_pod(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_process_time_pod\n"
        : : "a" (p_sthing));
}

void process_soul(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_process_soul\n"
        : : "a" (p_sthing));
}

void process_bang(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_process_bang\n"
        : : "a" (p_sthing));
}

void finish_bang(struct SimpleThing *p_sthing, ThingIdx thing)
{
    remove_sthing(thing);
    delete_snode(p_sthing);
}

void FIRE_process_flame(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_FIRE_process_flame\n"
        : : "a" (p_sthing));
}

void FIRE_finish_flame(struct SimpleThing *p_sthing, ThingIdx thing)
{
    ReleaseLoopedSample(thing, 16);
    play_dist_ssample(p_sthing, 0x17, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
    remove_sthing(thing);
    delete_snode(p_sthing);
}

#ifdef SW_FNCHK
static void process_spark_asm(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_process_spark\n"
        : : "a" (p_sthing) : "cc", "memory");
}
#endif

/** Move a simple thing to new coordinates; if the map tile changes, move
 * it to the head of the `mapwho` list of the new tile.  Tiles outside the
 * map are wrapped to 0..127.  Remade from move_smapwho.
 */
static void move_smapwho_c(struct SimpleThing *p_sthing, int *x, int y, int *z)
{
    if (((*x >> 16) >= MAP_TILE_WIDTH) || ((*z >> 16) >= MAP_TILE_HEIGHT) ||
      ((*x >> 16) < 0) || ((*z >> 16) < 0))
    {
        *x = (*x & 0xFFFF) | (((*x >> 16) & 0x7F) << 16);
        *z = (*z & 0xFFFF) | (((*z >> 16) & 0x7F) << 16);
    }
    if (((short)(*x >> 16) != (short)(p_sthing->X >> 16)) ||
      ((short)(*z >> 16) != (short)(p_sthing->Z >> 16)))
    {
        ushort tile;
        ThingIdx head;

        delete_snode(p_sthing);
        tile = (ushort)(((*z >> 16) << 7) + (*x >> 16));
        head = game_my_big_map[tile].Child;
        p_sthing->Next = head;
        if (head > 0)
            things[head].Parent = p_sthing->ThingOffset;
        else if (head < 0)
            sthings[head].Parent = p_sthing->ThingOffset;
        p_sthing->Parent = 0;
        game_my_big_map[tile].Child = p_sthing->ThingOffset;
    }
    p_sthing->X = *x;
    p_sthing->Z = *z;
    p_sthing->Y = y;
}

/** A spark flies on, falling, until its timer ends or it goes into the
 * ground.  Remade from process_spark.
 */
static void process_spark_c(struct SimpleThing *p_sthing)
{
    short timer;
    int x, y, z;

    timer = p_sthing->Timer1;
    p_sthing->Timer1 = timer - 1;
    if ((timer == 0) ||
      (alt_at_point((short)(p_sthing->X >> 8), (short)(p_sthing->Z >> 8)) > p_sthing->Y))
    {
        remove_sthing((short)(p_sthing - sthings));
        delete_snode(p_sthing);
        return;
    }
    x = p_sthing->X + p_sthing->U.UEffect.VX;
    z = p_sthing->Z + p_sthing->U.UEffect.VZ;
    y = p_sthing->Y + p_sthing->U.UEffect.VY;
    p_sthing->U.UEffect.OX = (short)(p_sthing->X >> 8);
    p_sthing->U.UEffect.OY = (short)(p_sthing->Y >> 8);
    p_sthing->U.UEffect.VY -= 32;
    p_sthing->U.UEffect.OZ = (short)(p_sthing->Z >> 8);
    move_smapwho_c(p_sthing, &x, y, &z);
}

void process_spark(struct SimpleThing *p_sthing)
{
    FNCHK_VOID("process_spark", process_spark_asm(p_sthing), process_spark_c(p_sthing));
}

#ifdef SW_FNCHK
static void process_intelligent_door_asm(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_process_intelligent_door\n"
        : : "a" (p_sthing) : "cc", "memory");
}
#endif

/** Sets the texture of a door face to the given frame of an animation
 * page (frames side by side).  Remade from apply_texture_to_face.
 */
static void apply_texture_to_face(ushort frame, ushort page, ushort face)
{
    struct SingleFloorTexture *p_tex;
    struct AnimPage *p_anpg;
    ubyte u, v, w, h;

    p_tex = &game_textures[game_object_faces4[face].Texture];
    p_anpg = &door_anim_page[page];
    w = p_anpg->Width;
    h = p_anpg->Height;
    u = (ubyte)(p_anpg->X + frame * w);
    v = p_anpg->Y;
    p_tex->Page = p_anpg->PageNo;
    p_tex->TMapX1 = u;
    p_tex->TMapY1 = v + h;
    p_tex->TMapX2 = u;
    p_tex->TMapY2 = v;
    p_tex->TMapX3 = u + w;
    p_tex->TMapY3 = v + h;
    p_tex->TMapX4 = u + w;
    p_tex->TMapY4 = v;
}

/** A door opening (state 1), open and waiting (3), closing (2).
 * Remade from process_intelligent_door.
 */
static void process_intelligent_door_c(struct SimpleThing *p_sthing)
{
    ushort frame;

    if (((p_sthing->Flag & TngF_Unkn0001) != 0) &&
      (p_sthing->State != 1) && (p_sthing->State != 3))
    {
        p_sthing->State = 1;
        p_sthing->Flag &= ~TngF_Unkn0001;
    }
    switch ((ushort)p_sthing->State)
    {
    case 1:
        p_sthing->Flag &= ~TngF_Unkn0001;
        frame = p_sthing->Frame + 1;
        p_sthing->Frame = frame;
        if (frame < door_anim_page[(ushort)p_sthing->StartFrame].No) {
            apply_texture_to_face(frame, p_sthing->StartFrame, p_sthing->Parent);
        } else {
            p_sthing->State = 3;
            p_sthing->Timer1 = 7;
            p_sthing->Frame = frame - 1;
            p_sthing->Flag |= TngF_Destroyed;
        }
        break;
    case 2:
        frame = p_sthing->Frame - 1;
        p_sthing->Frame = frame;
        if (frame <= door_anim_page[(ushort)p_sthing->StartFrame].No) {
            apply_texture_to_face(frame, p_sthing->StartFrame, p_sthing->Parent);
        } else {
            game_object_faces4[p_sthing->Parent].WalkHeader = 0;
            remove_sthing(p_sthing->ThingOffset);
        }
        break;
    case 3:
        p_sthing->Flag &= ~TngF_Unkn0001;
        if (p_sthing->Timer1 != 0) {
            p_sthing->Timer1--;
            break;
        }
        if (check_for_people(p_sthing->X, p_sthing->Y, p_sthing->Z, 100) != NULL)
            break;
        p_sthing->State = 2;
        play_dist_ssample(p_sthing, 0x35, 0x7F, 0x40, 100, 0, 1);
        break;
    default:
        break;
    }
}

void process_intelligent_door(struct SimpleThing *p_sthing)
{
    FNCHK_VOID("process_intelligent_door", process_intelligent_door_asm(p_sthing),
      process_intelligent_door_c(p_sthing));
}

void process_unkn58(struct SimpleThing *p_sthing)
{
    ushort frm;

    frm = frame[p_sthing->Frame].Next;

    if ((frame[frm].Flags & 0x01) != 0) {
        remove_sthing(p_sthing->ThingOffset);
        delete_snode(p_sthing);
        return;
    }

    p_sthing->Frame = frm;
}

ubyte napalm_person(struct Thing *p_person, ushort multi)
{
    ubyte ret;
    asm volatile ("call ASM_napalm_person\n"
        : "=r" (ret) : "a" (p_person), "d" (multi));
    return ret;
}

void process_drift_smoke(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
    asm volatile (
      "call ASM_process_drift_smoke\n"
        : : "a" (p_sthing), "d" (thing_item));
}

void process_mushroom(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
    asm volatile (
      "call ASM_process_mushroom\n"
        : : "a" (p_sthing), "d" (thing_item));
}

#ifdef SW_FNCHK
static void process_special_drift_smoke_asm(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
    asm volatile (
      "call ASM_process_special_drift_smoke\n"
        : : "a" (p_sthing), "d" (thing_item) : "cc", "memory");
}
#endif

/** Smoke drifting on a spiral (State counts the turns), slowing down
 * and rising, until its timer ends.  Remade from process_special_drift_smoke.
 */
static void process_special_drift_smoke_c(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
    struct STngUEffect *p_eff;
    int x, y, z;
    ushort turn, angle;
    s32 cs, sn;

    p_eff = &p_sthing->U.UEffect;
    p_sthing->State++;
    p_sthing->Frame = frame[(ushort)p_sthing->Frame].Next;
    x = p_sthing->X + (p_eff->VX << 2);
    y = p_sthing->Y + (p_eff->VY << 2);
    z = p_sthing->Z + (p_eff->VZ << 2);
    turn = p_sthing->State;
    angle = (turn << 5) & 0x7FF;
    cs = (lbSinTable[angle + 512] * turn) >> 11;
    sn = (lbSinTable[angle] * turn) >> 11;
    x += (short)cs;
    z += (short)sn;
    move_smapwho_c(p_sthing, &x, y, &z);
    p_sthing->Object += p_sthing->StartFrame;
    if (p_sthing->StartTimer1 > 0)
    {
        short slow;

        p_sthing->StartTimer1--;
        slow = p_eff->OX;
        p_eff->VX = (p_eff->VX * slow) >> 8;
        p_eff->VY = (p_eff->VY * slow) >> 8;
        p_eff->VZ = (p_eff->VZ * slow) >> 8;
    }
    p_eff->VY += p_eff->OY;
    if (p_sthing->Timer1 > 0)
    {
        p_sthing->Timer1--;
        if (p_sthing->Timer1 == 0) {
            remove_sthing(thing_item);
            delete_snode(p_sthing);
        }
    }
}

void process_special_drift_smoke(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
    FNCHK_VOID("process_special_drift_smoke", process_special_drift_smoke_asm(p_sthing, thing_item),
      process_special_drift_smoke_c(p_sthing, thing_item));
}

void process_napalm_flame(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
    asm volatile (
      "call ASM_process_napalm_flame\n"
        : : "a" (p_sthing), "d" (thing_item));
}

void process_flame1(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_process_flame1\n"
        : : "a" (p_sthing));
}

void process_splash(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_process_splash\n"
        : : "a" (p_sthing));
}

void process_scale_effect(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
#if 0
    asm volatile (
      "call ASM_process_scale_effect\n"
        : : "a" (p_sthing), "d" (thing_item));
#endif
    switch (p_sthing->SubType)
    {
    case 30:
        process_drift_smoke(p_sthing, thing_item);
        break;
    case 31:
        process_mushroom(p_sthing, thing_item);
        break;
    case 44:
        process_special_drift_smoke(p_sthing, thing_item);
        break;
    case 45:
        process_drift_smoke(p_sthing, thing_item);
        break;
    case 46:
        process_napalm_flame(p_sthing, thing_item);
        break;
    case 49:
        if (p_sthing->Timer1 == 4)
            p_sthing->Frame = nstart_ani[1104];
        process_drift_smoke(p_sthing, thing_item);
        break;
    case 52:
        process_flame1(p_sthing);
        break;
    case 56:
    case 57:
        process_flame1(p_sthing);
        break;
    case 58:
        process_unkn58(p_sthing);
        break;
    case 60:
        process_splash(p_sthing);
        break;
    }
}

void test_stuff(int x, int y, int z, ushort owner)
{
    asm volatile (
      "call ASM_test_stuff\n"
        : : "a" (x), "d" (y), "b" (z), "c" ((int)owner));
}

void process_nuclear_bomb(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
    asm volatile (
      "call ASM_process_nuclear_bomb\n"
        : : "a" (p_sthing), "d" (thing_item));
}

#ifdef SW_FNCHK
static void process_smoke_generator_asm(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_process_smoke_generator\n"
        : : "a" (p_sthing) : "cc", "memory");
}
#endif

/** A smoke generator puffs a smoke effect when its timer runs out, with
 * the generator's velocity, swayed by the game turn, and restarts the timer
 * at a random time between its period and twice that.
 */
static void process_smoke_generator_c(struct SimpleThing *p_sthing)
{
    struct SimpleThing *p_smoke;
    int x, y, z;
    short timer, period;
    ushort angle;

    x = p_sthing->X >> 8;
    y = p_sthing->Y >> 8;
    z = p_sthing->Z >> 8;
    timer = p_sthing->Timer1;
    p_sthing->Timer1 = timer - 1;
    if (timer >= 0)
        return;
    period = p_sthing->U.UEffect.OZ;
    p_sthing->Timer1 = (int)LbRandomAnyShort() % period + period;
    p_smoke = create_scale_effect(x, y, z, 0x38B, (short)(0x50 + (LbRandomAnyShort() & 0xF)));
    if (p_smoke == NULL)
        return;
    p_smoke->SubType = 0x2C;
    p_smoke->Object = 0x80;
    p_smoke->StartFrame = 2;
    p_smoke->U.UEffect.VX = p_sthing->U.UEffect.VX;
    p_smoke->U.UEffect.VY = p_sthing->U.UEffect.VY;
    p_smoke->U.UEffect.VZ = p_sthing->U.UEffect.VZ;
    angle = (gameturn << 7) & 0x7FF;
    p_smoke->U.UEffect.VX += (lbSinTable[angle + 512] >> 10) - 0x20;
    p_smoke->U.UEffect.VY += (lbSinTable[angle] >> 10) - 0x20;
    p_smoke->U.UEffect.OX = p_sthing->U.UEffect.OX;
    p_smoke->U.UEffect.OY = p_sthing->U.UEffect.OY;
    p_smoke->U.UEffect.OZ = p_sthing->U.UEffect.OZ;
    p_smoke->StartTimer1 = p_sthing->StartTimer1;
    p_smoke->State = 0;
    p_smoke->Owner2 = p_sthing->Owner2;
}

void process_smoke_generator(struct SimpleThing *p_sthing)
{
    FNCHK_VOID("process_smoke_generator", process_smoke_generator_asm(p_sthing),
      process_smoke_generator_c(p_sthing));
}

void process_thing_checksum(struct Thing *p_thing, ThingIdx thing)
{
    ingame.fld_unkC4B += thing;
    ingame.fld_unkC4B += p_thing->Y + p_thing->X + p_thing->Type + p_thing->State;
}

TbBool process_thing_unkflag02000000(struct Thing *p_thing, ThingIdx thing)
{
    if (((p_thing->Flag2 & TgF2_Unkn02000000) != 0) && ((gameturn + thing) & 3) != 0) {
        return true;
    }
    p_thing->Flag2 &= ~TgF2_Unkn02000000;
    return false;
}

TbBool process_thing_unkflag0002(struct Thing *p_thing)
{
    if ((p_thing->Flag2 & TgF2_Unkn0002) != 0)
    {
        if ((gameturn & 3) != 0)
            return true;
        p_thing->U.UPerson.SpecialTimer--;
        if (p_thing->U.UPerson.SpecialTimer >= 0)
            return true;
        if ((p_thing->Flag & TngF_InVehicle) == 0)
            add_node_thing(p_thing->ThingOffset);
        p_thing->Flag2 &= ~TgF2_Unkn0002;
        return true;
    }
    return false;
}

void process_thing(struct Thing *p_thing, ThingIdx thing)
{
    switch (p_thing->Type)
    {
    case TT_VEHICLE:
        AMIGA_PROF_BEGIN(39);
        process_vehicle(p_thing);
        AMIGA_PROF_END(39);
        break;
    case TT_PERSON:
    case TT_UNKN4:
        AMIGA_PROF_BEGIN(40);
        process_shield(p_thing);
        process_person(p_thing);
        AMIGA_PROF_END(40);
        if ((p_thing->Flag & TngF_TriggerUse) != 0)
            p_thing->Flag2 |= TgF2_Unkn0400;
        else
            p_thing->Flag2 &= ~TgF2_Unkn0400;
        break;
    case TT_ROCKET:
        process_rocket(p_thing);
        break;
    case TT_BUILDING:
        AMIGA_PROF_BEGIN(41);
        process_building(p_thing);
        AMIGA_PROF_END(41);
        break;
    case TT_LASER_GUIDED:
        process_laser_guided(p_thing);
        break;
    case TT_MINE:
        process_mine((struct SimpleThing *)p_thing);
        break;
    case TT_GRENADE:
        process_grenade(p_thing);
        break;
    case TT_LASER_ELEC:
        process_laser_elec(p_thing);
        break;
    case TT_RAZOR_WIRE: // exploding wire it a subtype of razor wire
        process_razor_wire(p_thing);
        break;
    case TT_AIR_STRIKE:
        process_air_strike(p_thing);
        break;
    case TT_UNKN35:
        process_unkn35(p_thing);
        break;
    case TT_LASER11:
    case TT_LASER29:
    case TT_LASER38: // on PSX, laser type=38 has additional light effect
        process_laser(p_thing);
        break;
    default:
        break;
    }
}

void process_sthing_checksum(struct SimpleThing *p_sthing, ThingIdx thing)
{
    ingame.fld_unkC4B += p_sthing->Y + p_sthing->X + p_sthing->Type + p_sthing->State;
}

void process_carried_item(struct SimpleThing *p_item)
{
    ThingIdx item;
    struct Thing *p_owner;

    p_owner = &things[p_item->U.UWeapon.Owner];
    p_item->X = p_owner->X;
    p_item->Y = p_owner->Y;
    p_item->Z = p_owner->Z;
    if (p_owner->State == PerSt_DEAD) {
        item = p_item->ThingOffset;
        p_item->Type = SmTT_DROPPED_ITEM;
        add_node_sthing(item);
    }
}

void process_temp_light(struct SimpleThing *p_sthing)
{
    int brig, rng;
    short lgtng_x, lgtng_y, lgtng_z;

    p_sthing->Timer1--;
    if (p_sthing->Timer1 < 0) {
        remove_sthing(p_sthing->ThingOffset);
        return;
    }
    brig = p_sthing->U.ULight.MinBright;
    if (p_sthing->U.ULight.RangeBright != 0) {
        rng = p_sthing->U.ULight.RangeBright;
        brig += (ushort)LbRandomAnyShort() % rng;
    }
    get_thing_position_mapcoords(&lgtng_x, &lgtng_y, &lgtng_z, p_sthing->ThingOffset);
    apply_full_light(lgtng_x, lgtng_y >> 3, lgtng_z, brig, 0);
}

void set_thing_frame(struct Thing *p_thing, ushort anim_start)
{
    assert(p_thing->Type != TT_PERSON);
    p_thing->StartFrame = anim_start;
    p_thing->Frame = nstart_ani[p_thing->StartFrame];
}

void reset_thing_frame(struct Thing *p_thing)
{
    assert(p_thing->Type != TT_PERSON);
    p_thing->Frame = nstart_ani[p_thing->StartFrame];
}

void reset_sthing_frame(struct SimpleThing *p_sthing)
{
    p_sthing->Frame = nstart_ani[p_sthing->StartFrame];
}

void process_burning_static(struct SimpleThing *p_sthing)
{
    p_sthing->Timer1--;
    if (p_sthing->Timer1 != 0)
        return;
    p_sthing->State = 13;
    reset_sthing_frame(p_sthing);
    stop_sample_using_heap(p_sthing->ThingOffset, 29);
}

void process_static(struct SimpleThing *p_sthing)
{
    switch (p_sthing->State)
    {
    case 36:
        play_dist_ssample(p_sthing, 29, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 2);
        process_burning_static(p_sthing);
        break;
    case 12:
        process_burning_static(p_sthing);
        break;
    }
}

void process_sfx(struct SimpleThing *p_sthing)
{
    switch (p_sthing->State)
    {
    case 1:
        if (!dont_bother_with_explode_faces)
            break;
        stop_sample_using_heap(p_sthing->ThingOffset, 0x2E);
        play_dist_ssample(p_sthing, 0x2F, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
        p_sthing->State = 2;
        p_sthing->Timer1 = 100;
        break;
    default:
        p_sthing->Timer1--;
        if (p_sthing->Timer1 >= 0)
            break;
        remove_sthing(p_sthing->ThingOffset);
        if (p_sthing->State != 2)
            break;
        ingame.SoundThing = 0;
        break;
    }
}

void process_sthing(struct SimpleThing *p_sthing, ThingIdx thing)
{
    switch (p_sthing->Type)
    {
    case SmTT_CARRIED_ITEM:
        process_carried_item(p_sthing);
        break;
    case SmTT_ELECTRIC_STRAND:
        process_electric_strand(p_sthing);
        break;
    case SmTT_CANISTER:
        process_canister(p_sthing);
        break;
    case SmTT_STASIS_POD:
        process_stasis_pod(p_sthing);
        break;
    case SmTT_TIME_POD:
        process_time_pod(p_sthing);
        break;
    case SmTT_SOUL:
        process_soul(p_sthing);
        break;
    case SmTT_BANG:
        if ((p_sthing->U.UEffect.VY != 0) || (p_sthing->U.UEffect.VX != 0)) {
            process_bang(p_sthing);
        } else {
            finish_bang(p_sthing, thing);
        }
        break;
    case SmTT_FIRE:
        if (p_sthing->U.UFire.flame != 0) {
            FIRE_process_flame(p_sthing);
        } else {
            FIRE_finish_flame(p_sthing, thing);
        }
        break;
    case SmTT_SFX:
        process_sfx(p_sthing);
        break;
    case SmTT_TEMP_LIGHT:
        process_temp_light(p_sthing);
        break;
    case SmTT_SPARK:
        process_spark(p_sthing);
        break;
    case SmTT_STATIC:
        process_static(p_sthing);
        break;
    case SmTT_INTELLIG_DOOR:
        process_intelligent_door(p_sthing);
        break;
    case SmTT_SCALE_EFFECT:
        process_scale_effect(p_sthing, thing);
        break;
    case SmTT_NUCLEAR_BOMB:
        process_nuclear_bomb(p_sthing, thing);
        break;
    case SmTT_SMOKE_GENERATOR:
        process_smoke_generator(p_sthing);
        break;
    case SmTT_DROPPED_ITEM:
        if (p_sthing->SubType == WEP_EXPLMINE || p_sthing->SubType == WEP_ELEMINE) {
            process_mine(p_sthing);
        } else {
            if (!in_network_game || p_sthing->U.UWeapon.WeaponType == 31)
                break;
            p_sthing->U.UWeapon.Ammo++;
        }
        break;
    default:
        break;
    }
}

#if defined(SW_AMIGA)
int amiga_getenv_int(const char *name, int dflt);
#endif

/** Far civilians option: 0 off (as the original), 1 or 2 - a wandering
 * civilian far from the camera and from all the local player's agents is
 * processed every 2nd or 4th turn; 3 - such a civilian is not processed at
 * all while it is also off the scanner (it stands still until the view or
 * the scanner reaches it). On a 68060 the simulation of the whole
 * city's crowd is a large part of a turn, while nearly all of it is far
 * from the view (mission 1: 285 of 308 people). They walk slower there,
 * which cannot be seen. Not in network games and packet recording or
 * playback, which need the original course of the game.
 * ENV:SyndWarsCivLOD on the Amiga and the Warp ARM (default 3 there),
 * SW_CIVLOD elsewhere (default 0: the PC build stays the reference).
 */
static int far_civilians_level(void)
{
    static int level = -1;

    if (level < 0)
    {
#if defined(SW_AMIGA)
        // on by default on the Amiga and the Warp ARM (the user found no
        // change in the game); SyndWarsCivLOD 0 turns it off
        level = amiga_getenv_int("SyndWarsCivLOD", 3);
#else
        const char *s = getenv("SW_CIVLOD");
        level = (s != NULL) ? atoi(s) : 0;
#endif
        if ((level < 0) || (level > 3))
            level = 0;
        if (level == 3)
            LOGWARN("far civilians off the scanner not processed");
        else if (level > 0)
            LOGWARN("far civilians processed every %d turns", 1 << level);
    }
    return level;
}

#define FAR_CIVILIAN_DIST TILE_TO_MAPCOORD(16, 0)

static TbBool map_coords_within(MapCoord x1, MapCoord z1, MapCoord x2, MapCoord z2, MapCoord dist)
{
    return (abs(x1 - x2) < dist) && (abs(z1 - z2) < dist);
}

/** Is the map point surely off the scanner. The scanner shows a square
 * around (MX, MZ) (in map coords >> 7), Zoom/2 map coords per pixel, turned
 * by its angle: the test takes the square round that turned one, plus 2
 * tiles. */
static TbBool map_coords_off_scanner(MapCoord cor_x, MapCoord cor_z)
{
    int range, half;

    range = ingame.Scanner.X2 - ingame.Scanner.X1;
    if (ingame.Scanner.Y2 - ingame.Scanner.Y1 < range)
        range = ingame.Scanner.Y2 - ingame.Scanner.Y1;
    range >>= 1;
    half = (range * ingame.Scanner.Zoom / 2) * 3 / 2 + TILE_TO_MAPCOORD(2, 0);
    return !map_coords_within(cor_x, cor_z, ingame.Scanner.MX << 7,
      ingame.Scanner.MZ << 7, half);
}

/** Is this the turn to skip the person, under the far civilians option. */
static TbBool far_civilian_skip_turn(struct Thing *p_person, ThingIdx thing)
{
    int level;
    MapCoord cor_x, cor_z;
    ushort i;

    level = far_civilians_level();
    if (level == 0)
        return false;
    if ((level < 3) && (((gameturn + thing) & ((1 << level) - 1)) == 0))
        return false;
    if (in_network_game || (pktrec_mode != PktR_NONE))
        return false;
    if (p_person->State != PerSt_WANDER)
        return false;
    if ((p_person->Flag & (TngF_PlayerAgent | TngF_Persuaded | TngF_InVehicle | TngF_Destroyed)) != 0)
        return false;
    cor_x = PRCCOORD_TO_MAPCOORD(p_person->X);
    cor_z = PRCCOORD_TO_MAPCOORD(p_person->Z);
    if (map_coords_within(cor_x, cor_z, engn_xc, engn_zc, FAR_CIVILIAN_DIST))
        return false;
    for (i = 0; i < playable_agents; i++)
    {
        struct Thing *p_agent;

        p_agent = players[local_player_no].MyAgent[i];
        if (p_agent == NULL)
            continue;
        if (map_coords_within(cor_x, cor_z, PRCCOORD_TO_MAPCOORD(p_agent->X),
          PRCCOORD_TO_MAPCOORD(p_agent->Z), FAR_CIVILIAN_DIST))
            return false;
    }
    if ((level == 3) && !map_coords_off_scanner(cor_x, cor_z))
        return false;
    return true;
}

void process_things(void)
{
    int i;
    ushort plyr;

    for (plyr = 0; plyr < PLAYERS_LIMIT; plyr++)
    {
        PlayerInfo *p_player;
        struct Thing *p_dcthing;
        ThingIdx dcthing;

        p_player = &players[plyr];
        dcthing = p_player->DirectControl[0];
        p_dcthing = &things[dcthing];
        if (((1 << plyr) & ingame.InNetGame_UNSURE) != 0
          && (p_dcthing->Flag & TngF_SelectedAgent) == 0)
        {
#if 0
            for (i = 0; i < playable_agents; i++)
              ;
#endif
            p_dcthing->Flag |= TngF_SelectedAgent;
        }
    }

    if (in_network_game)
    {
        if (ingame.fld_unkCB7 > 150)
            process_things_mines_explode((rand() & 0x1F) + 2);
        if ((net_game_play_flags & NGPF_Unkn10) != 0 && (gameturn & 0xF) == 0)
            process_things_unkn_sub2(login_control[0].TechLevel, ingame.fld_unkCB7 > 100);
        if (things_used > 900)
            process_things_bang(16);
        else if ( things_used > 700 || ((gameturn & 0xF) == 0))
            process_things_bang(1);
    }

    if ((gameturn & 0x1F) == 0)
    {
        for (i = 0; i < PEOPLE_GROUPS_LIMIT+1; i++) {
            group_actions[i].Storming &= ~GrpAF_UnderAttack;
        }
    }

    shield_frames_cycle();

    player_update_thermal(local_player_no);

#if 0
    merged_noop_unkn1(gameturn);
#endif
    AMIGA_PROF_BEGIN(35);
    build_same_type_headers();
    ingame.fld_unkC4B = 0;
    unkn_update_lights();
    unkn_full_update_lights();
    AMIGA_PROF_END(35);

    if (!execute_commands)
        return;
    if ((ingame.Flags & TngF_ProgressAction) == 0)
        return;
    monitor_all_samples();

    if (!in_network_game && (pktrec_mode == PktR_NONE)
      && ((ingame.Flags & GamF_Unkn0004) != 0) && ((gameturn & 0xF) != 0))
        return;

    if (execute_commands)
    {
        struct Thing *p_thing;
        int remain;
        ThingIdx thing, nxthing;

        AMIGA_PROF_BEGIN(36);
        remain = things_used;
        for (thing = things_used_head; thing > 0; thing = nxthing)
        {
            if (--remain == -1) {
                break;
            }
            p_thing = &things[thing];
            nxthing = p_thing->LinkChild;

            process_thing_checksum(p_thing, thing);

            if (process_thing_unkflag02000000(p_thing, thing))
                continue;

            if (process_thing_unkflag0002(p_thing))
                continue;

            if ((p_thing->Type == TT_PERSON) && far_civilian_skip_turn(p_thing, thing))
                continue;

            process_thing(p_thing, thing);
        }
        AMIGA_PROF_END(36);
    }

    if (execute_commands)
    {
        struct SimpleThing *p_sthing;
        int remain;
        ThingIdx thing, nxthing;

        AMIGA_PROF_BEGIN(37);
        remain = sthings_used;
        for (thing = sthings_used_head; thing < 0; thing = nxthing)
        {
            if (--remain == -1) {
                break;
            }
            p_sthing = &sthings[thing];
            nxthing = p_sthing->LinkChild;

            process_sthing_checksum(p_sthing, thing);

            process_sthing(p_sthing, thing);
        }
        AMIGA_PROF_END(37);
    }

    if (execute_commands)
    {
        AMIGA_PROF_BEGIN(38);
        people_intel(0);
        AMIGA_PROF_END(38);
    }
    navi_onscreen_debug(1);
}

const char *thing_type_name(ubyte tngtype, ubyte subtype)
{
    if (tngtype >= sizeof(thing_type_names)/sizeof(thing_type_names[1]))
        return "OUTRANGED";
    if (tngtype == TT_PERSON)
        return person_type_name(subtype);
    if (tngtype == TT_VEHICLE)
        return vehicle_type_name(subtype);
    if (tngtype == TT_BUILDING)
        return building_type_name(subtype);
    // TODO support some sub-types
    return thing_type_names[tngtype];
}

void snprint_thing(char *buf, ulong buflen, struct Thing *p_thing)
{
    char *s;
    ubyte nparams;

    s = buf;

    sprintf(s, "T%02d = %s( ", (int)p_thing->ThingOffset, thing_type_name(p_thing->Type, p_thing->SubType));
    s += strlen(s);
    nparams = 0;

    {
        if (nparams) { sprintf(s, ", "); s += strlen(s); }
        sprintf(s, "Coord(%d,%d,%d)", (int)PRCCOORD_TO_MAPCOORD(p_thing->X),
          (int)PRCCOORD_TO_MAPCOORD(p_thing->Y), (int)PRCCOORD_TO_MAPCOORD(p_thing->Z));
        s += strlen(s);
        nparams++;
    }
    if (p_thing->Health != 0) {
        if (nparams) { sprintf(s, ", "); s += strlen(s); }
        sprintf(s, "Health(%hd)", p_thing->Health);
        s += strlen(s);
        nparams++;
    }
    if (p_thing->Flag != 0) {
        if (nparams) { sprintf(s, ", "); s += strlen(s); }
        sprintf(s, "Flag(0x%lx)", p_thing->Flag);
        s += strlen(s);
        nparams++;
    }
    if (p_thing->Type == TT_PERSON) {
        if (nparams) { sprintf(s, ", "); s += strlen(s); }
        snprint_person_state(s, buflen - (s-buf), p_thing);
        s += strlen(s);
        nparams++;
    }
    // TODO support more properties

    snprintf(s, buflen - (s-buf), " )");
}

void snprint_sthing(char *buf, ulong buflen, struct SimpleThing *p_sthing)
{
    char *s;
    ubyte nparams;

    s = buf;

    sprintf(s, "T%02d = %s( ", (int)p_sthing->ThingOffset, thing_type_name(p_sthing->Type, p_sthing->SubType));
    s += strlen(s);
    nparams = 0;

    {
        if (nparams) { sprintf(s, ", "); s += strlen(s); }
        sprintf(s, "Coord(%d,%d,%d)", (int)PRCCOORD_TO_MAPCOORD(p_sthing->X),
          (int)PRCCOORD_TO_MAPCOORD(p_sthing->Y), (int)PRCCOORD_TO_MAPCOORD(p_sthing->Z));
        s += strlen(s);
        nparams++;
    }
    if (p_sthing->Flag != 0) {
        if (nparams) { sprintf(s, ", "); s += strlen(s); }
        sprintf(s, "Flag(0x%lx)", p_sthing->Flag);
        s += strlen(s);
        nparams++;
    }
    // TODO support more properties

    snprintf(s, buflen - (s-buf), " )");
}

TbBool thing_is_destroyed(ThingIdx thing)
{
    if (thing == 0) {
        return true;
    } else if (thing > 0) {
        struct Thing *p_thing;
        p_thing = &things[thing];
        return ((p_thing->Flag & TngF_Destroyed) != 0);
    } else {
        struct SimpleThing *p_sthing;
        p_sthing = &sthings[thing];
        return ((p_sthing->Flag & TngF_Destroyed) != 0);
    }
}

TbBool thing_is_pickup_item(ThingIdx thing)
{
    struct SimpleThing *p_sthing;

    if (thing >= 0)
        return false;

    p_sthing = &sthings[thing];
    return thing_type_is_pickup_item(p_sthing->Type);
}

/** Map coordinate in range for the `mapwho` lists (tile 0..127, as in asm). */
static inline TbBool mapwho_coord_in_range(s32 cor)
{
    return ((cor >> 8) >= 0) && ((cor >> 8) < 0x8000);
}

static inline struct MyMapElement *mapwho_mapel(s32 cor_x, s32 cor_z)
{
    return &game_my_big_map[MAP_TILE_WIDTH * (cor_z >> 16) + (cor_x >> 16)];
}

/** Set the `Parent` of a thing or sthing (by sign of the index). */
static inline void mapwho_set_parent(ThingIdx thing, short parent)
{
    if (thing > 0)
        things[thing].Parent = parent;
    else if (thing < 0)
        sthings[thing].Parent = parent;
}

#ifdef SW_FNCHK
static TbResult delete_node_asm(struct Thing *p_thing)
{
    TbResult ret;
    asm volatile ("call ASM_delete_node\n"
        : "=a" (ret) : "a" (p_thing) : "cc", "memory");
    return ret;
}
#endif

/** Remove a thing (or sthing - same leading fields) from its `mapwho` list. */
static TbResult delete_node_c(struct Thing *p_thing)
{
    if (!mapwho_coord_in_range(p_thing->X) || !mapwho_coord_in_range(p_thing->Z))
        return 0;

    if (p_thing->Parent > 0)
        things[p_thing->Parent].Next = p_thing->Next;
    else if (p_thing->Parent < 0)
        sthings[p_thing->Parent].Next = p_thing->Next;
    else
        mapwho_mapel(p_thing->X, p_thing->Z)->Child = p_thing->Next;

    mapwho_set_parent(p_thing->Next, p_thing->Parent);

    p_thing->Parent = 0;
    p_thing->Next = 0;
    return 0;
}

TbResult delete_node(struct Thing *p_thing)
{
    TbResult ret;
    FNCHK_RET("delete_node", ret, delete_node_asm(p_thing), delete_node_c(p_thing));
    return ret;
}

#ifdef SW_FNCHK
static TbResult delete_snode_asm(struct SimpleThing *p_sthing)
{
    TbResult ret;
    asm volatile ("call ASM_delete_snode\n"
        : "=a" (ret) : "a" (p_sthing) : "cc", "memory");
    return ret;
}
#endif

/** Remove an sthing from its `mapwho` list. Unlike delete_node(), the
 * original neither checks the coordinates nor clears the links.
 */
static TbResult delete_snode_c(struct SimpleThing *p_sthing)
{
    if (p_sthing->Parent > 0)
        things[p_sthing->Parent].Next = p_sthing->Next;
    else if (p_sthing->Parent < 0)
        sthings[p_sthing->Parent].Next = p_sthing->Next;
    else
        game_my_big_map[((p_sthing->Z >> 16) << 7) + (p_sthing->X >> 16)].Child = p_sthing->Next;

    if (p_sthing->Next > 0)
        things[p_sthing->Next].Parent = p_sthing->Parent;
    else if (p_sthing->Next < 0)
        sthings[p_sthing->Next].Parent = p_sthing->Parent;
    return 0;
}

TbResult delete_snode(struct SimpleThing *p_sthing)
{
    TbResult ret;
    FNCHK_RET("delete_snode", ret, delete_snode_asm(p_sthing), delete_snode_c(p_sthing));
    return ret;
}

#ifdef SW_FNCHK
static void add_node_thing_asm(ThingIdx new_thing)
{
    asm volatile ("call ASM_add_node_thing\n"
        : : "a" (new_thing) : "cc", "memory");
}
#endif

/** Put a thing at the head of the `mapwho` list of its map tile.
 * The index is taken as unsigned, like in the original.
 */
static void add_node_thing_c(ThingIdx new_thing)
{
    struct Thing *p_thing;
    struct MyMapElement *p_mapel;

    p_thing = &things[(ushort)new_thing];
    p_thing->Parent = 0;
    if (!mapwho_coord_in_range(p_thing->X) || !mapwho_coord_in_range(p_thing->Z))
        return;
    p_mapel = mapwho_mapel(p_thing->X, p_thing->Z);
    p_thing->Next = p_mapel->Child;
    mapwho_set_parent(p_thing->Next, new_thing);
    p_mapel->Child = new_thing;
}

void add_node_thing(ThingIdx new_thing)
{
    FNCHK_VOID("add_node_thing", add_node_thing_asm(new_thing),
      add_node_thing_c(new_thing));
}

#ifdef SW_FNCHK
static short get_new_thing_asm(void)
{
    short ret;
    asm volatile ("call ASM_get_new_thing\n"
        : "=a" (ret) : : "cc", "memory");
    return ret;
}
#endif

/** Take a thing from the head of the empty list and put it at the head
 * of the used list. Returns 0 if the limit is reached.
 */
static short get_new_thing_c(void)
{
    struct Thing *p_thing;
    ushort new_thing, used_head, next_empty;

    if (things_used > THINGS_LIMIT - 5)
        return 0;
    new_thing = (ushort)things_empty_head;
    used_head = (ushort)things_used_head;

    things[used_head].LinkParent = new_thing;
    p_thing = &things[new_thing];
    p_thing->LinkParent = 0;
    p_thing->Flag = 0;
    next_empty = (ushort)p_thing->LinkChild;
    p_thing->Flag2 = 0;
    p_thing->U.UPerson.Flag3 = 0;
    p_thing->LinkChild = used_head;
    things[next_empty].LinkParent = 0;
    things_used++;

    things_used_head = new_thing;
    things_empty_head = next_empty;
    return new_thing;
}

short get_new_thing(void)
{
    short ret;
    FNCHK_RET("get_new_thing", ret, get_new_thing_asm(), get_new_thing_c());
    return ret;
}

void remove_thing(short tngno)
{
    asm volatile ("call ASM_remove_thing\n"
        : : "a" (tngno));
}

#ifdef SW_FNCHK
static void add_node_sthing_asm(ThingIdx new_thing)
{
    asm volatile ("call ASM_add_node_sthing\n"
        : : "a" (new_thing) : "cc", "memory");
}
#endif

/** Put an sthing at the head of the `mapwho` list of its map tile.
 */
static void add_node_sthing_c(ThingIdx new_thing)
{
    struct SimpleThing *p_sthing;
    struct MyMapElement *p_mapel;

    p_sthing = &sthings[new_thing];
    p_sthing->Parent = 0;
    if (!mapwho_coord_in_range(p_sthing->X) || !mapwho_coord_in_range(p_sthing->Z))
        return;
    p_mapel = mapwho_mapel(p_sthing->X, p_sthing->Z);
    p_sthing->Next = p_mapel->Child;
    mapwho_set_parent(p_sthing->Next, new_thing);
    p_mapel->Child = new_thing;
}

void add_node_sthing(ThingIdx new_thing)
{
    FNCHK_VOID("add_node_sthing", add_node_sthing_asm(new_thing),
      add_node_sthing_c(new_thing));
}

#ifdef SW_FNCHK
static short get_new_sthing_asm(void)
{
    short ret;
    asm volatile ("call ASM_get_new_sthing\n"
        : "=a" (ret) : : "cc", "memory");
    return ret;
}
#endif

/** Take an sthing from the head of the empty list and put it at the head
 * of the used list. Returns 0 if the limit is reached.
 */
static short get_new_sthing_c(void)
{
    struct SimpleThing *p_sthing;
    ThingIdx new_thing, used_head, next_empty;

    if (sthings_used > STHINGS_LIMIT - 5)
        return 0;
    new_thing = sthings_empty_head;
    used_head = sthings_used_head;

    sthings[used_head].LinkParent = new_thing;
    p_sthing = &sthings[new_thing];
    p_sthing->LinkParent = 0;
    next_empty = p_sthing->LinkChild;
    p_sthing->Flag = 0;
    p_sthing->LinkChild = used_head;
    sthings[next_empty].LinkParent = 0;
    sthings_used++;

    sthings_used_head = new_thing;
    sthings_empty_head = next_empty;
    return new_thing;
}

short get_new_sthing(void)
{
    short ret;
    FNCHK_RET("get_new_sthing", ret, get_new_sthing_asm(), get_new_sthing_c());
    return ret;
}

#ifdef SW_FNCHK
static void remove_sthing_asm(short tngno)
{
    asm volatile ("call ASM_remove_sthing\n"
        : : "a" (tngno) : "cc", "memory");
}
#endif

/** Take an sthing off the used list and put it at the head of the empty list.
 * The original reads the old empty head as the upper half of a 32-bit read
 * at `sthings_used` (the next variable in its memory layout).
 */
static void remove_sthing_c(short tngno)
{
    struct SimpleThing *p_sthing;
    ThingIdx child, parent, old_empty;

    p_sthing = &sthings[tngno];
    child = p_sthing->LinkChild;
    parent = p_sthing->LinkParent;
    p_sthing->Type = 0;
    if (child < 0)
        sthings[child].LinkParent = parent;
    if (tngno == sthings_used_head)
        sthings_used_head = child;
    else
        sthings[parent].LinkChild = child;

    old_empty = sthings_empty_head;
    p_sthing->LinkParent = 0;
    p_sthing->LinkChild = old_empty;
    sthings_empty_head = tngno;
    sthings_used--;
    sthings[old_empty].LinkParent = tngno;
}

void remove_sthing(short tngno)
{
    FNCHK_VOID("remove_sthing", remove_sthing_asm(tngno), remove_sthing_c(tngno));
}

short add_static(int x, int y, int z, ushort frame, int timer)
{
#if 0
    short ret;
    asm volatile (
      "push %5\n"
      "call ASM_add_static\n"
        : "=r" (ret) : "a" (x), "d" (y), "b" (z), "c" (frame), "g" (timer));
    return ret;
#else
    ThingIdx thing;
    struct SimpleThing *p_sthing;

    if (map_coords_limit(NULL, NULL, NULL, x, y, z))
        return 0;
    if (sthings_used > STHINGS_LIMIT - 5) {
        return 0;
    }

    thing = get_new_sthing();
    if (thing == 0) {
        return 0;
    }
    if (thing <= -STHINGS_LIMIT-1) {
        return 0;
    }
    p_sthing = &sthings[thing];
    p_sthing->Z = MAPCOORD_TO_PRCCOORD(z,127);
    p_sthing->X = MAPCOORD_TO_PRCCOORD(x,127);
    p_sthing->Y = y;
    p_sthing->Parent = 0;
    p_sthing->StartFrame = frame - 1;
    //TODO verify and/or unify to allow use of reset_sthing_frame(p_sthing);
    p_sthing->Frame = nstart_ani[p_sthing->StartFrame + 1];
    add_node_sthing(thing);
    p_sthing->Type = SmTT_STATIC;
    p_sthing->Radius = 64;
    p_sthing->Flag = 4;
    p_sthing->Timer1 = timer;
    return thing;
#endif
}

TbBool thing_is_within_circle(ThingIdx thing, short X, short Z, ushort R)
{
    s32 dtX, dtZ, r2;

    if (thing <= 0) {
        struct SimpleThing *p_sthing;
        p_sthing = &sthings[thing];
        dtX = PRCCOORD_TO_MAPCOORD(p_sthing->X) - X;
        dtZ = PRCCOORD_TO_MAPCOORD(p_sthing->Z) - Z;
    } else {
        struct Thing *p_thing;
        p_thing = &things[thing];
        dtX = PRCCOORD_TO_MAPCOORD(p_thing->X) - X;
        dtZ = PRCCOORD_TO_MAPCOORD(p_thing->Z) - Z;
    }
    r2 = R * R;
    return ((dtZ * dtZ + dtX * dtX) < r2);
}

TbBool thing_intersects_circle(ThingIdx thing, short X, short Z, ushort R)
{
    s32 dtX, dtZ, cyl_r2, tng_r;

    if (thing <= 0) {
        struct SimpleThing *p_sthing;
        p_sthing = &sthings[thing];
        dtX = PRCCOORD_TO_MAPCOORD(p_sthing->X) - X;
        dtZ = PRCCOORD_TO_MAPCOORD(p_sthing->Z) - Z;
        tng_r = p_sthing->Radius;
    } else {
        struct Thing *p_thing;
        p_thing = &things[thing];
        dtX = PRCCOORD_TO_MAPCOORD(p_thing->X) - X;
        dtZ = PRCCOORD_TO_MAPCOORD(p_thing->Z) - Z;
        tng_r = p_thing->Radius;
    }
    cyl_r2 = R * R;
    return ((dtZ * dtZ + dtX * dtX) < cyl_r2 + tng_r * tng_r);
}

TbBool thing_intersects_cylinder(ThingIdx thing, short X, short Y, short Z, ushort R, ushort H)
{
    s32 dtX, dtY, dtZ, cyl_r2, tng_r, h_min, h_max;

    if (thing <= 0) {
        struct SimpleThing *p_sthing;
        p_sthing = &sthings[thing];
        dtX = PRCCOORD_TO_MAPCOORD(p_sthing->X) - X;
        dtY = PRCCOORD_TO_MAPCOORD(p_sthing->Y) - Y;
        dtZ = PRCCOORD_TO_MAPCOORD(p_sthing->Z) - Z;
        tng_r = p_sthing->Radius;
        h_min = H;
        h_max = H;
    } else {
        struct Thing *p_thing;
        p_thing = &things[thing];
        dtX = PRCCOORD_TO_MAPCOORD(p_thing->X) - X;
        dtY = PRCCOORD_TO_MAPCOORD(p_thing->Y) - Y;
        dtZ = PRCCOORD_TO_MAPCOORD(p_thing->Z) - Z;
        tng_r = p_thing->Radius;
        h_min = H;
        h_max = H;
        // Things represented by 3D objects require lower minimum
        if ((p_thing->Type == TT_VEHICLE) || (p_thing->Type == TT_BUILDING)) {
            h_min += 80;
        }
    }
    cyl_r2 = R * R;
    return ((dtZ * dtZ + dtX * dtX) < cyl_r2 + tng_r * tng_r)
        && ((dtY > -h_min) && (dtY < h_max));
}

void build_same_type_headers(void)
{
#if 0
    asm volatile ("call ASM_build_same_type_headers\n"
        :  :  : "eax" );
    return;
#endif
    struct Thing *p_thing;
    short i;
    ThingIdx thing;
    TbBool link_all_people;

    // On specific modes, people are linked into same-type lists even
    // while under a flag which would normally exclude them
    link_all_people = (word_1552F8 == 36) || (word_1552F8 == 18);

    for (i = 0; i < 256 + PEOPLE_GROUPS_LIMIT + 1; i++)
        same_type_head[i] = 0;

    for (thing = things_used_head; thing != 0; thing = p_thing->LinkChild)
    {
        p_thing = &things[thing];

        switch (p_thing->Type)
        {
        case TT_VEHICLE:
            if (p_thing->SubType == SubTT_VEH_SHIP) {
                p_thing->LinkSame = same_type_head[5];
                same_type_head[5] = thing;
            } else {
                p_thing->LinkSame = same_type_head[2];
                same_type_head[2] = thing;
            }
            break;
        case TT_PERSON:
        case TT_UNKN4:
            if ((p_thing->Flag2 & (TgF2_ExistsOffMap|TgF2_AlteredSubType)) == 0 || link_all_people)
            {
                ubyte group;

                p_thing->LinkSame = same_type_head[1];
                same_type_head[1] = thing;

                group = p_thing->U.UPerson.Group & PEOPLE_GROUPS_INDEX_MASK;
                p_thing->LinkSameGroup = same_type_head[256 + group];
                same_type_head[256 + group] = thing;
            }
            break;
        case TT_BUILDING:
            if (p_thing->SubType == SubTT_BLD_MGUN) {
                p_thing->LinkSame = same_type_head[7];
                same_type_head[7] = thing;
            } else {
                p_thing->LinkSame = same_type_head[3];
                same_type_head[3] = thing;
            }
            break;
        case TT_MINE:
            if (p_thing->SubType == 48) {
                p_thing->LinkSame = same_type_head[6];
                same_type_head[6] = thing;
            }
            break;
        default:
            break;
        }
    }
}

short get_thing_same_type_head(short ttype, short subtype)
{
    ThingIdx thing;

    switch (ttype)
    {
    case TT_PERSON:
    case TT_UNKN4:
        thing = same_type_head[1];
        break;
    case TT_VEHICLE:
        if (subtype == SubTT_VEH_SHIP)
            thing = same_type_head[5];
        else
            thing = same_type_head[2];
        break;
    case TT_BUILDING:
        if (subtype == SubTT_BLD_MGUN)
            thing = same_type_head[7];
        else
            thing = same_type_head[3];
        break;
    case TT_MINE:
        if (subtype == 48)
            thing = same_type_head[6];
        else
            thing = 0;
        break;
    default:
        thing = 0;
        break;
    }
    return thing;
}

void new_thing_traffic_clone(struct SimpleThing *p_clsthing)
{
    asm volatile ("call ASM_new_thing_traffic_clone\n"
        : : "a" (p_clsthing));
}

short new_thing_smoke_gen_clone(struct SimpleThing *p_clsthing)
{
    struct SimpleThing *p_sthing;
    ThingIdx thing;

    thing = add_static(PRCCOORD_TO_MAPCOORD(p_clsthing->X), p_clsthing->Y,
      PRCCOORD_TO_MAPCOORD(p_clsthing->Z), 0, p_clsthing->Timer1);
    p_sthing = &sthings[thing];
    p_sthing->Type = SmTT_SMOKE_GENERATOR;
    p_sthing->U.UEffect.VX = p_clsthing->U.UEffect.VX;
    p_sthing->U.UEffect.VY = p_clsthing->U.UEffect.VY;
    p_sthing->U.UEffect.VZ = p_clsthing->U.UEffect.VZ;
    p_sthing->StartTimer1 = p_clsthing->StartTimer1;
    p_sthing->U.UEffect.OX = p_clsthing->U.UEffect.OX;
    p_sthing->U.UEffect.OY = p_clsthing->U.UEffect.OY;
    p_sthing->U.UEffect.OZ = p_clsthing->U.UEffect.OZ;
    return thing;
}

short new_thing_static_clone(struct SimpleThing *p_clsthing)
{
    struct SimpleThing *p_sthing;
    ThingIdx thing;
    ushort frame;

    thing = add_static(PRCCOORD_TO_MAPCOORD(p_clsthing->X), p_clsthing->Y,
      PRCCOORD_TO_MAPCOORD(p_clsthing->Z), p_clsthing->StartFrame + 1, p_clsthing->Timer1);
    p_sthing = &sthings[thing];
    p_sthing->U.UEffect.VZ = p_clsthing->U.UEffect.VZ;
    frame = p_sthing->StartFrame;
    if (frame != 999 && frame != 1002 && frame != 1004 && frame != 1008 &&
      frame != 1032 && frame != 1037 && frame != 1038 && frame != 1050) {
        p_sthing->SubType = 2;
    } else {
        p_sthing->SubType = 1;
        p_sthing->Radius = 128;
    }
    return thing;
}

ThingIdx new_thing_building_clone(struct Thing *p_clthing, struct M33 *p_clmatx, short shut_h)
{
    struct Thing *p_thing;
    struct SingleObject *p_sobj;
    int i;

    p_thing = create_building_thing(PRCCOORD_TO_MAPCOORD(p_clthing->X), p_clthing->Y,
      PRCCOORD_TO_MAPCOORD(p_clthing->Z), p_clthing->U.UObject.Object,
      p_clthing->U.UObject.NumbObjects, p_clthing->ThingOffset);

    p_thing->ThingOffset = p_clthing->ThingOffset;
    p_thing->Flag = p_clthing->Flag;
    p_thing->VX = p_clthing->VX;
    p_thing->VY = p_clthing->VY;
    p_thing->VZ = p_clthing->VZ;
    p_thing->SubType = p_clthing->SubType;

    ubyte styp;
    styp = p_thing->SubType;

    if (styp == SubTT_BLD_MGUN)
    {
        p_thing->U.UMGun.PathIndex = p_clthing->U.UMGun.PathIndex;
        p_thing->U.UMGun.UniqueID = p_clthing->U.UMGun.UniqueID;
        p_thing->U.UMGun.NextThing = p_clthing->U.UMGun.NextThing;
        p_thing->U.UMGun.PrevThing = p_clthing->U.UMGun.PrevThing;
        p_thing->U.UMGun.Token = p_clthing->U.UMGun.Token;
        p_thing->U.UMGun.TokenDir = p_clthing->U.UMGun.TokenDir;
        p_thing->U.UMGun.ObjectNo = p_clthing->U.UMGun.ObjectNo;
        p_thing->U.UMGun.CurrentWeapon = p_clthing->U.UMGun.CurrentWeapon;
    }
    else
    {
        p_thing->U.UObject.Token = p_clthing->U.UObject.Token;
        p_thing->U.UObject.TokenDir = p_clthing->U.UObject.TokenDir;
        p_thing->U.UObject.NextThing = p_clthing->U.UObject.NextThing;
        p_thing->U.UObject.PrevThing = p_clthing->U.UObject.PrevThing;
        p_thing->U.UObject.OffX = p_clthing->U.UObject.OffX;
        p_thing->U.UObject.OffZ = p_clthing->U.UObject.OffZ;
        p_thing->U.UObject.BuildStartVect = p_clthing->U.UObject.BuildStartVect;
        p_thing->U.UObject.BuildNumbVect = p_clthing->U.UObject.BuildNumbVect;
        // Copy all fields from Turn down
        p_thing->U.UObject.Turn = p_clthing->U.UObject.Turn;
        p_thing->U.UObject.TurnPadOnPS = p_clthing->U.UObject.TurnPadOnPS;
        for (i = 0; i < 4; i++) {
            p_thing->U.UObject.tnode[i] = p_clthing->U.UObject.tnode[i];
        }
        p_thing->U.UObject.player_in_me = p_clthing->U.UObject.player_in_me;
        p_thing->U.UObject.unkn_4D = p_clthing->U.UObject.unkn_4D;
        p_thing->U.UObject.DrawTurn = p_clthing->U.UObject.DrawTurn;
        for (i = 0; i < 4; i++) {
            p_thing->U.UObject.tnode_50[i] = p_clthing->U.UObject.tnode_50[i];
        }
    }

    p_sobj = &game_objects[p_clthing->U.UObject.Object];
    p_thing->U.UObject.MinY[0] = p_sobj->OffsetY - 500;
    p_thing->U.UObject.MaxY[0] = p_sobj->OffsetY;
    
    if (styp == SubTT_BLD_SHUTLDR)
    {
        if (((p_thing->Flag & TngF_Unkn0001) == 0)
          && (p_thing->U.UObject.PrevThing == 0 || p_thing->U.UObject.NextThing == 0))
        {
            p_thing->VX = p_thing->X >> 16;
            p_thing->VZ = p_thing->Z >> 16;
            p_thing->Flag |=  TngF_Unkn0001;
        }
        if (shut_h < 15)
            p_thing->Y >>= 3;
    }
    else if (styp == SubTT_BLD_MGUN)
    {
        assert(next_local_mat < LOCAL_MATS_COUNT);
        p_thing->U.UMGun.MatrixIndex = next_local_mat++;
        if (p_clmatx != NULL)
            memcpy(&local_mats[p_thing->U.UMGun.MatrixIndex], p_clmatx, sizeof(struct M33));
        else
            matrix_identity_fill(&local_mats[p_thing->U.UMGun.MatrixIndex]);
        p_thing->U.UMGun.AngleX = 1024;
        p_thing->U.UMGun.AngleY = 0;
        p_thing->Radius = 256;
        p_thing->U.UMGun.RecoilTimer = 0;
        p_thing->U.UMGun.MaxHealth = 6000;
        p_thing->U.UMGun.RecoilTimer = 0;
        p_thing->Health = p_thing->U.UMGun.MaxHealth;
    }
    else if (styp >= SubTT_BLD_MOVN_ROTOR && styp <= SubTT_BLD_37)
    {
        assert(next_local_mat < LOCAL_MATS_COUNT);
        p_thing->U.UObject.MatrixIndex = next_local_mat++;
        if (p_clmatx != NULL)
            memcpy(&local_mats[p_thing->U.UObject.MatrixIndex], p_clmatx, sizeof(struct M33));
        else
            matrix_identity_fill(&local_mats[p_thing->U.UObject.MatrixIndex]);
        p_thing->Flag |= TngF_SelectedAgent;
    }
    p_thing->SubState = p_clthing->SubState;
    p_thing->Timer1 = p_clthing->Timer1;

    // Should be have a separate UGate struct?
    if (styp >= SubTT_BLD_GATE && styp <= SubTT_BLD_26
      && p_thing->U.UObject.MinY[0] == *(ushort *)&p_thing->U.UObject.Group )
    {
        p_thing->U.UObject.MinY[0] = -500;
        p_thing->U.UObject.MaxY[0] = 0;
    }
    p_thing->State = p_clthing->State;
    if (p_thing->State == 9)
        p_thing->State = 0;
    p_thing->Frame = p_clthing->Frame;

    return p_thing->ThingOffset;
}

/** Maps fields from old Thing struct to the current one.
 */
void refresh_old_thing_format(struct Thing *p_thing, struct ThingOldV9 *p_oldthing, u32 fmtver)
{
    ushort len;

    LbMemorySet(p_thing, 0, sizeof(struct Thing));

    p_thing->Parent = p_oldthing->Parent;
    p_thing->Next = p_oldthing->Next;
    p_thing->LinkParent = p_oldthing->LinkParent;
    p_thing->LinkChild = p_oldthing->LinkChild;
    p_thing->SubType = p_oldthing->SubType;
    p_thing->Type = p_oldthing->Type;
    p_thing->State = p_oldthing->State;
    p_thing->Flag = p_oldthing->Flag;
    p_thing->LinkSame = p_oldthing->LinkSame;
    p_thing->Radius = p_oldthing->Radius;
    p_thing->X = p_oldthing->X;
    p_thing->Y = p_oldthing->Y;
    p_thing->Z = p_oldthing->Z;
    p_thing->Frame = p_oldthing->Frame;
    p_thing->StartFrame = p_oldthing->StartFrame;
    p_thing->Timer1 = p_oldthing->Timer1;
    p_thing->StartTimer1 = p_oldthing->StartTimer1;
    p_thing->LinkSameGroup = p_oldthing->LinkSameGroup;
    p_thing->ThingOffset = p_oldthing->ThingOffset;
    p_thing->VX = p_oldthing->VX;
    p_thing->VY = p_oldthing->VY;
    p_thing->VZ = p_oldthing->VZ;
    p_thing->Speed = p_oldthing->Speed;
    p_thing->Health = p_oldthing->Health;
    p_thing->Owner = p_oldthing->Owner;
    p_thing->PathOffset = p_oldthing->PathOffset;
    // The target pointer will be cleared anyway
    p_thing->PTarget = NULL; // p_oldthing->PTarget;
    p_thing->GotoThingIndex = p_oldthing->GotoThingIndex;
    p_thing->SubState = p_oldthing->SubState;

    // Type-dependent fields
    if (p_thing->Type == TT_PERSON)
    {
        p_thing->U.UPerson.AnimMode = p_oldthing->PersonAnimMode;
        p_thing->U.UPerson.OldAnimMode = p_oldthing->PersonOldAnimMode;
        p_thing->U.UPerson.Timer2 = p_oldthing->Timer2;
        p_thing->U.UPerson.StartTimer2 = p_oldthing->StartTimer2;
        p_thing->U.UPerson.Angle = p_oldthing->PersonAngle;
        p_thing->U.UPerson.GotoX = p_oldthing->PersonGotoX;
        p_thing->U.UPerson.GotoZ = p_oldthing->PersonGotoZ;
        p_thing->U.UPerson.Group = p_oldthing->PersonGroup;
        p_thing->U.UPerson.EffectiveGroup = p_oldthing->PersonEffectiveGroup;
        p_thing->U.UPerson.WeaponsCarried = p_oldthing->PersonWeaponsCarried;
        p_thing->U.UPerson.CurrentWeapon = p_oldthing->PersonCurrentWeapon;
        p_thing->U.UPerson.ComHead = p_oldthing->PersonComHead;
        p_thing->U.UPerson.ComCur = p_oldthing->PersonComCur;
        p_thing->U.UPerson.ComTimer = p_oldthing->PersonComTimer;
        p_thing->U.UPerson.Brightness = p_oldthing->PersonBrightness;
        p_thing->U.UPerson.MaxShieldEnergy = p_oldthing->PersonMaxShieldEnergy;
        p_thing->U.UPerson.UniqueID = p_oldthing->PersonUniqueID;
        p_thing->U.UPerson.PathIndex = p_oldthing->PersonPathIndex;
        p_thing->U.UPerson.WeaponTimer = p_oldthing->PersonWeaponTimer;
        p_thing->U.UPerson.WeaponTurn = p_oldthing->PersonWeaponTurn;
        p_thing->U.UPerson.Energy = p_oldthing->PersonEnergy;
        p_thing->U.UPerson.MaxEnergy = p_oldthing->PersonMaxEnergy;
        // Uncertain fields
        p_thing->U.UPerson.ShieldEnergy = p_oldthing->PersonShieldEnergy;
        p_thing->U.UPerson.SpecialTimer = p_oldthing->PersonSpecialTimer;
        p_thing->U.UPerson.ComRange = p_oldthing->PersonComRange;
        p_thing->U.UPerson.BumpMode = p_oldthing->PersonBumpMode;
        p_thing->U.UPerson.BumpCount = p_oldthing->PersonBumpCount;
        p_thing->U.UPerson.Vehicle = p_oldthing->PersonVehicle;
        p_thing->U.UPerson.LinkPassenger = p_oldthing->PersonLinkPassenger;
        p_thing->U.UPerson.Within = p_oldthing->PersonWithin;
        p_thing->U.UPerson.LastDist = p_oldthing->PersonLastDist;
        p_thing->U.UPerson.OnFace = p_oldthing->PersonOnFace;
        // End of uncertain fields
        p_thing->U.UPerson.ComRange = p_oldthing->PersonComRange;
        p_thing->U.UPerson.Shadows[0] = p_oldthing->PersonShadows[0];
        p_thing->U.UPerson.Shadows[1] = p_oldthing->PersonShadows[1];
        p_thing->U.UPerson.Shadows[2] = p_oldthing->PersonShadows[2];
        p_thing->U.UPerson.Shadows[3] = p_oldthing->PersonShadows[3];
        p_thing->U.UPerson.RecoilTimer = p_oldthing->PersonRecoilTimer;
        p_thing->U.UPerson.MaxHealth = p_oldthing->PersonMaxHealth;
        p_thing->U.UPerson.RecoilDir = p_oldthing->PersonRecoilDir;
        // Having cybernetic mods is kind of important; but we do not have location of these
        //p_thing->U.UPerson.UMod.Mods = p_oldthing->?;
        if (fmtver >= 12) {
            p_thing->U.UPerson.Stamina = p_oldthing->PersonStamina;
            p_thing->U.UPerson.MaxStamina = p_oldthing->PersonMaxStamina;
        }
        // Field FrameId
        if (p_thing->SubType == SubTT_PERS_PUNK_F) {
            // Randomize hair color - 50% normal red(0), 25% blonde(1), 25% blue(2)
            // The ThingOffset should be random enough
            len = (p_thing->ThingOffset ^ p_thing->U.UPerson.UniqueID) & 0xF;
            if (len < 4)
                p_thing->U.UPerson.FrameId.Version[0] = 1;
            else if (len < 8)
                p_thing->U.UPerson.FrameId.Version[0] = 2;
        }
        if ((fmtver < 8) && (p_thing->U.UPerson.Group == 0)) {
            // Some very old formats may have effective group set and normal zeroed out
            p_thing->U.UPerson.Group = p_thing->U.UPerson.EffectiveGroup;
        }
        // The current weapon should always be in a list of carried wepons (issue mostly for fmtver < 5)
        if (p_thing->U.UPerson.CurrentWeapon > 0)
            p_thing->U.UPerson.WeaponsCarried |= (1 << (p_thing->U.UPerson.CurrentWeapon - 1));
    }
    else if (p_thing->Type == TT_VEHICLE)
    {
        p_thing->U.UVehicle.Object = p_oldthing->VehicleObject;
        p_thing->U.UVehicle.NumbObjects = p_oldthing->VehicleNumbObjects;
        p_thing->U.UVehicle.Timer2 = p_oldthing->Timer2;
        p_thing->U.UVehicle.AngleX = p_oldthing->VehicleAngleX;
        p_thing->U.UVehicle.AngleY = p_oldthing->VehicleAngleY;
        p_thing->U.UVehicle.AngleZ = p_oldthing->VehicleAngleZ;
        p_thing->U.UVehicle.GotoX = p_oldthing->VehicleGotoX;
        p_thing->U.UVehicle.GotoY = p_oldthing->VehicleGotoY;
        p_thing->U.UVehicle.GotoZ = p_oldthing->VehicleGotoZ;
        p_thing->U.UVehicle.MatrixIndex = p_oldthing->VehicleMatrixIndex;
        p_thing->U.UVehicle.UniqueID = p_oldthing->VehicleUniqueID;
        p_thing->U.UVehicle.MaxSpeed = p_oldthing->VehicleReqdSpeed;
        p_thing->U.UVehicle.ReqdSpeed = p_oldthing->VehicleReqdSpeed;
        p_thing->U.UVehicle.PassengerHead = p_oldthing->VehiclePassengerHead;
        p_thing->U.UVehicle.TNode = p_oldthing->VehicleTNode;
        p_thing->U.UVehicle.AngleDY = p_oldthing->VehicleAngleDY;
        p_thing->U.UVehicle.RecoilTimer = p_oldthing->VehicleRecoilTimer;
        // Only one group seem to exist in the old vehicles
        p_thing->U.UVehicle.Group = p_oldthing->VehicleEffectiveGroup;
        p_thing->U.UVehicle.EffectiveGroup = p_oldthing->VehicleEffectiveGroup;
        // In old format, MaxHealth is stored in additional vehicle block, not in the thing
        //p_thing->U.UVehicle.MaxHealth = ?;
    }
    else if (p_thing->Type == TT_BUILDING)
    {
        p_thing->U.UObject.Object = p_oldthing->ObjectObject;
        p_thing->U.UObject.NumbObjects = p_oldthing->ObjectNumbObjects;
        p_thing->U.UObject.Timer[0] = p_oldthing->ObjectTimer0;
        p_thing->U.UObject.Timer[1] = p_oldthing->ObjectTimer1;
        p_thing->U.UObject.Angle = p_oldthing->ObjectAngle;
        p_thing->U.UObject.TargetDX = p_oldthing->ObjectTargetDX;
        p_thing->U.UObject.TargetDY = p_oldthing->ObjectTargetDY;
        p_thing->U.UObject.TargetDZ = p_oldthing->ObjectTargetDZ;
        // Only one group seem to exist in the old Objects
        p_thing->U.UObject.Group = p_oldthing->ObjectEffectiveGroup;
        p_thing->U.UObject.EffectiveGroup = p_oldthing->ObjectEffectiveGroup;
        if (p_thing->SubType == SubTT_BLD_GATE) {
            p_thing->U.UObject.RaiseDY[0] = p_oldthing->ObjectRaiseDY[0];
            p_thing->U.UObject.RaiseDY[1] = p_oldthing->ObjectRaiseDY[1];
            p_thing->VX = 0;
            p_thing->VY = 0;
            p_thing->VZ = 0;
            p_thing->U.UObject.MinY[0] = p_oldthing->ObjectMinY0;
            p_thing->U.UObject.RaiseY[1] = p_oldthing->ObjectRaiseY1;
            p_thing->Frame = 0;
            p_thing->StartFrame = 0;
        }
        else if (p_thing->SubType == SubTT_BLD_MGUN) {
            p_thing->U.UMGun.Object = p_oldthing->MGunObject;
            p_thing->U.UMGun.NumbObjects = p_oldthing->MGunNumbObjects;
            p_thing->U.UMGun.AngleX = p_oldthing->MGunAngleX;
            p_thing->U.UMGun.AngleY = p_oldthing->MGunAngleY;
            p_thing->U.UMGun.AngleZ = p_oldthing->MGunAngleZ;
            p_thing->U.UMGun.GotoX = p_oldthing->MGunGotoX;
            p_thing->U.UMGun.GotoY = p_oldthing->MGunGotoY;
            p_thing->U.UMGun.GotoZ = p_oldthing->MGunGotoZ;
            p_thing->U.UMGun.UniqueID = p_oldthing->MGunUniqueID;
            // Only one group seem to exist in the old MGun
            p_thing->U.UMGun.Group = p_oldthing->MGunEffectiveGroup;
            p_thing->U.UMGun.EffectiveGroup = p_oldthing->MGunEffectiveGroup;
        }
    }
}

#ifdef SW_FNCHK
static struct SimpleThing *create_item_asm(int x, int y, int z, ushort frame, ubyte subtype)
{
    struct SimpleThing *ret;
    asm volatile (
      "push %5\n"
      "call ASM_create_item\n"
        : "=a" (ret) : "a" (x), "d" (y), "b" (z), "c" (frame), "g" ((u32)subtype)
        : "cc", "memory");
    return ret;
}
#endif

/** Creates an item lying on the ground (a dropped weapon of type subtype).
 * The sprite comes from the item type; frame only sets the start frame
 * first.  Remade from create_item.
 */
static struct SimpleThing *create_item_c(int x, int y, int z, ushort frame, ubyte subtype)
{
    struct SimpleThing *p_sthing;
    short thing;
    ushort anim;

    if (((u32)x >= 0x8000) || ((u32)z >= 0x8000))
        return NULL;
    thing = get_new_sthing();
    if ((thing > -1) || (thing < -1500))
        return NULL;
    p_sthing = &sthings[thing];
    p_sthing->StartTimer1 = 5;
    p_sthing->Timer1 = 5;
    p_sthing->X = x << 8;
    p_sthing->Z = z << 8;
    p_sthing->Y = y << 8;
    p_sthing->Type = SmTT_DROPPED_ITEM;
    p_sthing->U.UWeapon.OnFace = 0;
    p_sthing->U.UWeapon.Ammo = 0;
    p_sthing->Radius = 30;
    p_sthing->Frame = nstart_ani[frame];
    p_sthing->StartFrame = frame - 1;
    p_sthing->U.UWeapon.WeaponType = subtype;
    switch (subtype)
    {
    case 0:
        anim = 0x423;
        break;
    case 2: case 3: case 4: case 8: case 15: case 17: case 20:
        anim = 0x420;
        break;
    case 6: case 11: case 13:
        anim = 0x440;
        break;
    case 10: case 12:
        anim = 0x441;
        break;
    default:
        anim = 0x3EC;
        break;
    }
    p_sthing->Frame = nstart_ani[anim];
    p_sthing->StartFrame = anim - 1;
    ingame.fld_unkCB7++;
    add_node_sthing(thing);
    p_sthing->Flag = TngF_Unkn0004;
    p_sthing->SubType = subtype;
    return p_sthing;
}

struct SimpleThing *create_item(int x, int y, int z, ushort frame, ubyte subtype)
{
    struct SimpleThing *ret;
    FNCHK_RET("create_item", ret, create_item_asm(x, y, z, frame, subtype),
      create_item_c(x, y, z, frame, subtype));
    return ret;
}

#ifdef SW_FNCHK
static struct SimpleThing *create_scale_effect_asm(int x, int y, int z, ushort frame, short timer)
{
    struct SimpleThing *ret;
    asm volatile (
      "push %5\n"
      "call ASM_create_scale_effect\n"
        : "=a" (ret) : "a" (x), "d" (y), "b" (z), "c" (frame), "g" ((s32)timer)
        : "cc", "memory");
    return ret;
}
#endif

/** Creates a growing sprite effect (smoke, flash) at map coordinates.
 * Remade from create_scale_effect.
 */
static struct SimpleThing *create_scale_effect_c(int x, int y, int z, ushort frame, short timer)
{
    struct SimpleThing *p_sthing;
    short thing;

    if ((x < 0) || (x >= 0x8000))
        return NULL;
    if ((z < 0) || (z >= 0x8000))
        return NULL;
    if (sthings_used > 0x5D7)
        return NULL;
    thing = get_new_sthing();
    if (thing == 0)
        return NULL;
    if ((thing > -1) || (thing < -1500))
        return NULL;
    p_sthing = &sthings[thing];
    p_sthing->Z = z << 8;
    p_sthing->X = x << 8;
    p_sthing->Y = y << 8;
    p_sthing->Frame = nstart_ani[frame];
    p_sthing->StartFrame = frame - 1;
    add_node_sthing(thing);
    p_sthing->Type = SmTT_SCALE_EFFECT;
    p_sthing->SubType = 0;
    p_sthing->Radius = 64;
    p_sthing->Flag = TngF_Unkn0004;
    p_sthing->Timer1 = timer;
    return p_sthing;
}

struct SimpleThing *create_scale_effect(int x, int y, int z, ushort frame, short timer)
{
    struct SimpleThing *ret;
    FNCHK_RET("create_scale_effect", ret, create_scale_effect_asm(x, y, z, frame, timer),
      create_scale_effect_c(x, y, z, frame, timer));
    return ret;
}

struct SimpleThing *create_sound_effect(int x, int y, int z, ushort sample, int vol, int loop)
{
    struct SimpleThing *ret;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[2];

    stkargs[0] = (int)(intptr_t)vol;
    stkargs[1] = (int)(intptr_t)loop;

    asm volatile (
      "push 4(%5)\n"
      "push 0(%5)\n"
      "call ASM_create_sound_effect\n"
        : "=r" (ret)
        : "a" (x), "d" (y), "b" (z), "c" (sample), "S" (stkargs)
        : "cc", "memory");
    return ret;
}

struct SimpleThing *create_stasis_pod(MapCoord x, MapCoord y, MapCoord z,
  ushort timer, struct Thing *p_owner)
{
    struct SimpleThing *ret;
    asm volatile (
      "push %5\n"
      "call ASM_create_stasis_pod\n"
        : "=r" (ret) : "a" (x), "d" (y), "b" (z), "c" (timer), "g" (p_owner));
    return ret;
}

struct SimpleThing *create_time_pod(MapCoord x, MapCoord y, MapCoord z,
  ushort timer)
{
#if 0
    struct SimpleThing *ret;
    asm volatile (
      "call ASM_create_time_pod\n"
        : "=r" (ret) : "a" (x), "d" (y), "b" (z), "c" (timer));
    return ret;
#endif
    struct SimpleThing *p_podtng;
    ThingIdx thing;

    if (sthings_used > STHINGS_LIMIT - 5) {
        return NULL;
    }
    if ( x < 0 || z < 0 )
        return NULL;
    thing = get_new_sthing();
    if (thing == 0) {
        return NULL;
    }
    if (thing <= -STHINGS_LIMIT-1) {
        return NULL;
    }
    p_podtng = &sthings[thing];
    p_podtng->Radius = 0;
    p_podtng->StartFrame = 0;
    p_podtng->Type = 30;
    p_podtng->State = 0;
    p_podtng->X = MAPCOORD_TO_PRCCOORD(x, 0);
    p_podtng->Z = MAPCOORD_TO_PRCCOORD(z, 0);
    p_podtng->Y = MAPCOORD_TO_PRCCOORD(y, 0);
    p_podtng->Timer1 = timer;
    add_node_sthing(thing);
    return p_podtng;
}

struct SimpleThing *create_electric_strand(MapCoord x, MapCoord y, MapCoord z,
  MapCoord x2, MapCoord y2, MapCoord z2, int sound)
{
#if 1
    struct SimpleThing *ret;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[3];

    stkargs[0] = (int)(intptr_t)y2;
    stkargs[1] = (int)(intptr_t)z2;
    stkargs[2] = (int)(intptr_t)sound;

    asm volatile (
      "push 8(%5)\n"
      "push 4(%5)\n"
      "push 0(%5)\n"
      "call ASM_create_electric_strand\n"
        : "=r" (ret)
        : "a" ((s32)x), "d" ((s32)y), "b" ((s32)z), "c" ((s32)x2), "S" (stkargs)
        : "cc", "memory");
    return ret;
#endif
}

void mine_detonate(struct Thing *p_thing)
{
    play_dist_sample(p_thing, 37, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
    bang_new4(p_thing->X, p_thing->Y, p_thing->Z, 20);
    p_thing->State = 13;
    p_thing->Flag |= TngF_Destroyed;
    set_thing_frame(p_thing, 1069);
}

int mine_hit_by_bullet(struct Thing *p_thing, short hp,
  int vx, int vy, int vz, struct Thing *p_attacker, ushort type)
{
    if (p_thing->State == 13) {
        return 0;
    }
    p_thing->Health -= hp;
    if (p_thing->Health <= 0) {
        mine_detonate(p_thing);
        return p_thing->Health + hp;
    }
    return 0;
}

int static_hit_by_bullet(struct SimpleThing *p_sthing, short hp,
  int vx, int vy, int vz, struct Thing *p_attacker, ushort type)
{
    p_sthing->U.UScenery.Health -= hp;
    if (p_sthing->U.UScenery.Health <= 0) {
        set_static_on_fire(p_sthing);
    }
    return 100;
}

struct Thing *replace_thing_given_thing_idx(int x, int y, int z, short owner, ThingIdx pv_thing)
{
    struct Thing *p_pv_thing;
    struct Thing *p_nx_thing;

    p_pv_thing = &things[pv_thing];
    if (on_mapwho(p_pv_thing))
        delete_node(p_pv_thing);
    unkn01_thing_idx = pv_thing;
    p_nx_thing = new_sim_person(x, y, z, 101u);
    p_nx_thing->State = 0;
    p_nx_thing->Owner = owner;
    return p_nx_thing;
}

/******************************************************************************/
