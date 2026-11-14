/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file bmcrater.c
 *     On-map explosion effect implementation.
 * @par Purpose:
 *     Implement functions displaying an explosion within the game world.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     19 Sep 2023 - 17 Mar 2024
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "bmcrater.h"

#include <string.h>
#include "lvdraw3d.h"

#include "bfutility.h"

#include "bigmap.h"
#include "game.h"
#include "swlog.h"
#include "fncheck.h"
#include "people.h"
#include "thing.h"
#include "thing_fire.h"
#include "vehicle.h"
/******************************************************************************/

#define MAP_CRATERS_COUNT 128

/******************************************************************************/

extern ubyte map_crater_first;
extern ubyte map_crater_next;
extern struct MapCreater map_craters[MAP_CRATERS_COUNT];

struct CraterTileSpec {
    sbyte dx, dz;
    ubyte depth;
    TbBool gated; /**< If true, the tile only has 50% chance of being stamped. */
};

/** size=0: only the center tile, always stamped. */
static const struct CraterTileSpec crater_tiles_size0[] = {
  { 0,  0, 0, false},
};

/** size=1: center plus the 4 orthogonal neighbours, each with 50% chance. */
static const struct CraterTileSpec crater_tiles_size1[] = {
  { 0,  0, 1, false},
  { 0, -1, 0, true},
  { 0,  1, 0, true},
  {-1,  0, 0, true},
  { 1,  0, 0, true},
};

/** size=2: center, the 4 orthogonal neighbours (always), and the 4 diagonal
 * neighbours (each with 50% chance).
 */
static const struct CraterTileSpec crater_tiles_size2[] = {
  { 0,  0, 2, false},
  { 0, -1, 1, false},
  { 0,  1, 1, false},
  {-1,  0, 1, false},
  { 1,  0, 1, false},
  {-1, -1, 0, true},
  { 1, -1, 0, true},
  {-1,  1, 0, true},
  { 1,  1, 0, true},
};

/** size=3: center, the 4 orthogonal and 4 diagonal neighbours (always), and
 * the 12 tiles of the distance-2 ring, where the corner tiles of each ring
 * edge have a 50% chance and the middle tile of each edge is always stamped.
 */
static const struct CraterTileSpec crater_tiles_size3[] = {
  { 0,  0, 3, false},
  { 0, -1, 2, false},
  { 0,  1, 2, false},
  {-1,  0, 2, false},
  { 1,  0, 2, false},
  {-1, -1, 1, false},
  { 1, -1, 1, false},
  {-1,  1, 1, false},
  { 1,  1, 1, false},
  {-2, -1, 0, true},
  {-2,  0, 0, false},
  {-2,  1, 0, true},
  { 2, -1, 0, true},
  { 2,  0, 0, false},
  { 2,  1, 0, true},
  {-1, -2, 0, true},
  { 0, -2, 0, false},
  { 1, -2, 0, true},
  {-1,  2, 0, true},
  { 0,  2, 0, false},
  { 1,  2, 0, true},
};

/******************************************************************************/

void create_crater(short tile_x, short tile_z, short depth)
{
    int cratr_no;

    LOGSYNC("crater at (%d,%d) depth %d\n", tile_x, tile_z, depth);
    if (tile_x < 0 || tile_x >= MAP_TILE_WIDTH)
        return;
    if (tile_z < 0 || tile_z >= MAP_TILE_HEIGHT)
        return;
    cratr_no = map_crater_next;
    map_crater_next++;
    if (map_crater_next >= MAP_CRATERS_COUNT)
        map_crater_next = 0;
    map_craters[cratr_no].MapX = tile_x;
    map_craters[cratr_no].MapY = tile_z;
    map_craters[cratr_no].Depth = depth;
    map_craters[cratr_no].Iterations = 0;
}

/** Stamps a single crater tile.
 *
 * Randomizes ground altitude and marks the tile with the crater texture,
 * unless it is a fluid surface, or (for gated tiles) a random chance
 * for damage does not land.
 */
static void quick_crater_stamp_tile(int x, int z, int depth, TbBool gated,
  int *min_x, int *max_x, int *min_z, int *max_z)
{
    struct MyMapElement *p_mapel;
    ushort textr;

    if (gated && ((LbRandomAnyShort() & 1) == 0))
        return;
    if ((x < 0) || (x > MAP_TILE_WIDTH - 1))
        return;
    if ((z < 0) || (z > MAP_TILE_HEIGHT - 1))
        return;

    p_mapel = &game_my_big_map[MAP_TILE_WIDTH * z + x];
    textr = p_mapel->Texture & 0x3FFF;
    if (floor_texture_is_fluid(textr))
        return;

    p_mapel->Texture = (p_mapel->Texture & 0xC000) | crater_unkn_texture_flags;
    p_mapel->ColumnHead &= COL_COLUMN_MASK; // clear TearAround, shared zip-pass will fill it in
    p_mapel->Alt -= (short)(2 * depth + (LbRandomAnyShort() & 3));
    floor_alt_grid_lower(x, z, p_mapel->Alt);

    if (x < *min_x) *min_x = x;
    if (x > *max_x) *max_x = x;
    if (z < *min_z) *min_z = z;
    if (z > *max_z) *max_z = z;
}

static void quick_crater_stamp_shape(int cx, int cz, const struct CraterTileSpec *tiles, int count,
  int *min_x, int *max_x, int *min_z, int *max_z)
{
    int i;
    for (i = 0; i < count; i++)
    {
        quick_crater_stamp_tile(cx + tiles[i].dx, cz + tiles[i].dz, tiles[i].depth, tiles[i].gated,
          min_x, max_x, min_z, max_z);
    }
}

/** Digs a crater of given size (0..3) centered at map tile (x,z).
 *
 * Randomizes ground altitude and sets the crater texture/flags on the
 * stamped tiles (a shape depending on `size`, with most non-center tiles
 * having only 50% chance to be stamped), then smooths the result: tiles
 * neighbouring the crater on at least 3 of their 4 orthogonal sides are
 * absorbed into the crater, and a marching-squares pass selects a
 * TearAround (diagonal/edge blend) value for every non-crater tile
 * touching the crater, to avoid harsh texture transitions.
 */
void quick_crater(int tl_cent_x, int tl_cent_z, int size)
{
#if 0
    asm volatile ("call ASM_quick_crater\n"
        :  : "a" (tl_cent_x), "d" (tl_cent_z), "b" (size));
    return;
#endif
    int min_x, max_x, min_z, max_z;
    int tl_x, tl_z;

    if (current_map == 11) // map011 Orbital Station
        return;
    if ((size < 0) || (size > 3)) {
        LOGWARN("crater at (%d,%d) has invalid size %d\n", tl_cent_x, tl_cent_z, size);
        return;
    }

    min_x = max_x = tl_cent_x;
    min_z = max_z = tl_cent_z;

    switch (size)
    {
    case 0:
        quick_crater_stamp_shape(tl_cent_x, tl_cent_z, crater_tiles_size0,
          sizeof(crater_tiles_size0) / sizeof(crater_tiles_size0[0]),
          &min_x, &max_x, &min_z, &max_z);
        break;
    case 1:
        quick_crater_stamp_shape(tl_cent_x, tl_cent_z, crater_tiles_size1,
          sizeof(crater_tiles_size1) / sizeof(crater_tiles_size1[0]),
          &min_x, &max_x, &min_z, &max_z);
        break;
    case 2:
        quick_crater_stamp_shape(tl_cent_x, tl_cent_z, crater_tiles_size2,
          sizeof(crater_tiles_size2) / sizeof(crater_tiles_size2[0]),
          &min_x, &max_x, &min_z, &max_z);
        break;
    case 3:
        quick_crater_stamp_shape(tl_cent_x, tl_cent_z, crater_tiles_size3,
          sizeof(crater_tiles_size3) / sizeof(crater_tiles_size3[0]),
          &min_x, &max_x, &min_z, &max_z);
        break;
    }

    min_x--; max_x++;
    min_z--; max_z++;
    if (min_x < 0) min_x = 0;
    if (max_x > MAP_TILE_WIDTH - 1) max_x = MAP_TILE_WIDTH - 1;
    if (min_z < 0) min_z = 0;
    if (max_z > MAP_TILE_HEIGHT - 1) max_z = MAP_TILE_HEIGHT - 1;

    // Pass 1: absorb tiles bordering the crater on at least 3 of 4 sides
    // into the crater, to get rid of thin un-cratered slivers.
    for (tl_z = min_z; tl_z <= max_z; tl_z++)
    {
        for (tl_x = min_x; tl_x <= max_x; tl_x++)
        {
            ushort own_tex;
            TbBool match_e, match_w, match_s, match_n;

            own_tex = floor_texture_at_tile(tl_x, tl_z);
            if (own_tex == crater_unkn_texture_flags)
                continue;

            match_e = (floor_texture_at_tile(tl_x + 1, tl_z + 0) == crater_unkn_texture_flags);
            match_w = (floor_texture_at_tile(tl_x - 1, tl_z + 0) == crater_unkn_texture_flags);
            match_s = (floor_texture_at_tile(tl_x + 0, tl_z + 1) == crater_unkn_texture_flags);
            match_n = (floor_texture_at_tile(tl_x + 0, tl_z - 1) == crater_unkn_texture_flags);

            if ((match_e && match_w && match_s) || (match_e && match_w && match_n)
              || (match_w && match_s && match_n) || (match_e && match_s && match_n))
            {
                struct MyMapElement *p_mapel;
                p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tl_z + tl_x];
                p_mapel->Texture = (p_mapel->Texture & 0xC000) | crater_unkn_texture_flags;
                p_mapel->ColumnHead &= COL_COLUMN_MASK;
            }
        }
    }

    // Pass 2: marching-squares style selection of a TearAround (diagonal
    // edge blend) value for tiles neighbouring the crater, based on which
    // of the 8 surrounding tiles are crater texture.
    for (tl_z = min_z; tl_z <= max_z; tl_z++)
    {
        for (tl_x = min_x; tl_x <= max_x; tl_x++)
        {
            ushort own_tex;
            ushort tear;
            TbBool match_nw, match_n, match_ne, match_w, match_e, match_sw, match_s, match_se;

            own_tex = floor_texture_at_tile(tl_x, tl_z);
            if (own_tex == crater_unkn_texture_flags)
                continue;
            if (sea_texture(own_tex))
                continue;

            match_nw = (floor_texture_at_tile(tl_x - 1, tl_z - 1) == crater_unkn_texture_flags);
            match_n  = (floor_texture_at_tile(tl_x + 0, tl_z - 1) == crater_unkn_texture_flags);
            match_ne = (floor_texture_at_tile(tl_x + 1, tl_z - 1) == crater_unkn_texture_flags);
            match_w  = (floor_texture_at_tile(tl_x - 1, tl_z + 0) == crater_unkn_texture_flags);
            match_e  = (floor_texture_at_tile(tl_x + 1, tl_z + 0) == crater_unkn_texture_flags);
            match_sw = (floor_texture_at_tile(tl_x - 1, tl_z + 1) == crater_unkn_texture_flags);
            match_s  = (floor_texture_at_tile(tl_x + 0, tl_z + 1) == crater_unkn_texture_flags);
            match_se = (floor_texture_at_tile(tl_x + 1, tl_z + 1) == crater_unkn_texture_flags);

            tear = 0;
            if (match_e && match_s)
                tear = 12;
            else if (match_n && match_e)
                tear = 9;
            else if (match_w && match_n)
                tear = 6;
            else if (match_s && match_w)
                tear = 3;
            else if (match_s)
                tear = 1;
            else if (match_e)
                tear = 10;
            else if (match_n)
                tear = 7;
            else if (match_w)
                tear = 4;
            else if (match_se)
                tear = 11;
            else if (match_ne)
                tear = 8;
            else if (match_nw)
                tear = 5;
            else if (match_sw)
                tear = 2;

            if (tear != 0) {
                struct MyMapElement *p_mapel;
                p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tl_z + tl_x];
                p_mapel->ColumnHead = (p_mapel->ColumnHead & COL_COLUMN_MASK) | (tear << 12);
            }
        }
    }
}

#ifdef SW_FNCHK
static void process_map_crater_asm(ubyte crater)
{
    asm volatile ("call ASM_process_map_crater\n"
        : : "a" ((u32)crater) : "cc", "memory");
}
#endif

/** True when the tile's texture (flags removed) is the crater texture. */
static TbBool crater_tile_is_crater(struct MyMapElement *p_mapel)
{
    return ((p_mapel->Texture & 0x3FFF) == crater_unkn_texture_flags);
}

/** Puts the crater texture on the tile and clears its TearAround. */
static void crater_tile_set(struct MyMapElement *p_mapel)
{
    p_mapel->Texture &= 0xC000;
    p_mapel->Texture |= crater_unkn_texture_flags;
    p_mapel->ColumnHead &= COL_COLUMN_MASK;
}

/** Tries to spread the crater from the current tile to the neighbour tile
 * `next`: a random chance below `thr`; the neighbour gets level `lvl`. */
static TbBool crater_spread_to(short *queue, ubyte *level, ushort *p_tail, short next,
  ubyte lvl, short cur, short low, short thr)
{
    int r;

    r = (int)(ushort)LbRandomAnyShort() % (int)(cur - low);
    if (r >= thr)
        return false;
    queue[*p_tail] = next;
    (*p_tail)++;
    level[next] = lvl;
    return true;
}

/** Progress the crater with given index by one step: for 5 turns it spreads
 * at random from its centre over the tiles around (as far as its depth
 * allows), lowering the ground and texturing it, hurting people and
 * setting fire to statics there; then the borders are smoothed like in
 * quick_crater() (without the sea test).  After 5 turns the crater is done.
 * The scratch buffer holds the queue of tiles (words) and, at 0x8000,
 * the spread level of each tile.  Remade from process_map_crater.
 */
static void process_map_crater_c(ubyte crater)
{
    struct MapCreater *p_crat;
    short *queue;
    ubyte *level;
    ushort head, tail;
    short cur, thr;
    ubyte maxlvl;
    int min_x, max_x, min_z, max_z;
    int tl_x, tl_z;

    p_crat = &map_craters[crater];
    p_crat->Iterations++;
    if (p_crat->Iterations > 5)
    {
        map_crater_first++;
        if ((sbyte)map_crater_first < 0)
            map_crater_first = 0;
        map_craters[crater].Iterations = 0;
        return;
    }
    cur = (short)((p_crat->Depth * p_crat->Iterations) / 5);
    queue = (short *)scratch_buf1;
    level = scratch_buf1 + 0x8000;
    memset(level, 0, 0x4000);
    thr = (short)((3 * cur) / 7);
    maxlvl = (ubyte)(cur / 10);
    if (maxlvl < 3)
        maxlvl = 3;
    min_x = p_crat->MapX - 1;
    max_x = p_crat->MapX + 1;
    min_z = p_crat->MapY - 1;
    max_z = p_crat->MapY + 1;
    if (min_x < 0) min_x = 0;
    if (min_x >= 128) min_x = 127;
    if (max_x < 0) max_x = 0;
    if (max_x >= 128) max_x = 127;
    if (min_z < 0) min_z = 0;
    if (min_z >= 128) min_z = 127;
    if (max_z < 0) max_z = 0;
    if (max_z >= 128) max_z = 127;

    head = 0;
    tail = 0;
    queue[tail] = (short)((p_crat->MapY << 7) + p_crat->MapX);
    tail++;
    level[queue[head]] = 1;
    while (head != tail)
    {
        struct MyMapElement *p_mapel;
        short tile, low;
        ThingIdx thing;
        ubyte lvl;
        int drop;

        tile = queue[head];
        if ((tile < 0) || floor_texture_is_fluid(game_my_big_map[tile].Texture & 0x3FFF)) {
            head++;
            continue;
        }
        p_mapel = &game_my_big_map[tile];
        crater_tile_set(p_mapel);
        lvl = level[tile];
        drop = (short)(map_craters[crater].Depth - (lvl * map_craters[crater].Depth) / maxlvl) / 5;
        drop = ((drop + (ushort)LbRandomAnyShort()) & 0xF) >> 3;
        p_mapel->Alt -= drop;

        for (thing = p_mapel->Child; thing != 0; )
        {
            if (thing > 0)
            {
                struct Thing *p_thing;

                p_thing = &things[thing];
                if (p_thing->Type == TT_PERSON)
                {
                    s32 tx, tz;

                    tx = ((queue[head] % 128) << 8) + 0x80;
                    tz = ((queue[head] / 128) << 8) + 0x80;
                    thing_hit_by_bullet(p_thing, 600, (short)((p_thing->X >> 8) - tx), 0,
                      (short)((p_thing->Z >> 8) - tz), NULL, 9);
                    things[thing].Flag2 &= ~TgF2_InsideBuilding;
                    things[thing].Y = alt_at_point(things[thing].X >> 8, things[thing].Z >> 8);
                }
                thing = things[thing].Next;
            }
            else
            {
                struct SimpleThing *p_sthing;

                p_sthing = &sthings[thing];
                if (can_the_tank_run_over(p_sthing))
                {
                    p_sthing->Y = alt_at_point(p_sthing->X >> 8, p_sthing->Z >> 8);
                    set_static_on_fire(p_sthing);
                }
                thing = sthings[thing].Next;
            }
        }

        low = cur - (lvl * cur) / maxlvl;
        if (low == cur)
            low--;
        if (lvl < maxlvl)
        {
            lvl++;
            tile = queue[head];
            if (((tile % 128) != 0) && (level[tile - 1] == 0) &&
              crater_spread_to(queue, level, &tail, tile - 1, lvl, cur, low, thr))
            {
                if ((queue[(ushort)(tail - 1)] % 128) == min_x) {
                    min_x--;
                    if (min_x < 0) min_x = 0;
                }
            }
            tile = queue[head];
            if (((tile % 128) < 127) && (level[tile + 1] == 0) &&
              crater_spread_to(queue, level, &tail, tile + 1, lvl, cur, low, thr))
            {
                if ((queue[(ushort)(tail - 1)] % 128) == max_x) {
                    max_x++;
                    if (max_x > 127) max_x = 127;
                }
            }
            tile = queue[head];
            if ((tile >= 128) && (level[tile - 128] == 0) &&
              crater_spread_to(queue, level, &tail, tile - 128, lvl, cur, low, thr))
            {
                if ((queue[(ushort)(tail - 1)] / 128) == min_z) {
                    min_z--;
                    if (min_z < 0) min_z = 0;
                }
            }
            tile = queue[head];
            if ((tile < 0x3F80) && (level[tile + 128] == 0) &&
              crater_spread_to(queue, level, &tail, tile + 128, lvl, cur, low, thr))
            {
                if ((queue[(ushort)(tail - 1)] / 128) == max_z) {
                    max_z++;
                    if (max_z > 127) max_z = 127;
                }
            }
        }
        head++;
    }

    // Absorb tiles with crater on at least 3 of the 4 sides
    for (tl_z = min_z; tl_z <= max_z; tl_z++)
    {
        for (tl_x = min_x; tl_x <= max_x; tl_x++)
        {
            struct MyMapElement *p_mapel;
            TbBool e, w, s, n;

            if ((tl_x < 0) || (tl_x >= 128) || (tl_z < 0) || (tl_z >= 128))
                continue;
            p_mapel = &game_my_big_map[(tl_z << 7) + tl_x];
            if (crater_tile_is_crater(p_mapel))
                continue;
            e = crater_tile_is_crater(p_mapel + 1);
            w = crater_tile_is_crater(p_mapel - 1);
            s = crater_tile_is_crater(p_mapel + 128);
            n = crater_tile_is_crater(p_mapel - 128);
            if ((e && w && s) || (e && w && n) || (w && s && n) || (e && s && n))
                crater_tile_set(p_mapel);
        }
    }

    // TearAround (edge blend) for the tiles around the crater
    for (tl_z = min_z; tl_z <= max_z; tl_z++)
    {
        for (tl_x = min_x; tl_x <= max_x; tl_x++)
        {
            struct MyMapElement *p_mapel;
            TbBool nw, n, ne, w, e, sw, s, se;
            ushort tear;

            if ((tl_x < 0) || (tl_x >= 128) || (tl_z < 0) || (tl_z >= 128))
                continue;
            p_mapel = &game_my_big_map[(tl_z << 7) + tl_x];
            if (crater_tile_is_crater(p_mapel))
                continue;
            nw = crater_tile_is_crater(p_mapel - 129);
            n  = crater_tile_is_crater(p_mapel - 128);
            ne = crater_tile_is_crater(p_mapel - 127);
            w  = crater_tile_is_crater(p_mapel - 1);
            e  = crater_tile_is_crater(p_mapel + 1);
            sw = crater_tile_is_crater(p_mapel + 127);
            s  = crater_tile_is_crater(p_mapel + 128);
            se = crater_tile_is_crater(p_mapel + 129);
            if (e && s)
                tear = 12;
            else if (n && e)
                tear = 9;
            else if (w && n)
                tear = 6;
            else if (s && w)
                tear = 3;
            else if (s)
                tear = 1;
            else if (e)
                tear = 10;
            else if (n)
                tear = 7;
            else if (w)
                tear = 4;
            else if (se)
                tear = 11;
            else if (ne)
                tear = 8;
            else if (nw)
                tear = 5;
            else if (sw)
                tear = 2;
            else
                continue;
            p_mapel->ColumnHead = (p_mapel->ColumnHead & COL_COLUMN_MASK) | (tear << 12);
        }
    }
}

void process_map_crater(ubyte crater)
{
    FNCHK_VOID("process_map_crater", process_map_crater_asm(crater),
      process_map_crater_c(crater));
}

#ifdef SW_FNCHK
static void process_map_craters_asm(void)
{
    asm volatile ("call ASM_process_map_craters\n"
        : : : "eax", "cc", "memory");
}
#endif

static void process_map_craters_c(void)
{
    ubyte crater;

    // The loop index wraps at the sign bit, which is MAP_CRATERS_COUNT.
    // process_map_crater() may move map_crater_first; the loop ends at
    // map_crater_next, read again on every step.
    crater = map_crater_first;
    while (crater != map_crater_next)
    {
        process_map_crater(crater);
        crater++;
        if ((sbyte)crater < 0)
            crater = 0;
    }
}

void process_map_craters(void)
{
    FNCHK_VOID("process_map_craters", process_map_craters_asm(),
      process_map_craters_c());
}

/******************************************************************************/
