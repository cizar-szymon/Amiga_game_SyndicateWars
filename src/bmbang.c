/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file bmbang.c
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
#include "bmbang.h"

#include "enginshrapn.h"

#include "bfmath.h"
#include "bfutility.h"
#include "bigmap.h"
#include "thing.h"
#include "swlog.h"
#include <stdlib.h>
#include "building.h"
#include "febrief.h"
#include "fncheck.h"
#include "frame_sprani.h"
#include "game.h"
#include "game_options.h"
#include "misstat.h"
#include "people.h"
#include "sound.h"
#include "tngcolisn.h"
#include "vehicle.h"

extern short link__MapColListEmptyHead;

void do_shockwave_building(int dist, int intensity, struct Thing *p_thing, struct Thing *p_owner);
void do_shockwave_vehicle(int dx, int dz, int dist, int intensity,
  struct Thing *p_vehicle, struct Thing *p_owner);
void do_shockwave_person(int dx, int dz, int dist, int intensity,
  struct Thing *p_person, struct Thing *p_owner);
void do_shockwave_scale_effect(int dx, int dz, int dist, int intensity,
  struct SimpleThing *p_sthing, struct Thing *p_owner);
void init_recoil(struct Thing *p_person, short vx, short vy, short vz, ushort type);
/******************************************************************************/

#pragma pack(1)

struct CircleDetail {
    s32 dangle;
    s32 babynumber;
};

struct SphereDetail {
    s32 dangley;
    s32 babyshift;
    s32 babynumber;
};

struct BangDetail {
    struct CircleDetail circle[4];
    struct SphereDetail sphere[4];
    u32 mask;
};

#pragma pack()

void bang_init(void)
{
#if 0
    asm volatile ("call ASM_bang_init\n"
        :  :  : "eax" );
#endif
    shrapnel_init();
    phwoar_init();
}

/** Fill in the type-specific fields of a freshly allocated Phwoar entry.
 */
void fillin_phwoar_type_info(struct Phwoar *p_phwoar)
{
    asm volatile ("call ASM_fillin_phwoar_type_info\n"
        : : "a" (p_phwoar) : "cc", "memory");
}

void new_bang(int x, int y, int z, int type, int owner, int c)
{
#if 0
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[2];

    stkargs[0] = (int)(intptr_t)owner;
    stkargs[1] = (int)(intptr_t)c;

    asm volatile (
      "push 4(%4)\n"
      "push 0(%4)\n"
      "call ASM_new_bang\n"
        : : "a" (x), "d" (y), "b" (z), "c" (type), "S" (stkargs)
        : "cc", "memory");
    return;
#endif
    short thing;
    struct SimpleThing *p_sthing;
    int level; // "phwoar type"/level parameter, stored into the phwoar entry
    int ang0, ang1, ang2; // quadrant angle-spread sizes (also the shrapnel counts)
    int fixed_dir; // if set, shrapnel fly in one fixed (not random) direction
    int fixed_vy; // the fixed vy value used when fixed_dir is set
    int state_override; // -1 if State keeps its default (0) value
    int total, i;
    ThingIdx tng;

    if (x < 0)
        return;
    if ((x >> 8) >= MAP_COORD_WIDTH)
        return;
    if (z < 0)
        return;
    if ((z >> 8) >= MAP_COORD_HEIGHT)
        return;
    if (phwoar_free == 0)
        return;
    c++;
    if (shrapnel_free == 0)
        return;

    // Light-only types
    switch (type)
    {
    case 0xa:
        new_bang(x, y, z, 0xb, owner, c);
        new_bang(x, y, z, 0xc, owner, c);
        create_temp_light(x, y + 0x6400, z, 0x80, 0x19, 0x80);
        return;
    case 0xf:
        new_bang(x, y, z, 0x10, owner, c);
        new_bang(x, y, z, 0x11, owner, c);
        new_bang(x, y, z, 0x12, owner, c);
        create_temp_light(x, y + 0x6400, z, 0x50, 0x14, 0x50);
        return;
    case 0x14:
        new_bang(x, y, z, 0x15, owner, c);
        new_bang(x, y, z, 0x16, owner, c);
        create_temp_light(x, y + 0x6400, z, 0x32, 0x5, 0x32);
        return;
    case 0x19:
        new_bang(x, y, z, 0x1a, owner, c);
        new_bang(x, y, z, 0x1b, owner, c);
        new_bang(x, y, z, 0x1c, owner, c);
        create_temp_light(x, y + 0xc800, z, 0x100, 0x5, 0xa);
        return;
    case 0x2d:
        new_bang(x, y, z, 0x2e, owner, c);
        new_bang(x, y, z, 0x2f, owner, c);
        new_bang(x, y, z, 0x30, owner, c);
        create_temp_light(x, y + 0x9600, z, 0x80, 0x19, 0x80);
        return;
    default:
        break;
    }

    tng = get_new_sthing();
    if (tng == 0)
        return;

    thing = tng;
    p_sthing = &sthings[thing];
    p_sthing->Type = SmTT_BANG;
    p_sthing->State = 0;
    p_sthing->X = x;
    p_sthing->Y = y;
    p_sthing->Z = z;
    p_sthing->Owner2 = owner;
    p_sthing->U.UBang.shrapnel = 0;
    p_sthing->U.UBang.phwoar = 0;

    ang0 = 0;
    ang1 = 0;
    ang2 = 0;
    fixed_dir = 0;
    fixed_vy = 0;
    state_override = -1;

    switch (type)
    {
    case 0xb:
        ang0 = (LbRandomAnyShort() & 0x3) + 6;
        ang1 = (LbRandomAnyShort() & 0x3) + 6;
        ang2 = (LbRandomAnyShort() & 0x3) + 6;
        level = 0x35;
        break;
    case 0xc:
        level = 0x33;
        break;
    case 0x10:
        ang0 = (LbRandomAnyShort() & 0x3) + 4;
        ang1 = (LbRandomAnyShort() & 0x3) + 4;
        ang2 = (LbRandomAnyShort() & 0x3) + 4;
        level = 0x40;
        break;
    case 0x11:
    case 0x16:
        level = 0x3d;
        break;
    case 0x12:
        level = 0x5a;
        break;
    case 0x15:
        ang1 = (LbRandomAnyShort() & 0x7) + 4;
        level = 0x3f;
        break;
    case 0x1e:
        level = 0x64;
        break;
    case 0x1b:
        level = 0x56;
        break;
    case 0x1c:
        level = 0x58;
        break;
    case 0x1a:
        level = 0x54;
        break;
    case 0x23:
        level = 0x65;
        break;
    case 0x28:
        level = 0x52;
        break;
    case 0x2e:
        level = 0x63;
        break;
    case 0x2f:
        level = 0x5f;
        break;
    case 0x30:
        level = 0x61;
        break;
    case 0x32:
        state_override = 0x64;
        level = 0x68;
        break;
    case 0x35:
        level = 0x68;
        break;
    case 0x37:
        state_override = 0xf;
        level = 0x6a;
        break;
    case 0x3c:
        level = 0x6f;
        break;
    case 0x41:
        level = 0x72;
        break;
    case 0x4b:
        level = 0x96;
        break;
    case 0x50:
        level = 0x8c;
        break;
    case 0x55:
        level = 0x82;
        break;
    case 0x5a:
        level = 0x78;
        break;
    case 0x5f:
        level = 0xbe;
        break;
    case 0x64:
        level = 0xb4;
        break;
    case 0x69:
        level = 0xaa;
        break;
    case 0x6e:
        level = 0xa0;
        break;
    case 0x73:
        create_temp_light(x, y + 0x6400, z, 0x32, 0xf, 0x32);
        level = 0x96;
        fixed_dir = 1;
        fixed_vy = 0x7f;
        break;
    case 0x78:
        create_temp_light(x, y + 0x6400, z, 0x32, 0xa, 0x32);
        level = 0x96;
        fixed_dir = 1;
        fixed_vy = 0x40;
        break;
    default:
        LOGERR("bang at (0x%x,0x%x,0x%x) owner=%d type=%d c=%d, bad type",
            x, y, z, owner, type, c);
        return;
    }

    if (state_override >= 0)
        p_sthing->State = state_override;

    total = ang0 + ang1 + ang2 + (fixed_dir ? 1 : 0);
    for (i = 0; i < total; i++) {
        short shr_idx;
        struct Shrapnel *p_shrap;
        ubyte quadrant;

        shr_idx = shrapnel_free;
        if (shr_idx == 0)
            break;
        shrapnel_free = shrapnel[shr_idx].child;

        if (i < ang0)
            quadrant = 1;
        else if (i < ang0 + ang1)
            quadrant = 3;
        else if (i < ang0 + ang1 + ang2)
            quadrant = 2;
        else
            quadrant = 4;

        p_shrap = &shrapnel[shr_idx];
        p_shrap->type = quadrant;
        p_shrap->x = x;
        p_shrap->y = y;
        p_shrap->z = z;
        if (fixed_dir) {
            p_shrap->vx = 0;
            p_shrap->vz = 0;
            p_shrap->vy = fixed_vy;
        } else {
            int angle1, angle2;
            int vx, vy, vz;

            angle1 = LbRandomAnyShort() & 0x7ff;
            angle2 = LbRandomAnyShort() & 0x1ff;

            vx = (lbSinTable[angle1] >> 10) * lbSinTable[angle2 + LbFPMath_PI/2];
            vx >>= 16;
            vz = (lbSinTable[angle1 + LbFPMath_PI/2] >> 10) * lbSinTable[angle2 + LbFPMath_PI/2];
            vz >>= 16;
            vy = lbSinTable[angle2] >> 10;

            p_shrap->vx = (sbyte)vx;
            p_shrap->vz = (sbyte)vz;
            p_shrap->vy = (sbyte)vy;
        }
        p_shrap->vyaw = LbRandomAnyShort() & 0xf;
        p_shrap->vpitch = LbRandomAnyShort() & 0x1f;

        p_shrap->child = p_sthing->U.UBang.shrapnel;
        p_sthing->U.UBang.shrapnel = shr_idx;
    }

    if (level != 0) {
        short ph_idx;

        ph_idx = phwoar_free;
        if (ph_idx != 0) {
            phwoar_free = phwoar[ph_idx].child;
            phwoar_num_used++;
            phwoar_num_free--;
        }
        if (ph_idx != 0) {
            struct Phwoar *p_phwoar;

            p_sthing->U.UBang.phwoar = ph_idx;
            p_phwoar = &phwoar[ph_idx];
            p_phwoar->x = x;
            p_phwoar->y = y;
            p_phwoar->z = z;
            p_phwoar->vx = 0;
            p_phwoar->vz = 0;
            p_phwoar->vy = 0;
            p_phwoar->type = level;
            p_phwoar->child = 0;
            p_phwoar->fc = 0;
            fillin_phwoar_type_info(p_phwoar);
        }
    }

    add_node_sthing(thing);
}

void bang_new5(int x, int y, int z, int type, int owner)
{
    new_bang(x, y, z, type, owner, 0);
}

void bang_new4(int x, int y, int z, int type)
{
    new_bang(x, y, z, type, 0, 0);
}

ubyte unused_func_026(ubyte a1)
{
    ubyte ret;
    asm volatile (
      "call ASM_unused_func_026\n"
        : "=r" (ret) : "a" (a1));
    return ret;
}

void unused_func_025(short a1, short a2, short a3)
{
    asm volatile (
      "call ASM_unused_func_025\n"
        : : "a" (a1), "d" (a2), "b" (a3));
}

#ifdef SW_FNCHK
static void do_shockwave_asm(int x, int y, int z, int radius, int intensity, struct Thing *p_owner)
{
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[2];

    stkargs[0] = (int)(intptr_t)intensity;
    stkargs[1] = (int)(intptr_t)p_owner;

    asm volatile (
      "push 4(%4)\n"
      "push 0(%4)\n"
      "call ASM_do_shockwave\n"
        : : "a" (x), "d" (y), "b" (z), "c" (radius), "S" (stkargs)
        : "cc", "memory");
}
#endif

/** Removes the first collision vector of the tile belonging to the thing.
 * Remade from del_thing_vectors_from_mapwho.
 */
static TbBool del_thing_vectors_from_mapwho(struct MyMapElement *p_mapel, short thing)
{
    ushort cvl, prev, next;

    prev = 0;
    for (cvl = p_mapel->ColHead; cvl != 0; cvl = next & 0x7FFF)
    {
        next = game_col_vects_list[cvl].NextColList;
        if (game_col_vects_list[cvl].Object == thing)
        {
            if (prev != 0)
                game_col_vects_list[prev].NextColList = next;
            else
                p_mapel->ColHead = next & 0x7FFF;
            game_col_vects_list[cvl].NextColList = link__MapColListEmptyHead;
            link__MapColListEmptyHead = cvl;
            return true;
        }
        prev = cvl;
    }
    return false;
}

/** Distance approximation used by the shockwave: longer + shorter / 2. */
static int shockwave_dist(int dx, int dz)
{
    int adx, adz;

    adx = abs(dx);
    adz = abs(dz);
    if (adx > adz)
        return adx + (adz >> 1);
    return adz + (adx >> 1);
}

/** Damages the things around (x,y,z) (map coordinates): persons, vehicles,
 * buildings, mines, scale effects and dropped items.  A negative radius
 * only affects dropped items.  Remade from do_shockwave.
 */
static void do_shockwave_c(int x, int y, int z, int radius, int intensity, struct Thing *p_owner)
{
    TbBool items_only;
    int r, tile_x, tile_z;
    int tile_x_beg, tile_z_beg;

    items_only = false;
    if (radius < 0) {
        items_only = true;
        radius = -radius;
    }
    r = radius >> 8;
    if (r < 1)
        r = 1;
    tile_x_beg = (x >> 8) - r;
    tile_z_beg = (z >> 8) - r;

    for (tile_x = tile_x_beg; tile_x <= tile_x_beg + 2 * r; tile_x++)
    {
        for (tile_z = tile_z_beg; tile_z <= tile_z_beg + 2 * r; tile_z++)
        {
            struct MyMapElement *p_mapel;
            short thing;

            if ((tile_x <= 0) || (tile_x >= MAP_TILE_WIDTH))
                continue;
            if ((tile_z <= 0) || (tile_z >= MAP_TILE_HEIGHT))
                continue;
            p_mapel = &game_my_big_map[tile_z * MAP_TILE_WIDTH + tile_x];
            if (!items_only)
            {
                short obj;

                obj = game_col_vects_list[p_mapel->ColHead].Object;
                if (obj > 0)
                {
                    if (things[obj].Type == TT_BUILDING)
                        do_shockwave_building(0x200, intensity, &things[obj], p_owner);
                    else if (obj < 1000)
                        del_thing_vectors_from_mapwho(p_mapel, obj);
                    // else the original only formats a "strange thing" message
                }
            }
            for (thing = p_mapel->Child; thing != 0; )
            {
                int dx, dy, dz, dist;

                if (thing > 0)
                {
                    struct Thing *p_thing;

                    p_thing = &things[thing];
                    if (!items_only)
                    {
                        if ((p_thing->Type == TT_PERSON) &&
                          ((ushort)p_thing->State != PerSt_PERSON_BURNING) &&
                          ((p_thing->Flag & TngF_Destroyed) == 0))
                        {
                            dy = (p_thing->Y >> 8) - y;
                            if ((dy < 0x55) && (dy > -0x55))
                            {
                                dx = (p_thing->X >> 8) - x;
                                dz = (p_thing->Z >> 8) - z;
                                dist = shockwave_dist(dx, dz);
                                if (dist < (ushort)p_thing->Radius + radius)
                                    do_shockwave_person(dx, dz, dist, intensity, p_thing, p_owner);
                            }
                        }
                        if ((p_thing->Type == TT_VEHICLE) ||
                          ((p_thing->Type == TT_BUILDING) && (p_thing->SubType == 0x20)))
                        {
                            dy = (p_thing->Y >> 8) - y;
                            if (((ushort)p_thing->State != 0x0D) && (dy < 0xB9) && (dy > -0xCD))
                            {
                                dx = (p_thing->X >> 8) - x;
                                dz = (p_thing->Z >> 8) - z;
                                dist = shockwave_dist(dx, dz);
                                if (dist < (ushort)p_thing->Radius + radius)
                                    do_shockwave_vehicle(dx, dz, dist, intensity, p_thing, p_owner);
                            }
                        }
                        if ((p_thing->Type == TT_MINE) && (p_thing->SubType == 0x30) &&
                          ((p_thing->Flag & TngF_Destroyed) == 0) &&
                          ((ushort)p_thing->State != 0x0D))
                        {
                            dist = shockwave_dist((p_thing->X >> 8) - x, (p_thing->Z >> 8) - z);
                            if ((radius - dist) * intensity / radius > 0)
                            {
                                p_thing->Health -= (short)intensity;
                                if (p_thing->Health <= 0)
                                {
                                    play_dist_sample(p_thing, 0x25, 0x7F, 0x40, 100, 0, 3);
                                    bang_new4(p_thing->X, p_thing->Y, p_thing->Z, 0x14);
                                    p_thing->State = 0x0D;
                                    p_thing->Flag |= TngF_Destroyed;
                                    p_thing->StartFrame = 0x42D;
                                    p_thing->Frame = nstart_ani[0x42D];
                                }
                            }
                        }
                    }
                    thing = p_thing->Next;
                }
                else
                {
                    struct SimpleThing *p_sthing;

                    p_sthing = &sthings[thing];
                    if ((p_sthing->Type == SmTT_SCALE_EFFECT) && !items_only)
                    {
                        dy = (p_sthing->Y >> 8) - y;
                        if ((dy < 0xB9) && (dy > -0xCD))
                        {
                            dx = (p_sthing->X >> 8) - x;
                            dz = (p_sthing->Z >> 8) - z;
                            dist = shockwave_dist(dx, dz);
                            if (dist < (ushort)p_sthing->Radius + radius)
                                do_shockwave_scale_effect(dx, dz, dist, intensity, p_sthing, NULL);
                        }
                    }
                    if (((p_sthing->Type == SmTT_DROPPED_ITEM) && (p_sthing->SubType == 0x0D)) ||
                      (p_sthing->SubType == 0x0C))
                    {
                        dy = (p_sthing->Y >> 8) - y;
                        if ((dy < 0xB9) && (dy > -0xCD))
                        {
                            dist = shockwave_dist((p_sthing->X >> 8) - x, (p_sthing->Z >> 8) - z);
                            if (dist < (ushort)p_sthing->Radius + radius)
                            {
                                p_sthing->Flag |= TngF_Unkn00040000;
                                if (p_sthing->Timer1 == 999)
                                    p_sthing->Timer1 = 140;
                            }
                        }
                    }
                    thing = sthings[thing].Next;
                }
            }
        }
    }
}

void do_shockwave(int x, int y, int z, int radius, int intensity, struct Thing *p_owner)
{
    FNCHK_VOID("do_shockwave", do_shockwave_asm(x, y, z, radius, intensity, p_owner),
      do_shockwave_c(x, y, z, radius, intensity, p_owner));
}

/** Remade from do_shockwave_building. */
void do_shockwave_building(int dist, int intensity, struct Thing *p_thing, struct Thing *p_owner)
{
    if ((p_thing->Flag & TngF_Destroyed) != 0)
        return;
    p_thing->Health = (short)(p_thing->Health - (intensity >> 4));
    if (p_thing->Health < 0)
        collapse_building(p_thing->X >> 8, p_thing->Y >> 8, p_thing->Z >> 8, p_thing);
}

/** Remade from do_shockwave_vehicle. */
void do_shockwave_vehicle(int dx, int dz, int dist, int intensity,
  struct Thing *p_vehicle, struct Thing *p_owner)
{
    int div;

    if ((p_vehicle->Flag & TngF_Destroyed) != 0)
        return;
    div = (dist >> 7) * (dist >> 7);
    if (div > 0)
        intensity /= div;
    if (p_vehicle->SubType == 0x33)
        intensity >>= 1;
    p_vehicle->Health = (short)(p_vehicle->Health - intensity);
    p_vehicle->U.UVehicle.RecoilTimer = 10;
    if (p_vehicle->Health > 0)
        return;
    if (p_vehicle->Type == TT_VEHICLE)
        start_crashing(p_vehicle);
    else
        init_mgun_explode(p_vehicle);
}

/** Remade from do_shockwave_person. */
void do_shockwave_person(int dx, int dz, int dist, int intensity,
  struct Thing *p_person, struct Thing *p_owner)
{
    int div;

    if ((p_person->Flag & TngF_Destroyed) != 0)
        return;
    div = (dist >> 7) * (dist >> 7);
    if (div > 0)
        intensity /= div;
    if (((p_person->U.UPerson.UMod.Mods >> 6) & 7) < 4)
        p_person->Health -= (short)intensity;
    if ((p_person->Type == TT_MINE) && (p_person->Health <= 0))
    {
        play_dist_ssample((struct SimpleThing *)p_person, 0x25, 0x7F, 0x40, 100, 0, 3);
        // the original shifts only y here
        bang_new4(p_person->X, p_person->Y >> 8, p_person->Z, 0x14);
        remove_thing(p_person->ThingOffset);
        delete_node(p_person);
        return;
    }
    p_person->U.UPerson.ShieldEnergy = 0;
    if (p_person->Health < 0)
    {
        if (p_owner != NULL)
        {
            if (!in_network_game)
            {
                if (p_owner->U.UPerson.EffectiveGroup == ingame.MyGroup)
                {
                    switch (p_person->SubType)
                    {
                    case 1: case 2: case 3: case 9: case 12:
                        mission_status[open_brief].SP.EnemiesKilled++;
                        break;
                    case 4: case 5: case 10: case 11: case 13: case 14:
                        mission_status[open_brief].SP.CivsKilled++;
                        break;
                    case 6: case 7: case 8:
                        mission_status[open_brief].SP.SecurityKilled++;
                        break;
                    default:
                        break;
                    }
                }
            }
            else if ((p_person->Flag & TngF_PlayerAgent) != 0)
            {
                struct Thing *p_killer;

                p_killer = NULL;
                if ((p_owner->Flag & TngF_PlayerAgent) != 0) {
                    p_killer = p_owner;
                } else if ((p_owner->Flag & TngF_Persuaded) != 0) {
                    if ((things[(ushort)p_owner->Owner].Flag & TngF_PlayerAgent) != 0)
                        p_killer = &things[(ushort)p_owner->Owner];
                }
                if ((p_killer != NULL) && ((p_killer->Flag & TngF_PlayerAgent) != 0))
                    mission_status[p_killer->U.UPerson.ComCur >> 2].MP.AgentsKilled[p_person->U.UPerson.ComCur >> 2]++;
            }
        }
        set_person_dead(p_person, 0x0B);
        return;
    }
    if ((p_person->Flag2 & TgF2_KnockedOut) != 0)
        return;
    if ((dx == 0) && (dz == 0))
    {
        dx = (LbRandomAnyShort() & 0x1FF) - 0x100;
        dz = (LbRandomAnyShort() & 0x1FF) - 0x100;
    }
    init_recoil(p_person, (short)dx, 0, (short)dz, 0);
    p_person->U.UPerson.RecoilTimer = 10;
}

/** Remade from do_shockwave_scale_effect. */
void do_shockwave_scale_effect(int dx, int dz, int dist, int intensity,
  struct SimpleThing *p_sthing, struct Thing *p_owner)
{
    int div;

    if (intensity > 2000)
        intensity = 2000;
    div = (dist >> 5) * (dist >> 5);
    if (div > 0)
        intensity /= div;
    p_sthing->U.UEffect.VX += (dx * intensity) >> 3;
    p_sthing->U.UEffect.VZ += (dz * intensity) >> 3;
}
/******************************************************************************/
