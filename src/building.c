/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file building.c
 *     Buildings related functions.
 * @par Purpose:
 *     Implement functions for handling buildings.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     27 May 2022 - 12 Jun 2023
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "building.h"
#include "pathtrig.h"
#include "triangls.h"
#include "trstate.h"
#include "thing_expld.h"
#include "enginfexpl.h"
#include <stdlib.h>

#include <assert.h>
#include "bfmath.h"
#include "bfmemory.h"
#include "bfutility.h"

#include "enginprops.h"
#include "enginsngobjs.h"

#include "ssampply.h"
#include "bigmap.h"
#include "bmbang.h"
#include "bmcrater.h"
#include "game_options.h"
#include "game_speed.h"
#include "game.h"
#include "matrix.h"
#include "sound.h"
#include "thing.h"
#include "tngcolisn.h"
#include "vehtraffic.h"
#include "weapon.h"
#include "people.h"
#include "pepgroup.h"
#include "swlog.h"
#include "fncheck.h"
/******************************************************************************/
ubyte dome_open_speed = 4;

struct BuildingStat bldng_type_stats[] = {
  {"BLD_NONE",},
  {"BLD_1",},
  {"BLD_2",},
  {"BLD_3",},
  {"BLD_4",},
  {"BLD_5",},
  {"BLD_6",},
  {"BLD_7",},
  {"BLD_8",},
  {"BLD_9",},
  {"BLD_A",},
  {"BLD_B",},
  {"BLD_C",},
  {"BLD_D",},
  {"BLD_E",},
  {"BLD_F",},
  {"BLD_10",},
  {"BLD_11",},
  {"BLD_12",},
  {"BLD_13",},
  {"SHUTTL_LDR",},
  {"BEZIER_ROAD",},
  {"BLD_16",},
  {"BLD_17",},
  {"DOME",},
  {"BLD_19",},
  {"BLD_1A",},
  {"STANDRD BLD",},
  {"TRAIN_TRACK",},
  {"BLD_1D",},
  {"BLD_1E",},
  {"BLD_1F",},
  {"MOUNT_GUN",},
  {"BLD_21",},
  {"BLD_22",},
  {"BLD_23",},
  {"BLD_24",},
  {"BLD_GATE",},
  {"BLD_26",},
  {"BLD_27",},
  {"BLD_28",},
  {"BLD_29",},
  {"BLD_2A",},
  {"BLD_2B",},
  {"BLD_2C",},
  {"BLD_2D",},
  {"BLD_2E",},
  {"TRAIN_STATN",},
  {"BLD_30",},
  {"BLD_31",},
  {"BLD_32",},
  {"BLD_33",},
  {"BLD_34",},
  {"BLD_35",},
  {"MOVN_ROTOR",},
  {"BLD_37",},
  {"BLD_38",},
  {"BLD_39",},
  {"BLD_3A",},
  {"BLD_3B",},
  {"BLD_3C",},
  {"BILLBOARD",},
  {"BLD_3E",},
  {"BLD_3F",},
  {"BLD_40",},
  {"BLD_41",},
  {"BLD_42",},
  {"BLD_43",},
  {"BLD_44",},
};


const char *building_type_name(ushort btype)
{
    struct BuildingStat *p_bldstat;

    p_bldstat = &bldng_type_stats[btype];
    if (strlen(p_bldstat->Name) == 0)
        return "OUTRNG_BLDNG";
    return p_bldstat->Name;
}

TbBool building_can_transform_open(ThingIdx bldng)
{
    struct Thing *p_building;

    p_building = &things[bldng];
    if (p_building->Type != TT_BUILDING)
        return false;
    if ((p_building->Flag & TngF_Destroyed) != 0)
        return false;
    return (p_building->SubType == SubTT_BLD_DOME);
}

#ifdef SW_FNCHK
static struct Thing *create_building_thing_asm(int x, int y, int z, ushort obj, ushort nobj, ushort a6)
{
    struct Thing *ret;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[2];

    stkargs[0] = (int)(intptr_t)nobj;
    stkargs[1] = (int)(intptr_t)a6;

    asm volatile (
      "push 4(%5)\n"
      "push 0(%5)\n"
      "call ASM_create_building_thing\n"
        : "=a" (ret)
        : "a" (x), "d" (y), "b" (z), "c" (obj), "S" (stkargs)
        : "cc", "memory");
    return ret;
}
#endif

/** Move thing of given index from the empty list to the used list, so that
 * a level can re-create a thing at its original index. Returns 0 if the
 * index is above the limit or the thing is already in use.
 */
static ushort get_new_thing_with_index(ushort thing)
{
    struct Thing *p_thing;
    ThingIdx used;

    if (thing > THINGS_LIMIT)
        return 0;
    p_thing = &things[thing];

    for (used = things_used_head; used != 0; used = things[(ushort)used].LinkChild)
    {
        if ((ushort)used == thing)
            return 0;
    }

    if (thing == (ushort)things_empty_head)
    {
        things_empty_head = p_thing->LinkChild;
        things[p_thing->LinkChild].LinkParent = 0;
    }
    else
    {
        if (p_thing->LinkParent != 0)
            things[p_thing->LinkParent].LinkChild = p_thing->LinkChild;
        if (p_thing->LinkChild != 0)
            things[p_thing->LinkChild].LinkParent = p_thing->LinkParent;
    }
    things[(ushort)things_used_head].LinkParent = thing;
    p_thing->LinkParent = 0;
    p_thing->LinkChild = things_used_head;
    things_used_head = thing;
    things_used++;
    p_thing->Flag = 0;
    p_thing->Flag2 = 0;
    return 1;
}

/** Create a building thing for objects `obj` .. `obj`+`nobj`-1, at given
 * index `a6` if possible (when it is not negative), else at any free one.
 */
static struct Thing *create_building_thing_c(int x, int y, int z, ushort obj, ushort nobj, ushort a6)
{
    struct Thing *p_building;
    ushort thing, i;

    thing = a6;
    if (((short)a6 < 0) || (get_new_thing_with_index(a6) == 0))
        thing = get_new_thing();

    p_building = &things[thing];
    p_building->Type = TT_BUILDING;
    p_building->X = x << 8;
    p_building->Z = z << 8;
    p_building->U.UObject.Angle = 0;
    p_building->Y = y;
    p_building->U.UObject.Object = obj;
    if ((nobj == 0) || (nobj > 250))
        nobj = 1;
    p_building->U.UObject.NumbObjects = nobj;
    p_building->Health = 8024;
    for (i = 0; i < nobj; i++)
        game_objects[obj + i].ThingNo = thing;
    add_node_thing(thing);
    return p_building;
}

struct Thing *create_building_thing(int x, int y, int z, ushort obj, ushort nobj, ushort a6)
{
    struct Thing *ret;
    FNCHK_RET("create_building_thing", ret,
      create_building_thing_asm(x, y, z, obj, nobj, a6),
      create_building_thing_c(x, y, z, obj, nobj, a6));
    return ret;
}

void do_dome_rotate1(struct Thing *p_building)
{
#if 0
    asm volatile ("call ASM_do_dome_rotate1\n"
        : : "a" (p_building));
#endif
    ushort obj, i;
    ushort pt1_beg, pt1_num;

    obj = p_building->U.UObject.Object;
    pt1_beg = game_objects[obj].StartPoint;
    pt1_num = game_objects[obj].EndPoint - pt1_beg;
    for (i = obj + 1; i < obj + 8; i++)
    {
        int factrS, factrC;
        short angle;
        ushort pt2_beg;
        ushort dpt;

        angle = (i - obj) * p_building->SubState;
        pt2_beg = game_objects[i].StartPoint;
        factrC = lbSinTable[angle + LbFPMath_PI/2];
        factrS = lbSinTable[angle];
        for (dpt = 0; dpt <= pt1_num; dpt++)
        {
            struct SinglePoint *p_point1;
            struct SinglePoint *p_point2;
            p_point1 = &game_object_points[pt1_beg + dpt];
            p_point2 = &game_object_points[pt2_beg + dpt];
            p_point2->Y = (factrC * p_point1->Y - factrS * p_point1->Z) >> 16;
            p_point2->Z = (factrC * p_point1->Z + factrS * p_point1->Y) >> 16;
        }
    }
}

void process_dome1(struct Thing *p_building)
{
#if 0
    asm volatile ("call ASM_process_dome1\n"
        : : "a" (p_building));
#endif
    uint timer0;

    switch (p_building->State)
    {
    case BldSt_TRA_OPENING:
        timer0 = p_building->SubState - dome_open_speed;
        p_building->SubState = timer0;
        if (timer0 <= 127)
        {
            if ((timer0 >= 111u) && (timer0 < 111u + dome_open_speed)) {
                set_dome_col(p_building, 1);
            }
            do_dome_rotate1(p_building);
        }
        else
        {
            play_dist_sample(p_building, 47, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
            p_building->SubState = 0;
            p_building->Timer1 = 100;
            p_building->State = BldSt_TRA_OPENED;
        }
        break;
    case BldSt_TRA_CLOSING:
        timer0 = p_building->SubState + dome_open_speed;
        p_building->SubState = timer0;
        if (timer0 <= 127)
        {
            do_dome_rotate1(p_building);
        }
        else
        {
            play_dist_sample(p_building, 47, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
            p_building->SubState = 127;
            p_building->Timer1 = 100;
            p_building->State = BldSt_TRA_CLOSED;
        }
        break;
    case BldSt_TRA_OPENED:
        if ((p_building->Flag & TngF_TransCloseRq) != 0) {
            play_dist_sample(p_building, 100, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
            p_building->State = BldSt_TRA_CLOSING;
        }
        p_building->Flag &= ~(TngF_TransCloseRq|TngF_TransOpenRq);
        break;
    case BldSt_TRA_CLOSED:
        if ((p_building->Flag & TngF_TransOpenRq) != 0) {
            play_dist_sample(p_building, 100, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
            p_building->State = BldSt_TRA_OPENING;
        }
        p_building->Flag &= ~(TngF_TransCloseRq|TngF_TransOpenRq);
        break;
    default:
        p_building->SubState = 127;
        p_building->Timer1 = 100;
        p_building->State = BldSt_TRA_CLOSED;
        break;
    }
}

#ifdef SW_FNCHK
static ushort explode_thing_building_asm(short thing, short x, short y, short z)
{
    int ret;
    asm volatile ("call ASM_explode_thing_building\n"
        : "=a" (ret) : "a" (thing), "d" (x), "b" (y), "c" (z) : "cc", "memory");
    return ret;
}
#endif

extern ubyte redo_scanner[128][4];
extern s32 redo_scanner_last;
extern s32 redo_scanner_first;
extern short link__MapColListEmptyHead;
void set_person_dead(struct Thing *p_person, ushort anim_mode);
void do_shockwave_person(int dx, int dz, int dist, int intensity,
  struct Thing *p_thing, struct Thing *p_owner);
void init_mech_explode(struct Thing *p_vehicle);
void start_crashing(struct Thing *p_vehicle);

/** Whether the face (negative for a quad face) belongs to one of the
 * objects of the building.  Remade from sit_on_my_face. */
static TbBool sit_on_my_face(short face, struct Thing *p_building)
{
    int i;

    for (i = 0; i < p_building->U.UObject.NumbObjects; i++)
    {
        struct SingleObject *p_sobj;
        int first;

        p_sobj = &game_objects[p_building->U.UObject.Object + i];
        if (face < 0) {
            first = p_sobj->StartFace4;
            if ((-face >= first) && (-face <= first + p_sobj->NumbFaces4))
                return true;
        } else {
            first = p_sobj->StartFace;
            if ((face >= first) && (face <= first + p_sobj->NumbFaces))
                return true;
        }
    }
    return false;
}

/** Removes from the tile the first collision vector of the thing; returns
 * whether one was found.  Remade from del_thing_vectors_from_mapwho. */
static TbBool del_thing_vectors_from_mapwho(struct MyMapElement *p_mapel, short thing)
{
    ushort colvl, prev;

    prev = 0;
    colvl = p_mapel->ColHead;
    while (colvl != 0)
    {
        struct ColVectList *p_colvl;
        ushort next;

        p_colvl = &game_col_vects_list[colvl];
        next = p_colvl->NextColList;
        if (p_colvl->Object == thing)
        {
            if (prev != 0)
                game_col_vects_list[prev].NextColList = next;
            else
                p_mapel->ColHead = next & 0x7FFF;
            p_colvl->NextColList = link__MapColListEmptyHead;
            link__MapColListEmptyHead = colvl;
            return true;
        }
        prev = colvl;
        colvl = next & 0x7FFF;
    }
    return false;
}

/** People standing on the faces of an exploding building fall to the
 * ground; those over a deadly tile die.
 * Remade from do_people_on_exploding_building. */
static void do_people_on_exploding_building(short building)
{
    ThingIdx thing;
    ushort count;

    count = things_used;
    thing = things_used_head;
    if (thing <= 0)
        return;
    do
    {
        struct Thing *p_person;

        if (count-- == 0)
            break;
        p_person = &things[thing];
        thing = p_person->LinkChild;
        if ((p_person->Type != TT_PERSON) || ((p_person->Flag & 0x10000000) != 0))
            continue;
        if (p_person->U.UPerson.OnFace == 0)
            continue;
        if (!sit_on_my_face(p_person->U.UPerson.OnFace, &things[building]))
            continue;
        p_person->Y = alt_at_point((short)(p_person->X >> 8), (short)(p_person->Z >> 8));
        p_person->U.UPerson.OnFace = 0;
        if ((p_person->Flag & TngF_Destroyed) != 0)
            continue;
        if ((game_my_big_map[((p_person->Z >> 16) << 7) + (p_person->X >> 16)].Flags2 & 0x04) == 0)
            continue;
        p_person->U.UPerson.WeaponsCarried = 0;
        set_person_dead(p_person, 0x0A);
    }
    while (thing > 0);
}

static TbBool building_subtype_with_things_inside(ubyte subtype)
{
    return (subtype == SubTT_BLD_SHUTLDR) || (subtype == SubTT_BLD_1D)
      || (subtype == SubTT_BLD_TRAINTRK) || (subtype == SubTT_BLD_BEZIER_ROAD)
      || (subtype == SubTT_BLD_MGUN);
}

/** A building blows up: its faces become flying explode faces, the map
 * under it is cleared (collision vectors, traffic links, things standing
 * there), and things around get the shockwave.  Returns the next object of
 * the last object of the building.  Remade from explode_thing_building.
 */
static ushort explode_thing_building_c(short thing, short x, short y, short z)
{
    struct Thing *p_building;
    struct SingleObject *p_sobj;
    s32 min_x, min_z, max_x, max_z;
    s32 intensity, mid_x, mid_z;
    s32 tx, tz;
    int i, obj;

    dont_bother_with_explode_faces = 0;
    p_building = &things[thing];
    p_sobj = NULL;
    obj = p_building->U.UObject.Object;
    for (i = 0; i < p_building->U.UObject.NumbObjects; i++, obj++)
    {
        s32 base_x, base_y, base_z;
        int face, n;

        p_sobj = &game_objects[obj];
        if (p_building->SubType == SubTT_BLD_MGUN) {
            base_x = p_building->X >> 8;
            base_y = p_building->Y >> 5;
            base_z = p_building->Z >> 8;
        } else {
            do_people_on_exploding_building(thing);
            base_x = p_sobj->MapX;
            base_y = p_sobj->OffsetY;
            base_z = p_sobj->MapZ;
        }
        n = p_sobj->NumbFaces;
        for (face = p_sobj->StartFace; face < p_sobj->StartFace + n; face++)
        {
            struct SingleObjectFace3 *p_face;
            struct SinglePoint *p_pt;
            s32 px[3], py[3], pz[3];
            s32 cx, cz;
            ushort eface;
            int k;

            if (n <= 0)
                break;
            p_face = &game_object_faces3[face];
            if ((p_face->GFlags & 0x04) != 0) {
                p_face->GFlags &= ~0x04;
                make_triangle_solid(p_face->UnknTringl);
            }
            for (k = 0; k < 3; k++) {
                p_pt = &game_object_points[(ushort)p_face->PointNo[k]];
                px[k] = base_x + p_pt->X;
                py[k] = base_y + p_pt->Y;
                pz[k] = base_z + p_pt->Z;
            }
            cx = (px[0] + px[1] + px[2]) / 3;
            cz = (pz[0] + pz[1] + pz[2]) / 3;
            eface = explode_face_alloc();
            if (eface != 0)
            {
                struct ExplodeFace *p_exface;

                p_exface = &ex_faces[eface];
                p_exface->Type = 3;
                p_exface->Texture = p_face->Texture;
                p_exface->Flags = p_face->Flags;
                p_exface->Col = (ubyte)p_face->ExCol;
                p_exface->X0 = px[0]; p_exface->Y0 = py[0]; p_exface->Z0 = pz[0];
                p_exface->X1 = px[1]; p_exface->Y1 = py[1]; p_exface->Z1 = pz[1];
                p_exface->X2 = px[2]; p_exface->Y2 = py[2]; p_exface->Z2 = pz[2];
                p_exface->DX = (cx - base_x) >> 7;
                p_exface->DY = 1 - (LbRandomAnyShort() & 7);
                p_exface->Timer = 1;
                p_exface->DZ = (cz - base_z) >> 7;
            }
        }
        n = p_sobj->NumbFaces4;
        for (face = p_sobj->StartFace4; face < p_sobj->StartFace4 + n; face++)
        {
            struct SingleObjectFace4 *p_face;
            struct SinglePoint *p_pt;
            s32 px[4], py[4], pz[4];
            s32 cx, cz;
            ushort eface;
            int k;

            if (n <= 0)
                break;
            p_face = &game_object_faces4[face];
            if ((p_face->GFlags & 0x04) != 0) {
                p_face->GFlags &= ~0x04;
                make_triangle_solid(p_face->UnknTringl2);
                make_triangle_solid(p_face->UnknTringl1);
            }
            for (k = 0; k < 4; k++) {
                p_pt = &game_object_points[(ushort)p_face->PointNo[k]];
                px[k] = base_x + p_pt->X;
                py[k] = base_y + p_pt->Y;
                pz[k] = base_z + p_pt->Z;
            }
            cx = (px[0] + px[1] + px[2] + px[3]) / 4;
            cz = (pz[0] + pz[1] + pz[2] + pz[3]) / 4;
            eface = explode_face_alloc();
            if (eface != 0)
            {
                struct ExplodeFace *p_exface;

                p_exface = &ex_faces[eface];
                p_exface->Type = 4;
                p_exface->Texture = p_face->Texture;
                p_exface->Flags = p_face->Flags;
                p_exface->Col = (ubyte)p_face->ExCol;
                p_exface->X0 = px[0]; p_exface->Y0 = py[0]; p_exface->Z0 = pz[0];
                p_exface->X1 = px[1]; p_exface->Y1 = py[1]; p_exface->Z1 = pz[1];
                p_exface->X2 = px[2]; p_exface->Y2 = py[2]; p_exface->Z2 = pz[2];
                p_exface->X3 = px[3]; p_exface->Y3 = py[3]; p_exface->Z3 = pz[3];
                p_exface->DX = (cx - base_x) >> 7;
                p_exface->Timer = 1;
                p_exface->DZ = (cz - base_z) >> 7;
                p_exface->DY = 1 - (LbRandomAnyShort() & 7);
            }
        }
    }

    // Area of the map taken by the building
    max_x = (s32)0x80000000;
    max_z = (s32)0x80000000;
    min_x = 0x7FFFFFFF;
    min_z = 0x7FFFFFFF;
    obj = p_building->U.UObject.Object;
    for (i = 0; i < p_building->U.UObject.NumbObjects; i++, obj++)
    {
        int pt;

        p_sobj = &game_objects[obj];
        for (pt = p_sobj->StartPoint; pt <= p_sobj->EndPoint; pt++)
        {
            s32 px, pz;

            px = p_sobj->MapX + game_object_points[pt].X;
            pz = p_sobj->MapZ + game_object_points[pt].Z;
            if (max_x <= px) max_x = px;
            if (min_x >= px) min_x = px;
            if (max_z <= pz) max_z = pz;
            if (min_z >= pz) min_z = pz;
        }
    }
    max_x = (max_x + 0x100) >> 8;
    min_x >>= 8;
    min_z >>= 8;
    max_z = (max_z + 0x100) >> 8;
    if (min_x < 0) min_x = 0;
    if (min_z < 0) min_z = 0;
    if (max_x > 127) max_x = 127;
    if (max_z > 127) max_z = 127;
    intensity = ((max_x - min_x) + (max_z - min_z)) << 9;
    if (intensity > 20000)
        intensity = 20000;
    mid_x = (min_x + max_x) << 15;
    mid_z = (min_z + max_z) << 15;

    for (tx = min_x; tx <= max_x; tx++)
    {
        for (tz = min_z; tz <= max_z; tz++)
        {
            struct MyMapElement *p_mapel;

            p_mapel = &game_my_big_map[(tz << 7) + tx];
            p_mapel->Flags &= 0x7F;
            p_mapel->ColumnHead = 0;
            p_mapel->Flags2 &= ~0x08;
            if (p_building->SubType == SubTT_BLD_GATE)
            {
                int tn;

                p_mapel->Flags2 |= 0x08;
                for (tn = 1; tn < next_traffic_node; tn++)
                {
                    struct TrafficNode *p_tnode;

                    p_tnode = &game_traffic_nodes[tn];
                    if (p_tnode->GateLink == thing) {
                        p_tnode->GateLink = 0;
                        p_tnode->Flags &= 0x7FFF;
                    }
                }
            }
            while (del_thing_vectors_from_mapwho(p_mapel, thing))
                ;
            if (building_subtype_with_things_inside(p_building->SubType))
                continue;
            {
                ThingIdx child;

                child = p_mapel->Child;
                while (child != 0)
                {
                    if (child > 0)
                    {
                        struct Thing *p_thing;

                        p_thing = &things[child];
                        child = p_thing->Next;
                        if (p_thing->Type == TT_BUILDING)
                        {
                            if (!building_subtype_with_things_inside(p_thing->SubType))
                                continue;
                            if ((p_thing->Flag & TngF_Destroyed) != 0)
                                continue;
                            if (p_thing->SubType == SubTT_BLD_MGUN)
                                init_mgun_explode(p_thing);
                            else
                                collapse_building(x, y, z, p_thing);
                        }
                        else if ((p_thing->Type == TT_MINE) && (p_thing->SubType == 0x30))
                        {
                            bang_new4(p_thing->X, p_thing->Y, p_thing->Z, 0x14);
                            p_thing->State = 0x0D;
                            p_thing->Flag |= TngF_Destroyed;
                            remove_thing(p_thing->ThingOffset);
                            delete_node(p_thing);
                        }
                    }
                    else
                    {
                        struct SimpleThing *p_sthing;

                        p_sthing = &sthings[child];
                        child = p_sthing->Next;
                        if ((p_sthing->Type == SmTT_DROPPED_ITEM) && (p_sthing->U.UWeapon.OnFace != 0))
                        {
                            remove_sthing(p_sthing->ThingOffset);
                            delete_snode(p_sthing);
                        }
                    }
                }
            }
        }
    }

    triangulation_unkn_func_002(min_x, min_z, max_x, max_z);
    {
        s32 next;

        next = (redo_scanner_last + 1) & 0x7F;
        if (next != redo_scanner_first)
        {
            redo_scanner[redo_scanner_last][0] = min_z << 1;
            redo_scanner[redo_scanner_last][1] = min_x << 1;
            redo_scanner[redo_scanner_last][2] = max_z << 1;
            redo_scanner[redo_scanner_last][3] = max_x << 1;
            redo_scanner_last = next;
        }
    }

    // Shockwave on the surroundings
    min_x -= 6;
    min_z -= 6;
    max_x += 6;
    max_z += 6;
    if (min_x < 0) min_x = 0;
    if (min_z < 0) min_z = 0;
    if (max_x > 127) max_x = 127;
    if (max_z > 127) max_z = 127;
    for (tx = min_x; tx <= max_x; tx++)
    {
        for (tz = min_z; tz <= max_z; tz++)
        {
            ThingIdx child;

            child = game_my_big_map[(tz << 7) + tx].Child;
            while (child != 0)
            {
                struct Thing *p_thing;

                if (child < 0) {
                    child = sthings[child].Next;
                    continue;
                }
                p_thing = &things[child];
                if (p_thing->Type == TT_PERSON)
                    p_thing->Flag2 &= ~0x20000000;
                if ((p_thing->Type == TT_PERSON) && ((p_thing->Flag & TngF_Destroyed) == 0)
                  && (p_thing->State != 0x24))
                {
                    s32 dx, dz, dist_x, dist_z, dist;
                    s32 ptx, ptz, d1, d2;

                    dx = (p_thing->X - mid_x) >> 8;
                    dz = (p_thing->Z - mid_z) >> 8;
                    ptx = p_thing->X >> 16;
                    ptz = p_thing->Z >> 16;
                    d1 = abs(ptx - (min_x + 6));
                    d2 = abs(ptx - (max_x - 6));
                    dist_x = (d1 >= d2) ? d2 : d1;
                    d1 = abs(ptz - (min_z + 6));
                    d2 = abs(ptz - (max_z - 6));
                    dist_z = (d1 < d2) ? d1 : d2;
                    dist = (dist_x < dist_z) ? dist_x : dist_z;
                    do_shockwave_person(dx, dz, (dist << 7) + 0x100, intensity, p_thing, NULL);
                }
                if (p_thing->Type == TT_VEHICLE)
                {
                    short dmg;

                    p_thing->U.UVehicle.WorkPlace |= 0x0080;
                    dmg = 3500;
                    if (p_thing->SubType == 0x33)
                        dmg = 1750;
                    if (p_thing->SubType == 0x3B)
                        dmg >>= 2;
                    p_thing->Health -= dmg;
                    if (p_thing->Health <= 0)
                    {
                        p_thing->Flag |= TngF_Destroyed;
                        if (p_thing->SubType == 0x3B) {
                            init_mech_explode(p_thing);
                        } else {
                            start_crashing(p_thing);
                            play_dist_sample(p_thing, 0x5F, 0x7F, 0x40, 100, 0, 1);
                        }
                    }
                }
                child = p_thing->Next;
            }
        }
    }
    if (p_sobj == NULL)
        return 0;
    return p_sobj->NextObject;
}

ushort explode_thing_building(short thing, short x, short y, short z)
{
    ushort ret;

    FNCHK_RET("explode_thing_building", ret, explode_thing_building_asm(thing, x, y, z),
      explode_thing_building_c(thing, x, y, z));
    return ret;
}

void collapse_building_process_tnodes(struct Thing *p_building)
{
    short tnode1, tnode2;
    int i;

    for (i = 0; i < 4; i++)
    {
        short tnode;
        tnode = p_building->U.UObject.tnode_50[i];
        if (tnode != 0) {
            struct TrafficNode *p_tnode;
            p_tnode = &game_traffic_nodes[tnode];
            p_tnode->Flags |= (0x2000|0x1000);
        }
    }

    tnode1 = p_building->U.UObject.tnode_50[0];
    tnode2 = p_building->U.UObject.tnode_50[1];
    if ((tnode1 != 0) && (tnode2 != 0))
    {
        struct TrafficNode *p_tnode;
        p_tnode = &game_traffic_nodes[tnode1];
        for (i = 0; i < 4; i++)
        {
            if (p_tnode->UTraffic.Link[i] == -tnode2) {
                break;
            }
            if (p_tnode->UTraffic.Link[i] == 0) {
                p_tnode->UTraffic.Link[i] = -tnode2;
                p_tnode->UTraffic.Flags[i] = 0x40;
                break;
            }
        }
    }

    tnode1 = p_building->U.UObject.tnode_50[2];
    tnode2 = p_building->U.UObject.tnode_50[3];
    if ((tnode1 != 0) && (tnode2 != 0))
    {
        struct TrafficNode *p_tnode;
        p_tnode = &game_traffic_nodes[tnode1];
        for (i = 0; i < 4; i++)
        {
            if (p_tnode->UTraffic.Link[i] == -tnode2) {
                break;
            }
            if (p_tnode->UTraffic.Link[i] == 0) {
                p_tnode->UTraffic.Link[i] = -tnode2;
                p_tnode->UTraffic.Flags[i] = 0x40;
                break;
            }
        }
    }

    tnode1 = p_building->U.UObject.tnode_50[0];
    tnode2 = p_building->U.UObject.tnode_50[3];
    if ((tnode1 != 0) && (tnode2 != 0))
    {
        struct TrafficNode *p_tnode;
        p_tnode = &game_traffic_nodes[tnode1];
        for (i = 0; i < 4; i++)
        {
            if (p_tnode->UTraffic.Link[i] == -tnode2) {
                p_tnode->UTraffic.Link[i] = 0;
                break;
            }
        }
    }

    tnode1 = p_building->U.UObject.tnode_50[2];
    tnode2 = p_building->U.UObject.tnode_50[1];
    if ((tnode1 != 0) && (tnode2 != 0))
    {
        struct TrafficNode *p_tnode;
        p_tnode = &game_traffic_nodes[tnode1];
        for (i = 0; i < 4; i++)
        {
            if (p_tnode->UTraffic.Link[i] == -tnode2) {
                p_tnode->UTraffic.Link[i] = 0;
                break;
            }
        }
    }
}

void collapse_building_shuttle_loader(short x, short y, short z, struct Thing *p_building)
{
    struct Thing *p_thing;
    short thing;

    p_thing = p_building;
    thing = p_building->ThingOffset;
    while ( 1 )
    {
        short pvthing;
        pvthing = thing;
        thing = p_thing->U.UObject.NextThing;
        if (thing == 0)
            break;
        if (thing <= 0)
        {
            if (p_building->SubType == SubTT_BLD_BEZIER_ROAD) {
                tnode_all_unlink_thing(pvthing);
            }
            break;
        }
        else
        {
            p_thing = &things[thing];
            if ((p_thing->Flag & TngF_Destroyed) != 0)
                break;
            p_thing->Flag |= TngF_Destroyed;
            explode_thing_building(thing, x, y, z);
            p_thing->Type = TT_UNKN55;
        }
    }

    p_thing = p_building;
    thing = p_building->ThingOffset;
    while ( 1 )
    {
        short nxthing;
        nxthing = thing;
        thing = p_thing->U.UObject.PrevThing;
        if (thing == 0)
            break;
        if (thing <= 0)
        {
            if (p_building->SubType == SubTT_BLD_BEZIER_ROAD) {
                tnode_all_unlink_thing(nxthing);
            }
            break;
        }
        else
        {
            p_thing = &things[thing];
            if ((p_thing->Flag & TngF_Destroyed) != 0)
                break;
            p_thing->Flag |= TngF_Destroyed;
            explode_thing_building(thing, x, y, z);
            p_thing->Type = TT_UNKN55;
        }
    }
}

void collapse_building_station(struct Thing *p_building)
{
    MapCoord cntr_cor_x, cntr_cor_z;
    short cntr_x, cntr_z;
    short x, z;

    get_thing_position_mapcoords(&cntr_cor_x, NULL, &cntr_cor_z, p_building->ThingOffset);
    cntr_x = MAPCOORD_TO_TILE(cntr_cor_x);
    cntr_z = MAPCOORD_TO_TILE(cntr_cor_z);

    for (x = cntr_x - 8; x <= cntr_x + 8; x++)
    {
        if ((x < 0) || (x > MAP_TILE_WIDTH-1))
            continue;
        for (z = cntr_z - 8; z <= cntr_z + 8; z++)
        {
            struct MyMapElement *p_mapel;
            short thing;

            if ((z < 0) || (z > MAP_TILE_HEIGHT-1))
                continue;

            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * z + x];
            thing = p_mapel->Child;
            while (thing != 0)
            {
                if (thing > 0)
                {
                    struct Thing *p_thing;
                    p_thing = &things[thing];
                    if ((p_thing->Type == TT_BUILDING)
                      && (p_thing->SubType == SubTT_BLD_TRAINTRK)
                      && ((p_thing->Flag & TngF_Destroyed) == 0))
                    {
                        collapse_building(TILE_TO_MAPCOORD(x,0), 0, TILE_TO_MAPCOORD(z,0), p_thing);
                        break; // do not expect more than one building on a tile
                    }
                    thing = p_thing->Next;
                }
                else
                {
                    struct SimpleThing *p_sthing;
                    p_sthing = &sthings[thing];
                    thing = p_sthing->Next;
                }
            }
        }
    }
}

#ifdef SW_FNCHK
static void init_mgun_explode_asm(struct Thing *p_thing)
{
    asm volatile (
      "call ASM_init_mgun_explode\n"
        : "+a" (p_thing) : : "cc", "memory");
}
#endif

/** A mounted gun is destroyed: a bang, the gun goes and a shock wave as big
 * as 4 times its radius.  Remade from init_mgun_explode. */
static void init_mgun_explode_c(struct Thing *p_thing)
{
    if ((p_thing->Flag & TngF_Destroyed) != 0)
        return;
    p_thing->Flag |= TngF_Destroyed;
    play_dist_sample(p_thing, 0x25, 0x7F, 0x40, 100, 0, 3);
    p_thing->State = 0x0D;
    if ((p_thing->X < 0) || ((p_thing->X >> 8) >= 0x8000) ||
      ((p_thing->Z >> 8) < 0) || ((p_thing->Z >> 8) >= 0x8000))
        return;
    bang_new4(p_thing->X, p_thing->Y, p_thing->Z, 0x0F);
    delete_node(p_thing);
    remove_thing(p_thing->ThingOffset);
    do_shockwave(p_thing->X >> 8, p_thing->Y >> 8, p_thing->Z >> 8,
      (ushort)p_thing->Radius << 2, 5000, NULL);
}

void collapse_building(short x, short y, short z, struct Thing *p_building)
{
#if 0
    asm volatile (
      "call ASM_collapse_building\n"
        :  : "a" (x), "d" (y), "b" (z), "c" (p_thing));
    return;
#endif
    struct SimpleThing *p_sthing;
    short thing;
    int i;

    if (ingame.SoundThing != 0)
    {
        p_sthing = &sthings[ingame.SoundThing];
        p_sthing->X = p_building->X;
        p_sthing->Y = p_building->Y;
        p_sthing->Z = p_building->Z;
        p_sthing->State = 1;
    }
    else
    {
        short tng_x, tng_y, tng_z;
        get_thing_position_mapcoords(&tng_x, &tng_y, &tng_z, p_building->ThingOffset);
        play_dist_sample(p_building, 45, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
        p_sthing = create_sound_effect(tng_x, tng_y, tng_z, 46, FULL_VOL, -1);
        if (p_sthing != NULL)
        {
            p_sthing->State = 1;
            ingame.SoundThing = p_sthing->ThingOffset;
        }
    }
    thing = p_building->ThingOffset;

    collapse_building_process_tnodes(p_building);

    switch (p_building->SubType)
    {
    case SubTT_BLD_SHUTLDR:
    case SubTT_BLD_1D:
    case SubTT_BLD_TRAINTRK:
    case SubTT_BLD_BEZIER_ROAD:
        collapse_building_shuttle_loader(x, y, z, p_building);
        break;
    case SubTT_BLD_STATION:
        collapse_building_station(p_building);
        break;
    }

    if ((p_building->Flag & TngF_Destroyed) == 0)
    {
        struct SingleObject *p_sobj;

        p_sobj = &game_objects[p_building->U.UObject.Object];
        if (((p_sobj->field_1C & 0x0100) == 0) || ((render_floor_flags & RendFlrF_WobblyTerrain) != 0))
        {
            short cra_cor_x, cra_cor_z;

            get_thing_position_mapcoords(&cra_cor_x, NULL, &cra_cor_z, p_building->ThingOffset);
            quick_crater(MAPCOORD_TO_TILE(cra_cor_x), MAPCOORD_TO_TILE(cra_cor_z), 3);
            for (i = 0; i < 32; i++)
            {
                int dx, dz;
                int bang_x, bang_z;
                dz = (LbRandomAnyShort() & 0xFF) - 127;
                dx = (LbRandomAnyShort() & 0xFF) - 127;
                bang_z = ((z + dz) << 8);
                bang_x = ((x + dx) << 8);
                bang_new4(bang_x, 0, bang_z, 65);
            }
        }
        p_building->Flag |= TngF_Destroyed;
        explode_thing_building(thing, x, y, z);
        p_building->Type = TT_UNKN55;
    }
}

short create_a_pod(struct Thing *p_thing, struct Thing *p_loader, ushort pod_id)
{
    short ret;
    asm volatile (
      "call ASM_create_a_pod\n"
        : "=r" (ret) : "a" (p_thing), "d" (p_loader), "b" (pod_id));
    return ret;
}

#ifdef SW_FNCHK
static void process_shuttle_loader_asm(struct Thing *p_building)
{
    asm volatile ("call ASM_process_shuttle_loader\n"
        : : "a" (p_building) : "cc", "memory");
}
#endif

/** A shuttle loader puts each person on its tile (VX, VZ) into a pod.
 * Remade from process_shuttle_loader. */
static void process_shuttle_loader_c(struct Thing *p_building)
{
    ThingIdx thing;
    s32 tile_x, tile_z;

    tile_x = p_building->VX;
    tile_z = p_building->VZ;
    if ((tile_x < 0) || (tile_x >= 0x80) || (tile_z < 0) || (tile_z >= 0x80))
        return;
    thing = game_my_big_map[(tile_z << 7) + tile_x].Child;
    while (thing != 0)
    {
        if (thing > 0)
        {
            struct Thing *p_thing;

            p_thing = &things[thing];
            if ((p_thing->Type == TT_PERSON) && ((p_thing->Flag & 0x4000) == 0))
                thing = create_a_pod(p_thing, p_building, 0);
            else
                thing = p_thing->Next;
        }
        else
        {
            thing = sthings[thing].Next;
        }
    }
}

void process_shuttle_loader(struct Thing *p_building)
{
    FNCHK_VOID("process_shuttle_loader", process_shuttle_loader_asm(p_building),
      process_shuttle_loader_c(p_building));
}

#ifdef SW_FNCHK
static void bul_hit_vector_asm(int x, int y, int z, short col, int hp, int type)
{
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[2];

    stkargs[0] = (int)(intptr_t)hp;
    stkargs[1] = (int)(intptr_t)type;

    asm volatile (
      "push 4(%4)\n"
      "push 0(%4)\n"
      "call ASM_bul_hit_vector\n"
        : : "a" (x), "d" (y), "b" (z), "c" (col), "S" (stkargs)
        : "cc", "memory");
}
#endif

/** A shot hit the wall of an object (col is a ColVectList index): a
 * building loses health hp/2 and collapses below zero.
 * Remade from bul_hit_vector; x, y, z and type are not used.
 */
static void bul_hit_vector_c(int x, int y, int z, short col, int hp, int type)
{
    struct ColVectList *p_cvlist;
    struct Thing *p_building;
    int health;

    p_cvlist = &game_col_vects_list[col];
    if (game_col_vects[p_cvlist->Vect].Face == 0)
        return;
    p_building = &things[p_cvlist->Object];
    if ((p_building->Flag & TngF_Destroyed) != 0)
        return;
    play_dist_sample(p_building, 0x41, 0x7F, 0x40, 100, 0, 1);
    health = p_building->Health - (((short)hp) >> 1);
    if (health < 0)
        collapse_building(p_building->X >> 8, p_building->Y >> 8, p_building->Z >> 8, p_building);
    p_building->Health = (short)health;
}

void bul_hit_vector(int x, int y, int z, short col, int hp, int type)
{
    FNCHK_VOID("bul_hit_vector", bul_hit_vector_asm(x, y, z, col, hp, type),
      bul_hit_vector_c(x, y, z, col, hp, type));
}

#ifdef SW_FNCHK
static void rotate_object_asm(struct Thing *p_object)
{
    asm volatile ("call ASM_rotate_object\n"
        : "+a" (p_object) : : "cc", "memory");
}
#endif

void rotate_obj(short angle_a, short angle_b, short angle_c, struct M33 *p_mat);

/** Turns a mounted gun by its turn rate and sets its matrix from its
 * angles.  Remade from rotate_object. */
static void rotate_object_c(struct Thing *p_object)
{
    struct M33 *p_mat;

    p_object->U.UMGun.AngleY = (p_object->U.UMGun.AngleY + (p_object->U.UMGun.AngleDY >> 3)
      + 0x800) & 0x7FF;
    p_mat = &local_mats[p_object->U.UMGun.MatrixIndex];
    if (p_object->U.UMGun.AngleX > 0x400)
        p_object->U.UMGun.AngleX -= 0x800;
    rotate_obj(0, -p_object->U.UMGun.AngleY, 0, p_mat);
    rotate_object_axis(p_mat, 0, -p_object->U.UMGun.AngleX, 0);
}

#ifdef SW_FNCHK
static int mounted_los_asm(int x1, int y1, int z1, int x2, int y2, int z2)
{
    int ret;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[2];

    stkargs[0] = (int)(intptr_t)y2;
    stkargs[1] = (int)(intptr_t)z2;

    asm volatile (
      "push 4(%5)\n"
      "push 0(%5)\n"
      "call ASM_mounted_los\n"
        : "=r" (ret)
        : "a" (x1), "d" (y1), "b" (z1), "c" (x2), "S" (stkargs)
        : "cc", "memory");
    return ret;
}

static ThingIdx aquire_target_asm(struct Thing *p_gun)
{
    ThingIdx ret;
    asm volatile (
      "call ASM_aquire_target\n"
        : "=a" (ret) : "0" (p_gun) : "cc", "memory");
    return ret;
}
#endif

/** Line of sight of a mounted gun from (x1,y1,z1) to (x2,y2,z2): free when
 * no collision column is hit in steps of 128 along the line; never beyond
 * 0xC00.  Remade from mounted_los. */
static int mounted_los_c(int x1, int y1, int z1, int x2, int y2, int z2)
{
    s32 dx, dy, dz, adx, ady, adz, dist, n, sx, sy, sz, x, y, z, i;

    dx = x2 - x1;
    dy = y2 - y1;
    dz = z2 - z1;
    adx = abs(dx);
    ady = abs(dy);
    adz = abs(dz);
    if ((adx > ady) && (adx > adz))
        dist = adx - (adx >> 4) + ((ady + adz) >> 1) - ((ady + adz) >> 3);
    else if ((adx <= ady) && (ady > adz))
        dist = ady - (ady >> 4) + ((adx + adz) >> 1) - ((adx + adz) >> 3);
    else
        dist = adz - (adz >> 4) + ((ady + adx) >> 1) - ((ady + adx) >> 3);
    if (dist > 0xC00)
        return 0;
    n = dist >> 7;
    if (n == 0)
        return 0;
    sx = dx / n;
    sy = dy / n;
    sz = dz / n;
    x = x1;
    y = y1;
    z = z1;
    for (i = 0; i < n; i++)
    {
        if (check_col_collision(x, y, z))
            return 0;
        x += sx;
        y += sy;
        z += sz;
    }
    return 1;
}

int mounted_los(int x1, int y1, int z1, int x2, int y2, int z2)
{
    int ret;

    FNCHK_RET("mounted_los", ret, mounted_los_asm(x1, y1, z1, x2, y2, z2),
      mounted_los_c(x1, y1, z1, x2, y2, z2));
    return ret;
}

/** True when `p_me` shoots `p_him` on sight by the war flags of their
 * groups (a building uses its Group, others their EffectiveGroup); never
 * people with Flag2 0x00400000.  Remade from kill_flags. */
static TbBool kill_flags(struct Thing *p_me, struct Thing *p_him)
{
    ubyte my_grp, his_grp;

    if ((p_him->Type == TT_PERSON) && ((p_him->Flag2 & 0x00400000) != 0))
        return false;
    if (p_me->Type == TT_BUILDING)
        my_grp = p_me->U.UObject.Group;
    else
        my_grp = p_me->U.UPerson.EffectiveGroup;
    if (p_him->Type == TT_BUILDING)
        his_grp = p_him->U.UObject.Group;
    else
        his_grp = p_him->U.UPerson.EffectiveGroup;
    return (war_flags[my_grp].KillOnSight & (1u << (his_grp & 0x1F))) != 0;
}

/** A gun picks the nearest person within 8 tiles it should shoot at
 * (alive); PTarget becomes &things[that], &things[0] when none.  Returns
 * the thing index.  Remade from aquire_target. */
static ThingIdx aquire_target_c(struct Thing *p_gun)
{
    s32 best_dist;
    ThingIdx best;
    short tile_x, tile_z, i, k;

    best_dist = 0x7FFFFFFF;
    best = 0;
    tile_x = p_gun->X >> 16;
    tile_z = p_gun->Z >> 16;
    for (i = -8; i < 8; i++)
    {
        for (k = -8; k < 8; k++)
        {
            int cx, cz;
            ThingIdx thing;

            cx = tile_x + k;
            if ((cx <= 0) || (cx >= 128))
                continue;
            cz = tile_z + i;
            if ((cz <= 0) || (cz >= 128))
                continue;
            thing = game_my_big_map[(cz << 7) + cx].Child;
            while (thing != 0)
            {
                if (thing > 0)
                {
                    struct Thing *p_thing;

                    p_thing = &things[thing];
                    if (kill_flags(p_gun, p_thing) && (p_thing->Type == TT_PERSON)
                      && ((p_thing->Flag & 0x0002) == 0))
                    {
                        s32 adx, adz, dist;

                        adx = abs((p_gun->X - p_thing->X) >> 8);
                        adz = abs((p_gun->Z - p_thing->Z) >> 8);
                        if (adx <= adz)
                            dist = adz + (adx >> 1);
                        else
                            dist = adx + (adz >> 1);
                        if (dist < best_dist) {
                            best_dist = dist;
                            best = thing;
                        }
                    }
                    thing = p_thing->Next;
                }
                else
                {
                    thing = sthings[thing].Next;
                }
            }
        }
    }
    p_gun->PTarget = &things[best];
    return best;
}

ThingIdx aquire_target(struct Thing *p_gun)
{
    ThingIdx ret;

    FNCHK_RET("aquire_target", ret, aquire_target_asm(p_gun), aquire_target_c(p_gun));
    return ret;
}

ubyte track_target(struct Thing *p_mgun);
void rotate_object(struct Thing *p_object);

#ifdef SW_FNCHK
static ubyte track_target_asm(struct Thing *p_mgun)
{
    int ret;
    asm volatile (
      "call ASM_track_target\n"
        : "=a" (ret) : "0" (p_mgun) : "cc", "memory");
    return ret;
}
#endif

/** Brings an angle difference into -0x400..0x400 as the asm does: only
 * the low 16 bits are adjusted. */
static short mgun_wrap_angle(short diff)
{
    if (diff < -0x400)
        return (short)(diff + 0x800);
    if (diff > 0x400)
        return (short)(diff - 0x800);
    return diff;
}

/** A mounted gun turns towards its target: the turn rate (AngleDY) is set
 * from the horizontal angle difference, the gun is tilted (AngleX) half way
 * to the target.  True when it points at the target, within 200 both ways.
 * As the original, the tilt angle is computed from the squares of the
 * height and of the distance.  Remade from track_target. */
static ubyte track_target_c(struct Thing *p_mgun)
{
    struct Thing *p_target;
    s32 dx, dz, dy, dist2;
    short angle, diff;

    if (p_mgun->U.UMGun.WeaponTurn != 0)
        p_mgun->U.UMGun.WeaponTurn--;
    p_target = p_mgun->PTarget;
    dx = (p_target->X >> 8) - (p_mgun->X >> 8);
    dz = (p_target->Z >> 8) - (p_mgun->Z >> 8);
    angle = arctan(dx, dz);
    diff = mgun_wrap_angle((short)(p_mgun->U.UMGun.AngleY - angle));
    if (diff < 0) {
        if (diff > -20)
            p_mgun->U.UMGun.AngleDY = 0;
        else
            p_mgun->U.UMGun.AngleDY = 0x100;
    } else if (diff > 0) {
        if (diff < 20)
            p_mgun->U.UMGun.AngleDY = 0;
        else
            p_mgun->U.UMGun.AngleDY = -0x100;
    }
    if ((diff <= -200) || (diff >= 200))
        return 0;
    dist2 = (s32)((u32)dz * (u32)dz + (u32)dx * (u32)dx);
    dy = (p_target->Y >> 4) - (p_mgun->Y >> 5);
    angle = arctan((s32)((u32)dy * (u32)dy), dist2) - 0x400;
    diff = mgun_wrap_angle((short)(p_mgun->U.UMGun.AngleX - angle));
    p_mgun->U.UMGun.AngleX = ((p_mgun->U.UMGun.AngleX - (diff >> 1)) + 0x800) & 0x7FF;
    if (abs(diff) < 200)
        return 1;
    return 0;
}

ubyte mgun_shoot_at_target(struct Thing *p_mgun)
{
    ubyte turn;

    if (p_mgun->U.UMGun.WeaponTurn != 0) {
        return 0;
    }
    turn = p_mgun->U.UMGun.ShotTurn;
    p_mgun->U.UMGun.WeaponTurn = 8;
    p_mgun->U.UMGun.ShotTurn = (turn == 0);
    switch (p_mgun->U.UMGun.CurrentWeapon)
    {
    default:
        LOGWARN("Mounted gun weapon %d not supported, using laser",
          (int)p_mgun->U.UMGun.CurrentWeapon);
        // fall through
    case WEP_LASER:
        init_mgun_laser(p_mgun, 7);
        break;
    }
    return 0;
}

void process_mounted_gun(struct Thing *p_building)
{
#if 0
    asm volatile ("call ASM_process_mounted_gun\n"
        : : "a" (p_building));
#endif
    struct Thing *p_target;

    if (p_building->U.UMGun.RecoilTimer > 0)
        p_building->U.UMGun.RecoilTimer--;
    if (p_building->PTarget == NULL)
        p_building->State = 10;

    switch(p_building->State)
    {
    case 10:
        if (((gameturn + p_building->ThingOffset) & 0x07) == 0 && aquire_target(p_building) > 0)
            p_building->State = 11;
        break;
    case 11:
        if (((gameturn + p_building->ThingOffset) & 0x1F) == 0 && aquire_target(p_building) == 0)
            p_building->State = 10;
        if (!track_target(p_building))
        {
            rotate_object(p_building);
            break;
        }
        p_target = p_building->PTarget;
        if (p_target != NULL) // Invalid pointer correction
        {
            if ((p_target <= &things[0]) || (p_target >= &things[THINGS_LIMIT])) {
                LOGERR("Tried to shoot at invalid target 0x%p", p_target);
                p_building->PTarget = p_target = NULL;
            }
        }
        if (p_target != NULL)
        {
            MapCoord tg_cor_x, tg_cor_y, tg_cor_z; // Target/victim coords
            MapCoord at_cor_x, at_cor_y, at_cor_z; // Attacker coords

            tg_cor_x = PRCCOORD_TO_MAPCOORD(p_target->X);
            tg_cor_y = PRCCOORD_TO_MAPCOORD(p_target->Y);
            tg_cor_z = PRCCOORD_TO_MAPCOORD(p_target->Z);
            if (p_target->Type == TT_PERSON)
                tg_cor_y += 20;
            at_cor_x = PRCCOORD_TO_MAPCOORD(p_building->X);
            at_cor_y = PRCCOORD_TO_MAPCOORD(p_building->Y) + 40;
            at_cor_z = PRCCOORD_TO_MAPCOORD(p_building->Z);
            if (mounted_los(at_cor_x, at_cor_y, at_cor_z, tg_cor_x, tg_cor_y, tg_cor_z))
            {
                mgun_shoot_at_target(p_building);
            }
            rotate_object(p_building);
        }
        break;
    }
}

#ifdef SW_FNCHK
static void process_gate1_asm(struct Thing *p_building)
{
    asm volatile ("call ASM_process_gate1\n"
        : : "a" (p_building) : "cc", "memory");
}
#endif

/** Moves the points of one wing of a gate object along x (or z if dx is
 * 0): the even wing (flag 0) and the odd one use different point sets.
 * Remade from apply_object_morph.
 */
static void apply_object_morph(ushort obj, short dx, short dz, ushort flag)
{
    static const ubyte pts_on[5] = {0, 3, 4, 7, 9};
    static const ubyte pts_off[5] = {1, 2, 5, 6, 8};
    const ubyte *pts;
    ushort start;
    int i;

    pts = (flag != 0) ? pts_on : pts_off;
    start = game_objects[obj].StartPoint;
    if (dx != 0)
    {
        for (i = 0; i < 5; i++)
            game_object_points[start + pts[i]].X += dx;
    }
    else if (dz != 0)
    {
        for (i = 0; i < 5; i++)
            game_object_points[start + pts[i]].Z += dz;
    }
}

/** One wing pair (wing 1 and 3, or 2 and 4) of a gate: the height of its
 * objects follows RaiseY, the points slide by (sx,sz). */
static void gate_set_wing_y(struct Thing *p_building, ushort wing, short y)
{
    game_objects[p_building->U.UObject.Object + wing].OffsetY = y;
}

/** A gate opening (state 0x0D, wings sliding down/apart) or closing (0x0F),
 * and waiting closed/open (0x13) for a request.
 * Remade from process_gate1.
 */
static void process_gate1_c(struct Thing *p_building)
{
    struct TngUObject *p_obj;
    short tdx, tdz;

    p_obj = &p_building->U.UObject;
    tdx = p_obj->TargetDX;
    tdz = p_obj->TargetDZ;
    switch ((ushort)p_building->State)
    {
    case 0x0D:
        if (p_obj->Timer[0] == 0)
        {
            if (p_obj->RaiseY[0] > -500)
            {
                apply_object_morph(p_obj->Object + 1, -tdx, -tdz, 1);
                apply_object_morph(p_obj->Object + 3, tdx, tdz, 0);
                gate_set_wing_y(p_building, 1, p_obj->RaiseY[0] + p_obj->MinY[0] + 500);
                gate_set_wing_y(p_building, 3, p_obj->RaiseY[0] + p_obj->MinY[0] + 500);
                p_obj->RaiseY[0] += p_obj->RaiseDY[0];
            }
            else
            {
                gate_set_wing_y(p_building, 1, p_obj->MinY[0]);
                gate_set_wing_y(p_building, 3, p_obj->MinY[0]);
            }
        }
        else
        {
            p_obj->Timer[0]--;
        }
        if (p_obj->Timer[1] == 0)
        {
            if (p_obj->RaiseY[1] > -500)
            {
                apply_object_morph(p_obj->Object + 2, -tdx, -tdz, 0);
                apply_object_morph(p_obj->Object + 4, tdx, tdz, 1);
                gate_set_wing_y(p_building, 2, p_obj->RaiseY[1] + p_obj->MinY[0] + 500);
                gate_set_wing_y(p_building, 4, p_obj->RaiseY[1] + p_obj->MinY[0] + 500);
                p_obj->RaiseY[1] += p_obj->RaiseDY[1];
            }
            else
            {
                gate_set_wing_y(p_building, 2, p_obj->MinY[0]);
                gate_set_wing_y(p_building, 4, p_obj->MinY[0]);
            }
        }
        else
        {
            p_obj->Timer[1]--;
        }
        if ((p_obj->RaiseY[0] <= -500) && (p_obj->RaiseY[1] <= -500))
        {
            p_building->Flag = 0x40;
            p_building->State = 0x13;
        }
        break;
    case 0x0F:
        if ((p_building->Flag & 0x20) != 0)
        {
            p_building->State = 0x0D;
            p_building->Flag = 0;
            p_obj->Timer[0] = 3;
            p_obj->Timer[1] = 0;
            p_obj->RaiseDY[0] = -50;
            p_obj->RaiseDY[1] = -50;
            break;
        }
        if (p_obj->Timer[0] == 0)
        {
            if (p_obj->RaiseY[0] < 0)
            {
                apply_object_morph(p_obj->Object + 1, tdx, tdz, 1);
                apply_object_morph(p_obj->Object + 3, -tdx, -tdz, 0);
                gate_set_wing_y(p_building, 1, p_obj->RaiseY[0] + p_obj->MinY[0] + 500);
                gate_set_wing_y(p_building, 3, p_obj->RaiseY[0] + p_obj->MinY[0] + 500);
                p_obj->RaiseY[0] += p_obj->RaiseDY[0];
            }
            else
            {
                gate_set_wing_y(p_building, 1, p_obj->MinY[0] + 500);
                gate_set_wing_y(p_building, 3, p_obj->MinY[0] + 500);
            }
        }
        else
        {
            p_obj->Timer[0]--;
        }
        if (p_obj->Timer[1] == 0)
        {
            if (p_obj->RaiseY[1] < 0)
            {
                apply_object_morph(p_obj->Object + 2, tdx, tdz, 0);
                apply_object_morph(p_obj->Object + 4, -tdx, -tdz, 1);
                gate_set_wing_y(p_building, 2, p_obj->RaiseY[1] + p_obj->MinY[0] + 500);
                gate_set_wing_y(p_building, 4, p_obj->RaiseY[1] + p_obj->MinY[0] + 500);
                p_obj->RaiseY[1] += p_obj->RaiseDY[1];
            }
            else
            {
                gate_set_wing_y(p_building, 2, p_obj->MinY[0] + 500);
                gate_set_wing_y(p_building, 4, p_obj->MinY[0] + 500);
            }
        }
        else
        {
            p_obj->Timer[1]--;
        }
        if ((p_obj->RaiseY[0] >= 0) && (p_obj->RaiseY[1] >= 0))
        {
            p_obj->RaiseY[0] = 0;
            p_obj->RaiseY[1] = 0;
            gate_set_wing_y(p_building, 1, p_obj->MinY[0] + 500);
            gate_set_wing_y(p_building, 3, p_obj->MinY[0] + 500);
            gate_set_wing_y(p_building, 2, p_obj->MinY[0] + 500);
            gate_set_wing_y(p_building, 4, p_obj->MinY[0] + 500);
            p_building->Flag = 0x80;
            p_building->State = 0x13;
        }
        break;
    case 0x13:
        if ((p_building->Flag & 0x20) != 0)
        {
            p_building->State = 0x0D;
            p_building->Flag = 0;
            p_obj->Timer[0] = 3;
            p_obj->Timer[1] = 0;
            p_obj->RaiseDY[0] = -50;
            p_obj->RaiseDY[1] = -50;
        }
        if ((p_building->Flag & 0x100) != 0)
        {
            p_building->State = 0x0F;
            p_building->Flag = 0;
            p_obj->Timer[0] = 0;
            p_obj->Timer[1] = 3;
            p_obj->RaiseDY[0] = 50;
            p_obj->RaiseDY[1] = 50;
        }
        break;
    default:
        break;
    }
}

void process_gate1(struct Thing *p_building)
{
    FNCHK_VOID("process_gate1", process_gate1_asm(p_building), process_gate1_c(p_building));
}

void process_bld36(struct Thing *p_building)
{
    struct M33 *p_matx;
    assert(p_building->U.UObject.MatrixIndex < next_local_mat);
    p_matx = &local_mats[p_building->U.UObject.MatrixIndex];
    rotate_object_axis(p_matx, 0, 0, 32);
}

void process_building(struct Thing *p_building)
{
#if 0
    asm volatile ("call ASM_process_building\n"
        : : "a" (p_building));
#endif
    if ((p_building->Flag & TngF_Unkn00040000) != 0)
    {
        if ((p_building->Flag & TngF_Destroyed) != 0)
        {
            p_building->Flag &= ~TngF_Unkn00040000;
            return;
        }
        collapse_building(p_building->X >> 8, p_building->Y >> 8, p_building->Z >> 8, p_building);
        return;
    }

    switch (p_building->SubType)
    {
    case SubTT_BLD_SHUTLDR:
        if ((p_building->Flag & TngF_Destroyed) == 0)
        {
            if ((p_building->U.UObject.PrevThing == 0) && ((p_building->Flag & TngF_Unkn08000000) == 0)) {
                process_shuttle_loader(p_building);
            }
            if ((p_building->U.UObject.PrevThing == 0) && ((p_building->Flag & TngF_Unkn08000000) != 0)
              && ((LbRandomAnyShort() & 3) == 0) && (((gameturn + p_building->ThingOffset) & 7) == 0)) {
                create_a_pod(0, p_building, 0);
            }
        }
        break;
    case SubTT_BLD_DOME:
        if ((p_building->Flag & TngF_Destroyed) == 0)
            process_dome1(p_building);
        break;
    case SubTT_BLD_MGUN:
        if ((p_building->Flag & TngF_Destroyed) == 0)
            process_mounted_gun(p_building);
        break;
    case SubTT_BLD_GATE:
        process_gate1(p_building);
        break;
    case SubTT_BLD_MOVN_ROTOR:
        process_bld36(p_building);
        break;
    default:
        if (p_building->State == BldSt_OBJ_UNKN09) {
            collapse_building(p_building->X >> 8, p_building->Y >> 8, p_building->Z >> 8, p_building);
        }
        break;
    }
}

int building_hit_by_bullet(struct Thing *p_thing, short hp,
  int vx, int vy, int vz, struct Thing *p_attacker, ushort type)
{
    if ((p_attacker != NULL) && (p_attacker->Type == TT_PERSON)) {
        p_attacker->U.UPerson.Flag3 |= PrsF3_Unkn40;
    }
    if (p_thing->SubType != 32)
    {
        if ((p_thing->Flag & TngF_Destroyed) == 0)
        {
            int health;

            play_dist_sample(p_thing, 65, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 1);
            health = p_thing->Health - (hp >> 1);
            if (health < 0) {
                collapse_building(PRCCOORD_TO_MAPCOORD(p_thing->X),
                  PRCCOORD_TO_MAPCOORD(p_thing->Y), PRCCOORD_TO_MAPCOORD(p_thing->Z), p_thing);
            }
            p_thing->Health = health;
        }
        return hp;
    }
    p_thing->Flag |= TngF_Unkn01000000;
    if ((p_thing->Flag & TngF_Destroyed) == 0)
    {
        int health_decr;

        health_decr = hp >> 1 >> 1;
        p_thing->Health -= health_decr;
        p_thing->U.UObject.Cost = 5;
        if (p_thing->Health <= 0)
        {
            init_mgun_explode(p_thing);
            return p_thing->Health + health_decr;
        }
    }
    return 0;
}

/******************************************************************************/

ubyte track_target(struct Thing *p_mgun)
{
    ubyte ret;
    FNCHK_RET("track_target", ret, track_target_asm(p_mgun), track_target_c(p_mgun));
    return ret;
}

void rotate_object(struct Thing *p_object)
{
    FNCHK_VOID("rotate_object", rotate_object_asm(p_object), rotate_object_c(p_object));
}

void init_mgun_explode(struct Thing *p_thing)
{
    FNCHK_VOID("init_mgun_explode", init_mgun_explode_asm(p_thing), init_mgun_explode_c(p_thing));
}
