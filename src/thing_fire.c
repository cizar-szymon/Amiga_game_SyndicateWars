/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file thing_fire.c
 *     Support for creating fires burning on the map.
 * @par Purpose:
 *     Implement creation and updating of the representation of fire.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     19 Aug 2025 - 25 Aug 2025
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "thing_fire.h"

#include <stdlib.h>

#include "bfutility.h"

#include "enginshrapn.h"

#include "bigmap.h"
#include "enginfexpl.h"
#include "sound.h"
#include "swlog.h"
#include "thing.h"
#include "game_speed.h"
#include "lvdraw3d.h"
#include "tngcolisn.h"
#include "people.h"
#include "bfmath.h"
#include "vehicle.h"
#include "frame_sprani.h"
#include "bmbang.h"
#include "enginlights.h"
#include "fncheck.h"

/******************************************************************************/

void FIRE_add_flame(ThingIdx firetng, ushort fflame)
{
    struct SimpleThing *p_fire;
    struct FireFlame *p_fflame;

    if (fflame == 0) {
        return;
    }
    p_fire = &sthings[firetng];
    p_fflame = &FIRE_flame[fflame];

    p_fflame->next = p_fire->U.UFire.flame;
    p_fire->U.UFire.flame = fflame;
}

void FIRE_new(int x, int y, int z, ubyte type)
{
#if 0
    asm volatile ("call ASM_FIRE_new\n"
        : : "a" (x), "d" (y), "b" (z), "c" (type));
#endif
    struct SimpleThing *p_fire;
    struct MyMapElement *p_mapel;
    int cor_x, cor_z;
    int cor_y;
    ThingIdx firetng;
    ushort fflame;
    short tile_x, tile_z;
    short sib_tl_x, sib_tl_z;
    ubyte flame_count;

    if ((PRCCOORD_TO_MAPCOORD(x) >= MAP_COORD_WIDTH)
      || (PRCCOORD_TO_MAPCOORD(z) >= MAP_COORD_HEIGHT)) {
        return;
    }
    if (word_1E08B8 == 0) {
        return;
    }

    firetng = get_new_sthing();
    if (firetng == 0) {
        return;
    }

    p_fire = &sthings[firetng];
    p_fire->Type = SmTT_FIRE;
    p_fire->U.UFire.flame = 0;
    p_fire->Z = z;
    p_fire->Y = y;
    p_fire->X = x;
    play_dist_ssample(p_fire, 0x10u, 0x7Fu, 0x40u, 100, -1, 1);

    cor_x = x >> 8;
    cor_z = z >> 8;
    cor_y = y;

    switch (type)
    {
    case 1u:
        flame_count = 18 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x3F, 100, 4, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 18 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x3F, 100, 4, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 18 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x3F, 100, 4, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 18 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x3F, 100, 4, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 18 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 30, 4, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 18 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 30, 4, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 18 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 30, 4, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 18 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 30, 4, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 100, 5, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 100, 5, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 100, 5, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 100, 5, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x1FF, 30, 5, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x1FF, 30, 5, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x1FF, 30, 5, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x1FF, 30, 5, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x1FF, 30, 5, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x1FF, 30, 5, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x1FF, 30, 5, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x1FF, 30, 5, flame_count);
        FIRE_add_flame(firetng, fflame);
        break;
    case 2u:
        flame_count = 2;
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x3F, 30, 10, flame_count);
        FIRE_add_flame(firetng, fflame);
        break;
    case 3u:
        tile_z = cor_z >> 8;
        tile_x = cor_x >> 8;
        p_mapel = &game_my_big_map[128 * (z >> 16) + (cor_x >> 8)];
        p_mapel->Flags2 &= ~0x80;

        sib_tl_z = tile_z - 1;
        sib_tl_x = tile_x - 1;
        if ( sib_tl_x <= 0x7F && sib_tl_z <= 0x7F )
        {
          p_mapel = &game_my_big_map[128 * sib_tl_z + sib_tl_x];
          p_mapel->Flags2 &= 0xF;
          p_mapel->Flags2 |= 0x70;
        }
        sib_tl_z = tile_z - 1;
        if ( tile_x <= 0x7F && sib_tl_z <= 0x7F )
        {
          p_mapel = &game_my_big_map[128 * sib_tl_z + tile_x];
          p_mapel->Flags2 &= 0xF;
          p_mapel->Flags2 |= 0x50;
        }
        sib_tl_x = tile_x + 1;
        sib_tl_z = tile_z - 1;
        if ( (tile_x + 1) >= 0 && sib_tl_x <= 127 && sib_tl_z <= 0x7F )
        {
          p_mapel = &game_my_big_map[128 * sib_tl_z + sib_tl_x];
          p_mapel->Flags2 &= 0xF;
          p_mapel->Flags2 |= 0x30;
        }
        sib_tl_x = tile_x + 1;
        if ( (tile_x + 1) >= 0 && sib_tl_x <= 127 && tile_z <= 0x7F )
        {
          p_mapel = &game_my_big_map[128 * tile_z + sib_tl_x];
          p_mapel->Flags2 &= 0xF;
          p_mapel->Flags2 |= 0x10;
        }
        sib_tl_z = tile_z + 1;
        sib_tl_x = tile_x + 1;
        if ( (tile_x + 1) >= 0 && sib_tl_x <= 127 && sib_tl_z <= 0x7F )
        {
          p_mapel = &game_my_big_map[128 * sib_tl_z + sib_tl_x];
          p_mapel->Flags2 &= 0xF;
          p_mapel->Flags2 |= 0xF0;
        }
        sib_tl_z = tile_z + 1;
        if ( tile_x <= 0x7F && sib_tl_z <= 0x7F )
        {
          p_mapel = &game_my_big_map[128 * sib_tl_z + tile_x];
          p_mapel->Flags2 &= 0xF;
          p_mapel->Flags2 |= 0xD0;
        }
        sib_tl_z = tile_z + 1;
        sib_tl_x = tile_x - 1;
        if ( (tile_x - 1) >= 0 && sib_tl_x <= 127 && sib_tl_z <= 0x7F )
        {
          p_mapel = &game_my_big_map[128 * sib_tl_z + sib_tl_x];
          p_mapel->Flags2 &= 0xF;
          p_mapel->Flags2 |= 0xB0;
        }
        sib_tl_x = tile_x - 1;
        if ( sib_tl_x <= 0x7F && tile_z <= 0x7F )
        {
          p_mapel = &game_my_big_map[(tile_z << 7) + sib_tl_x];
          p_mapel->Flags2 &= 0xF;
          p_mapel->Flags2 |= 0x90;
        }

        flame_count = 43 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x3F, 100, 6, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 43 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x3F, 100, 6, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 43 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x3F, 100, 6, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 43 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 30, 7, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 43 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 30, 7, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 43 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 30, 7, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 53 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 100, 8, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 53 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 100, 8, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 53 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 100, 8, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 53 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x1FF, 30, 9, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 53 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x1FF, 30, 9, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 53 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x1FF, 30, 9, flame_count);
        FIRE_add_flame(firetng, fflame);
        break;
    case 4u:
        flame_count = 6;
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x3F, 30, 21, flame_count);
        FIRE_add_flame(firetng, fflame);
        goto LABEL_184;
    case 5u:
LABEL_184:
        flame_count = 12;
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x3F, 30, 21, flame_count);
        FIRE_add_flame(firetng, fflame);
        break;
    default:
        break;
    }
    add_node_sthing(firetng);
}

extern s32 dword_1E08BC;

#ifdef SW_FNCHK
static void snap_razor_wire_asm(struct Thing *p_wire)
{
    asm volatile ("call ASM_snap_razor_wire\n"
        : "+a" (p_wire) : : "cc", "memory");
}
#endif

/** Breaks a razor wire hit by fire: removes its collision vector and the
 * wire thing.  Remade from snap_razor_wire; the second point is built
 * from the wire's VX/VY/VZ as in the original. */
static void snap_razor_wire_c(struct Thing *p_wire)
{
    delete_vect((short)(p_wire->X >> 8), (short)((p_wire->Y >> 5) + 60),
      (short)(p_wire->Z >> 8), (short)((short)p_wire->VX + 50),
      (short)(((short)p_wire->VY << 3) + 60), (short)p_wire->VZ,
      p_wire->Frame, 2);
    remove_thing(p_wire->ThingOffset);
    delete_node(p_wire);
}

static void snap_razor_wire(struct Thing *p_wire)
{
    FNCHK_VOID("snap_razor_wire", snap_razor_wire_asm(p_wire), snap_razor_wire_c(p_wire));
}

/** Takes a flame from the free list, fills it and puts it at the head of
 * the flames of the fire.  The random numbers for the position, life and
 * speed are drawn by the caller (in the original order), the frame and the
 * counter here.  Returns the link to the next flame of the new flame. */
static ushort *fire_flame_add(struct SimpleThing *p_fire, short x, short y, short z,
  ubyte type, sbyte big, ushort anim, ubyte life, ubyte count, sbyte fvel)
{
    struct FireFlame *p_fflame;
    ushort fflame, frm;

    fflame = word_1E08B8;
    if (fflame == 0)
        return NULL;
    word_1E08B8 = FIRE_flame[fflame].next;
    dword_1E08BC++;
    p_fflame = &FIRE_flame[fflame];
    p_fflame->x = x;
    p_fflame->z = z;
    p_fflame->y = y;
    p_fflame->type = type;
    p_fflame->big = big;
    p_fflame->dbig = 0;
    p_fflame->ddbig = -1;
    for (frm = nstart_ani[anim]; ; frm = frame[frm].Next)
    {
        p_fflame->frame = frm;
        if ((LbRandomAnyShort() & 3) == 0)
            break;
    }
    p_fflame->life = life;
    p_fflame->count = count;
    p_fflame->fvel = fvel;
    p_fflame->fcount = LbRandomAnyShort() & 0x7F;
    p_fflame->next = p_fire->U.UFire.flame;
    p_fire->U.UFire.flame = fflame;
    return &p_fflame->next;
}

/** A new flame randomly placed round a point: the life, z and x offsets
 * and speed drawn in that order.  Masks are of the offsets, which are
 * centred. */
static ushort *fire_flame_add_near(struct SimpleThing *p_fire, int cx, int y, int cz,
  ushort mask, ubyte type, sbyte big, ushort anim, ubyte life_base, ubyte count)
{
    ubyte life;
    int x, z;
    sbyte fvel;

    life = (LbRandomAnyShort() & 0xF) + life_base;
    z = cz + (LbRandomAnyShort() & mask) - (mask >> 1);
    x = cx + (LbRandomAnyShort() & mask) - (mask >> 1);
    fvel = (LbRandomAnyShort() & 0x3F) + 50;
    return fire_flame_add(p_fire, x, y, z, type, big, anim, life, count, fvel);
}

/** Spreading flame of a burning ground: a count drawn first, height of
 * the ground. */
static ushort *fire_flame_add_ground(struct SimpleThing *p_fire, int cx, int cz,
  ushort mask, ubyte type, sbyte big, ushort anim, ubyte base)
{
    ubyte count, life;
    int x, y, z;
    sbyte fvel;

    count = base - (LbRandomAnyShort() & 3);
    life = (LbRandomAnyShort() & 0xF) + base;
    z = cz + (LbRandomAnyShort() & mask) - (mask >> 1);
    y = alt_at_point((short)cx, (short)cz) >> 5;
    x = cx + (LbRandomAnyShort() & mask) - (mask >> 1);
    fvel = (LbRandomAnyShort() & 0x3F) + 50;
    return fire_flame_add(p_fire, x, y, z, type, big, anim, life, count, fvel);
}

/** Sparks flying out of a flame in a random direction. */
static void fire_flame_add_sparks(struct SimpleThing *p_fire, struct FireFlame *p_fflame,
  int sin_shift, ushort anim, ubyte count_base, ubyte life_base)
{
    int i;

    for (i = 0; i < 2; i++)
    {
        ushort angle;
        int dx, dz, mag;
        ubyte count, life;
        sbyte fvel;

        angle = LbRandomAnyShort() & 0x7FF;
        dx = lbSinTable[angle] >> sin_shift;
        dz = lbSinTable[angle + 512] >> sin_shift;
        mag = LbRandomAnyShort() & 0xFF;
        dx *= mag;
        dz *= mag;
        count = (LbRandomAnyShort() & 0xF) + count_base;
        life = (LbRandomAnyShort() & 0xF) + life_base;
        fvel = (LbRandomAnyShort() & 0x3F) + 50;
        fire_flame_add(p_fire, p_fflame->x + (dx >> 8), p_fflame->y, p_fflame->z + (dz >> 8),
          5, 100, anim, life, count, fvel);
    }
}

/** A fire: lights the area, burns the people and sets on fire the statics
 * on its tile, snaps razor wires, and animates its flames, which at the end
 * of their count spawn new flames (rising and spreading) by their type.
 * Remade from FIRE_process_flame.
 */
void FIRE_process_flame_c(struct SimpleThing *p_fire)
{
    struct MyMapElement *p_mapel;
    ushort *link;
    ushort fflame, next;
    int cx, cy, cz;
    ThingIdx thing;
    ushort colvl;

    cx = p_fire->X >> 8;
    cy = p_fire->Y;
    cz = p_fire->Z >> 8;
    apply_super_quick_light((short)cx, (short)cz, LbRandomAnyShort() & 0xF);

    p_mapel = &game_my_big_map[((cz >> 8) << 7) + (cx >> 8)];
    thing = p_mapel->Child;
    while (thing != 0)
    {
        if (thing > 0)
        {
            struct Thing *p_person;

            p_person = &things[thing];
            if ((p_person->Type == TT_PERSON) && ((p_person->Flag & TngF_Destroyed) == 0)
              && (p_person->Health >= 0))
            {
                int dmg;

                dmg = 0x20 - (((p_person->U.UPerson.UMod.Mods >> 6) & 7) << 4);
                if (dmg > 0)
                {
                    p_person->Health -= dmg;
                    if (p_person->Health < 0)
                        set_person_dead(p_person, 0x12);
                }
            }
            thing = p_person->Next;
        }
        else
        {
            struct SimpleThing *p_sthing;

            p_sthing = &sthings[thing];
            if ((p_sthing->Type == SmTT_STATIC) && ((p_sthing->Flag & TngF_Destroyed) == 0))
                set_static_on_fire(p_sthing);
            thing = p_sthing->Next;
        }
    }

    colvl = p_mapel->ColHead;
    while (colvl != 0)
    {
        struct ColVectList *p_colvl;
        short obj;

        p_colvl = &game_col_vects_list[colvl];
        obj = p_colvl->Object;
        colvl = p_colvl->NextColList & 0x7FFF;
        if ((obj < 0) && ((LbRandomAnyShort() & 7) == 0))
        {
            struct Thing *p_wire;

            p_wire = &things[-obj];
            if (p_wire->SubType != 0)
            {
                bang_new4(p_fire->X, p_fire->Y, p_fire->Z, 15);
                do_shockwave(p_fire->X >> 8, p_fire->Y >> 8, p_fire->Z >> 8, 0x400, 1000,
                  &things[(ushort)p_wire->Owner]);
            }
            snap_razor_wire(p_wire);
        }
    }

    link = (ushort *)&p_fire->U.UFire.flame;
    fflame = (ushort)p_fire->U.UFire.flame;
    while (fflame != 0)
    {
        struct FireFlame *p_fflame;
        ushort *p_new;

        p_fflame = &FIRE_flame[fflame];
        next = p_fflame->next;
        p_fflame->life--;
        if (p_fflame->life == 0)
        {
            FIRE_flame[fflame].next = word_1E08B8;
            word_1E08B8 = fflame;
            *link = next;
            dword_1E08BC--;
            fflame = next;
            continue;
        }
        p_fflame->big += p_fflame->dbig >> 3;
        if ((gameturn & 3) == 0)
            p_fflame->dbig += p_fflame->ddbig;
        p_fflame->fcount -= p_fflame->fvel;
        if (p_fflame->fcount <= 0)
        {
            p_fflame->fcount = 100;
            p_fflame->frame = frame[p_fflame->frame].Next;
        }
        p_fflame->count--;
        fflame = next;
        if (p_fflame->count != 0)
        {
            link = &p_fflame->next;
            continue;
        }
        switch (p_fflame->type)
        {
        case 1: case 5: case 20: case 24:
            link = &p_fflame->next;
            p_fflame->count = 0x7F;
            break;
        case 2: case 3:
            break;
        case 4:
            if ((LbRandomAnyShort() & 1) == 0)
            {
                ushort *p_first;

                p_first = fire_flame_add_near(p_fire, cx, cy, cz, 0xFF, 5, 30, 0x39B, 0x21, 100);
                fire_flame_add_near(p_fire, cx, cy, cz, 0x1FF, 5, 30, 0x39C, 0x21, 100);
                fire_flame_add_near(p_fire, cx, cy, cz, 0x1FF, 5, 30, 0x39C, 0x21, 100);
                if ((link == &word_1E08B8) && (p_first != NULL))
                    link = p_first;
                else
                    link = &p_fflame->next;
            }
            else
            {
                link = &p_fflame->next;
            }
            p_fflame->count = 0x7F;
            break;
        case 6: case 7: case 8: case 9:
            switch (p_fflame->type)
            {
            case 6:
                p_new = fire_flame_add_ground(p_fire, cx, cz, 0x3F, 6, 100, 0x39B, 0x2B);
                break;
            case 7:
                p_new = fire_flame_add_ground(p_fire, cx, cz, 0xFF, 7, 30, 0x39B, 0x2B);
                break;
            case 8:
                p_new = fire_flame_add_ground(p_fire, cx, cz, 0xFF, 8, 100, 0x39C, 0x35);
                break;
            default:
                p_new = fire_flame_add_ground(p_fire, cx, cz, 0x1FF, 9, 30, 0x39C, 0x35);
                break;
            }
            if ((link == &word_1E08B8) && (p_new != NULL))
                link = p_new;
            else
                link = &p_fflame->next;
            break;
        case 10: case 11: case 12: case 13: case 14:
            p_new = fire_flame_add_near(p_fire, cx, p_fflame->y + 0x32, cz, 0x3F,
              p_fflame->type + 1, 30, 0x39B, 0x35, 3);
            if ((link == &word_1E08B8) && (p_new != NULL))
                link = p_new;
            else
                link = &p_fflame->next;
            p_fflame->count = 0x7F;
            break;
        case 15: case 16: case 17: case 18: case 19:
            p_new = fire_flame_add_near(p_fire, cx, p_fflame->y + 0x32, cz, 0x3F,
              p_fflame->type + 1, 30, 0x39B, 0x35, 3);
            fire_flame_add_sparks(p_fire, p_fflame, 9, 0x39B, 0x12, 0x2B);
            fire_flame_add_sparks(p_fire, p_fflame, 8, 0x39C, 0x18, 0x35);
            if ((link == &word_1E08B8) && (p_new != NULL))
                link = p_new;
            else
                link = &p_fflame->next;
            p_fflame->count = 0x7F;
            break;
        case 21: case 22: case 23:
            if (p_fflame->type == 21)
                p_new = fire_flame_add_near(p_fire, cx, p_fflame->y + 0x23, cz, 0x7F,
                  p_fflame->type + 1, 30, 0x39B, 0x35, 8);
            else if (p_fflame->type == 22)
                p_new = fire_flame_add_near(p_fire, cx, p_fflame->y + 0x37, cz, 0x3F,
                  p_fflame->type + 1, 30, 0x39B, 0x35, 8);
            else
                p_new = fire_flame_add_near(p_fire, cx, p_fflame->y + 0x37, cz, 0x1F,
                  p_fflame->type + 1, 30, 0x39B, 0x35, 8);
            if ((link == &word_1E08B8) && (p_new != NULL))
                link = p_new;
            else
                link = &p_fflame->next;
            p_fflame->count = 0x7F;
            break;
        default:
            break;
        }
    }
}

#ifdef SW_FNCHK
static void set_static_on_fire_asm(struct SimpleThing *p_static)
{
    asm volatile ("call ASM_set_static_on_fire\n"
        : : "a" (p_static) : "cc", "memory");
}
#endif

struct SimpleThing *init_nuclear_bomb(MapCoord x, MapCoord y, MapCoord z);

/** Approximate length of a vector from its absolute coordinates (as in
 * the vehicle navigation). */
static s32 fire_dist_approx(s32 adx, s32 adz)
{
    s32 hi, lo;

    if (adx < adz) {
        hi = adz;
        lo = adx;
    } else {
        hi = adx;
        lo = adz;
    }
    return hi - (hi >> 5) - (hi >> 7) + (lo >> 2) + (lo >> 3) + (lo >> 6) + (lo >> 7);
}

/** Flying vehicles (in some flight states) on the 5x5 tiles round the
 * point get damage falling with the distance; one with no health left
 * starts crashing.  Remade from apply_unkn_vehicle50_damage. */
static void apply_unkn_vehicle50_damage(s32 x, s32 z, s32 damage)
{
    s32 tile_x, tile_z, tx, tz;

    tile_x = x >> 16;
    tile_z = z >> 16;
    for (tx = tile_x - 2; tx != tile_x + 3; tx++)
    {
        if ((tx < 0) || (tx > MAP_TILE_WIDTH - 1))
            continue;
        for (tz = tile_z - 2; tz != tile_z + 3; tz++)
        {
            ThingIdx thing;

            if ((tz < 0) || (tz > MAP_TILE_HEIGHT - 1))
                continue;
            thing = game_my_big_map[(tz << 7) + tx].Child;
            while (thing != 0)
            {
                struct Thing *p_vehicle;
                s32 dist, dmg;
                ushort st;

                if (thing < 0) {
                    thing = sthings[thing].Next;
                    continue;
                }
                p_vehicle = &things[thing];
                thing = p_vehicle->Next;
                if ((p_vehicle->Type != TT_VEHICLE) || (p_vehicle->SubType != 0x32))
                    continue;
                st = (ushort)p_vehicle->State;
                if ((st != 0x3A) && (st != 0x41) && (st != 0x42) && (st != 0x43))
                    continue;
                dist = fire_dist_approx(abs(p_vehicle->X - x), abs(p_vehicle->Z - z));
                dmg = ((600 - (dist >> 8)) * damage) >> 9;
                if (dmg <= 0)
                    continue;
                p_vehicle->U.UVehicle.WorkPlace |= 0x0080;
                if (p_vehicle->SubType == 0x33)
                    dmg >>= 1;
                if (p_vehicle->SubType == 0x3B)
                    dmg >>= 2;
                p_vehicle->Health -= (short)dmg;
                if (p_vehicle->Health > 0)
                    continue;
                p_vehicle->Flag |= 0x0002;
                if (p_vehicle->SubType == 0x3B) {
                    init_mech_explode(p_vehicle);
                } else {
                    start_crashing(p_vehicle);
                    play_dist_sample(p_vehicle, 0x5F, 0x7F, 0x40, 100, 0, 1);
                }
            }
        }
    }
}

/** A static object set on fire, by its kind (StartFrame): its lights go
 * out, and it explodes (bang, shockwave, sometimes damage to flyers), burns
 * (fire), becomes a scale effect or launches a bomb.
 * Remade from set_static_on_fire. */
static void set_static_on_fire_c(struct SimpleThing *p_static)
{
    ushort light, sf;
    int i;

    if ((p_static < &sthings[-1500]) || (p_static > &sthings[0]))
        return;
    p_static->State = 0x24;
    p_static->StartTimer1 = 10;
    p_static->Flag |= 0x0002;
    light = p_static->U.UScenery.LightHead;
    for (i = 0; (light != 0) && (i < 1000) && (light < next_full_light); i++)
    {
        struct FullLight *p_light;
        ushort cmd;

        p_light = &game_full_lights[light];
        p_light->Intensity = 0;
        cmd = (ushort)p_light->Command;
        if ((cmd <= next_light_command) && (cmd != 0))
            game_light_commands[cmd].lcfld_0[1] = 5;
        light = (ushort)p_light->NextFull;
    }
    sf = (ushort)p_static->StartFrame;
    if ((sf >= 0x424) && (sf <= 0x428))
    {
        p_static->State = 0x0D;
        p_static->Timer1 = 0;
        return;
    }
    if ((sf > 0x400) && (sf < 0x405))
    {
        struct SimpleThing *p_bomb;

        p_static->Timer1 = 100;
        switch (sf - 0x401)
        {
        case 0:
        case 1:
            p_bomb = init_nuclear_bomb(p_static->X >> 8, p_static->Y >> 8, p_static->Z >> 8);
            if (p_bomb != NULL) {
                p_bomb->Type = 0x20;
                p_bomb->SubType = (sf == 0x401) ? 4 : 5;
                play_dist_ssample(p_bomb, (sf == 0x401) ? 0x20 : 0x1F, 0x7F, 0x40, 100, -1, 3);
            }
            break;
        case 2:
            p_bomb = init_nuclear_bomb(p_static->X >> 8, p_static->Y >> 8, p_static->Z >> 8);
            if (p_bomb != NULL) {
                p_bomb->Timer1 = 0x13;
                p_bomb->Radius = -5;
            }
            break;
        case 3:
            play_dist_ssample(p_static, 0x25, 0x7F, 0x40, 100, 0, 3);
            bang_new4(p_static->X, p_static->Y, p_static->Z, 0x14);
            do_shockwave(p_static->X >> 8, p_static->Y >> 8, p_static->Z >> 8, 0x300, 200, NULL);
            break;
        }
        p_static->StartFrame = 0x422;
        p_static->Frame = nstart_ani[1058];
        return;
    }
    if ((sf > 0x3EC) && (sf < 0x3F0))
    {
        struct SimpleThing *p_effect;

        p_effect = create_scale_effect(p_static->X >> 8, p_static->Y >> 8, p_static->Z >> 8, 0x39C, 40);
        if (p_effect != NULL)
        {
            p_effect->SubType = 0x39;
            p_effect->Object = 0;
            p_effect->StartFrame = 0;
            p_effect->U.UEffect.VX = 0;
            p_effect->U.UEffect.VY = 0;
            p_effect->U.UEffect.VZ = 0;
            p_effect->U.UEffect.OX = p_static->X >> 8;
            p_effect->U.UEffect.OY = p_static->Y >> 8;
            p_effect->U.UEffect.OZ = p_static->Z >> 8;
        }
        remove_sthing(p_static->ThingOffset);
        delete_snode(p_static);
        return;
    }
    switch (sf)
    {
    case 0x3E7: case 0x3EA: case 0x3EC: case 0x3F0:
    case 0x408: case 0x40D: case 0x40E: case 0x41A:
    {
        s32 damage;

        play_dist_ssample(p_static, 0x36, 0x7F, 0x40, 100, 0, 1);
        p_static->State = 0x0C;
        p_static->Timer1 = 9;
        if ((sf == 0x40E) || (sf == 0x3EC)) {
            bang_new4(p_static->X, p_static->Y, p_static->Z, 0x78);
            damage = 2500;
        } else {
            bang_new4(p_static->X, p_static->Y, p_static->Z, 0x73);
            damage = 5000;
        }
        apply_unkn_vehicle50_damage(p_static->X, p_static->Z, damage);
        p_static->StartFrame = 0x41C;
        do_shockwave(p_static->X >> 8, p_static->Y >> 8, p_static->Z >> 8, 0x300, 100, NULL);
        return;
    }
    case 0x3E9: case 0x406: case 0x40A: case 0x40B: case 0x417: case 0x419:
        p_static->Timer1 = 40;
        p_static->State = 0x0C;
        switch (sf)
        {
        case 0x3E9:
            p_static->StartFrame = 0x425;
            FIRE_new(p_static->X, p_static->Y >> 5, p_static->Z, 4);
            FIRE_new(p_static->X, p_static->Y >> 5, p_static->Z, 4);
            break;
        case 0x40A:
            p_static->StartFrame = 0x428;
            FIRE_new(p_static->X, p_static->Y >> 5, p_static->Z, 2);
            break;
        case 0x40B:
            p_static->StartFrame = 0x426;
            FIRE_new(p_static->X, p_static->Y >> 5, p_static->Z, 2);
            break;
        case 0x417:
            p_static->StartFrame = 0x429;
            FIRE_new(p_static->X, p_static->Y >> 5, p_static->Z, 2);
            break;
        case 0x419:
            p_static->StartFrame = 0x427;
            FIRE_new(p_static->X, p_static->Y >> 5, p_static->Z, 4);
            FIRE_new(p_static->X, p_static->Y >> 5, p_static->Z, 4);
            break;
        default: // 0x406
            break;
        }
        return;
    default:
        play_dist_ssample(p_static, 0x25, 0x3C, 0x40, 100, 0, 3);
        bang_new4(p_static->X, p_static->Y, p_static->Z, 0x14);
        do_shockwave(p_static->X >> 8, p_static->Y >> 8, p_static->Z >> 8, 0x300, 100, NULL);
        p_static->Timer1 = 40;
        remove_sthing(p_static->ThingOffset);
        delete_snode(p_static);
        return;
    }
}

void set_static_on_fire(struct SimpleThing *p_static)
{
    FNCHK_VOID("set_static_on_fire", set_static_on_fire_asm(p_static),
      set_static_on_fire_c(p_static));
}

/******************************************************************************/
