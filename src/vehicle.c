/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file vehicle.c
 *     Routines implementing vehicle.
 * @par Purpose:
 *     Implement functions for handling vehicle things.
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
#include "vehicle.h"

#include "bfmath.h"
#include "bfmemut.h"
#include "bfutility.h"
#include "ssampply.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "bigmap.h"
#include "fncheck.h"
#include "engincam.h"
#include "game_options.h"
#include "bmbang.h"
#include "enginpriobjs.h"
#include "enginsngobjs.h"
#include "enginsngtxtr.h"
#include "engintrns.h"
#include "game.h"
#include "thing_fire.h"
#include "bmcrater.h"
#include "thing_expld.h"
#include "febrief.h"
#include "misstat.h"
#include "frame_sprani.h"
#include "game_speed.h"
#include "matrix.h"
#include "pathtrig.h"
#include "people.h"
#include "sound.h"
#include "thing.h"
#include "lvwalk.h"
#include "thing_onface.h"
#include "swlog.h"
#include "weapon.h"
/******************************************************************************/

#pragma pack(1)

// TODO Maybe this is the same as players count, maybe not; owner field does not typically store player number
#define MECH_OWNER_LIMIT 8

struct unkn_mech_struc1 { // sizeof=0x11
    ubyte field_0;
    s32 field_1;
    s32 field_5;
    s32 field_9;
    s32 field_D;
};

struct unkn_mech_struc2 { // sizeof=0x89
    s32 field_0;
    ubyte field_4[132];
    ubyte field_88;
};

struct unkn_mech_struc3 { // sizeof=0x76
    struct unkn_mech_struc4 *mech3_arr4_ptr;
    s32 mech3_unkn_arr4_idx;
    s32 mech3_unkn_fld_8;
    struct M33 mech3_unkn_mat_C;
    s32 mech3_unkn_X;
    s32 mech3_unkn_Y;
    s32 mech3_unkn_Z;
    ubyte mech3_unkn_fld_3C[13];
    s32 mech3_unkn_fld_49;
    ubyte mech3_unkn_fld_4D[7];
    s32 mech3_unkn_fld_54[3];
    ubyte field_60;
    ubyte field_61[3];
    s32 field_64;
    s32 field_68;
    s32 field_6C;
    ubyte field_70[2];
    ubyte field_72[2];
    ubyte field_74;
    ubyte field_75;
};

struct unkn_mech_struc4 { // sizeof=0x7A
    short thing;
    ubyte field_2[33];
    short field_23;
    s32 field_25;
    s32 field_29;
    s32 field_2D;
    s32 field_31;
    s32 field_35;
    s32 field_39;
    s32 field_3D;
    s32 field_41;
    s32 field_45;
    s32 angle_49;
    s32 angle_4D;
    s32 angle_51;
    s32 field_55;
    s32 field_59;
    ubyte field_5D[4];
    struct M31 field_61;
    struct M31 field_6D;
    ubyte field_79;
};

struct unkn_mech_struc5_s1 { // sizeof=24
    s32 unkst5sub_04;
    s32 unkst5sub_08;
    s32 unkst5sub_0C;
    s32 unkst5sub_10;
    s32 unkst5sub_14;
    ubyte unkst5sub_18[4];
};

struct unkn_mech_struc5 { // sizeof=0x1A9
    s32 field_0;
    struct unkn_mech_struc5_s1 field_4[16];
    ubyte field_184[8];
    ubyte field_18C[24];
    ubyte field_1A4[4];
    ubyte field_1A8;
};

struct unkn_mech_struc7 { // sizeof=0x13488
    struct unkn_mech_struc3 field_0[MECH_OWNER_LIMIT];
    struct unkn_mech_struc4 field_3B0[256];
    struct unkn_mech_struc5 field_7DB0[64];
    struct unkn_mech_struc1 field_E7F0[1024];
    ubyte field_12BF0[8];
    struct unkn_mech_struc2 field_12BF8[16];
};

#pragma pack()

/** Configuration options for each person state.
 */
struct VehStateConfig vehicle_states[] = {
  {"NONE_STATE",},
  {"UNKN_1",},
  {"UNKN_2",},
  {"UNKN_3",},
  {"UNKN_4",},
  {"UNKN_5",},
  {"UNKN_6",},
  {"UNKN_7",},
  {"UNKN_8",},
  {"UNKN_9",},
  {"UNKN_A",},
  {"UNKN_B",},
  {"UNKN_C",},
  {"UNKN_D",},
  {"UNKN_E",},
  {"UNKN_F",},
  {"UNKN_10",},
  {"WANDER",},
  {"UNKN_12",},
  {"UNKN_13",},
  {"UNKN_14",},
  {"UNKN_15",},
  {"UNKN_16",},
  {"UNKN_17",},
  {"UNKN_18",},
  {"UNKN_19",},
  {"UNKN_1A",},
  {"UNKN_1B",},
  {"UNKN_1C",},
  {"UNKN_1D",},
  {"UNKN_1E",},
  {"UNKN_1F",},
  {"UNKN_20",},
  {"PARKED_PARAL",},
  {"UNKN_22",},
  {"UNKN_23",},
  {"UNKN_24",},
  {"UNKN_25",},
  {"UNKN_26",},
  {"UNKN_27",},
  {"UNKN_28",},
  {"UNKN_29",},
  {"UNKN_2A",},
  {"UNKN_2B",},
  {"UNKN_2C",},
  {"UNKN_2D",},
  {"UNKN_2E",},
  {"UNKN_2F",},
  {"UNKN_30",},
  {"UNKN_31",},
  {"GOTO_LOC",},
  {"UNKN_33",},
  {"UNKN_34",},
  {"UNKN_35",},
  {"UNKN_36",},
  {"UNKN_37",},
  {"UNKN_38",},
  {"UNKN_39",},
  {"UNKN_3A",},
  {"FLY_LANDING",},
  {"UNKN_3C",},
  {"PARKED_PERPN",},
  {"UNKN_3E",},
  {"UNKN_3F",},
  {"UNKN_40",},
  {"UNKN_41",},
  {"UNKN_42",},
  {"UNKN_43",},
  {"UNKN_44",},
  {"UNKN_45",},
  {"UNKN_46",},
  {"UNKN_47",},
  {"UNKN_48",},
  {"UNKN_49",},
  {"UNKN_4A",},
  {"UNKN_4B",},
  {"UNKN_4C",},
  {"UNKN_4D",},
  {"UNKN_4E",},
  {"UNKN_4F",},
  {"UNKN_50",},
  {"UNKN_51",},
  {"UNKN_52",},
};

void *dword_177750 = NULL;
struct unkn_mech_struc7 *unkn_mech_stct7 = NULL;

struct unkn_mech_struc3 *unkn_mech_arr3 = NULL;
struct unkn_mech_struc4 *unkn_mech_arr4 = NULL;
struct unkn_mech_struc5 *unkn_mech_arr5 = NULL;
struct unkn_mech_struc1 *unkn_mech_arr1 = NULL;
ubyte *unkn_mech_arr6 = NULL;
struct unkn_mech_struc2 *unkn_mech_arr2 = NULL;
struct M33 unkn_mech_mat8;

s32 mech_last_cor_x = TILE_TO_MAPCOORD(MAP_TILE_WIDTH/2, 127);
s32 mech_last_cor_y = TILE_TO_MAPCOORD(1, 0);
s32 mech_last_cor_z = TILE_TO_MAPCOORD(MAP_TILE_WIDTH/2, 127);

// to be removed after ASM use is gone
s32 mech_unkn_tile_y2;
s32 mech_unkn_tile_x3;
s32 mech_unkn_tile_y3;

s32 mech_rocket1_launch_cor_x = TILE_TO_MAPCOORD(MAP_TILE_WIDTH/2, 127);
s32 mech_rocket1_launch_cor_y = TILE_TO_MAPCOORD(1, 0);
s32 mech_rocket1_launch_cor_z = TILE_TO_MAPCOORD(MAP_TILE_WIDTH/2, 127);

s32 mech_rocket2_launch_cor_x = TILE_TO_MAPCOORD(MAP_TILE_WIDTH/2, 127);
s32 mech_rocket2_launch_cor_y = TILE_TO_MAPCOORD(1, 0);
s32 mech_rocket2_launch_cor_z = TILE_TO_MAPCOORD(MAP_TILE_WIDTH/2, 127);

s32 unkn_mech_var10 = 0;
s32 unkn_mech_var11 = 0;

const char *vehicle_type_name(ushort vtype)
{
#if 0
// Enable when we have vehicle config file with names
    struct VehicleDef *p_vhdef;

    p_vhdef = &vehicle_def[vtype];
    if (strlen(p_vhdef->Name) == 0)
        return "OUTRNG_VEHICLE";
    return p_vhdef->Name;
#endif
    switch (vtype)
    {
    case SubTT_VEH_TRAIN:
        return "TRAIN";
    case SubTT_VEH_GROUND:
        return "VEH_GROUND";
    case SubTT_VEH_SHUTTLE_POD:
        return "SHUTTLE_POD";
    case SubTT_VEH_FLYING:
        return "VEH_FLYING";
    case SubTT_VEH_TANK:
        return "TANK";
    case SubTT_VEH_SHIP:
        return "SHIP";
    case SubTT_VEH_MECH:
        return "MECH";
    default:
        break;
    }
    return "VEHICLE";
}

void snprint_vehicle_state(char *buf, ulong buflen, struct Thing *p_thing)
{
    char *s;
    //ubyte nparams;
    struct VehStateConfig *p_vstatcfg;

    p_vstatcfg = &vehicle_states[p_thing->State];
    s = buf;

    sprintf(s, "%s( ", p_vstatcfg->Name);
    s += strlen(s);
    //nparams = 0;

    // TODO support parameters of states

    snprintf(s, buflen - (s-buf), " )");
}

void mech_clear_all_fld72(void)
{
    ushort i;
    ushort owner;

    for (owner = 0; owner < MECH_OWNER_LIMIT; owner++)
    {
        struct unkn_mech_struc3 *p_u3itm;

        p_u3itm = &unkn_mech_arr3[owner];
        for (i = 0; i < 2; i++) {
            p_u3itm->field_72[i] = 0;
        }
    }
}

void mech_unkn_func_02(void)
{
#if 0
    asm volatile ("call ASM_mech_unkn_func_02\n"
        :  :  : "eax" );
    return;
#endif
    unkn_mech_var10 = 0;
    unkn_mech_var11 = 0;

    mech_clear_all_fld72();
}

int load_mech_dat(const char *fname)
{
#if 0
    int ret;
    asm volatile ("call ASM_load_mech_dat\n"
        : "=r" (ret) : "a" (fname));
    return ret;
#endif
    TbFileHandle fh;
    int i, k;

    fh = LbFileOpen(fname, Lb_FILE_MODE_READ_ONLY);
    if (fh == INVALID_FILE) {
        return 0;
    }

    for (i = 0; i < 64; i++)
    {
        struct unkn_mech_struc5 *p_u5itm;

        p_u5itm = &unkn_mech_arr5[i];
        LbFileRead(fh, p_u5itm, sizeof(struct unkn_mech_struc5));

        for (k = 0; k < 17; k++)
        {
            p_u5itm->field_4[k].unkst5sub_0C = 0x800 - p_u5itm->field_4[k].unkst5sub_0C;
            p_u5itm->field_4[k].unkst5sub_14 = 0x800 - p_u5itm->field_4[k].unkst5sub_14;
            p_u5itm->field_4[k].unkst5sub_0C &= 0x7FF;
            p_u5itm->field_4[k].unkst5sub_10 &= 0x7FF;
            p_u5itm->field_4[k].unkst5sub_14 &= 0x7FF;
            p_u5itm->field_4[k].unkst5sub_04 = -p_u5itm->field_4[k].unkst5sub_04;
        }
    }

    for (i = 0; i < 16; i++) {
        LbFileRead(fh, &unkn_mech_arr2[i], sizeof(struct unkn_mech_struc2));
    }

    for (i = 0; i < 1024; i++) {
        LbFileRead(fh, &unkn_mech_arr1[i], sizeof(struct unkn_mech_struc1));
    }

    LbFileClose(fh);
    return 1;
}

void init_mech(void)
{
#if 0
    asm volatile ("call ASM_init_mech\n"
        :  :  : "eax" );
#endif
    u32 a1idx;

    unkn_mech_arr4 = &unkn_mech_stct7->field_3B0[0];
    unkn_mech_arr5 = &unkn_mech_stct7->field_7DB0[0];
    unkn_mech_arr1 = &unkn_mech_stct7->field_E7F0[0];
    unkn_mech_arr6 = unkn_mech_stct7->field_12BF0;
    unkn_mech_arr2 = &unkn_mech_stct7->field_12BF8[0];
    unkn_mech_arr3 = &unkn_mech_stct7->field_0[0];
    memset(unkn_mech_stct7, 0, sizeof(struct unkn_mech_struc7));

    memset(&unkn_mech_mat8, 0, sizeof(struct M33));
    unkn_mech_mat8.R[0][0] = 0x4000;
    unkn_mech_mat8.R[1][1] = 0x4000;
    unkn_mech_mat8.R[2][2] = 0x4000;

    load_mech_dat("data/mech.dat");

    mech_clear_all_fld72();
    unkn_mech_arr3->field_6C = 0;
    a1idx = unkn_mech_arr2[unkn_mech_arr3->field_6C].field_0;
    unkn_mech_arr3->field_64 = a1idx;
    unkn_mech_arr3->mech3_unkn_fld_54[2] = unkn_mech_arr1[a1idx].field_9;
}

void mech_gameturn_reinit(void)
{
    // Mech rocket coordinates should be recomputed later using proper matrix
    // transforms; but to avoid them being unset, init to last body location
    //TODO why do we think there is a chance of the coords not being properly updated from things?
    mech_rocket1_launch_cor_x = mech_last_cor_x;
    mech_rocket1_launch_cor_y = mech_last_cor_y;
    mech_rocket1_launch_cor_z = mech_last_cor_z;

    mech_rocket2_launch_cor_x = mech_last_cor_x;
    mech_rocket2_launch_cor_y = mech_last_cor_y;
    mech_rocket2_launch_cor_z = mech_last_cor_z;
}

TbBool vehicle_is_destroyed(ThingIdx thing)
{
    struct Thing *p_thing;

    if (thing <= 0)
        return false;

    p_thing = &things[thing];
    return thing_is_destroyed(thing) || (p_thing->Type != TT_VEHICLE);
}

/* defined below, with the other mech code */
static void mech_unkn_func_09_c(ThingIdx thing);
#ifdef SW_FNCHK
static void mech_unkn_func_09_asm(ThingIdx thing)
{
    asm volatile ("call ASM_mech_unkn_func_09\n"
        : : "a" (thing) : "cc", "memory");
}
#endif

void mech_unkn_func_09(ThingIdx thing)
{
    FNCHK_VOID("mech_unkn_func_09", mech_unkn_func_09_asm(thing), mech_unkn_func_09_c(thing));
}

void veh_add(struct Thing *p_vehicle, short frame)
{
#if 0
    asm volatile ("call ASM_veh_add\n"
        : : "a" (p_vehicle), "d" (frame));
    return;
#endif
    struct Thing *p_mgun;
    struct SingleObject *p_snobj;
    short coord_x, coord_y, coord_z;
    short mgun;
    ushort snobj;
    ushort matx;

    switch (frame)
    {
    case VehOM_MBTANK:
        mgun = get_new_thing();
        if (mgun <= 0) {
            LOGERR("No thing slots for mounted gun");
            break;
        }
        p_vehicle->U.UVehicle.SubThing = mgun;
        p_vehicle->SubType = SubTT_VEH_TANK;

        p_mgun = &things[mgun];
        p_mgun->U.UMGun.NextThing = 0;
        p_mgun->U.UVehicle.SubThing = 0;
        assert(p_vehicle->U.UVehicle.MatrixIndex < next_local_mat);
        assert(next_local_mat < LOCAL_MATS_COUNT);
        matx = next_local_mat++;
        LbMemoryCopy(&local_mats[matx], &local_mats[p_vehicle->U.UVehicle.MatrixIndex], sizeof(struct M33));
        p_mgun->U.UMGun.MatrixIndex = matx;
        p_mgun->StartFrame = frame;

        coord_x = PRCCOORD_TO_MAPCOORD(p_vehicle->X);
        coord_y = PRCCOORD_TO_MAPCOORD(p_vehicle->Y);
        coord_z = PRCCOORD_TO_MAPCOORD(p_vehicle->Z);
        snobj = copy_prim_obj_to_game_object(coord_x, coord_z, -19 - prim_unknprop01, coord_y + 20);
        p_mgun->X = 0;
        p_mgun->Y = 0x2800;
        p_mgun->Z = 0;
        unkn_object_shift_03(snobj);
        p_mgun->Type = TT_VEH_TURRET;
        p_mgun->U.UMGun.Object = snobj;
        p_snobj = &game_objects[snobj];
        p_snobj->ThingNo = p_mgun - things;
        break;
    case 26: // Replaced tank (object slots reused for mech)
        mgun = get_new_thing();
        if (mgun <= 0) {
            LOGERR("No thing slots for mounted gun");
            break;
        }
        p_vehicle->U.UVehicle.SubThing = mgun;
        p_vehicle->SubType = SubTT_VEH_TANK;

        p_mgun = &things[mgun];
        p_mgun->U.UMGun.NextThing = 0;
        p_mgun->U.UVehicle.SubThing = 0;
        assert(p_vehicle->U.UVehicle.MatrixIndex < next_local_mat);
        assert(next_local_mat < LOCAL_MATS_COUNT);
        matx = next_local_mat++;
        LbMemoryCopy(&local_mats[matx], &local_mats[p_vehicle->U.UVehicle.MatrixIndex], sizeof(struct M33));
        p_mgun->U.UMGun.MatrixIndex = matx;
        p_mgun->StartFrame = frame;

        coord_x = PRCCOORD_TO_MAPCOORD(p_vehicle->X);
        coord_y = PRCCOORD_TO_MAPCOORD(p_vehicle->Y);
        coord_z = PRCCOORD_TO_MAPCOORD(p_vehicle->Z);
        snobj = copy_prim_obj_to_game_object(coord_x, coord_z, -27 - prim_unknprop01, coord_y + 20);
        p_mgun->X = 0;
        p_mgun->Y = 0x1E00;
        p_mgun->Z = 0;
        unkn_object_shift_03(snobj);
        p_mgun->Type = TT_VEH_TURRET;
        p_mgun->U.UMGun.Object = snobj;
        p_snobj = &game_objects[snobj];
        p_snobj->ThingNo = p_mgun - things;
        break;
    case VehOM_CLAWTANK:
        mgun = get_new_thing();
        if (mgun <= 0) {
            LOGERR("No thing slots for mounted gun");
            break;
        }
        p_vehicle->U.UVehicle.SubThing = mgun;
        p_vehicle->SubType = SubTT_VEH_TANK;

        p_mgun = &things[mgun];
        p_mgun->U.UMGun.NextThing = 0;
        p_mgun->U.UVehicle.SubThing = 0;
        assert(p_vehicle->U.UVehicle.MatrixIndex < next_local_mat);
        assert(next_local_mat < LOCAL_MATS_COUNT);
        matx = next_local_mat++;
        LbMemoryCopy(&local_mats[matx], &local_mats[p_vehicle->U.UVehicle.MatrixIndex], sizeof(struct M33));
        p_mgun->U.UMGun.MatrixIndex = matx;
        p_mgun->StartFrame = frame;

        coord_x = PRCCOORD_TO_MAPCOORD(p_vehicle->X);
        coord_y = PRCCOORD_TO_MAPCOORD(p_vehicle->Y);
        coord_z = PRCCOORD_TO_MAPCOORD(p_vehicle->Z);
        snobj = copy_prim_obj_to_game_object(coord_x, coord_z, -30 - prim_unknprop01, coord_y + 20);
        p_mgun->X = 0;
        p_mgun->Y = 0x1E00;
        p_mgun->Z = 0;
        unkn_object_shift_03(snobj);
        p_mgun->Type = TT_VEH_TURRET;
        p_mgun->U.UMGun.Object = snobj;
        p_snobj = &game_objects[snobj];
        p_snobj->ThingNo = p_mgun - things;
        break;
    default:
        p_vehicle->U.UVehicle.SubThing = 0;
        break;
    }
    p_vehicle->U.UVehicle.WobbleZP = 0;
    p_vehicle->U.UVehicle.WobbleZV = 0;
}

ushort veh_passenger_count(struct Thing *p_veh)
{
    struct Thing *p_thing;
    ThingIdx thing;
    ushort c;

    c = 0;

    thing = p_veh->U.UVehicle.PassengerHead;
    while (thing > 0 && c < THINGS_LIMIT)
    {
        c++;
        p_thing = &things[thing];
        thing = p_thing->U.UPerson.LinkPassenger;
    }
    return c;
}

static void rotate_obj(short angle_a, short angle_b, short angle_c, struct M33 *p_mat);

#ifdef SW_FNCHK
static void VNAV_unkn_func_207_asm(struct Thing *p_thing)
{
    asm volatile ("call ASM_VNAV_unkn_func_207\n"
        : : "a" (p_thing) : "cc", "memory");
}
#endif

/* scratch of the road navigation, in the asm data */
extern short VNAV_nearnodes[5];
extern s32 VNAV_nodedists[5];

/** Size of a vehicle model: 170/256 of the largest |X| or |Z| of its
 * points.  Remade from func_4225c.
 */
int vehicle_object_radius(ushort object)
{
    struct SingleObject *p_obj;
    ushort pt;
    int m;

    p_obj = &game_objects[object];
    m = 0;
    for (pt = p_obj->StartPoint; pt <= p_obj->EndPoint; pt++)
    {
        int v;

        v = abs(game_object_points[pt].Z);
        if (v > m)
            m = v;
        v = abs(game_object_points[pt].X);
        if (v > m)
            m = v;
        if (pt == 0xFFFF)
            break;
    }
    return (m * 170) >> 8;
}

/** The 5 traffic nodes nearest to the map point (not unused or flagged
 * 0x0A00).  Remade from VNAV_find_nearnodes.
 */
static void VNAV_find_nearnodes(short x, short z)
{
    s32 worst;
    int worst_i, i;
    ushort node;

    for (i = 0; i < 5; i++) {
        VNAV_nearnodes[i] = 0;
        VNAV_nodedists[i] = 0xFFFFFFF;
    }
    worst = 0xFFFFFFF;
    worst_i = 0;
    for (node = 1; node < next_traffic_node; node++)
    {
        struct TrafficNode *p_tnode;
        s32 dx, dz, dist;

        p_tnode = &game_traffic_nodes[node];
        if (p_tnode->Flags == 0xFF)
            continue;
        if ((p_tnode->Flags & 0x0A00) != 0)
            continue;
        dx = x - p_tnode->X;
        dz = z - p_tnode->Z;
        dist = dx * dx + dz * dz;
        if (dist >= worst)
            continue;
        VNAV_nearnodes[worst_i] = node;
        VNAV_nodedists[worst_i] = dist;
        worst = VNAV_nodedists[0];
        worst_i = 0;
        for (i = 1; i < 5; i++) {
            if (VNAV_nodedists[i] > worst) {
                worst = VNAV_nodedists[i];
                worst_i = i;
            }
        }
    }
}

/** Octagonal distance: the longer coordinate plus half the shorter one. */
static s32 vnav_oct_dist(s32 dx, s32 dz)
{
    s32 ax, az;

    ax = abs(dx);
    az = abs(dz);
    if (ax > az)
        return ax + (az >> 1);
    return az + (ax >> 1);
}

/** Distance of the point P from the road segment A-B, and where along it
 * (0..0xFF, in 1/256).  Remade from VNAV_qwertyuiop.
 */
static void VNAV_qwertyuiop(s32 *p_dist, s32 *p_frac, s32 ax, s32 az, s32 bx, s32 bz,
  s32 px, s32 pz)
{
    s32 abx, abz, apx, apz, dot1;
    s32 bax, baz, bpx, bpz, dot2;
    s32 cross, sa, ss, seglen;

    abx = bx - ax;
    abz = bz - az;
    apx = px - ax;
    apz = pz - az;
    dot1 = apx * abx + apz * abz;
    if (dot1 <= 0)
    {
        *p_frac = 0;
        *p_dist = vnav_oct_dist(apx, apz);
        return;
    }
    bax = ax - bx;
    baz = az - bz;
    bpx = px - bx;
    bpz = pz - bz;
    dot2 = bpx * bax + baz * bpz;
    if (dot2 <= 0)
    {
        *p_frac = 0xFF;
        *p_dist = vnav_oct_dist(bpx, bpz);
        return;
    }
    cross = apx * abz - apz * abx;
    sa = abs(abx);
    ss = abs(abz);
    if (sa > ss)
        seglen = sa + (ss >> 1);
    else
        seglen = ss + (sa >> 1);
    *p_dist = abs(cross) / seglen;
    *p_frac = abs(dot1) / ((sa * sa + ss * ss) >> 8);
}

/** The road segment (from one of the 5 nearest nodes along one of its
 * links into a traffic node) nearest to the map point.
 * Remade from VNAV_find_start_dropoff_point.
 */
static void VNAV_find_start_dropoff_point(short *p_node, ubyte *p_link, s32 *p_frac,
  short x, short z)
{
    s32 best;
    int i, k;

    if (VNAV_nearnodes[0] == 0)
        return;
    best = 0xFFFFFFF;
    *p_node = 9999;
    for (i = 0; i < 5; i++)
    {
        struct TrafficNode *p_tnode;
        s32 nx, nz;

        p_tnode = &game_traffic_nodes[VNAV_nearnodes[i]];
        nx = p_tnode->X;
        nz = p_tnode->Z;
        for (k = 0; k < 4; k++)
        {
            struct TrafficNode *p_tnode2;
            short link;
            s32 dist, frac;

            link = p_tnode->UTraffic.Link[k];
            if (link >= 0)
                continue;
            if ((p_tnode->UTraffic.Flags[k] & 0x40) != 0)
                continue;
            p_tnode2 = &game_traffic_nodes[-link];
            VNAV_qwertyuiop(&dist, &frac, nx, nz, p_tnode2->X, p_tnode2->Z, x, z);
            if (dist >= best)
                continue;
            *p_node = VNAV_nearnodes[i];
            *p_link = k;
            *p_frac = frac;
            best = dist;
        }
    }
}

/** Health of a vehicle from its size and armour. */
static void vehicle_init_size_health(struct Thing *p_vehicle)
{
    int health;

    p_vehicle->Radius = vehicle_object_radius(p_vehicle->U.UVehicle.Object);
    health = p_vehicle->Radius * 5 * p_vehicle->U.UVehicle.Armour;
    if (health > 32000)
        p_vehicle->Health = 32000;
    else
        p_vehicle->Health = (short)((short)(p_vehicle->Radius * 5) * p_vehicle->U.UVehicle.Armour);
}

/** Vehicle state on level load: size, health, and a ground vehicle is put
 * on the nearest road segment.  Remade from VNAV_unkn_func_207.
 */
static void VNAV_unkn_func_207_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;

    p_veh = &p_vehicle->U.UVehicle;
    switch (p_vehicle->SubType)
    {
    case 0x33:
    case 0x36:
    case SubTT_VEH_FLYING:
        p_vehicle->Speed = 0;
        if (p_vehicle->SubType == 0x33)
            p_vehicle->State = 0x21;
        if (p_vehicle->SubType == 0x36)
            p_veh->LeisurePlace = 0;
        vehicle_init_size_health(p_vehicle);
        p_vehicle->Speed = 0;
        p_veh->PathIndex = 0;
        p_vehicle->PathOffset = 0;
        p_veh->WorkPlace = 0;
        p_veh->AngleDY = 0;
        p_veh->MaxSpeed = 0;
        p_veh->MaxHealth = p_vehicle->Health;
        p_veh->ZebraOldHealth = p_vehicle->Health;
        return;
    case 0x3B:
        p_vehicle->Radius = 300;
        p_vehicle->Health = 25000;
        p_vehicle->Speed = 0;
        p_veh->PathIndex = 0;
        p_vehicle->PathOffset = 0;
        p_veh->WorkPlace = 0;
        p_veh->AngleDY = 0;
        p_veh->MaxSpeed = 0;
        p_veh->MaxHealth = p_vehicle->Health;
        p_veh->ZebraOldHealth = p_vehicle->Health;
        return;
    default:
        break;
    }

    {
        short node;
        ubyte link;
        s32 frac;
        short x, z;
        short tnode;

        p_veh->MaxSpeed = 0;
        p_veh->PathIndex = 0;
        p_vehicle->PathOffset = 0;
        p_vehicle->Speed = 0;
        p_veh->WorkPlace = 0;
        p_veh->AngleDY = 0;
        p_veh->AccelZ = 0;
        p_veh->AccelX = 0;
        p_veh->ZebraOldHealth = p_vehicle->Health;
        x = (short)(p_vehicle->X >> 8);
        z = (short)(p_vehicle->Z >> 8);
        VNAV_find_nearnodes(x, z);
        // left as they were when no node is found, as in the original
        node = 0;
        link = 0;
        frac = 0;
        VNAV_find_start_dropoff_point(&node, &link, &frac, x, z);
        p_veh->Dummy4b = -node;
        p_veh->Dummy4a = (p_veh->Dummy4a & ~0xFF) | ((p_veh->Dummy4a & 0x3F) | (link << 6));
        tnode = game_traffic_nodes[node].UTraffic.Link[link];
        p_veh->TNode = tnode;
        if ((tnode < 0) && ((game_traffic_nodes[-tnode].Flags & 0x0800) != 0))
        {
            struct TrafficNode *p_tn1, *p_tn2;
            s32 nx, nz;
            ThingIdx old_offs;

            p_veh->Dummy4a = (p_veh->Dummy4a & ~0xFF) | ((p_veh->Dummy4a & 0xC0) | 0x13);
            p_vehicle->State = 0x3D;
            p_tn1 = &game_traffic_nodes[node];
            p_tn2 = &game_traffic_nodes[-tnode];
            nx = (((p_tn2->X - p_tn1->X) * frac) >> 8) + p_tn1->X;
            nz = (((p_tn2->Z - p_tn1->Z) * frac) >> 8) + p_tn1->Z;
            old_offs = p_vehicle->ThingOffset;
            p_vehicle->ThingOffset = (short)(p_vehicle - things);
            move_mapwho(p_vehicle, nx << 8, p_vehicle->Y, nz << 8);
            p_vehicle->ThingOffset = old_offs;
            p_veh->AngleY = (ushort)arctan(p_tn2->X - (p_vehicle->X >> 8),
              p_tn2->Z - (p_vehicle->Z >> 8));
            rotate_obj(0, -p_veh->AngleY, 0, &local_mats[p_veh->MatrixIndex]);
        }
        else
        {
            p_vehicle->State = 0x21;
            p_veh->Dummy4a &= ~0x3F;
        }
        vehicle_init_size_health(p_vehicle);
        p_veh->EffectiveGroup = 99;
        p_veh->MaxHealth = p_vehicle->Health;
    }
}

void VNAV_unkn_func_207(struct Thing *p_thing)
{
    FNCHK_VOID("VNAV_unkn_func_207", VNAV_unkn_func_207_asm(p_thing), VNAV_unkn_func_207_c(p_thing));
}

/* Road route search state, in the asm data: VNAV_route[2] is the number of
 * nodes, the route itself (destination first) from VNAV_route[4];
 * veh_ma_heap[1..256] is the search heap, veh_ma_heap[257] its size. */
extern short VNAV_route[128];
extern s32 veh_ma_heap[258];
#define VEH_HEAP_COUNT veh_ma_heap[257]
#define VEH_HEAP_EMPTY 0xABCD
#define VNAV_ROUTE_FAIL 42

/* Search fields of a traffic node, kept in its unnamed bytes 0x16..0x19 */
#define TN_PARENT(p_tn) (*(short *)&(p_tn)->field_16[0])
#define TN_COST(p_tn) (*(short *)&(p_tn)->field_16[2])

int build_tank_path(struct Thing *p_thing, int x, int z);

/* The asm keeps two shorts of a vehicle as one 32-bit value in places;
 * these give them in that (little endian) order. */
static ulong veh_get_d88(struct TngUVehicle *p_veh)
{
    return (ushort)p_veh->Dummy4a | ((ulong)(ushort)p_veh->Dummy4b << 16);
}

static void veh_set_d88(struct TngUVehicle *p_veh, ulong val)
{
    p_veh->Dummy4a = (short)(val & 0xFFFF);
    p_veh->Dummy4b = (short)(val >> 16);
}

static ulong veh_get_d8c(struct TngUVehicle *p_veh)
{
    return (ushort)p_veh->Dummy5a | ((ulong)(ushort)p_veh->Dummy5b << 16);
}

static void veh_set_d8c(struct TngUVehicle *p_veh, ulong val)
{
    p_veh->Dummy5a = (short)(val & 0xFFFF);
    p_veh->Dummy5b = (short)(val >> 16);
}

static ulong veh_get_d90(struct TngUVehicle *p_veh)
{
    return (ushort)p_veh->AccelZ | ((ulong)(ushort)p_veh->AccelX << 16);
}

static void veh_set_d90(struct TngUVehicle *p_veh, ulong val)
{
    p_veh->AccelZ = (short)(val & 0xFFFF);
    p_veh->AccelX = (short)(val >> 16);
}

static void veh_set_d88_low(struct TngUVehicle *p_veh, ubyte val)
{
    p_veh->Dummy4a = (short)(((ushort)p_veh->Dummy4a & 0xFF00) | val);
}

static short veh_heap_cost(int i)
{
    return TN_COST(&game_traffic_nodes[-veh_ma_heap[i]]);
}

static void veh_heap_swap(int i, int k)
{
    s32 tmp;

    tmp = veh_ma_heap[i];
    veh_ma_heap[i] = veh_ma_heap[k];
    veh_ma_heap[k] = tmp;
}

/** Adds a traffic node to the route search heap.  Remade from veh_heap_add. */
static void veh_heap_add(s32 node)
{
    int i, p, sib, tgt;

    if (VEH_HEAP_COUNT < 0x100)
        VEH_HEAP_COUNT++;
    i = VEH_HEAP_COUNT;
    veh_ma_heap[i] = node;
    if (i == 1)
        return;
    if ((i & 1) == 0)
    {
        p = i >> 1;
        if (veh_heap_cost(p) < veh_heap_cost(i))
            return;
        veh_heap_swap(i, p);
        i = p;
    }
    while (i != 1)
    {
        short ci, cs, cp;

        sib = i ^ 1;
        p = i >> 1;
        ci = veh_heap_cost(i);
        cs = veh_heap_cost(sib);
        cp = veh_heap_cost(p);
        if (ci < cs) {
            if (cs < cp)
                tgt = i;
            else if (ci < cp)
                tgt = i;
            else
                tgt = p;
        } else {
            if (ci < cp)
                tgt = sib;
            else if (cp > cs)
                tgt = sib;
            else
                tgt = p;
        }
        if (tgt == p)
            return;
        veh_heap_swap(p, tgt);
        i = p;
    }
}

/** Takes the cheapest node from the route search heap. */
static s32 veh_heap_pop(void)
{
    s32 cur;
    int n, b;

    n = VEH_HEAP_COUNT;
    cur = veh_ma_heap[1];
    veh_ma_heap[1] = veh_ma_heap[n];
    veh_ma_heap[n] = VEH_HEAP_EMPTY;
    n--;
    VEH_HEAP_COUNT = n;
    if (n == 0)
        return cur;
    b = 1;
    for (;;)
    {
        int l, r, t;
        short cb, cl, cr;

        l = 2 * b;
        r = l ^ 1;
        if (r > n)
        {
            if ((b != n) && !(veh_heap_cost(b) < veh_heap_cost(n)))
                veh_heap_swap(b, n);
            break;
        }
        cb = veh_heap_cost(b);
        cl = veh_heap_cost(l);
        cr = veh_heap_cost(r);
        if (cb < cl) {
            if (cl < cr)
                t = b;
            else if (cr <= cb)
                t = r;
            else
                t = b;
        } else {
            if (cb < cr)
                t = l;
            else if (cl < cr)
                t = l;
            else
                t = r;
        }
        if (t == b)
            break;
        veh_heap_swap(t, b);
        b = t;
    }
    return cur;
}

static void vnav_build_route(short *p_route, short start, short dest)
{
    short node;
    int i;

    i = 0;
    node = dest;
    do {
        p_route[4 + i] = node;
        node = TN_PARENT(&game_traffic_nodes[-node]);
        i++;
    } while (node != start);
    p_route[4 + i] = start;
    p_route[2] = i + 1;
}

/** Cheapest route along roads between two traffic nodes (negative
 * indices), through stations too; written to p_route, destination first.
 * Remade from VNAV_do.
 */
static void VNAV_do(short start, short dest, short *p_route)
{
    TbBool retry;

    if ((start >= 0) || (dest >= 0)) {
        p_route[4] = VNAV_ROUTE_FAIL;
        return;
    }
    // the original searches once more when nothing is found
    retry = true;
    for (;;)
    {
        ushort n;

        if (start == dest) {
            p_route[4] = VNAV_ROUTE_FAIL;
            return;
        }
        for (n = 1; n < next_traffic_node; n++)
            TN_PARENT(&game_traffic_nodes[n]) = 0;
        TN_COST(&game_traffic_nodes[0]) = 0;
        veh_ma_heap[1] = VEH_HEAP_EMPTY;
        VEH_HEAP_COUNT = 0;
        TN_COST(&game_traffic_nodes[-start]) = 0;
        veh_heap_add(start);

        while (veh_ma_heap[1] != VEH_HEAP_EMPTY)
        {
            struct TrafficNode *p_cur;
            s32 cur;
            int k;

            cur = veh_heap_pop();
            if (cur >= 0)
                continue;
            p_cur = &game_traffic_nodes[-cur];
            for (k = 0; k < 4; k++)
            {
                s32 link;

                link = p_cur->UTraffic.Link[k];
                if (link == dest) {
                    TN_PARENT(&game_traffic_nodes[-dest]) = cur;
                    vnav_build_route(p_route, start, dest);
                    return;
                }
                if (link < 0)
                {
                    struct TrafficNode *p_tn2;

                    p_tn2 = &game_traffic_nodes[-link];
                    if ((TN_PARENT(p_tn2) == 0) && ((p_tn2->Flags & 0x2000) == 0))
                    {
                        s32 dx, dz, cost;

                        dx = abs(p_tn2->X - p_cur->X);
                        dz = abs(p_tn2->Z - p_cur->Z);
                        if (dx <= dz)
                            dx >>= 1;
                        else
                            dz >>= 1;
                        cost = dx + dz + TN_COST(p_cur);
                        if ((p_cur->UTraffic.Flags[k] & 0x40) != 0)
                            cost += 500;
                        TN_COST(p_tn2) = cost;
                        TN_PARENT(p_tn2) = cur;
                        veh_heap_add(link);
                    }
                }
                if (link > 0)
                {
                    struct Thing *p_station;
                    struct TrafficNode *p_tn3;
                    s32 snode;

                    p_station = &things[link];
                    snode = p_station->U.UObject.TNode;
                    if (snode == dest) {
                        TN_PARENT(&game_traffic_nodes[-dest]) = cur;
                        vnav_build_route(p_route, start, dest);
                        return;
                    }
                    p_tn3 = &game_traffic_nodes[-snode];
                    if (TN_PARENT(p_tn3) == 0)
                    {
                        TN_COST(p_tn3) = p_station->U.UObject.Cost + TN_COST(p_cur);
                        TN_PARENT(p_tn3) = cur;
                        veh_heap_add(snode);
                    }
                }
            }
        }
        if (!retry) {
            p_route[4] = VNAV_ROUTE_FAIL;
            return;
        }
        retry = false;
    }
}

/** Is there a vehicle on the map tile. */
static TbBool vnav_vehicle_on_tile(int tile_x, int tile_z)
{
    ThingIdx thing;

    thing = game_my_big_map[(tile_z << 7) + tile_x].Child;
    while (thing != 0)
    {
        if (thing > 0) {
            if (things[thing].Type == TT_VEHICLE)
                return true;
            thing = things[thing].Next;
        } else {
            thing = sthings[thing].Next;
        }
    }
    return false;
}

/** Can a car stop on the road segment into the node at (x,z): no vehicle
 * around the node or on the first half of the segment from it. */
static TbBool vnav_parking_free(s32 nx, s32 nz, s32 bx, s32 bz)
{
    s32 sx, sz, px, pz;
    int tx, tz, dx, dz, j;

    tx = bx >> 8;
    tz = bz >> 8;
    for (dz = -1; dz <= 1; dz++)
        for (dx = -1; dx <= 1; dx++)
            if (vnav_vehicle_on_tile(tx + dx, tz + dz))
                return false;
    sx = (nx - bx) >> 3;
    sz = (nz - bz) >> 3;
    px = bx;
    pz = bz;
    for (j = 0; j < 4; j++)
    {
        px += sx;
        pz += sz;
        if (vnav_vehicle_on_tile(px >> 8, pz >> 8))
            return false;
    }
    return true;
}

/** The road segment nearest to the map point where a car can leave its
 * passengers.  Remade from VNAV_find_dropoff_point.
 */
static void VNAV_find_dropoff_point(short *p_node, ubyte *p_link, s32 *p_frac,
  short x, short z)
{
    s32 best;
    int i, k;

    if (VNAV_nearnodes[0] == 0)
        return;
    best = 0xFFFFFFF;
    *p_node = 9999;
    for (i = 0; i < 5; i++)
    {
        struct TrafficNode *p_tnode;
        s32 nx, nz;

        p_tnode = &game_traffic_nodes[VNAV_nearnodes[i]];
        nx = p_tnode->X;
        nz = p_tnode->Z;
        for (k = 0; k < 4; k++)
        {
            struct TrafficNode *p_tnode2;
            short link;
            s32 bx, bz, dist, frac;

            link = p_tnode->UTraffic.Link[k];
            if (link >= 0)
                continue;
            if ((p_tnode->UTraffic.Flags[k] & 0x40) != 0)
                continue;
            p_tnode2 = &game_traffic_nodes[-link];
            bx = p_tnode2->X;
            bz = p_tnode2->Z;
            if (((p_tnode2->Flags & 0x0800) != 0) && !vnav_parking_free(nx, nz, bx, bz))
                continue;
            VNAV_qwertyuiop(&dist, &frac, nx, nz, bx, bz, x, z);
            if (dist >= best)
                continue;
            *p_node = VNAV_nearnodes[i];
            *p_link = k;
            *p_frac = frac;
            best = dist;
        }
    }
}

/** Can a car reach the road near the map point.
 * Remade from VNAV_can_i_get_there.
 */
static TbBool VNAV_can_i_get_there(struct Thing *p_vehicle, short x, short z)
{
    short node;
    ubyte link;
    s32 frac;

    VNAV_find_nearnodes(x, z);
    node = 9999;
    link = 0;
    frac = 0;
    VNAV_find_dropoff_point(&node, &link, &frac, x, z);
    if (node == 9999)
        return false;
    if (p_vehicle->U.UVehicle.TNode == -node)
        return true;
    VNAV_do(p_vehicle->U.UVehicle.TNode, -node, VNAV_route);
    if (VNAV_route[4] == VNAV_ROUTE_FAIL)
        return false;
    return true;
}

/** Approximate length of a vector from its absolute coordinates. */
static s32 vnav_dist_approx(s32 adx, s32 adz)
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

/** Stores the links to take at the next nodes of VNAV_route in the vehicle,
 * 2 bits each, up to 16, the first in the lowest bits (Dummy5a/5b as one
 * value). Returns how many; with `limited` the link is looked for among
 * the 4 of the node only, and -1 is returned when not found (as
 * VNAV_renavigate); without it the search goes on, as in VNAV_goto. */
static int vnav_route_to_turns(struct TngUVehicle *p_veh, TbBool limited)
{
    int m, n, s;
    ulong d;

    m = VNAV_route[2] - 2;
    n = 0;
    while (m >= 0)
    {
        struct TrafficNode *p_tn;
        short want;
        int k;

        p_tn = &game_traffic_nodes[-VNAV_route[4 + m + 1]];
        want = VNAV_route[4 + m];
        for (k = 0; ; k++)
        {
            short l;

            if (limited && (k > 3))
                return -1;
            l = ((short *)&p_tn->UTraffic)[k];
            if (l < 0) {
                if (l == want)
                    break;
            } else if (l > 0) {
                if (things[l].U.UObject.TNode == want)
                    break;
            }
        }
        d = veh_get_d8c(p_veh);
        d = (d & ~3UL) | (ulong)k;
        d = (d >> 2) | (d << 30);
        veh_set_d8c(p_veh, d);
        n++;
        m--;
        if (n >= 16)
            break;
    }
    d = veh_get_d8c(p_veh);
    s = (2 * n) & 31;
    if (s != 0)
        d = (d << s) | (d >> (32 - s));
    veh_set_d8c(p_veh, d);
    return n;
}

/** Sends a car along the roads to the drop-off point nearest to the map
 * point: 2 when it is already on that segment before the point, 1 with a
 * route, 0 when there is none.  Remade from VNAV_goto.
 */
static int VNAV_goto(struct Thing *p_vehicle, short x, short z)
{
    struct TngUVehicle *p_veh;
    short node;
    ubyte link;
    s32 frac;
    short tlink;
    ulong d;

    p_veh = &p_vehicle->U.UVehicle;
    p_veh->destx = x;
    p_veh->destz = z;
    VNAV_find_nearnodes(x, z);
    node = 9999;
    link = 0;
    frac = 0;
    VNAV_find_dropoff_point(&node, &link, &frac, x, z);
    if (node == 9999)
        return 0;
    if (frac < 0x23)
        frac = 0x23;
    if (frac > 0xDC)
        frac = 0xDC;
    tlink = game_traffic_nodes[node].UTraffic.Link[link];
    if ((tlink < 0) && ((game_traffic_nodes[-tlink].Flags & 0x0800) != 0))
        frac = 200;

    if ((p_veh->Dummy4b == -node) && (((veh_get_d88(p_veh) & 0xC0) >> 6) == link) &&
      (p_veh->TNode < 0))
    {
        struct TrafficNode *p_tn1, *p_tn2;
        s32 seglen, dist;

        p_tn1 = &game_traffic_nodes[node];
        p_tn2 = &game_traffic_nodes[-p_veh->TNode];
        seglen = vnav_dist_approx(abs(p_tn2->X - p_tn1->X), abs(p_tn2->Z - p_tn1->Z));
        dist = vnav_dist_approx(abs((p_vehicle->X >> 8) - p_tn1->X),
          abs((p_vehicle->Z >> 8) - p_tn1->Z));
        if (((dist << 8) / seglen) < frac)
        {
            s32 nx, nz;

            p_vehicle->State = 0x33;
            nx = (((p_tn2->X - p_tn1->X) * frac) >> 8) + p_tn1->X;
            nz = (((p_tn2->Z - p_tn1->Z) * frac) >> 8) + p_tn1->Z;
            veh_set_d8c(p_veh, ((ulong)nx << 16) | (ulong)nz);
            return 2;
        }
    }

    if (p_veh->TNode == -node)
    {
        veh_set_d8c(p_veh, link);
        d = veh_get_d88(p_veh) & ~0xFF00UL;
        d |= (ulong)frac << 8;
        d &= ~0x3FUL;
        veh_set_d88(p_veh, d);
    }
    else
    {
        int n;

        VNAV_do(p_veh->TNode, -node, VNAV_route);
        if (VNAV_route[4] == VNAV_ROUTE_FAIL)
            return 0;
        // links to take at the next nodes; not limited to the 4 links of
        // a node, as in the original
        n = vnav_route_to_turns(p_veh, false);
        d = veh_get_d88(p_veh) & ~0xFF00UL;
        d |= (ulong)frac << 8;
        d &= ~0x3FUL;
        d |= (ulong)n;
        veh_set_d88(p_veh, d);
    }
    veh_set_d90(p_veh, ((ulong)(ushort)(-node) << 16) | link);
    return 1;
}

static ulong vnav_dest_d8c(short x, short z)
{
    return ((ulong)(s32)x << 16) | (ulong)(s32)z;
}

/** A car stopping (state 0x36) drives on to new map point.
 * Remade from VNAV_goto_and_unstop. */
static int VNAV_goto_and_unstop(struct Thing *p_vehicle, short x, short z)
{
    struct TngUVehicle *p_veh;
    s32 v;

    p_veh = &p_vehicle->U.UVehicle;
    v = (ushort)p_veh->Dummy5a;
    v = 10 - (v * 10) / 17;
    veh_set_d8c(p_veh, vnav_dest_d8c(x, z));
    p_vehicle->State = 0x38;
    veh_set_d88_low(p_veh, (ubyte)(p_veh->Dummy4a & 0xC0));
    veh_set_d88(p_veh, veh_get_d88(p_veh) | (ulong)v);
    return 1;
}

/** A parked car (state 0x21) drives to new map point.
 * Remade from VNAV_goto_and_unpark. */
static int VNAV_goto_and_unpark(struct Thing *p_vehicle, short x, short z)
{
    struct TngUVehicle *p_veh;

    p_veh = &p_vehicle->U.UVehicle;
    p_vehicle->State = 0x38;
    veh_set_d8c(p_veh, vnav_dest_d8c(x, z));
    veh_set_d90(p_veh, veh_get_d90(p_veh) << 1);
    veh_set_d88_low(p_veh, (ubyte)((p_veh->Dummy4a & 0xC0) | 0x0A));
    return 1;
}

/** A car placed by the road (state 0x3D) drives to new map point.
 * Remade from VNAV_goto_and_unplace. */
static int VNAV_goto_and_unplace(struct Thing *p_vehicle, short x, short z)
{
    struct TngUVehicle *p_veh;

    p_veh = &p_vehicle->U.UVehicle;
    veh_set_d8c(p_veh, vnav_dest_d8c(x, z));
    p_vehicle->State = 0x3E;
    veh_set_d88_low(p_veh, (ubyte)((p_veh->Dummy4a & 0xC0) | 0x14));
    return 1;
}

/** Has the car taken all the turns of its route.  Remade from VNAV_there. */
static TbBool VNAV_there(struct Thing *p_vehicle)
{
    return ((p_vehicle->U.UVehicle.Dummy4a & 0x3F) == 0);
}

/** The link to take at the node reached, from the stored turns.
 * Remade from VNAV_turn. */
static int VNAV_turn(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    ulong d;
    int k;

    p_veh = &p_vehicle->U.UVehicle;
    veh_set_d88(p_veh, veh_get_d88(p_veh) - 1);
    d = veh_get_d8c(p_veh);
    k = d & 3;
    veh_set_d8c(p_veh, (ulong)((s32)d >> 2));
    return k;
}

/** A new route to the destination node kept by VNAV_goto.
 * Remade from VNAV_renavigate. */
static TbBool VNAV_renavigate(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    int n;

    p_veh = &p_vehicle->U.UVehicle;
    VNAV_do(p_veh->TNode, p_veh->AccelX, VNAV_route);
    if (VNAV_route[4] == VNAV_ROUTE_FAIL)
        return false;
    n = vnav_route_to_turns(p_veh, true);
    if (n < 0)
        return false;
    veh_set_d88_low(p_veh, (ubyte)(p_veh->Dummy4a & 0xC0));
    veh_set_d88(p_veh, veh_get_d88(p_veh) | (ulong)n);
    return true;
}

#ifdef SW_FNCHK
static void process_next_vnav_turn_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_next_vnav_turn\n"
        : : "a" (p_vehicle) : "cc", "memory");
}
#endif

/** A car at a traffic node: the next link of its route, or, at the end of
 * the route, the segment with the drop-off point and the point on it.
 * Remade from process_next_vnav_turn.
 */
static void process_next_vnav_turn_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    struct TrafficNode *p_tnode;
    int k;

    p_veh = &p_vehicle->U.UVehicle;
    if (!VNAV_there(p_vehicle) || ((p_veh->AccelX != p_veh->TNode)
      && VNAV_renavigate(p_vehicle)))
    {
        p_tnode = &game_traffic_nodes[-p_veh->TNode];
        p_veh->Dummy4b = p_veh->TNode;
        k = VNAV_turn(p_vehicle);
        veh_set_d88_low(p_veh, (ubyte)(p_veh->Dummy4a & 0x3F));
        veh_set_d88(p_veh, veh_get_d88(p_veh) | ((ulong)k << 6));
        p_veh->TNode = p_tnode->UTraffic.Link[k];
        if ((p_tnode->UTraffic.Flags[k] & 0x01) != 0)
            p_vehicle->Flag |= TngF_Unkn08000000;
        else
            p_vehicle->Flag &= ~TngF_Unkn08000000;
        if (p_veh->TNode == 0)
            start_crashing(p_vehicle);
        return;
    }
    if (p_veh->AccelX != p_veh->TNode)
        p_vehicle->State = 0x3C; // no route any more
    {
        struct TrafficNode *p_tn2;
        s32 frac, nx, nz;
        int link;

        p_tnode = &game_traffic_nodes[-p_veh->TNode];
        p_veh->Dummy4b = p_veh->TNode;
        link = p_veh->AccelZ & 0xF;
        veh_set_d88_low(p_veh, (ubyte)(p_veh->Dummy4a & 0x3F));
        veh_set_d88(p_veh, veh_get_d88(p_veh) | ((ulong)link << 6));
        p_veh->TNode = p_tnode->UTraffic.Link[link];
        p_vehicle->State = 0x33;
        p_tn2 = &game_traffic_nodes[-p_veh->TNode];
        frac = (veh_get_d88(p_veh) & 0xFF00) >> 8;
        if (frac < 0x30)
            frac = 0x30;
        else if (frac > 0xE0)
            frac = 0xE0;
        nx = (((p_tn2->X - p_tnode->X) * frac) >> 8) + p_tnode->X;
        nz = (((p_tn2->Z - p_tnode->Z) * frac) >> 8) + p_tnode->Z;
        veh_set_d8c(p_veh, ((ulong)nx << 16) | (ulong)nz);
    }
}

void process_next_vnav_turn(struct Thing *p_vehicle)
{
    FNCHK_VOID("process_next_vnav_turn", process_next_vnav_turn_asm(p_vehicle),
      process_next_vnav_turn_c(p_vehicle));
}

void remove_locks(struct Thing *p_vehicle);

/** Steps of a car leaving the roadside, from the last: AngleDY / 8,
 * AngleX added, ReqdSpeed. The asm data table `unplace`, as constants (the
 * Amiga build has the asm data as zeroed storage). */
static const short vehicle_unplace_steps[19][3] = {
    {  -5, 0,  200}, { -10, 0,  150}, { -20, 0,  100}, { -30, 0,   50},
    { -40, 0,    0}, { -50, 0,    0}, { -60, 0,    0}, { -70, 0,  -50},
    { -60, 0, -100}, { -50, 0, -150}, { -40, 0, -250}, { -30, 0, -300},
    { -20, 0, -350}, { -10, 0, -400}, { -10, 0, -450}, {  -5, 0, -450},
    {   0, 0, -500}, {   0, 0, -500}, {   0, 0, -500},
};

/** Octagonal distance of two points in map coords. */
static s32 vnav_oct_dist_pts(s32 x1, s32 z1, s32 x2, s32 z2)
{
    s32 adx, adz;

    adx = abs(x1 - x2);
    adz = abs(z1 - z2);
    if (adx > adz)
        adz >>= 1;
    else
        adx >>= 1;
    return adx + adz;
}

/** Is another car's front or back (its position moved along its heading
 * by its size) close to the map point, on the 5x5 tiles round it; with
 * `parked_only` only parked cars (state 0x21) count.
 * Remade from check_for_a_moving_vehicle_here (any car) and
 * check_for_a_vehicle_here (parked cars). */
static TbBool check_for_vehicle_near(s32 cor_x, s32 cor_z, struct Thing *p_self,
  TbBool parked_only)
{
    int tile_x, tile_z, tx, tz;

    tile_x = cor_x >> 8;
    tile_z = cor_z >> 8;
    for (tx = tile_x - 2; tx <= tile_x + 2; tx++)
    {
        for (tz = tile_z - 2; tz <= tile_z + 2; tz++)
        {
            ThingIdx thing;

            if ((tx < 0) || (tx > MAP_TILE_WIDTH - 1) || (tz < 0) || (tz > MAP_TILE_HEIGHT - 1))
                continue;
            thing = game_my_big_map[(tz << 7) + tx].Child;
            while (thing != 0)
            {
                struct Thing *p_thing;
                struct M33 *p_mat;
                s32 rad, dfx, dfz, fx, fz, bx, bz;

                if (thing < 0) {
                    thing = sthings[thing].Next;
                    continue;
                }
                p_thing = &things[thing];
                thing = p_thing->Next;
                if ((p_thing->Type != TT_VEHICLE) || (p_thing == p_self) || (p_thing->SubType != 0x28))
                    continue;
                if (parked_only && ((ushort)p_thing->State != 0x21))
                    continue;
                rad = ((ushort)p_thing->Radius * 400) >> 9;
                p_mat = &local_mats[p_thing->U.UVehicle.MatrixIndex];
                dfx = (-p_mat->R[0][2] * 4 * rad) >> 8;
                dfz = (-p_mat->R[2][2] * 4 * rad) >> 8;
                fx = (p_thing->X + dfx) >> 8;
                fz = (p_thing->Z + dfz) >> 8;
                bx = (p_thing->X - dfx) >> 8;
                bz = (p_thing->Z - dfz) >> 8;
                if (vnav_oct_dist_pts(cor_x, cor_z, fx, fz) < rad)
                    return true;
                if (vnav_oct_dist_pts(cor_x, cor_z, bx, bz) < rad)
                    return true;
            }
        }
    }
    return false;
}

#ifdef SW_FNCHK
static void process_stopping_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_stopping\n"
        : : "a" (p_vehicle) : "cc", "memory");
}

static void process_unstopping_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_unstopping\n"
        : : "a" (p_vehicle) : "cc", "memory");
}

static void process_unplacing_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_unplacing\n"
        : : "a" (p_vehicle) : "cc", "memory");
}
#endif

/** A car stopping at its drop-off point: steering to the kept angle, the
 * tilt and sideways slide fading, then parked (state 0x21, or 0x3D at a
 * roadside node).  Remade from process_stopping. */
static void process_stopping_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    s32 cnt, angle, ax, az, d;

    p_veh = &p_vehicle->U.UVehicle;
    cnt = (ushort)p_veh->Dummy5a;
    angle = p_veh->Dummy5b;
    ax = p_veh->AccelX;
    az = p_veh->AccelZ;
    if (cnt > 10) {
        p_veh->AngleZ = (0x11 - cnt) * 0x50;
        ax += ax >> 1;
        az += az >> 1;
    } else {
        p_veh->AngleZ = cnt * 0x28;
        ax -= ax >> 2;
        az -= az >> 2;
    }
    veh_set_d90(p_veh, ((ulong)ax << 16) | (ulong)(az & 0xFFFF));
    d = p_veh->AngleY - angle;
    if (d < -0x400)
        d += 0x800;
    else if (d > 0x400)
        d -= 0x800;
    if (d < 0) {
        if (d > -50)
            p_veh->AngleDY = -d * 2;
        else
            p_veh->AngleDY = (0x800 - (p_vehicle->Speed >> 3)) >> 2;
    } else if (d > 0) {
        if (d < 50)
            p_veh->AngleDY = -(d * 2);
        else
            p_veh->AngleDY = -((0x800 - (p_vehicle->Speed >> 3)) >> 2);
    }
    p_veh->ReqdSpeed = 0;
    if (cnt > 0)
        cnt--;
    p_veh->Dummy5a = cnt;
    if ((p_vehicle->Speed == 0) && (cnt == 0) && (abs(p_veh->AngleDY) < 20))
    {
        if ((p_veh->TNode < 0) && ((game_traffic_nodes[-p_veh->TNode].Flags & 0x0800) != 0))
            p_vehicle->State = 0x3D;
        else
            p_vehicle->State = 0x21;
        p_vehicle->Flag |= 0x0004;
        p_veh->WorkPlace &= ~0x0100;
        remove_locks(p_vehicle);
    }
}

/** A stopped car starting off again: waiting while another car is in front
 * or behind, then the tilt and slide fading and speed growing; at the end
 * it goes on to the kept map point.  Remade from process_unstopping. */
static void process_unstopping_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    s32 ax, az, step;

    p_veh = &p_vehicle->U.UVehicle;
    ax = p_veh->AccelX;
    az = p_veh->AccelZ;
    step = p_veh->Dummy4a & 0x3F;
    if (step == 10)
    {
        s32 cx, cz;

        cx = (p_vehicle->X >> 8) - 8 * ax;
        cz = (p_vehicle->Z >> 8) - 8 * az;
        if (check_for_vehicle_near(cx + 2 * az, cz - 2 * ax, p_vehicle, false)
          || check_for_vehicle_near(cx - 2 * az, cz + 2 * ax, p_vehicle, false))
        {
            p_veh->ReqdSpeed = 0;
            p_veh->WorkPlace |= 0x0200;
            return;
        }
    }
    p_veh->WorkPlace &= ~0x0200;
    if (step > 5) {
        p_veh->AngleZ = (0xB - step) * -100;
        ax += ax >> 1;
        az += az >> 1;
    } else {
        p_veh->AngleZ = step * -100;
        ax -= ax >> 2;
        az -= az >> 2;
    }
    veh_set_d90(p_veh, ((ulong)ax << 16) | (ulong)(az & 0xFFFF));
    p_veh->ReqdSpeed += 100;
    if (step > 0)
        step--;
    veh_set_d88_low(p_veh, (ubyte)(p_veh->Dummy4a & 0xC0));
    veh_set_d88(p_veh, veh_get_d88(p_veh) | (ulong)step);
    if (step == 0)
    {
        p_vehicle->State = 0x32;
        start_goto_vehicle(p_vehicle, p_veh->Dummy5b, p_veh->Dummy5a);
    }
}

/** A car leaving its roadside place: first onto the road (its node's first
 * link), then backing and turning by the steps of vehicle_unplace_steps;
 * at the end it goes on to the kept map point.
 * Remade from process_unplacing. */
static void process_unplacing_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    s32 step;

    p_veh = &p_vehicle->U.UVehicle;
    step = p_veh->Dummy4a & 0x3F;
    if (step == 0x14)
    {
        struct TrafficNode *p_tnode;
        int k;

        p_veh->Dummy4b = p_veh->TNode;
        p_tnode = &game_traffic_nodes[-p_veh->TNode];
        for (k = 0; k < 4; k++) {
            if (p_tnode->UTraffic.Link[k] != 0) {
                p_veh->TNode = p_tnode->UTraffic.Link[k];
                break;
            }
        }
        step = 0x13;
    }
    if (step <= 0x12)
    {
        const short *p_step;

        p_step = vehicle_unplace_steps[step];
        p_veh->ReqdSpeed = p_step[2];
        p_veh->AngleDY = p_step[0] << 3;
        p_veh->AngleX += p_step[1];
        if (step > 0)
            step--;
        if (step == 0)
        {
            p_vehicle->State = 0x32;
            start_goto_vehicle(p_vehicle, p_veh->Dummy5b, p_veh->Dummy5a);
        }
    }
    else
    {
        struct TrafficNode *p_tnode;
        s32 dx, dz;

        p_veh->ReqdSpeed = -600;
        p_tnode = &game_traffic_nodes[-p_veh->TNode];
        dx = (p_vehicle->X >> 8) - p_tnode->X;
        dz = (p_vehicle->Z >> 8) - p_tnode->Z;
        if (dx * dx + dz * dz < 0x10000)
            step = 0x12;
    }
    veh_set_d88_low(p_veh, (ubyte)(p_veh->Dummy4a & 0xC0));
    veh_set_d88(p_veh, veh_get_d88(p_veh) | (ulong)step);
}

void process_stopping(struct Thing *p_vehicle)
{
    FNCHK_VOID("process_stopping", process_stopping_asm(p_vehicle), process_stopping_c(p_vehicle));
}

void process_unstopping(struct Thing *p_vehicle)
{
    FNCHK_VOID("process_unstopping", process_unstopping_asm(p_vehicle), process_unstopping_c(p_vehicle));
}

void process_unplacing(struct Thing *p_vehicle)
{
    FNCHK_VOID("process_unplacing", process_unplacing_asm(p_vehicle), process_unplacing_c(p_vehicle));
}

/** Ship route: breadth-first search over the sea nodes (flag 0x0200) to
 * the one nearest to the map point; the route is left in the GateLink
 * fields, from the ship's node on.  Remade from send_ship_to.
 */
static void send_ship_to(struct Thing *p_vehicle, short x, short z)
{
    short queue[32];
    int head, tail;
    s32 best;
    short best_node, start, cur;
    ushort n;

    for (n = 1; n < next_traffic_node; n++) {
        if ((game_traffic_nodes[n].Flags & 0x0200) != 0)
            game_traffic_nodes[n].GateLink = -1;
    }
    start = p_vehicle->U.UVehicle.TNode;
    game_traffic_nodes[start].GateLink = 0;
    head = 0;
    tail = 0;
    best = 0xFFFFFFF;
    best_node = 0;
    cur = start;
    for (;;)
    {
        struct TrafficNode *p_tn;
        int k;

        p_tn = &game_traffic_nodes[cur];
        for (k = 0; k < 4; k++)
        {
            short link;

            link = p_tn->UTraffic.Link[k];
            if (link == 0)
                continue;
            if ((p_tn->UTraffic.Flags[k] & 0xC0) != 0)
                continue;
            if (game_traffic_nodes[-link].GateLink != -1)
                continue;
            queue[tail] = -link;
            tail = (tail + 1) & 0x1F;
            game_traffic_nodes[-link].GateLink = cur;
        }
        if (head == tail)
            break;
        {
            s32 dx, dz, dist;

            cur = queue[head];
            p_tn = &game_traffic_nodes[cur];
            dx = x - p_tn->X;
            dz = z - p_tn->Z;
            dist = dx * dx + dz * dz;
            head = (head + 1) & 0x1F;
            if (dist < best) {
                best = dist;
                best_node = cur;
            }
        }
    }
    if (best < 0x200000)
    {
        short node, prev, next;

        node = best_node;
        prev = 0;
        do {
            next = game_traffic_nodes[node].GateLink;
            game_traffic_nodes[node].GateLink = prev;
            prev = node;
            node = next;
        } while (next != 0);
    }
}

#ifdef SW_FNCHK
static int start_goto_vehicle_asm(struct Thing *p_vehicle, int x, int z)
{
    int ret;
    asm volatile (
      "call ASM_start_goto_vehicle\n"
        : "=a" (ret), "+d" (x), "+b" (z) : "0" (p_vehicle) : "cc", "memory");
    return ret;
}
#endif

/** Sends a vehicle to the map point: a car along the roads, flying
 * vehicles and tanks along a path, a ship over the sea nodes.
 * Remade from start_goto_vehicle.
 */
static int start_goto_vehicle_c(struct Thing *p_vehicle, int x, int z)
{
    struct TngUVehicle *p_veh;
    struct MyPath *p_path;

    p_veh = &p_vehicle->U.UVehicle;
    if ((ushort)p_vehicle->State == 0x45)
        return 0;
    if (x < 0)
        x = 0x80;
    else if (x >= 0x8000)
        x = 0x7F80;
    if (z < 0)
        z = 0x80;
    else if (z >= 0x8000)
        z = 0x7F80;

    switch (p_vehicle->SubType)
    {
    case 0x28:
        break;
    case SubTT_VEH_FLYING:
        remove_path(p_vehicle);
        if (ariadne_initialise_route(p_vehicle, x, z) <= 0)
            return 0;
        unkn_path_func_001(p_vehicle, 1);
        p_path = &my_paths[p_veh->PathIndex];
        p_veh->GotoX = p_path->X[p_vehicle->PathOffset & 3];
        p_veh->GotoZ = p_path->Z[p_vehicle->PathOffset & 3];
        p_vehicle->State = 0x39;
        p_veh->TNode = 0;
        return 1;
    case 0x33:
    case 0x3B:
        remove_path(p_vehicle);
        if (build_tank_path(p_vehicle, x, z) == 0)
            return 0;
        p_path = &my_paths[p_veh->PathIndex];
        p_veh->GotoX = p_path->X[p_vehicle->PathOffset & 3];
        p_veh->GotoZ = p_path->Z[p_vehicle->PathOffset & 3];
        p_vehicle->State = 0x32;
        return 1;
    case 0x36:
        send_ship_to(p_vehicle, x, z);
        return 0;
    default:
        return 0;
    }

    if (x < 0x200)
        x = 0x200;
    if (z < 0x200)
        z = 0x200;
    if (x > 0x7E00)
        x = 0x7E00;
    if (z > 0x7E00)
        z = 0x7E00;
    if (p_veh->TNode >= 0)
        return 0;
    switch ((ushort)p_vehicle->State)
    {
    case 0x36:
        if (!VNAV_can_i_get_there(p_vehicle, x, z))
            return 0;
        VNAV_goto_and_unstop(p_vehicle, x, z);
        p_vehicle->Flag &= ~0x0004;
        return 0;
    case 0x21:
        if (!VNAV_can_i_get_there(p_vehicle, x, z))
            return 0;
        VNAV_goto_and_unpark(p_vehicle, x, z);
        p_vehicle->Flag &= ~0x0004;
        return 0;
    case 0x3D:
        if (!VNAV_can_i_get_there(p_vehicle, x, z))
            return 0;
        VNAV_goto_and_unplace(p_vehicle, x, z);
        p_vehicle->Flag &= ~0x0004;
        return 0;
    default:
        break;
    }
    switch (VNAV_goto(p_vehicle, x, z))
    {
    case 1:
        p_vehicle->State = 0x32;
        p_veh->GotoY = 0;
        p_veh->GotoX = x;
        p_veh->GotoZ = z;
        p_vehicle->Flag &= ~0x0004;
        return 1;
    case 2:
        return 1;
    default:
        if ((ushort)p_vehicle->State == 0x36)
            return 0;
        p_vehicle->State = 0x3C;
        return 0;
    }
}

int start_goto_vehicle(struct Thing *p_vehicle, int x, int z)
{
    int ret;
    FNCHK_RET("start_goto_vehicle", ret, start_goto_vehicle_asm(p_vehicle, x, z),
      start_goto_vehicle_c(p_vehicle, x, z));
    return ret;
}

static TbBool check_for_vehicle_near(s32 cor_x, s32 cor_z, struct Thing *p_self,
  TbBool parked_only);

#ifdef SW_FNCHK
static int check_for_a_vehicle_here_asm(int x, int z, struct Thing *p_vehicle)
{
    int ret;
    asm volatile (
      "call ASM_check_for_a_vehicle_here\n"
        : "=a" (ret), "+d" (z) : "0" (x), "b" (p_vehicle) : "cc", "memory");
    return ret;
}
#endif

/** Is a parked car's front or back close to the map point.
 * Remade from check_for_a_vehicle_here. */
int check_for_a_vehicle_here(int x, int z, struct Thing *p_vehicle)
{
    int ret;
    FNCHK_RET("check_for_a_vehicle_here", ret, check_for_a_vehicle_here_asm(x, z, p_vehicle),
      check_for_vehicle_near(x, z, p_vehicle, true));
    return ret;
}

#ifdef SW_FNCHK
static void person_hit_by_car_asm(struct Thing *p_person, struct Thing *p_vehicle)
{
    asm volatile ("call ASM_person_hit_by_car\n"
        : : "a" (p_person), "d" (p_vehicle) : "cc", "memory");
}
#endif

short person_move(struct Thing *p_person);
void person_go_sleep(struct Thing *p_person);

/** A person hit by a car is pushed by the car's motion, turned to the side
 * farther from the car; the push stays only if the person was destroyed.
 * Remade from person_poke. */
static void person_poke(struct Thing *p_person, s32 cvx, s32 cvz, struct Thing *p_car)
{
    s32 px, pz, cx, cz, a, b, d1, d2, mvx, mvz;
    s32 old_vx, old_vz;
    short old_speed;

    px = p_person->X >> 8;
    pz = p_person->Z >> 8;
    cx = p_car->X >> 8;
    cz = p_car->Z >> 8;
    a = abs(px - cvz - cx);
    b = abs(pz + cvx - cz);
    if (a > b)
        b >>= 1;
    else
        a >>= 1;
    d1 = a + b;
    a = abs(cx - px) + cvz;
    b = abs(cz - pz) - cvx;
    if (a > b)
        b >>= 1;
    else
        a >>= 1;
    d2 = a + b;
    mvx = cvx;
    mvz = cvz;
    if (d2 > d1) {
        mvx = -cvx;
        mvz = -cvz;
    }
    old_vx = p_person->VX;
    old_vz = p_person->VZ;
    old_speed = p_person->Speed;
    p_person->VX = cvx - mvz;
    p_person->VZ = cvz + mvx;
    p_person->Speed = 16;
    person_move(p_person);
    if ((p_person->Flag & 0x0002) == 0)
    {
        p_person->VX = old_vx;
        p_person->VZ = old_vz;
        p_person->Speed = old_speed;
    }
}

/** A person hit by a car: pushed away and knocked down; a player agent
 * loses health and may die (counted in the mission stats for the driver's
 * side), others are knocked out.  Remade from person_hit_by_car. */
static void person_hit_by_car_c(struct Thing *p_person, struct Thing *p_vehicle)
{
    short dx, dz, angle, angle2;
    ushort base, driver;
    ubyte lost;

    dx = (short)((p_person->X - p_vehicle->X) >> 8);
    dz = (short)((p_person->Z - p_vehicle->Z) >> 8);
    person_poke(p_person, p_vehicle->VX, p_vehicle->VZ, p_vehicle);
    dx = (short)(dx + (short)p_vehicle->VZ);
    dz = (short)(dz - (short)p_vehicle->VX);
    angle = (short)arctan(dx, -dz);
    p_person->U.UPerson.RecoilTimer = 9;
    p_person->U.UPerson.RecoilDir = (ubyte)(angle >> 3);
    angle2 = (short)((angle & 0x00FF) | ((((angle >> 8) + 4) & 7) << 8));
    p_person->U.UPerson.Angle = (ubyte)(angle2 >> 8);
    if (((p_person->Flag & 0x00010000) == 0) && (p_person->U.UPerson.AnimMode != 0x10))
        p_person->U.UPerson.OldAnimMode = p_person->U.UPerson.AnimMode;
    p_person->U.UPerson.AnimMode = 0x10;
    base = people_frames[p_person->SubType][p_person->U.UPerson.AnimMode];
    p_person->Frame = nstart_ani[base + p_person->U.UPerson.Angle];
    p_person->StartFrame = people_frames[p_person->SubType][p_person->U.UPerson.AnimMode] - 1;
    p_person->U.UPerson.ShieldEnergy -= 0x23;
    p_person->Flag |= 0x00010000;
    if (p_person->U.UPerson.ShieldEnergy > 0)
    {
        if ((p_person->U.UPerson.WeaponsCarried & 0x00400000) != 0)
            p_person->Flag |= 0x00200000;
        return;
    }
    if ((p_person->Flag & TngF_PlayerAgent) == 0)
    {
        if (p_person->SubType != 7)
            person_go_sleep(p_person);
        return;
    }
    p_person->Health -= 200;
    if (p_person->Health > 0)
        return;
    if (((ushort)p_person->State == 0x0C) || ((ushort)p_person->State == 0x0D))
        return;
    lost = 0;
    if (!in_network_game)
        lost = mission_status[open_brief].AgentsLost;
    set_person_dead(p_person, 10);
    driver = (ushort)p_vehicle->U.UVehicle.PassengerHead;
    if (driver == 0)
        return;
    if (!in_network_game)
    {
        if (things[driver].U.UPerson.EffectiveGroup != ingame.MyGroup)
            return;
        switch (p_person->SubType)
        {
        case 1: case 2: case 3: case 9: case 12:
            if (mission_status[open_brief].AgentsLost == lost)
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
    else
    {
        struct Thing *p_driver;

        p_driver = &things[driver];
        if (((p_driver->Flag & TngF_PlayerAgent) != 0) && ((p_person->Flag & TngF_PlayerAgent) != 0))
            mission_status[(ushort)p_driver->U.UPerson.ComCur >> 2].MP.AgentsKilled[(ushort)p_person->U.UPerson.ComCur >> 2]++;
    }
}

void person_hit_by_car(struct Thing *p_person, struct Thing *p_vehicle)
{
    FNCHK_VOID("person_hit_by_car", person_hit_by_car_asm(p_person, p_vehicle),
      person_hit_by_car_c(p_person, p_vehicle));
}

/** Checks if the vehicle collides with another object, provided as separate params.
 */
static TbBool vehicle_check_collide_with_area(struct Thing *p_vehA,
  int posBx, int posBy, int posBz, int radB, int matBid)
{
    struct M33 *p_matA;
    int radA_incl_speed, matA02, matA22;
    int futureAx, futureAz;
    int posAx, posAz;
    struct M33 *p_matB;
    int radB_incl_speed, matB02, matB22;
    int futureBx, futureBz;
    int posB_beg_x, posB_beg_z;
    int posB_end_x, posB_end_z;
    int dist_begX, dist_begZ;
    int dist_endX, dist_endZ;
    int distXZ_sq;

    assert(p_vehA->U.UVehicle.MatrixIndex < next_local_mat);
    p_matA = &local_mats[p_vehA->U.UVehicle.MatrixIndex];
    matA02 = -4 * p_matA->R[0][2];
    matA22 = -4 * p_matA->R[2][2];
    radA_incl_speed = (386 * p_vehA->Radius >> 8)
      + (p_vehA->Speed >> 3) + (p_vehA->Speed >> 4);
    futureAx = radA_incl_speed * matA02 >> 8;
    futureAz = radA_incl_speed * matA22 >> 8;

    posAx = p_vehA->X + futureAx - (matA22 >> 2);
    posAz = p_vehA->Z + futureAz + (matA02 >> 2);

    if (abs(posBy - p_vehA->Y) >= 2048)
        return false;

    if ((p_vehA->X >> 16 == posBx >> 16) && (p_vehA->Z >> 16 == posBz >> 16))
        return true;

    assert(matBid < next_local_mat);
    p_matB = &local_mats[matBid];
    matB02 = -4 * p_matB->R[0][2];
    matB22 = -4 * p_matB->R[2][2];
    radB_incl_speed = (384 * radB >> 9);
    futureBx = radB_incl_speed * matB02 >> 8;
    futureBz = radB_incl_speed * matB22 >> 8;

    posB_beg_x = posBx - futureBx;
    posB_beg_z = posBz - futureBz;
    posB_end_x = posBx + futureBx;
    posB_end_z = posBz + futureBz;

    dist_begX = (posB_beg_x - posAx) >> 8;
    dist_begZ = (posB_beg_z - posAz) >> 8;
    dist_endX = (posB_end_x - posAx) >> 8;
    dist_endZ = (posB_end_z - posAz) >> 8;

    distXZ_sq = dist_endZ * dist_endZ + dist_endX * dist_endX;
    if (distXZ_sq >= dist_begZ * dist_begZ + dist_begX * dist_begX)
        distXZ_sq = dist_begZ * dist_begZ + dist_begX * dist_begX;

    if (distXZ_sq >= radB_incl_speed * radB_incl_speed)
        return false;

    return true;
}

/** Check if the vehicle is stopped due to state of tnode it rides on.
 */
TbBool vehicle_stopped_on_tnode(struct Thing *p_vehicle)
{
    if (p_vehicle->U.UVehicle.TNode >= 0)
        return false;

    // Repeat the stop conditions from process_vehicle_stop_for_pedestrians() exactly
    if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_Unkn0008) != 0)
    {
        if (p_vehicle->State == VehSt_GOTO_LOC || p_vehicle->State == VehSt_WANDER)
        {
            struct TrafficNode *p_tnode;
            p_tnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.TNode];
            if ((p_tnode->Flags & TNdF_Unkn0040) != 0)
            {
                if (p_tnode->GateLink == p_vehicle->ThingOffset)
                    return false;
                return true;
            }
        }
    }
    return false;
}

/** Check if the second vehicle is in the way of first vehicle.
 */
TbBool check_two_vehicles(struct Thing *p_vehA, struct Thing *p_vehB)
{
#if 0
    TbBool ret;
    asm volatile (
      "call ASM_check_two_vehicles\n"
        : "=r" (ret) : "a" (p_vehA), "d" (p_vehB));
    return ret;
#endif
    if (abs(p_vehA->U.UVehicle.AngleDY) > 20)
        return false;

    if (p_vehB->ThingOffset == p_vehA->ThingOffset)
        return false;

    if ((p_vehB->Flag & TngF_Unkn0004) != 0)
        return false;

    return vehicle_check_collide_with_area(p_vehA, p_vehB->X, p_vehB->Y, p_vehB->Z,
      p_vehB->Radius, p_vehB->U.UVehicle.MatrixIndex);
}

TbBool vehicles_check_second_is_blocked(struct Thing *p_vehA, struct Thing *p_vehB)
{
    if (!vehicle_stopped_on_tnode(p_vehB))
        return false;

    // Check if the vehicles are riding on the same TNode, or the second has Agok node set
    if ((p_vehA->U.UVehicle.TNode == p_vehB->U.UVehicle.TNode)
     || ((p_vehB->U.UVehicle.WorkPlace & VWPFlg_HasAgok) != 0))
    {
        // If one of the vehicles already claimed the TNode, it should go first
        struct TrafficNode *p_tnode;
        p_tnode = &game_traffic_nodes[-p_vehB->U.UVehicle.TNode];
        if (p_tnode->GateLink == p_vehA->ThingOffset)
            return true;
    }
    return false;
}

/** Of two competing (potentially colliding) vehicles, check if the first one has right-of-way to move forward.
 */
TbBool vehicle_first_has_right_of_way(struct Thing *p_vehA, struct Thing *p_vehB)
{
    // If no special circumstances, allow the one with higher ThingOffset first
    return (p_vehA->ThingOffset >= p_vehB->ThingOffset);
}

/** Collision check between two vehicles.
 */
static TbBool check_vehicle_col_with_veh(struct Thing *p_vehA, struct Thing *p_vehB,
  int posAx, int posAy, int posAz)
{
    struct M33 *p_matB;
    int radB_incl_speed, matB02, matB22;
    int futureBx, futureBz;
    int posB_beg_x, posB_beg_z;
    int posB_end_x, posB_end_z;
    int dist_begZ, dist_begX;
    int dist_endX, dist_endZ;
    int distXZ_sq;

    if (p_vehB->ThingOffset == p_vehA->ThingOffset)
        return false;

    if ((p_vehB->Flag & TngF_Unkn0004) != 0)
        return false;

    // The following is the same as in vehicle_check_collide_with_area(), but some position computations were moved
    // to upper function as speed optimization (maybe there's no need for that anymore?)
    if (abs(p_vehB->Y - posAy) >= 2048)
        return false;

    assert(p_vehB->U.UVehicle.MatrixIndex < next_local_mat);
    p_matB = &local_mats[p_vehB->U.UVehicle.MatrixIndex];
    matB02 = -4 * p_matB->R[0][2];
    matB22 = -4 * p_matB->R[2][2];
    radB_incl_speed = 384 * p_vehB->Radius >> 9;
    futureBx = radB_incl_speed * matB02 >> 8;
    futureBz = radB_incl_speed * matB22 >> 8;

    posB_beg_x = p_vehB->X - futureBx;
    posB_beg_z = p_vehB->Z - futureBz;
    posB_end_x = p_vehB->X + futureBx;
    posB_end_z = p_vehB->Z + futureBz;

    dist_begX = (posB_beg_x - posAx) >> 8;
    dist_begZ = (posB_beg_z - posAz) >> 8;
    dist_endX = (posB_end_x - posAx) >> 8;
    dist_endZ = (posB_end_z - posAz) >> 8;

    distXZ_sq = dist_endZ * dist_endZ + dist_endX * dist_endX;
    if (distXZ_sq >= dist_begZ * dist_begZ + dist_begX * dist_begX)
        distXZ_sq = dist_begZ * dist_begZ + dist_begX * dist_begX;

    if (distXZ_sq >= radB_incl_speed * radB_incl_speed)
        return false;

    // Check if the current vehicle has right-of-way over the second
    if (vehicle_first_has_right_of_way(p_vehA, p_vehB) && check_two_vehicles(p_vehB, p_vehA))
        return false;

    // Check if the 2nd vehicle is blocked, in which case the first should assume
    // right of way to avoid blockade; this workarounds an issue where on two crossroads
    // close to each other, two vehicles will get blocked on each other. The issue
    // was prominent on `-m 0,21`, where it would happen sooner or later without any
    // actions required from the player. A better solution to such jams may be developed
    // later, when the workings of the traffic system are better understood.
    if (vehicles_check_second_is_blocked(p_vehA, p_vehB))
        return false;

    LOGSYNC("Stopping %s thing %d rqspeed %d due to %s thing %d rqspeed %d",
      vehicle_type_name(p_vehA->SubType), (int)p_vehA->ThingOffset,
      (int)p_vehA->U.UVehicle.ReqdSpeed,
      vehicle_type_name(p_vehB->SubType), (int)p_vehB->ThingOffset,
      (int)p_vehB->U.UVehicle.ReqdSpeed);

    p_vehA->SubState = 3;
    p_vehA->U.UVehicle.ReqdSpeed = 0;
    p_vehA->U.UVehicle.AngleDY = 0;
    return true;
}

/** Simplified vehicle collision check for when the vehicles are on the same tile.
 * We don't have to check for overlap, as every vehicle is larger than a tile.
 */
static TbBool check_vehicle_col_same_mapel_with_veh(struct Thing *p_vehA, struct Thing *p_vehB,
  int pos_x, int pos_y, int pos_z)
{
    if ((p_vehB->Flag & TngF_Unkn0004) != 0)
        return false;

    if (abs(p_vehB->Y - pos_y) >= 2048)
        return false;

    // Check if the current vehicle has right-of-way over the second
    if (vehicle_first_has_right_of_way(p_vehA, p_vehB))
        return false;

    LOGSYNC("Stopping %s thing %d rqspeed %d due to %s thing %d rqspeed %d",
      vehicle_type_name(p_vehA->SubType), (int)p_vehA->ThingOffset,
      (int)p_vehA->U.UVehicle.ReqdSpeed,
      vehicle_type_name(p_vehB->SubType), (int)p_vehB->ThingOffset,
      (int)p_vehB->U.UVehicle.ReqdSpeed);

    p_vehA->SubState = 3;
    p_vehA->U.UVehicle.ReqdSpeed = 0;
    p_vehA->U.UVehicle.AngleDY = 0;
    return false; //TODO is that the best result?
}

static TbBool check_vehicle_col_with_pers(struct Thing *p_vehicle, struct Thing *p_person,
  int pos_x, int pos_y, int pos_z)
{
    int dx, dz, per_r_sq, veh_r_sq;

    if ((p_person->Flag2 & TgF2_KnockedOut) != 0)
        return false;
    if (p_person->State == PerSt_DEAD)
        return false;
    if ((p_person->Flag & TngF_Destroyed) != 0)
        return false;
    if (p_person->State == PerSt_PERSON_BURNING)
        return false;
    if (abs((p_person->Y >> 3) - pos_y) >= 2048)
        return false;

    dx = (p_person->X - pos_x) >> 8;
    dz = (p_person->Z - pos_z) >> 8;
    per_r_sq = p_person->Radius * p_person->Radius;
    veh_r_sq = p_vehicle->Radius * p_vehicle->Radius;

    if ((dx) * (dx) + (dz) * (dz) >= veh_r_sq + per_r_sq)
        return false;

    person_hit_by_car(p_person, p_vehicle);

    return false;
}

static TbBool check_vehicle_col_with_mine(struct Thing *p_vehicle, struct SimpleThing *p_minetng,
  int pos_x, int pos_y, int pos_z)
{
    if (p_minetng->SubType != 12)
        return false;

    if ((p_minetng->Timer1 != 999) && (p_minetng->Timer1 > 0))
        p_minetng->Timer1 = 0;

    return false;
}

static TbBool check_vehicle_col_on_mapel(struct Thing *p_vehicle, struct MyMapElement *p_mapel,
  int pos_x, int pos_y, int pos_z)
{
    ThingIdx thing;

    thing = p_mapel->Child;
    while (thing != 0)
    {
      if (thing > 0)
      {
          struct Thing *p_thing;
          p_thing = &things[thing];
          switch (p_thing->Type)
          {
          case TT_VEHICLE:
              if (check_vehicle_col_with_veh(p_vehicle, p_thing, pos_x, pos_y, pos_z))
                  return true;
              break;
          case TT_PERSON:
              if (check_vehicle_col_with_pers(p_vehicle, p_thing, p_vehicle->X, pos_y, p_vehicle->Z))
                  return true;
              break;
          default:
              break;
          }
          thing = p_thing->Next;
      }
      else
      {
          struct SimpleThing *p_sthing;
          p_sthing = &sthings[thing];
          switch (p_sthing->Type)
          {
          case TT_MINE:
              if (check_vehicle_col_with_mine(p_vehicle, p_sthing, p_vehicle->X, pos_y, p_vehicle->Z))
                  return true;
              break;
          default:
              break;
          }
          thing = p_sthing->Next;
      }
    }
    return false;
}

static TbBool check_vehicle_col_on_same_mapel(struct Thing *p_vehicle, struct MyMapElement *p_mapel,
  int pos_x, int pos_y, int pos_z)
{
    ThingIdx thing;

    thing = p_mapel->Child;
    while (thing != 0)
    {
      if (thing > 0)
      {
          struct Thing *p_thing;
          p_thing = &things[thing];
          switch (p_thing->Type)
          {
          case TT_VEHICLE:
              if (check_vehicle_col_same_mapel_with_veh(p_vehicle, p_thing, pos_x, pos_y, pos_z))
                  return true;
              break;
          default:
              break;
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
    return false;
}

/** Checks vehicle collisions.
 */
TbBool check_vehicle_col(struct Thing *p_vehicle)
{
#if 0
    TbBool ret;
    asm volatile (
      "call ASM_check_vehicle_col\n"
        : "=r" (ret) : "a" (p_vehicle));
    return ret;
#endif
    ushort r;
    struct M33 *p_matA;
    int rad_incl_speed, matA02, matA22;
    int tile_ctr_x, tile_ctr_z;
    int tile_x, tile_z;
    int pos_x, pos_y, pos_z;

    if (abs(p_vehicle->U.UVehicle.AngleDY) > 20)
        return false;

    r = p_vehicle->Radius;
    assert(p_vehicle->U.UVehicle.MatrixIndex < next_local_mat);
    p_matA = &local_mats[p_vehicle->U.UVehicle.MatrixIndex];
    pos_x = p_vehicle->X;
    pos_y = p_vehicle->Y;
    pos_z = p_vehicle->Z;
    rad_incl_speed = (p_vehicle->Speed >> 4) + (p_vehicle->Speed >> 3) + (386 * r >> 8);
    matA02 = -4 * p_matA->R[0][2];
    matA22 = -4 * p_matA->R[2][2];
    pos_x = (matA02 * rad_incl_speed >> 8) + pos_x - (matA22 >> 2);
    pos_z = (matA22 * rad_incl_speed >> 8) + pos_z + (matA02 >> 2);
    tile_ctr_z = pos_z >> 16;
    tile_ctr_x = pos_x >> 16;

    for (tile_x = tile_ctr_x - 2; tile_x <= tile_ctr_x + 2; tile_x++)
    {
        if ((tile_x < 0) || (tile_x >= 128))
            continue;
        for (tile_z = tile_ctr_z - 2; tile_z <= tile_ctr_z + 2; tile_z++)
        {
            if ((tile_z < 0) || (tile_z >= 128))
                continue;
            struct MyMapElement *p_mapel;
            p_mapel = &game_my_big_map[128 * tile_z + tile_x];
            if (check_vehicle_col_on_mapel(p_vehicle, p_mapel, pos_x, pos_y, pos_z))
                return true;
        }
    }

    struct MyMapElement *p_mapel;
    p_mapel = &game_my_big_map[128 * (p_vehicle->Z >> 16) + (p_vehicle->X >> 16)];
    if (check_vehicle_col_on_same_mapel(p_vehicle, p_mapel, pos_x, pos_y, pos_z))
        return true;

    return false;
}

#ifdef SW_FNCHK
static void start_crashing_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_start_crashing\n"
        : : "a" (p_vehicle) : "cc", "memory");
}
#endif

/** A vehicle hit hard starts crashing: a bang, its speed kept as wanted
 * speed halved; a mech explodes instead.  Remade from start_crashing. */
static void start_crashing_c(struct Thing *p_vehicle)
{
    if (p_vehicle->SubType == 0x3B)
    {
        p_vehicle->State = 0x0D;
        p_vehicle->Flag |= 0x0002;
        init_mech_explode(p_vehicle);
        return;
    }
    if ((ushort)p_vehicle->State == 0x45)
        return;
    p_vehicle->Flag |= 0x0002;
    p_vehicle->State = 0x45;
    if (p_vehicle->SubType == 0x1D)
    {
        struct Thing *p_station;

        p_station = &things[p_vehicle->U.UVehicle.TNode];
        if ((p_station->Flag & 0x0002) == 0)
            p_station->Flag |= 0x00040000;
    }
    p_vehicle->VY = 0;
    p_vehicle->U.UVehicle.ReqdSpeed = p_vehicle->Speed >> 1;
    bang_new4(p_vehicle->X, p_vehicle->Y, p_vehicle->Z, 0x5F);
}

void start_crashing(struct Thing *p_vehicle)
{
    FNCHK_VOID("start_crashing", start_crashing_asm(p_vehicle), start_crashing_c(p_vehicle));
}

#ifdef SW_FNCHK
static void init_vehicle_explode_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_init_vehicle_explode\n"
        : : "a" (p_vehicle) : "cc", "memory");
}
#endif

ubyte person_leave_vehicle(struct Thing *p_person, struct Thing *p_vehicle);

/** A vehicle blows up: passengers thrown out, its model broken into
 * flying faces, a crater (a flyer only near the ground), a bang and a
 * shockwave; the vehicle becomes a thing of type 1.
 * Remade from init_vehicle_explode. */
static void init_vehicle_explode_c(struct Thing *p_vehicle)
{
    struct Thing *p_owner;
    TbBool crater;
    ushort i;

    if (p_vehicle->SubType == 0x33)
        stop_sample_using_heap((ushort)p_vehicle->ThingOffset, 0x31);
    remove_locks(p_vehicle);
    p_vehicle->Flag |= 0x0002;
    play_dist_sample(p_vehicle, 0x25, 0x7F, 0x40, 100, 0, 3);
    p_vehicle->State = 0x0D;
    for (i = 0; (p_vehicle->U.UVehicle.PassengerHead != 0) && (i < 100); i++)
    {
        short passenger;

        passenger = p_vehicle->U.UVehicle.PassengerHead;
        person_leave_vehicle(&things[(ushort)passenger], p_vehicle);
        // stop when the passenger did not leave
        if ((s32)(ushort)p_vehicle->U.UVehicle.PassengerHead == (s32)passenger)
            break;
    }
    explode_thing_object(p_vehicle->ThingOffset);
    if (p_vehicle->SubType == SubTT_VEH_FLYING)
    {
        s32 alt;

        alt = alt_at_point((short)(p_vehicle->X >> 8), (short)(p_vehicle->Z >> 8));
        alt += (((ushort)p_vehicle->Radius * 193) * 2) >> 5;
        crater = (alt + 250 > p_vehicle->Y);
    }
    else
    {
        crater = true;
    }
    if (crater)
        quick_crater(p_vehicle->X >> 16, p_vehicle->Z >> 16, 2);
    bang_new4(p_vehicle->X, p_vehicle->Y, p_vehicle->Z, 0x0F);
    delete_node(p_vehicle);
    p_vehicle->Type = 1;
    p_owner = NULL;
    if (p_vehicle->OldTarget != 0)
        p_owner = &things[p_vehicle->OldTarget];
    do_shockwave(p_vehicle->X >> 8, p_vehicle->Y >> 8, p_vehicle->Z >> 8,
      (ushort)p_vehicle->Radius << 2, 5000, p_owner);
}

void init_vehicle_explode(struct Thing *p_vehicle)
{
    FNCHK_VOID("init_vehicle_explode", init_vehicle_explode_asm(p_vehicle),
      init_vehicle_explode_c(p_vehicle));
}

#ifdef SW_FNCHK
static void set_passengers_location_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_set_passengers_location\n"
        : : "a" (p_vehicle) : "cc", "memory");
}
#endif

/** The passengers of a vehicle are where the vehicle is; a selected agent
 * among them marks the vehicle (Flag2 0x2000).  Remade from
 * set_passengers_location (a second, counting-only loop of the original
 * for long passenger lists has no effect and is left out).
 */
static void set_passengers_location_c(struct Thing *p_vehicle)
{
    ThingIdx passngr;
    ushort n;

    p_vehicle->Flag2 &= ~TgF2_Unkn2000;
    passngr = p_vehicle->U.UVehicle.PassengerHead;
    for (n = 0; (passngr != 0) && (n < 410); n++)
    {
        struct Thing *p_passngr;

        p_passngr = &things[(ushort)passngr];
        p_passngr->X = p_vehicle->X;
        if (p_vehicle->SubType == SubTT_VEH_FLYING)
            p_passngr->Y = p_vehicle->Y >> 3;
        else
            p_passngr->Y = p_vehicle->Y;
        p_passngr->Z = p_vehicle->Z;
        passngr = p_passngr->U.UPerson.LinkPassenger;
        if ((p_passngr->Flag & TngF_SelectedAgent) != 0)
            p_vehicle->Flag2 |= TgF2_Unkn2000;
    }
}

void set_passengers_location(struct Thing *p_vehicle)
{
    FNCHK_VOID("set_passengers_location", set_passengers_location_asm(p_vehicle),
      set_passengers_location_c(p_vehicle));
}

void process_tank_turret(struct Thing *p_tank)
{
    struct Thing *p_turret;
    int target_x, target_y;
    int turret;
    short angle;
    int dt_angle;

    turret = p_tank->U.UVehicle.SubThing;
    if (turret == 0) {
        p_tank->OldTarget = LbFPMath_PI+1;
        return;
    }
    p_turret = &things[turret];
    if ((p_tank->Flag & TngF_ShootAtPos) != 0)
    {
        target_x = p_tank->U.UVehicle.TargetDX;
        target_y = p_tank->U.UVehicle.TargetDZ;
    }
    else
    {
        struct Thing *p_target;
        p_target = p_tank->PTarget;
        if (p_target == NULL)
        {
            p_tank->OldTarget = 20000;
            return;
        }
        target_x = PRCCOORD_TO_MAPCOORD(p_target->X);
        target_y = PRCCOORD_TO_MAPCOORD(p_target->Z);
    }
    angle = p_turret->U.UMGun.AngleY
        - angle_between_points(target_x, target_y, PRCCOORD_TO_MAPCOORD(p_tank->X), PRCCOORD_TO_MAPCOORD(p_tank->Z));
    if (angle < -LbFPMath_PI)
        angle += 2*LbFPMath_PI;
    else if (angle > LbFPMath_PI)
        angle -= 2*LbFPMath_PI;

    //TODO CONFIG how fast the tank can target could be a difficulty-related setting
    // Travel 1/8 of the distance in each game turn
    dt_angle = angle / 8;
    if (dt_angle > LbFPMath_PI/17)
        dt_angle = LbFPMath_PI/17;
    if (dt_angle < -LbFPMath_PI/17)
        dt_angle = -LbFPMath_PI/17;
    if (dt_angle == 0)
    {
        if (angle > 0)
            dt_angle = 1;
        if (angle < 0)
            dt_angle = -1;
    }

    // Despite being caused by the turret, we bind the sound samples to the vehicle part
    // of the tank. This is because turrets do not contain full position on map, so the
    // sound update would misplace the sound source if it was bound to the turret
    if ((p_turret->Flag2 & TgF2_Unkn0200) != 0)
    {
        if (dt_angle <= 1) {
            // Play rotation stop sample
            if (!IsSamplePlaying(p_tank->ThingOffset, 47, 0))
                play_dist_sample(p_tank, 47, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 1);
            p_turret->Flag2 &= ~TgF2_Unkn0200;
        }
    }
    else
    {
        // Play rotation sample if moving over 1.2 degree per turn and the angle is not getting smaller.
        // Huge values of OldTarget (beyond pi) indicate that previously we had no target.
        if ((abs(dt_angle) >= LbFPMath_PI/75) && (p_tank->OldTarget < abs(angle) || p_tank->OldTarget > LbFPMath_PI)) {
            if (!IsSamplePlaying(p_tank->ThingOffset, 48, 0))
                play_dist_sample(p_tank, 48, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 1);
            p_turret->Flag2 |= TgF2_Unkn0200;
        }
    }

    p_turret->U.UMGun.AngleY -= dt_angle;
    p_tank->OldTarget = abs(angle);
    p_turret->U.UMGun.AngleY = (p_turret->U.UMGun.AngleY + 2*LbFPMath_PI) & LbFPMath_AngleMask;
}

#ifdef SW_FNCHK
static int process_my_crashing_vehicle_asm(struct Thing *p_vehicle)
{
    int ret;
    asm volatile ("call ASM_process_my_crashing_vehicle\n"
        : "=a" (ret) : "0" (p_vehicle) : "cc", "memory");
    return ret;
}
#endif

/** A crashing vehicle tumbles, burns and falls; it blows up when it hits
 * the ground, leaves the map or reaches a tile next to a building.
 * Returns 0 when it blew up, 42 while it crashes on.
 * Remade from process_my_crashing_vehicle. */
static int process_my_crashing_vehicle_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    struct MyMapElement *p_mapel;
    TbBool hit;
    s32 d;

    p_veh = &p_vehicle->U.UVehicle;
    if ((LbRandomAnyShort() & 0xF) == 0)
    {
        ushort r1, r2;

        p_veh->AngleX = p_veh->AngleX - (LbRandomAnyShort() & 0x7F) + 0x3F;
        if (p_vehicle->SubType != 0x28)
        {
            p_vehicle->VY += 0x400;
            p_vehicle->Y += 0x2000;
            p_veh->AngleY -= (LbRandomAnyShort() & 0x7F);
            p_veh->AngleZ -= (LbRandomAnyShort() & 0x7F);
            p_veh->AngleY += 0x3F;
            p_veh->AngleZ += 0x3F;
        }
        else
        {
            p_veh->AngleY -= (LbRandomAnyShort() & 0x3F);
            p_veh->AngleZ -= (LbRandomAnyShort() & 0x3F);
            p_veh->AngleY += 0x1F;
            p_veh->AngleZ += 0x1F;
        }
        r1 = LbRandomAnyShort();
        r2 = LbRandomAnyShort();
        bang_new4(p_vehicle->X + r2, p_vehicle->Y >> 3, p_vehicle->Z + r1, 100);
    }
    if ((gameturn & 1) != 0)
        bang_new4(p_vehicle->X, p_vehicle->Y >> 3, p_vehicle->Z, 0x35);
    p_veh->AngleX += 5;
    if (p_vehicle->SubType != 0x28) {
        p_vehicle->VY -= 0x110;
        d = p_vehicle->VY >> 7;
    } else {
        p_vehicle->VY -= 0x80;
        d = p_vehicle->VY >> 8;
    }
    p_veh->AngleZ = p_veh->AngleZ - d;
    if (p_veh->AngleX < 0)
        p_veh->AngleX = 0;
    if (p_veh->AngleY < 0)
        p_veh->AngleY = 0;
    if (p_veh->AngleZ < 0)
        p_veh->AngleZ = 0;
    p_veh->AngleX &= 0x7FF;
    p_veh->AngleZ &= 0x7FF;
    p_mapel = &game_my_big_map[(p_vehicle->Z >> 16) * MAP_TILE_WIDTH + (p_vehicle->X >> 16)];
    if ((p_vehicle->SubType == 0x28) && ((gameturn & 1) != 0))
    {
        s32 alt;

        alt = alt_at_point((short)(p_vehicle->X >> 8), (short)(p_vehicle->Z >> 8));
        FIRE_new(p_vehicle->X, alt >> 5, p_vehicle->Z, 5);
    }
    if (p_vehicle->SubType == SubTT_VEH_FLYING)
    {
        s32 alt;

        alt = alt_at_point((short)(p_vehicle->X >> 8), (short)(p_vehicle->Z >> 8));
        alt += (((ushort)p_vehicle->Radius * 193) * 2) >> 5;
        hit = ((alt << 3) > p_vehicle->Y);
    }
    else
    {
        s32 alt;

        alt = alt_at_point((short)(p_vehicle->X >> 8), (short)(p_vehicle->Z >> 8));
        hit = ((alt << 3) + 800 > p_vehicle->Y);
    }
    if (!hit)
    {
        if ((p_vehicle->X < 0x10000) || (p_vehicle->Z < 0x10000)
          || (p_vehicle->X > 0x7E0000) || (p_vehicle->Z > 0x7E0000))
            hit = true;
        else if (((p_mapel - 1)->ColumnHead & 0xFFF) != 0)
            hit = true;
        else if (((p_mapel + 1)->ColumnHead & 0xFFF) != 0)
            hit = true;
        else if (((p_mapel - MAP_TILE_WIDTH)->ColumnHead & 0xFFF) != 0)
            hit = true;
        else if (((p_mapel + MAP_TILE_WIDTH)->ColumnHead & 0xFFF) != 0)
            hit = true;
    }
    if (!hit)
        return 42;
    p_vehicle->Y >>= 3;
    init_vehicle_explode(p_vehicle);
    p_vehicle->Y <<= 3;
    return 0;
}

int process_my_crashing_vehicle(struct Thing *p_vehicle)
{
    int ret;
    FNCHK_RET("process_my_crashing_vehicle", ret, process_my_crashing_vehicle_asm(p_vehicle),
      process_my_crashing_vehicle_c(p_vehicle));
    return ret;
}

#ifdef SW_FNCHK
static void move_vehicle_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_move_vehicle\n"
        : : "a" (p_vehicle) : "cc", "memory");
}
#endif

extern ushort last_swoosh_thing;
extern ulong last_swoosh_turn;

/** Rotation matrix (14-bit fixed point) of the three angles (0..0x7FF).
 * Remade from rotate_obj.
 */
static void rotate_obj(short angle_a, short angle_b, short angle_c, struct M33 *p_mat)
{
    s32 s1, c1, s2, c2, s3, c3;
    s32 a, b, c;

    a = angle_a & 0x7FF;
    b = angle_b & 0x7FF;
    c = angle_c & 0x7FF;
    s1 = lbSinTable[a] >> 1;
    c1 = lbSinTable[a + 512] >> 1;
    s3 = lbSinTable[c] >> 1;
    c3 = lbSinTable[c + 512] >> 1;
    s2 = lbSinTable[b] >> 1;
    c2 = lbSinTable[b + 512] >> 1;

    p_mat->R[0][0] = (c3 * c2) >> 16;
    p_mat->R[0][1] = (-(c2 * s3)) >> 16;
    p_mat->R[0][2] = s2 >> 1;
    p_mat->R[1][0] = (c1 * s3 + ((c3 * s1) >> 15) * s2) >> 16;
    p_mat->R[1][1] = (c3 * c1 - ((s1 * s3) >> 15) * s2) >> 16;
    p_mat->R[1][2] = (-(s1 * c2)) >> 16;
    p_mat->R[2][0] = (s1 * s3 - ((c3 * c1) >> 15) * s2) >> 16;
    p_mat->R[2][1] = (c3 * s1 + ((c1 * s2) >> 15) * s3) >> 16;
    p_mat->R[2][2] = (c1 * c2) >> 16;
}

/** The swoosh of a passing car near the camera.  Remade from swoosh_veh. */
static void swoosh_veh(struct Thing *p_vehicle)
{
    int adx, adz, dist;

    if ((last_swoosh_thing == (ushort)p_vehicle->ThingOffset) &&
      ((ulong)(gameturn - last_swoosh_turn) < 150))
        return;
    if (ingame.TrackThing == p_vehicle->ThingOffset)
        return;
    if ((p_vehicle->SubType != 0x28) && (p_vehicle->SubType != 0x2B) &&
      (p_vehicle->SubType != SubTT_VEH_FLYING))
        return;
    adx = abs((p_vehicle->X >> 8) - engn_xc);
    adz = abs((p_vehicle->Z >> 8) - engn_zc);
    if (adx > adz)
        dist = adx + (adz >> 1);
    else
        dist = adz + (adx >> 1);
    if (dist >= 0x400)
        return;
    if (IsSamplePlaying((ushort)p_vehicle->ThingOffset, 0x11, 0))
        return;
    last_swoosh_thing = p_vehicle->ThingOffset;
    last_swoosh_turn = gameturn;
    play_dist_sample(p_vehicle, 0x11, 0x7F, 0x40, 100, 0, 1);
}

/** A vehicle turns, tilts and moves on (its matrix from the angles, the
 * velocity along its heading).  Remade from move_vehicle.
 */
static void move_vehicle_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    struct M33 *p_mat;
    short dt;
    int speed;

    p_veh = &p_vehicle->U.UVehicle;
    if ((((ushort)p_vehicle->ThingOffset + gameturn) & 3) == 0)
        swoosh_veh(p_vehicle);
    if ((p_vehicle->SubType != SubTT_VEH_TRAIN) && (p_vehicle->SubType != SubTT_VEH_FLYING))
        check_vehicle_col(p_vehicle);

    p_veh->AngleY = (p_veh->AngleY + (p_veh->AngleDY >> 3)) & 0x7FF;
    dt = p_veh->AngleZ - p_veh->AngleDY;
    if (p_vehicle->SubType == SubTT_VEH_TRAIN)
        p_veh->AngleZ = 0;
    else if (dt < -100)
        p_veh->AngleZ += 100;
    else if (dt > 100)
        p_veh->AngleZ -= 100;
    else if (abs(dt) > 50)
        p_veh->AngleZ = p_veh->AngleDY;

    p_mat = &local_mats[p_veh->MatrixIndex];
    rotate_obj(0, -p_veh->AngleY, -(p_veh->AngleZ >> 1), p_mat);
    if (p_veh->AngleX != 0)
        rotate_object_axis(p_mat, 0, p_veh->AngleX, 0);
    speed = p_vehicle->Speed;
    p_vehicle->VX = (speed * -p_mat->R[0][2]) >> 10;
    if (p_vehicle->SubType == SubTT_VEH_TRAIN)
        p_vehicle->VY = (speed * -p_mat->R[1][2]) >> 10;
    else
        p_vehicle->VY = 0;
    p_vehicle->VZ = (speed * -p_mat->R[2][2]) >> 10;
    if (p_veh->AngleX != 0)
        rotate_object_axis(p_mat, 0, p_veh->AngleX, 0);

    if ((ushort)p_vehicle->State == 0x36)
    {
        p_vehicle->VX += p_veh->AccelX << 5;
        p_vehicle->VZ += p_veh->AccelZ << 5;
    }
    else if (((ushort)p_vehicle->State == 0x38) && ((p_veh->WorkPlace & 0x0200) == 0))
    {
        p_vehicle->VX -= p_veh->AccelX << 5;
        p_vehicle->VZ -= p_veh->AccelZ << 5;
    }
    move_mapwho(p_vehicle, p_vehicle->X + p_vehicle->VX, p_vehicle->Y + p_vehicle->VY,
      p_vehicle->Z + p_vehicle->VZ);
}

void move_vehicle(struct Thing *p_vehicle)
{
    FNCHK_VOID("move_vehicle", move_vehicle_asm(p_vehicle), move_vehicle_c(p_vehicle));
}

#ifdef SW_FNCHK
static void process_stop_as_soon_as_you_can_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_stop_as_soon_as_you_can\n"
        : : "a" (p_vehicle) : "cc", "memory");
}
#endif

/** A car told to stop: if it may stop on the segment it is on, it starts
 * stopping (state 0x36) facing along the segment, with a sideways slide
 * towards it.  Remade from process_stop_as_soon_as_you_can. */
static void process_stop_as_soon_as_you_can_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    struct TrafficNode *p_tnode, *p_tn2;
    s32 dx, dz, adx, adz, dist, ax, az;
    int link;
    ushort angle;

    p_veh = &p_vehicle->U.UVehicle;
    link = (veh_get_d88(p_veh) >> 6) & 3;
    if (p_veh->AngleDY >= 20)
        return;
    if (p_veh->TNode >= 0)
        return;
    if (p_veh->Dummy4b >= 0)
        return;
    p_tnode = &game_traffic_nodes[-p_veh->Dummy4b];
    if ((p_tnode->Flags & 0x8000) != 0)
        return;
    if ((p_tnode->UTraffic.Flags[link] & 0x40) != 0)
        return;
    p_tn2 = &game_traffic_nodes[-p_veh->TNode];
    dx = p_tn2->X - (p_vehicle->X >> 8);
    dz = p_tn2->Z - (p_vehicle->Z >> 8);
    angle = (ushort)arctan(dx, dz);
    veh_set_d8c(p_veh, ((ulong)angle << 16) | 0x11);
    adx = abs(dx);
    adz = abs(dz);
    if (adz > adx)
        dist = (adx >> 1) + adz;
    else
        dist = adx + (adz >> 1);
    az = (-dz) << 5;
    ax = dx << 5;
    if (dist != 0) {
        az /= dist;
        ax /= dist;
    }
    veh_set_d90(p_veh, ((ulong)az << 16) | (ulong)(ax & 0xFFFF));
    p_vehicle->State = 0x36;
}

void process_stop_as_soon_as_you_can(struct Thing *p_vehicle)
{
    FNCHK_VOID("process_stop_as_soon_as_you_can", process_stop_as_soon_as_you_can_asm(p_vehicle),
      process_stop_as_soon_as_you_can_c(p_vehicle));
}

#ifdef SW_FNCHK
static void set_vehicle_alt_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_set_vehicle_alt\n"
        : : "a" (p_vehicle) : "cc", "memory");
}
#endif

/** A ground vehicle on a traffic node follows the ground height (with
 * some lag), and tilts with it.  Remade from set_vehicle_alt.
 */
static void set_vehicle_alt_c(struct Thing *p_vehicle)
{
    struct MyMapElement *p_mapel;
    int dy;

    if (p_vehicle->U.UVehicle.TNode >= 0)
        return;
    p_mapel = &game_my_big_map[((p_vehicle->Z >> 16) << 7) + (p_vehicle->X >> 16)];
    if ((p_mapel->Flags2 & 0x08) != 0)
        return;
    if ((p_mapel->ColumnHead & 0x0FFF) != 0)
        return;
    dy = (alt_at_point((short)(p_vehicle->X >> 8), (short)(p_vehicle->Z >> 8)) + 0xF00
      - p_vehicle->Y) >> 3;
    p_vehicle->Y += dy;
    p_vehicle->U.UVehicle.AngleX = (short)((-dy) >> 3);
}

void set_vehicle_alt(struct Thing *p_vehicle)
{
    FNCHK_VOID("set_vehicle_alt", set_vehicle_alt_asm(p_vehicle), set_vehicle_alt_c(p_vehicle));
}

#ifdef SW_FNCHK
static void process_parked_flyer_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_parked_flyer\n"
        : : "a" (p_vehicle) : "cc", "memory");
}
#endif

/** Height steps of a parked flyer hovering, *16; 99 ends the loop, which
 * goes back 26 steps. The asm data table `quiver`, as constants. */
static const sbyte parked_flyer_quiver[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    -1, 0, 1, 0, -1, -3, -1, 0,
    1, 3, 4, 3, 1, 0, -2, -5,
    -7, -8, -7, -5, -2, 0, 2, 5,
    8, 10, 12, 13, 13, 13, 12, 10,
    8, 5, 2, -2, -5, -8, -10, -12,
    -13, -13, -13, -12, -10, -8, -5, -2,
    99,
};

/** A parked flyer hovers 3000 above the ground, bobbing.
 * Remade from process_parked_flyer. */
static void process_parked_flyer_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    ushort step;

    p_veh = &p_vehicle->U.UVehicle;
    step = p_veh->MaxSpeed;
    if (step == 0)
    {
        p_vehicle->Y = alt_at_point((short)(p_vehicle->X >> 8), (short)(p_vehicle->Z >> 8)) + 3000;
        p_veh->MaxSpeed = 1;
        return;
    }
    p_veh->MaxSpeed = step + 1;
    p_vehicle->Y += parked_flyer_quiver[step] << 4;
    if (parked_flyer_quiver[p_veh->MaxSpeed] == 99)
        p_veh->MaxSpeed -= 26;
}

void process_parked_flyer(struct Thing *p_vehicle)
{
    FNCHK_VOID("process_parked_flyer", process_parked_flyer_asm(p_vehicle),
      process_parked_flyer_c(p_vehicle));
}

#ifdef SW_FNCHK
static void move_flying_vehicle_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_move_flying_vehicle\n"
        : : "a" (p_vehicle) : "cc", "memory");
}

static void process_my_flying_vehicle_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_my_flying_vehicle\n"
        : : "a" (p_vehicle) : "cc", "memory");
}

static void process_hovering_vehicle_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_hovering_vehicle\n"
        : : "a" (p_vehicle) : "cc", "memory");
}

static void process_my_takeoff_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_my_takeoff\n"
        : : "a" (p_vehicle) : "cc", "memory");
}
#endif

/** Turns and banks a flyer, applies the wobble, rebuilds its matrix and
 * moves it forward along its heading.  Remade from move_flying_vehicle.
 */
static void move_flying_vehicle_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    struct M33 *p_mat;

    p_veh = &p_vehicle->U.UVehicle;
    if ((((ushort)p_vehicle->ThingOffset + gameturn) & 3) == 0)
        swoosh_veh(p_vehicle);
    p_veh->AngleY = (p_veh->AngleY + (p_veh->AngleDY >> 3)) & 0x7FF;
    if (p_vehicle->State != 0x45)
    {
        short d;

        d = p_veh->AngleZ - p_veh->AngleDY;
        if (d < -100)
            p_veh->AngleZ += 100;
        else if (d > 100)
            p_veh->AngleZ -= 100;
        else if (abs(d) > 50)
            p_veh->AngleZ = p_veh->AngleDY;
    }
    if ((p_veh->WobbleZP != 0) || (p_veh->WobbleZV != 0))
    {
        s32 zp, zv;

        p_veh->AngleZ += (p_veh->WobbleZP >> 16);
        zp = p_veh->WobbleZP + p_veh->WobbleZV;
        p_veh->WobbleZP = zp;
        zv = p_veh->WobbleZV - (zp >> 4);
        p_veh->WobbleZV = zv - (zv >> 6);
    }
    p_mat = &local_mats[p_veh->MatrixIndex];
    rotate_obj(0, -p_veh->AngleY, -(p_veh->AngleZ >> 1), p_mat);
    if (p_veh->AngleX != 0)
        rotate_object_axis(p_mat, 0, p_veh->AngleX, 0);
    p_vehicle->VX = (p_vehicle->Speed * -p_mat->R[0][2]) >> 10;
    p_vehicle->VZ = (p_vehicle->Speed * -p_mat->R[2][2]) >> 10;
    move_mapwho(p_vehicle, p_vehicle->X + p_vehicle->VX, p_vehicle->Y + p_vehicle->VY,
      p_vehicle->Z + p_vehicle->VZ);
}

void move_flying_vehicle(struct Thing *p_vehicle)
{
    FNCHK_VOID("move_flying_vehicle", move_flying_vehicle_asm(p_vehicle),
      move_flying_vehicle_c(p_vehicle));
}

/** Height a flyer aims for: above the ground of its tile (unless it is
 * over a building column or a no-ground tile), raised for every earlier
 * flyer of subtype 0x32 within 2 tiles, so they stack.  Remade from flying_alt.
 */
static s32 flying_alt(struct Thing *p_vehicle)
{
    struct MyMapElement *p_mapel;
    s32 alt;
    int tile_x, tile_z, x, z;

    alt = 0x32000;
    tile_x = p_vehicle->X >> 16;
    tile_z = p_vehicle->Z >> 16;
    if (tile_x < 0 || tile_x >= 128 || tile_z < 0 || tile_z >= 128)
        return alt;
    p_mapel = &game_my_big_map[tile_z * 128 + tile_x];
    if (((p_mapel->ColumnHead & 0xFFF) == 0) && ((p_mapel->Flags2 & 0x08) == 0))
        alt += p_mapel->Alt << 11;
    for (x = tile_x - 2; x != tile_x + 3; x++)
    {
        for (z = tile_z - 2; z != tile_z + 3; z++)
        {
            ThingIdx thing;

            if (x <= 0 || x >= 128 || z <= 0 || z >= 128)
                continue;
            thing = game_my_big_map[z * 128 + x].Child;
            while (thing != 0)
            {
                if (thing > 0)
                {
                    struct Thing *p_thing;

                    p_thing = &things[thing];
                    if ((p_thing->Type == TT_VEHICLE) && (p_thing->SubType == 0x32)
                      && (p_thing < p_vehicle))
                        alt += 0x1A000;
                    thing = p_thing->Next;
                }
                else
                {
                    thing = sthings[thing].Next;
                }
            }
        }
    }
    return alt;
}

/** Turns a flyer towards its goto point: snaps when within 15, otherwise
 * sets the turn rate (with 10 extra).  Shared code of the takeoff and the
 * flight; `angle` is the heading to the goal. */
static void flyer_turn_to(struct TngUVehicle *p_veh, ushort angle)
{
    int d;

    d = angle - p_veh->AngleY;
    if (d > 0x400)
        d -= 0x7FF;
    if (d < -0x400)
        d += 0x7FF;
    if (abs(d) < 15)
    {
        p_veh->AngleDY = 0;
        p_veh->AngleY = angle;
        return;
    }
    p_veh->AngleDY = d;
    if (d > 0)
        p_veh->AngleDY = d + 10;
    if (d < 0)
        p_veh->AngleDY -= 10;
}

/** A flyer waiting in the air: bobs when near its flying height, else
 * moves towards it.  Remade from process_hovering_vehicle. */
static void process_hovering_vehicle_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    s32 alt;

    p_veh = &p_vehicle->U.UVehicle;
    p_veh->ReqdSpeed = 0;
    p_veh->AngleDY = 0;
    alt = flying_alt(p_vehicle);
    if (abs(p_vehicle->Y - alt) < 0x6000)
    {
        ushort step;

        step = p_veh->MaxSpeed;
        p_veh->MaxSpeed = step + 1;
        p_vehicle->Y += parked_flyer_quiver[step] << 7;
    }
    else
    {
        p_vehicle->Y += (alt - p_vehicle->Y) >> 3;
    }
    if (parked_flyer_quiver[p_veh->MaxSpeed] == 99)
        p_veh->MaxSpeed -= 26;
}

void process_hovering_vehicle(struct Thing *p_vehicle)
{
    FNCHK_VOID("process_hovering_vehicle", process_hovering_vehicle_asm(p_vehicle),
      process_hovering_vehicle_c(p_vehicle));
}

/** A flyer climbing to its flying height; above half of it, it turns to
 * the goal.  TNode holds the climb speed.  Near the height it flies on
 * (state 0x42 after state 0x44, else 0x3A).  Remade from process_my_takeoff. */
static void process_my_takeoff_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    s32 alt, climb, y;

    p_veh = &p_vehicle->U.UVehicle;
    alt = flying_alt(p_vehicle);
    if ((alt >> 1) < p_vehicle->Y)
    {
        ushort angle;

        angle = (ushort)arctan(p_veh->GotoX - (p_vehicle->X >> 8),
          p_veh->GotoZ - (p_vehicle->Z >> 8));
        flyer_turn_to(p_veh, angle);
    }
    climb = (alt - p_vehicle->Y) >> 3;
    if (climb < 1000)
        climb = 1000;
    if (climb > 10000)
        climb = 10000;
    if (p_veh->TNode < climb)
        p_veh->TNode += 1000;
    if (p_veh->TNode > climb)
        p_veh->TNode -= 1000;
    y = p_vehicle->Y + p_veh->TNode;
    p_vehicle->Y = y;
    if (alt - y < 4000)
    {
        if (p_vehicle->State == 0x44)
            p_vehicle->State = 0x42;
        else
            p_vehicle->State = 0x3A;
        p_veh->AngleDY = 0;
        return;
    }
    if ((alt - (alt >> 4) < y) && (p_veh->AngleDY < 15))
        p_veh->ReqdSpeed = 1000;
    else
        p_veh->ReqdSpeed = 0;
}

void process_my_takeoff(struct Thing *p_vehicle)
{
    FNCHK_VOID("process_my_takeoff", process_my_takeoff_asm(p_vehicle),
      process_my_takeoff_c(p_vehicle));
}

/** Sets the goto point of a flyer to the current point of its path. */
static void flyer_goto_path_point(struct Thing *p_vehicle)
{
    struct MyPath *p_path;
    ubyte offs;

    p_path = &my_paths[p_vehicle->U.UVehicle.PathIndex];
    offs = p_vehicle->PathOffset & 3;
    p_vehicle->U.UVehicle.GotoX = p_path->X[offs];
    p_vehicle->U.UVehicle.GotoZ = p_path->Z[offs];
}

/** A flyer with nowhere to go picks a random point on a map edge and tries
 * to route there (up to 32 tries).  Returns 1 when it flies off (state 0x42),
 * 0 when it gives up (state 0x41).  Remade from continue_flying_wander. */
static int continue_flying_wander(struct Thing *p_vehicle)
{
    int n;

    for (n = 0; n < 32; n++)
    {
        ushort rnd;
        int hi, lo, a, b, c, d;
        s32 old_x, old_z, new_x, new_z;

        rnd = LbRandomAnyShort();
        hi = (rnd & 0x7F00) >> 8;
        lo = rnd & 0x7F;
        if (rnd & 0x80)
        {
            if (rnd & 0x8000) {
                a = 3; b = lo; d = 0x7D;
            } else {
                a = 0x7D; b = lo; d = 3;
            }
            c = hi;
        }
        else
        {
            if (rnd & 0x8000) {
                b = 3; c = 0x7D;
            } else {
                b = 0x7D; c = 3;
            }
            a = lo; d = hi;
        }
        /* The tile of the start becomes x a/z b, the route goes to d/c */
        new_x = a << 16;
        new_z = b << 16;
        old_x = p_vehicle->X;
        old_z = p_vehicle->Z;
        p_vehicle->X = new_x;
        p_vehicle->Z = new_z;
        if (ariadne_initialise_route(p_vehicle, d << 8, c << 8) <= 0)
        {
            p_vehicle->X = old_x;
            p_vehicle->Z = old_z;
            continue;
        }
        unkn_path_func_001(p_vehicle, 1);
        p_vehicle->X = old_x;
        p_vehicle->Z = old_z;
        move_mapwho(p_vehicle, new_x, p_vehicle->Y, new_z);
        flyer_goto_path_point(p_vehicle);
        p_vehicle->State = 0x42;
        p_vehicle->U.UVehicle.TNode = 0;
        p_vehicle->U.UVehicle.AngleY = arctan(p_vehicle->U.UVehicle.GotoX - (p_vehicle->X >> 8),
          p_vehicle->U.UVehicle.GotoZ - (p_vehicle->Z >> 8));
        return 1;
    }
    p_vehicle->State = 0x41;
    return 0;
}

/** A flyer following its path at flying height.  At the end of the path
 * a flyer in state 0x42 heads for the nearest map edge and wanders
 * (state 0x43); others hover (state 0x41).
 * Remade from process_my_flying_vehicle. */
static void process_my_flying_vehicle_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    s32 alt, dx, dz;

    p_veh = &p_vehicle->U.UVehicle;
    unkn_path_func_001(p_vehicle, 1);
    alt = flying_alt(p_vehicle);
    if (alt > p_vehicle->Y)
        p_vehicle->Y += (alt - p_vehicle->Y) >> 2;
    else
        p_vehicle->Y += (alt - p_vehicle->Y) >> 4;
    dx = p_veh->GotoX - (p_vehicle->X >> 8);
    dz = p_veh->GotoZ - (p_vehicle->Z >> 8);
    flyer_turn_to(p_veh, (ushort)arctan(dx, dz));

    if ((s32)((u32)dx * (u32)dx + (u32)dz * (u32)dz) < 0x40000)
    {
        if (p_vehicle->State == 0x43)
        {
            continue_flying_wander(p_vehicle);
        }
        else if (do_next_navigate_index(p_vehicle))
        {
            if (p_vehicle->State == 0x42)
            {
                s32 x, z, ex, ez, mx, mz, m;

                x = p_vehicle->X;
                z = p_vehicle->Z;
                ex = 0x800000 - x;
                ez = 0x800000 - z;
                mx = (x < ex) ? x : ex;
                mz = (z < ez) ? z : ez;
                m = (mx < mz) ? mx : mz;
                if (x == m)
                    p_veh->GotoX = 1;
                else if (ex == m)
                    p_veh->GotoX = 0x7E00;
                else if (z == m)
                    p_veh->GotoZ = 1;
                else
                    p_veh->GotoZ = 0x7E00;
                p_vehicle->State = 0x43;
            }
            else
            {
                p_veh->PathIndex = 0;
                p_vehicle->State = 0x41;
                p_veh->TNode = 0;
                p_veh->ReqdSpeed = 0;
                p_veh->AngleDY = 0;
                p_veh->MaxSpeed = 0;
            }
        }
        else
        {
            flyer_goto_path_point(p_vehicle);
        }
    }
    p_veh->ReqdSpeed = 2000 - abs(p_veh->AngleDY << 2);
}

void process_my_flying_vehicle(struct Thing *p_vehicle)
{
    FNCHK_VOID("process_my_flying_vehicle", process_my_flying_vehicle_asm(p_vehicle),
      process_my_flying_vehicle_c(p_vehicle));
}

#ifdef SW_FNCHK
static void start_landing_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_start_landing\n"
        : : "a" (p_vehicle) : "cc", "memory");
}

static void process_my_land_vehicle_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_my_land_vehicle\n"
        : : "a" (p_vehicle) : "cc", "memory");
}

static void process_vehicle_goto_point_fly_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_vehicle_goto_point_fly\n"
        : : "a" (p_vehicle) : "cc", "memory");
}
#endif

/** A flyer told to land starts landing (state 0x3B) if the place below
 * is good for it.  Remade from start_landing. */
static void start_landing_c(struct Thing *p_vehicle)
{
    if (p_vehicle->State == 0x3B)
        return;
    if (!is_this_a_nice_place_for_a_picnic(p_vehicle->X, p_vehicle->Z))
        return;
    p_vehicle->U.UVehicle.PathIndex = 0;
    p_vehicle->State = 0x3B;
    p_vehicle->U.UVehicle.TNode = 0;
}

void start_landing(struct Thing *p_vehicle)
{
    FNCHK_VOID("start_landing", start_landing_asm(p_vehicle), start_landing_c(p_vehicle));
}

/** A landing flyer sinks towards the ground (TNode holds the sink speed),
 * stops turning, and parks (state 0x21) when it touches down.
 * Remade from process_my_land_vehicle. */
static void process_my_land_vehicle_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    s32 ground, sink;

    p_veh = &p_vehicle->U.UVehicle;
    p_veh->ReqdSpeed = 0;
    ground = alt_at_point((short)(p_vehicle->X >> 8), (short)(p_vehicle->Z >> 8)) * 8 + 0x5DC0;
    sink = (ground - p_vehicle->Y) >> 3;
    if (sink > -500)
        sink = -500;
    if (sink < -10000)
        sink = -10000;
    if (p_veh->TNode < sink)
        p_veh->TNode += 1500;
    if (p_veh->TNode > sink)
        p_veh->TNode -= 1500;
    p_vehicle->Y += p_veh->TNode;
    p_veh->AngleDY = p_veh->AngleDY - (p_veh->AngleDY >> 3);
    if (abs(p_veh->AngleDY) < 8)
        p_veh->AngleDY = 0;
    if (ground > p_vehicle->Y)
    {
        p_vehicle->State = 0x21;
        p_veh->MaxSpeed = 0;
    }
}

void process_my_land_vehicle(struct Thing *p_vehicle)
{
    FNCHK_VOID("process_my_land_vehicle", process_my_land_vehicle_asm(p_vehicle),
      process_my_land_vehicle_c(p_vehicle));
}

/** Looks 5 steps ahead along the flyer's velocity for the highest ground
 * and pitches the flyer (AngleX, limited to +-0x8C) to clear it.
 * Returns the highest ground, 0 when the flyer does not move.
 * Remade from scan_max_height_ahead. */
static s32 scan_max_height_ahead(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    s32 len, step_x, step_z, pos_x, pos_z, max_alt, dy;
    ushort i, best;
    short angle;

    p_veh = &p_vehicle->U.UVehicle;
    max_alt = -100000;
    len = LbSqrL((u32)p_vehicle->VX * (u32)p_vehicle->VX + (u32)p_vehicle->VZ * (u32)p_vehicle->VZ);
    if (len == 0)
        return 0;
    step_x = (p_vehicle->VX << 8) / len;
    step_z = (p_vehicle->VZ << 8) / len;
    pos_x = p_vehicle->X >> 8;
    pos_z = p_vehicle->Z >> 8;
    best = 0;
    for (i = 0; i < 5; i++)
    {
        s32 alt;

        alt = alt_at_point((short)pos_x, (short)pos_z);
        if (alt >= max_alt)
        {
            max_alt = alt;
            best = i;
        }
        pos_x += step_x;
        pos_z += step_z;
    }
    dy = (max_alt * 8 + 0x19000 - p_vehicle->Y) >> 3;
    angle = arctan((s32)((u32)dy * (u32)dy), ((s32)best * best) << 16) - 0x400;
    if (angle > 0xA0)
        angle = 0xA0;
    else if (angle < -0xA0)
        angle = -0xA0;
    if (dy < 0)
        p_veh->AngleX = p_veh->AngleX - (angle >> 1);
    else
        p_veh->AngleX = p_veh->AngleX + (angle >> 1);
    if (p_veh->AngleX > 0x8C)
        p_veh->AngleX = 0x8C;
    else if (p_veh->AngleX < -0x8C)
        p_veh->AngleX = -0x8C;
    return max_alt;
}

/** A flyer flying to a point: keeps clear of the ground ahead, carries
 * its passengers, slows down near the point and turns towards it.
 * Remade from process_vehicle_goto_point_fly. */
static void process_vehicle_goto_point_fly_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    s32 dx, dz, dist2;
    short diff;

    p_veh = &p_vehicle->U.UVehicle;
    scan_max_height_ahead(p_vehicle);
    set_passengers_location(p_vehicle);
    dx = p_veh->GotoX - (p_vehicle->X >> 8);
    dz = p_veh->GotoZ - (p_vehicle->Z >> 8);
    dist2 = (s32)((u32)dx * (u32)dx + (u32)dz * (u32)dz);
    if (dist2 < 0x8000)
        p_vehicle->SubState = 7;
    diff = p_veh->AngleY - arctan(dx, dz);
    if (diff < -0x400)
        diff += 0x800;
    else if (diff > 0x400)
        diff -= 0x800;
    if (dist2 > 0x400000)
        p_veh->ReqdSpeed = 0x5E8;
    else if (dist2 > 0x100000)
        p_veh->ReqdSpeed = 0x400;
    else
        p_veh->ReqdSpeed = 0x200;
    if (diff < 0)
    {
        if (diff > -50)
            p_veh->AngleDY = -diff * 2;
        else
            p_veh->AngleDY = 0x200;
    }
    else if (diff > 0)
    {
        if (diff < 50)
            p_veh->AngleDY = -(diff * 2);
        else
            p_veh->AngleDY = -0x200;
    }
}

void process_vehicle_goto_point_fly(struct Thing *p_vehicle)
{
    FNCHK_VOID("process_vehicle_goto_point_fly", process_vehicle_goto_point_fly_asm(p_vehicle),
      process_vehicle_goto_point_fly_c(p_vehicle));
}

#ifdef SW_FNCHK
static void process_vehicle_goto_point_land_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_vehicle_goto_point_land\n"
        : : "a" (p_vehicle) : "cc", "memory");
}
#endif

/** A flyer coming down on a point: carries its passengers, sinks until
 * it reaches the ground (state 0x21) and levels its pitch.
 * Remade from process_vehicle_goto_point_land. */
static void process_vehicle_goto_point_land_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    s32 ground;

    p_veh = &p_vehicle->U.UVehicle;
    set_passengers_location(p_vehicle);
    ground = alt_at_point((short)(p_vehicle->X >> 8), (short)(p_vehicle->Z >> 8)) << 3;
    if (ground < p_vehicle->Y)
    {
        p_vehicle->Speed = 0;
        p_vehicle->Y -= 0x800;
    }
    else
    {
        p_vehicle->State = 0x21;
    }
    if (p_veh->AngleX > 20)
        p_veh->AngleX -= 20;
    else if (p_veh->AngleX < -20)
        p_veh->AngleX += 20;
}

void process_vehicle_goto_point_land(struct Thing *p_vehicle)
{
    FNCHK_VOID("process_vehicle_goto_point_land", process_vehicle_goto_point_land_asm(p_vehicle),
      process_vehicle_goto_point_land_c(p_vehicle));
}

#ifdef SW_FNCHK
static void process_tank_stationary_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_tank_stationary\n"
        : : "a" (p_vehicle) : "cc", "memory");
}
#endif

/** True for statics a tank crushes (and sets on fire) when running over them.
 * Remade from can_the_tank_run_over. */
TbBool can_the_tank_run_over(struct SimpleThing *p_sthing)
{
    ushort frame;

    frame = p_sthing->StartFrame;
    if ((frame > 0x400) && (frame < 0x405))
        return true;
    if ((frame > 0x3EC) && (frame < 0x3F0))
        return true;
    switch (frame)
    {
    case 0x3E7: case 0x3E9: case 0x3EA: case 0x3EC: case 0x3F0:
    case 0x406: case 0x408: case 0x40A: case 0x40B: case 0x40D:
    case 0x40E: case 0x417: case 0x419: case 0x41A:
        return true;
    }
    return false;
}

/** A parked or landed car (subtype 0x28) run over by a tank is squashed:
 * its model points and its height shrink by 1/16.
 * Remade from flatten_vehicle. */
static void flatten_vehicle(struct Thing *p_vehicle)
{
    struct SingleObject *p_obj;
    ushort pt;

    if (p_vehicle->SubType != 0x28)
        return;
    if ((p_vehicle->State != 0x21) && (p_vehicle->State != 0x3D))
        return;
    p_obj = &game_objects[p_vehicle->U.UVehicle.Object];
    for (pt = p_obj->StartPoint; pt <= p_obj->EndPoint; pt++)
    {
        struct SinglePoint *p_pt;

        p_pt = &game_object_points[pt];
        p_pt->Y = p_pt->Y - (p_pt->Y >> 4);
    }
    if (p_vehicle->Y > 0)
        p_vehicle->Y -= p_vehicle->Y >> 4;
}

/** Distance estimate used by the tank: the larger axis plus half the smaller. */
static s32 tank_dist(s32 dx, s32 dz)
{
    dx = abs(dx);
    dz = abs(dz);
    if (dx > dz)
        dz >>= 1;
    else
        dx >>= 1;
    return dx + dz;
}

/** A tank rams the vehicles near its front and back: both are damaged
 * (another tank, subtype 0x33, just stops it), crushed cars are flattened,
 * and statics it can run over catch fire.  Remade from check_tank_collision. */
static void check_tank_collision(struct Thing *p_tank)
{
    struct M33 *p_mat;
    s32 ax, az, front_x, front_z, back_x, back_z;
    int tile_x, tile_z, x, z;

    p_mat = &local_mats[p_tank->U.UVehicle.MatrixIndex];
    ax = (-p_mat->R[0][2]) << 2;
    az = (-p_mat->R[2][2]) << 2;
    front_x = p_tank->X + ax + (ax >> 4);
    back_x = p_tank->X - ax + (ax >> 4);
    front_z = p_tank->Z + az + (az >> 4);
    back_z = p_tank->Z - az + (az >> 4);
    tile_x = p_tank->X >> 16;
    tile_z = p_tank->Z >> 16;
    for (x = tile_x - 3; x != tile_x + 4; x++)
    {
        if (x < 0 || x > 127)
            continue;
        for (z = tile_z - 3; z != tile_z + 4; z++)
        {
            ThingIdx thing;

            if (z < 0 || z > 127)
                continue;
            thing = game_my_big_map[z * 128 + x].Child;
            while (thing != 0)
            {
                if (thing > 0)
                {
                    struct Thing *p_thing;
                    s32 d_front, d_back, range;

                    p_thing = &things[thing];
                    thing = p_thing->Next;
                    if ((p_thing->Type != TT_VEHICLE) || (p_thing == p_tank))
                        continue;
                    if (abs(p_tank->Y - p_thing->Y) >= 0x1000)
                        continue;
                    d_front = tank_dist(p_thing->X - front_x, p_thing->Z - front_z);
                    d_back = tank_dist(p_thing->X - back_x, p_thing->Z - back_z);
                    range = ((ushort)p_thing->Radius << 8) + 0x13800;
                    if ((d_front >= range) && (d_back >= range))
                        continue;
                    if (p_thing->SubType == 0x33)
                    {
                        struct Thing *p_pasngr;

                        p_pasngr = &things[(ushort)p_tank->U.UVehicle.PassengerHead];
                        if ((p_pasngr->Flag & 0x2000) != 0)
                            p_tank->U.UVehicle.ReqdSpeed = 0;
                        else if (p_tank < p_thing)
                            p_tank->U.UVehicle.ReqdSpeed = 0;
                        continue;
                    }
                    p_thing->Health = p_thing->Health - (p_thing->Health >> 2) - 16;
                    p_thing->U.UVehicle.RecoilTimer = 5;
                    if (p_thing->Health <= 0)
                    {
                        init_vehicle_explode(p_thing);
                        p_thing->Flag |= 0x0002;
                    }
                    p_tank->Health--;
                    p_tank->U.UVehicle.RecoilTimer = 5;
                    if (p_tank->Health <= 0)
                    {
                        start_crashing(p_tank);
                        p_tank->Flag |= 0x0002;
                    }
                    flatten_vehicle(p_thing);
                }
                else
                {
                    struct SimpleThing *p_sthing;
                    s32 d_front, d_back, adx, adz, range;

                    p_sthing = &sthings[thing];
                    thing = p_sthing->Next;
                    if ((p_sthing->Flag & 0x0200) != 0)
                        continue;
                    d_front = tank_dist(p_sthing->X - front_x, p_sthing->Z - front_z);
                    adx = abs(p_sthing->X - back_x);
                    adz = abs(p_sthing->Z - back_z);
                    d_back = tank_dist(adx, adz);
                    range = ((ushort)p_sthing->Radius << 8) + 0x13800;
                    if ((range <= d_front) && (d_back >= range))
                        continue;
                    if ((adx == 2) || (adz == 2))
                        continue;
                    if (can_the_tank_run_over(p_sthing))
                        set_static_on_fire(p_sthing);
                }
            }
        }
    }
}

/** Every 8 turns: starts the engine loop of a manned tank, stops it
 * when the tank is empty. */
static void tank_engine_sound(struct Thing *p_vehicle)
{
    if ((gameturn & 7) != 0)
        return;
    if (!IsSamplePlaying((ushort)p_vehicle->ThingOffset, 0x31, 0))
    {
        if (p_vehicle->U.UVehicle.PassengerHead != 0)
            play_dist_sample(p_vehicle, 0x31, 0x7F, 0x40, 100, -1, 1);
    }
    else if (p_vehicle->U.UVehicle.PassengerHead == 0)
    {
        stop_sample_using_heap((ushort)p_vehicle->ThingOffset, 0x31);
    }
}

/** A tank standing still: turns its turret, plays its engine sound while
 * manned, rams what is near, and blows up on bad ground.
 * Remade from process_tank_stationary. */
static void process_tank_stationary_c(struct Thing *p_vehicle)
{
    struct Thing *p_turret;
    struct MyMapElement *p_mapel;

    p_turret = &things[p_vehicle->U.UVehicle.SubThing];
    rotate_obj(0, p_turret->U.UVehicle.AngleY, 0, &local_mats[p_turret->U.UVehicle.MatrixIndex]);
    tank_engine_sound(p_vehicle);
    check_tank_collision(p_vehicle);
    p_mapel = &game_my_big_map[(p_vehicle->Z >> 16) * 128 + (p_vehicle->X >> 16)];
    if (((p_mapel->Flags2 & 0x05) != 0) && ((p_mapel->Flags2 & 0x08) == 0))
        init_vehicle_explode(p_vehicle);
}

void process_tank_stationary(struct Thing *p_vehicle)
{
    FNCHK_VOID("process_tank_stationary", process_tank_stationary_asm(p_vehicle),
      process_tank_stationary_c(p_vehicle));
}

#ifdef SW_FNCHK
static void process_tank_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_tank\n"
        : : "a" (p_vehicle) : "cc", "memory");
}
#endif

/** True when tile flags make a tank blow up (water-like, not bridged). */
static TbBool tank_bad_tile(s32 x, s32 z)
{
    ubyte flags;

    flags = game_my_big_map[(z >> 16) * 128 + (x >> 16)].Flags2;
    return ((flags & 0x05) != 0) && ((flags & 0x08) == 0);
}

/** A tank or mech eases to 0xDAC above the ground, unless the tile has
 * no ground. */
static void walker_follow_ground(struct Thing *p_vehicle)
{
    s32 ground;

    if ((game_my_big_map[(p_vehicle->Z >> 16) * 128 + (p_vehicle->X >> 16)].Flags2 & 0x08) != 0)
        return;
    ground = alt_at_point((short)(p_vehicle->X >> 8), (short)(p_vehicle->Z >> 8)) + 0xDAC;
    if (ground > p_vehicle->Y)
        p_vehicle->Y += ((ground - p_vehicle->Y) >> 4) + 0x40;
    if (ground < p_vehicle->Y)
        p_vehicle->Y -= ((p_vehicle->Y - ground) >> 4) + 0x40;
}

/** A driving tank: follows the ground, turns towards its path point
 * (slower when turning), stops at the end of the path (state 0x33, then
 * parked 0x21), rams what is near and drives on unless the next tile is bad.
 * Remade from process_tank. */
static void process_tank_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    struct Thing *p_turret;
    struct M33 *p_mat;
    s32 dx, dz, dist2, nx, ny, nz;
    int d;

    p_veh = &p_vehicle->U.UVehicle;
    tank_engine_sound(p_vehicle);
    walker_follow_ground(p_vehicle);
    dx = p_veh->GotoX - (p_vehicle->X >> 8);
    dz = p_veh->GotoZ - (p_vehicle->Z >> 8);
    d = (ushort)arctan(dx, dz) - p_veh->AngleY;
    if (d > 0x400)
        d -= 0x7FF;
    if (d < -0x400)
        d += 0x7FF;
    p_veh->AngleDY = d >> 1;
    if (p_veh->AngleDY > 300)
        p_veh->AngleDY = 300;
    else if (p_veh->AngleDY < -300)
        p_veh->AngleDY = -300;
    dist2 = (s32)((u32)dx * (u32)dx + (u32)dz * (u32)dz);
    p_veh->ReqdSpeed = 500 - 2 * abs(p_veh->AngleDY) - abs(p_veh->AngleDY);
    if (p_vehicle->State == 0x33)
    {
        p_veh->ReqdSpeed = 0;
        if (p_veh->AngleDY > 0)
            p_veh->AngleDY -= 8;
        if (p_veh->AngleDY < 0)
            p_veh->AngleDY += 8;
        if (p_veh->AngleDY <= 8)
            p_veh->AngleDY = 0;
        if (p_vehicle->Speed == 0)
            p_vehicle->State = 0x21;
    }
    else if (dist2 < 0x30000)
    {
        if (do_next_navigate_index(p_vehicle))
        {
            p_veh->PathIndex = 0;
            p_vehicle->State = 0x33;
        }
        else
        {
            flyer_goto_path_point(p_vehicle);
        }
    }
    check_tank_collision(p_vehicle);
    if (p_vehicle->Speed < p_veh->ReqdSpeed)
        p_vehicle->Speed += 12;
    if (p_vehicle->Speed > p_veh->ReqdSpeed)
        p_vehicle->Speed -= 0x26;
    p_veh->AngleY = (p_veh->AngleY + (p_veh->AngleDY >> 3)) & 0x7FF;
    p_mat = &local_mats[p_veh->MatrixIndex];
    p_turret = &things[p_veh->SubThing];
    rotate_obj(0, p_turret->U.UVehicle.AngleY, 0, &local_mats[p_turret->U.UVehicle.MatrixIndex]);
    rotate_obj(0, -p_veh->AngleY, 0, p_mat);
    p_vehicle->VX = (-p_mat->R[0][2] * p_vehicle->Speed) >> 10;
    p_vehicle->VY = (-p_mat->R[1][2] * p_vehicle->Speed) >> 10;
    p_vehicle->VZ = (-p_mat->R[2][2] * p_vehicle->Speed) >> 10;
    nx = p_vehicle->X + p_vehicle->VX;
    ny = p_vehicle->Y + p_vehicle->VY;
    nz = p_vehicle->Z + p_vehicle->VZ;
    if (tank_bad_tile(p_vehicle->X, p_vehicle->Z))
    {
        init_vehicle_explode(p_vehicle);
        return;
    }
    if (tank_bad_tile(nx, nz))
        return;
    move_mapwho(p_vehicle, nx, ny, nz);
}

void process_tank(struct Thing *p_vehicle)
{
    FNCHK_VOID("process_tank", process_tank_asm(p_vehicle), process_tank_c(p_vehicle));
}

#ifdef SW_FNCHK
static void process_shuttle_pod_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_shuttle_pod\n"
        : : "a" (p_vehicle) : "cc", "memory");
}
#endif

/** Sets the draw mode of all faces of a model; mode 5 also sets fixed
 * shades 0x80/0x3F on its quads, others 0x20.  Remade from set_object_flag. */
static void set_object_flag(ushort object, ushort flag)
{
    struct SingleObject *p_obj;
    ushort face, end;

    p_obj = &game_objects[object];
    end = p_obj->StartFace + p_obj->NumbFaces;
    for (face = p_obj->StartFace; face < end; face++)
        game_object_faces3[face].Flags = flag;
    end = p_obj->StartFace4 + p_obj->NumbFaces4;
    for (face = p_obj->StartFace4; face < end; face++)
    {
        struct SingleObjectFace4 *p_face4;

        p_face4 = &game_object_faces4[face];
        p_face4->Flags = flag;
        if (flag == 5)
        {
            p_face4->Shade0 = 0x80;
            p_face4->Shade1 = 0x80;
            p_face4->Shade2 = 0x3F;
            p_face4->Shade3 = 0x3F;
        }
        else
        {
            p_face4->Shade0 = 0x20;
            p_face4->Shade1 = 0x20;
            p_face4->Shade2 = 0x20;
            p_face4->Shade3 = 0x20;
        }
    }
}

/** Lets out the next passenger of an arrived pod at its exit tile.
 * Returns 1 when the pod is empty.  Remade from exit_pod. */
static int exit_pod(struct Thing *p_pod)
{
    struct Thing *p_person;
    short pasngr;

    pasngr = p_pod->U.UVehicle.PassengerHead;
    if (pasngr == 0)
    {
        stop_sample_using_heap((ushort)p_pod->ThingOffset, 9);
        return 1;
    }
    if (pasngr < 0)
        return 1;
    p_person = &things[pasngr];
    p_person->X = (p_pod->VX << 16) + 0x8000;
    p_person->Z = (p_pod->VZ << 16) + 0x8000;
    p_person->Y = alt_at_point((short)(p_person->X >> 8), (short)(p_person->Z >> 8));
    p_person->Flag = (p_person->Flag & 0xFFFFBFEB) | 0x14;
    p_pod->U.UVehicle.PassengerHead = p_person->Next;
    add_node_thing((ushort)p_person->ThingOffset);
    return 0;
}

/** A shuttle pod rides with its carrier (TNode, lit while it rides) until
 * the carrier ends its path; then TNode is negated and the pod unloads,
 * and when empty it is removed.  Remade from process_shuttle_pod. */
static void process_shuttle_pod_c(struct Thing *p_vehicle)
{
    struct Thing *p_carrier;
    short tnode;

    tnode = p_vehicle->U.UVehicle.TNode;
    if (tnode < 0)
    {
        if (exit_pod(p_vehicle) != 0)
        {
            set_object_flag(things[(ushort)-tnode].U.UVehicle.Object, 0x15);
            remove_thing(p_vehicle->ThingOffset);
        }
        return;
    }
    p_carrier = &things[tnode];
    set_object_flag(things[(ushort)tnode].U.UVehicle.Object, 5);
    if (p_carrier->U.UVehicle.UniqueID != 0)
        set_object_flag(things[p_carrier->U.UVehicle.UniqueID].U.UVehicle.Object, 0x15);
    p_vehicle->X = p_carrier->X;
    p_vehicle->Y = p_carrier->Y;
    p_vehicle->Z = p_carrier->Z;
    if (p_carrier->U.UVehicle.PathIndex == 0)
    {
        p_vehicle->U.UVehicle.TNode = -p_vehicle->U.UVehicle.TNode;
        p_vehicle->VX = p_carrier->VX;
        p_vehicle->VZ = p_carrier->VZ;
        return;
    }
    p_vehicle->U.UVehicle.TNode = p_carrier->U.UVehicle.PathIndex;
}

void process_shuttle_pod(struct Thing *p_vehicle)
{
    FNCHK_VOID("process_shuttle_pod", process_shuttle_pod_asm(p_vehicle),
      process_shuttle_pod_c(p_vehicle));
}

#ifdef SW_FNCHK
static void process_mech_stationary_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_mech_stationary\n"
        : : "a" (p_vehicle) : "cc", "memory");
}
#endif

/** Turn rate of a mech towards a point: the angle difference (kept also
 * in OldTarget as its absolute value), wrapped and limited to +-0x186. */
static void mech_turn_to(struct Thing *p_vehicle, s32 x, s32 z)
{
    int d;

    d = (ushort)arctan(x - (p_vehicle->X >> 8), z - (p_vehicle->Z >> 8))
      - p_vehicle->U.UVehicle.AngleY;
    p_vehicle->OldTarget = abs(d);
    if (d > 0x400)
        d -= 0x7FF;
    if (d < -0x400)
        d += 0x7FF;
    if (d > 0x186)
        d = 0x186;
    if (d < -0x186)
        d = -0x186;
    p_vehicle->U.UVehicle.AngleDY = d;
}

/** A standing mech turns towards its target thing or target point, else
 * its turning slows down.  Remade from process_mech_stationary. */
static void process_mech_stationary_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;

    p_veh = &p_vehicle->U.UVehicle;
    if (p_vehicle->PTarget != NULL)
    {
        mech_turn_to(p_vehicle, p_vehicle->PTarget->X >> 8, p_vehicle->PTarget->Z >> 8);
    }
    else if ((p_vehicle->Flag & 0x20000000) != 0)
    {
        mech_turn_to(p_vehicle, p_veh->TargetDX, p_veh->TargetDZ);
    }
    else
    {
        p_veh->AngleDY = p_veh->AngleDY - (p_veh->AngleDY >> 4);
        if (abs(p_veh->AngleDY) < 16)
            p_veh->AngleDY = 0;
    }
    p_veh->AngleY = (p_veh->AngleY + (p_veh->AngleDY >> 2)) & 0x7FF;
    rotate_obj(0, -p_veh->AngleY, 0, &local_mats[p_veh->MatrixIndex]);
}

void process_mech_stationary(struct Thing *p_vehicle)
{
    FNCHK_VOID("process_mech_stationary", process_mech_stationary_asm(p_vehicle),
      process_mech_stationary_c(p_vehicle));
}

#ifdef SW_FNCHK
static void process_mech_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_mech\n"
        : : "a" (p_vehicle) : "cc", "memory");
}
#endif

/** A walking mech: stomps (sound), follows the ground, walks along its
 * path; at the end (state 0x33) it turns to its target and stops, then
 * stands (state 0x21).  Remade from process_mech. */
static void process_mech_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    struct M33 *p_mat;
    s32 cx, cz, tx, tz, dx, dz, dist2;
    int d;

    p_veh = &p_vehicle->U.UVehicle;
    if ((((ushort)p_vehicle->ThingOffset + gameturn) & 0x7F) == 0)
        play_dist_sample(p_vehicle, 0x63, 0x7F, 0x40, 100, 0, 3);
    walker_follow_ground(p_vehicle);
    cx = p_vehicle->X >> 8;
    cz = p_vehicle->Z >> 8;
    if ((p_vehicle->State == 0x33) &&
      ((p_vehicle->PTarget != NULL) || ((p_vehicle->Flag & 0x20000000) != 0)))
    {
        if (p_vehicle->PTarget != NULL) {
            tx = p_vehicle->PTarget->X >> 8;
            tz = p_vehicle->PTarget->Z >> 8;
        } else {
            tx = p_veh->TargetDX;
            tz = p_veh->TargetDZ;
        }
    }
    else
    {
        tx = p_veh->GotoX;
        tz = p_veh->GotoZ;
    }
    dx = tx - cx;
    dz = tz - cz;
    d = (ushort)arctan(dx, dz) - p_veh->AngleY;
    if (d > 0x400)
        d -= 0x7FF;
    if (d < -0x400)
        d += 0x7FF;
    if (d > 0x186)
        d = 0x186;
    if (d < -0x186)
        d = -0x186;
    p_veh->AngleDY = d;
    p_veh->ReqdSpeed = 900 - 2 * abs(p_veh->AngleDY);
    dist2 = (s32)((u32)dx * (u32)dx + (u32)dz * (u32)dz);
    if (p_vehicle->State == 0x33)
    {
        p_veh->ReqdSpeed = 0;
        if (abs(p_vehicle->Speed) < 50)
        {
            p_vehicle->State = 0x21;
            p_vehicle->Speed = 0;
        }
    }
    else if (dist2 < 0x30000)
    {
        if (do_next_navigate_index(p_vehicle))
        {
            p_veh->PathIndex = 0;
            p_vehicle->State = 0x33;
        }
        else
        {
            flyer_goto_path_point(p_vehicle);
        }
    }
    if (p_vehicle->Speed < p_veh->ReqdSpeed)
        p_vehicle->Speed += 0x30;
    if (p_vehicle->Speed > p_veh->ReqdSpeed)
        p_vehicle->Speed -= 0x30;
    p_veh->AngleY = (p_veh->AngleY + (p_veh->AngleDY >> 3)) & 0x7FF;
    p_mat = &local_mats[p_veh->MatrixIndex];
    rotate_obj(0, -p_veh->AngleY, 0, p_mat);
    p_vehicle->VX = (-p_mat->R[0][2] * p_vehicle->Speed) >> 10;
    p_vehicle->VY = (-p_mat->R[1][2] * p_vehicle->Speed) >> 10;
    p_vehicle->VZ = (-p_mat->R[2][2] * p_vehicle->Speed) >> 10;
    move_mapwho(p_vehicle, p_vehicle->X + p_vehicle->VX, p_vehicle->Y + p_vehicle->VY,
      p_vehicle->Z + p_vehicle->VZ);
}

void process_mech(struct Thing *p_vehicle)
{
    FNCHK_VOID("process_mech", process_mech_asm(p_vehicle), process_mech_c(p_vehicle));
}

#ifdef SW_FNCHK
static void train_unkn_st18_func_1_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_train_unkn_st18_func_1\n"
        : : "a" (p_vehicle) : "cc", "memory");
}

static void train_unkn_st18_func_2_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_train_unkn_st18_func_2\n"
        : : "a" (p_vehicle) : "cc", "memory");
}
#endif

/** Keeps a train wagon at its distance from the wagon `other`: speeds up
 * by `delta` when too far, slows down when too close.
 * Remade from thing_unkn_change_speed. */
static void thing_unkn_change_speed(struct Thing *p_vehicle, ushort other, short delta)
{
    struct Thing *p_other;
    s32 r1, r2, dist, dx, dz, d2;

    p_other = &things[other];
    r1 = ((ushort)p_vehicle->Radius * 386) >> 8;
    r2 = ((ushort)p_other->Radius * 386) >> 8;
    dist = r1 + r2 + 0x40;
    dist = dist * dist;
    dx = (p_vehicle->X - p_other->X) >> 8;
    dz = (p_vehicle->Z - p_other->Z) >> 8;
    d2 = dx * dx + dz * dz;
    if (d2 > dist + 0x100)
        p_vehicle->Speed += delta;
    else if (d2 < dist - 0x100)
        p_vehicle->Speed -= delta;
}

/** The station 4 links on from `station` along the station list (NextThing or PrevThing). */
static short train_station_hop4(short station, TbBool forward)
{
    int i;

    for (i = 0; i < 4; i++)
    {
        if (forward)
            station = things[station].U.UObject.NextThing;
        else
            station = things[station].U.UObject.PrevThing;
    }
    return station;
}

/** Pitch of a train towards the height of its station. `sign` is the
 * pitch sign used when the station is higher. */
static void train_pitch_to_station(struct Thing *p_vehicle, s32 dx, s32 dz,
  s32 st_y, int sign_up)
{
    s32 cy;
    short angle;

    cy = (st_y >> 6) - (p_vehicle->Y >> 6);
    angle = arctan((s32)((u32)cy * (u32)cy), (s32)((u32)dz * (u32)dz + (u32)dx * (u32)dx)) - 0x400;
    if (angle > 100)
        angle = 100;
    else if (angle < -100)
        angle = -100;
    if (sign_up > 0 ? (cy > 0) : (cy < 0))
        p_vehicle->U.UVehicle.AngleX = -angle;
    else
        p_vehicle->U.UVehicle.AngleX = angle;
}

/** A train going backwards along its stations (Flag 0x08000000 set means
 * forwards): follows the wagon in GotoX, at the end station turns
 * around (state 0x13).  Remade from train_unkn_st18_func_1. */
static void train_unkn_st18_func_1_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    struct Thing *p_station;
    s32 dx, dz, d2, st_y;
    short tnode, diff;

    p_veh = &p_vehicle->U.UVehicle;
    tnode = p_veh->TNode;
    p_station = &things[tnode];
    dx = (p_station->X >> 8) - (p_vehicle->X >> 8);
    st_y = (p_station->Y + 0xF00) >> 3;
    dz = (p_station->Z >> 8) - (p_vehicle->Z >> 8);
    d2 = (s32)((u32)dx * (u32)dx + (u32)dz * (u32)dz);
    p_veh->ReqdSpeed = -0x546;
    if (p_veh->GotoX != 0)
    {
        struct Thing *p_lead;

        p_lead = &things[p_veh->GotoX];
        p_veh->ReqdSpeed = p_lead->U.UVehicle.ReqdSpeed;
        if ((p_vehicle->Flag & 0x08000000) != (p_lead->Flag & 0x08000000))
        {
            p_veh->TNode = train_station_hop4(p_station->U.UObject.NextThing, true);
            p_vehicle->Flag &= ~0x08000000;
            if (p_veh->ComHead != 0)
                play_dist_sample(p_vehicle, 0x62, 0x7F, 0x40, 100, 0, 1);
            p_lead = &things[p_veh->GotoX];
            p_vehicle->State = 0x15;
            p_veh->ComHead = p_lead->U.UVehicle.ComHead;
            p_veh->ComCur = p_lead->U.UVehicle.ComCur;
            p_veh->ReqdSpeed = 0;
        }
        else
        {
            thing_unkn_change_speed(p_vehicle, (ushort)p_veh->GotoX, -200);
        }
    }
    if ((p_vehicle->State != 0x15) && (d2 < 0x40000) && (tnode > 0))
    {
        if (p_station->U.UObject.PrevThing != 0)
        {
            p_veh->TNode = p_station->U.UObject.PrevThing;
        }
        else if (p_vehicle->Speed != 0)
        {
            p_veh->ReqdSpeed = 0;
        }
        else
        {
            p_vehicle->Flag &= ~0x08000000;
            p_veh->TNode = train_station_hop4(p_station->U.UObject.NextThing, true);
            p_vehicle->State = 0x13;
            p_veh->ComHead = p_station->U.UObject.ComHead;
            p_veh->ComCur = p_station->U.UObject.ComCur;
            p_veh->ReqdSpeed = 0;
        }
    }
    if (d2 > 0x4000)
        train_pitch_to_station(p_vehicle, dx, dz, st_y, 1);
    diff = p_veh->AngleY - (short)(((ushort)arctan(dx, dz) + 0x400) & 0x7FF);
    if (diff < -0x400)
        diff += 0x800;
    else if (diff > 0x400)
        diff -= 0x800;
    if (diff < 0)
    {
        if (diff < -100)
            p_vehicle->Speed = 0;
        if (diff > -50)
            p_veh->AngleDY = -diff * 2;
        else
            p_veh->AngleDY = 0x1E0;
    }
    else if (diff > 0)
    {
        if (diff > 100)
            p_vehicle->Speed = 0;
        if (diff < 50)
            p_veh->AngleDY = -(diff * 2);
        else
            p_veh->AngleDY = -0x1E0;
    }
}

void train_unkn_st18_func_1(struct Thing *p_vehicle)
{
    FNCHK_VOID("train_unkn_st18_func_1", train_unkn_st18_func_1_asm(p_vehicle),
      train_unkn_st18_func_1_c(p_vehicle));
}

/** A train going forwards along its stations: follows the wagon in Owner,
 * at the end station turns around (state 0x13).
 * Remade from train_unkn_st18_func_2. */
static void train_unkn_st18_func_2_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    struct Thing *p_station;
    s32 dx, dz, d2, st_y;
    short tnode, diff;

    p_veh = &p_vehicle->U.UVehicle;
    tnode = p_veh->TNode;
    p_station = &things[tnode];
    dx = (p_station->X >> 8) - (p_vehicle->X >> 8);
    st_y = (p_station->Y + 0xF00) >> 3;
    dz = (p_station->Z >> 8) - (p_vehicle->Z >> 8);
    d2 = (s32)((u32)dx * (u32)dx + (u32)dz * (u32)dz);
    p_veh->ReqdSpeed = 0x546;
    if (p_vehicle->Owner != 0)
    {
        struct Thing *p_lead;

        p_lead = &things[(ushort)p_vehicle->Owner];
        p_veh->ReqdSpeed = p_lead->U.UVehicle.ReqdSpeed;
        if ((p_vehicle->Flag & 0x08000000) != (p_lead->Flag & 0x08000000))
        {
            p_vehicle->Flag |= 0x08000000;
            p_veh->TNode = train_station_hop4(p_station->U.UObject.PrevThing, false);
            p_vehicle->State = 0x15;
            if (p_veh->ComHead != 0)
                play_dist_sample(p_vehicle, 0x62, 0x7F, 0x40, 100, 0, 1);
            p_lead = &things[(ushort)p_vehicle->Owner];
            p_veh->ComHead = p_lead->U.UVehicle.ComHead;
            p_veh->ReqdSpeed = 0;
            p_veh->ComCur = p_lead->U.UVehicle.ComCur;
        }
        else
        {
            thing_unkn_change_speed(p_vehicle, (ushort)p_vehicle->Owner, 200);
        }
    }
    if ((p_vehicle->State != 0x15) && (d2 < 0x40000) && (tnode > 0))
    {
        if (p_station->U.UObject.NextThing != 0)
        {
            p_veh->TNode = p_station->U.UObject.NextThing;
        }
        else if (p_vehicle->Speed != 0)
        {
            p_veh->ReqdSpeed = 0;
        }
        else
        {
            p_vehicle->Flag |= 0x08000000;
            p_veh->TNode = train_station_hop4(p_station->U.UObject.PrevThing, false);
            p_vehicle->State = 0x13;
            p_veh->ComHead = p_station->U.UObject.ComHead;
            p_veh->ComCur = p_station->U.UObject.ComCur;
            p_veh->ReqdSpeed = 0;
        }
    }
    if (d2 > 0x4000)
        train_pitch_to_station(p_vehicle, dx, dz, st_y, -1);
    diff = p_veh->AngleY - arctan(dx, dz);
    if (diff < -0x400)
        diff += 0x800;
    else if (diff > 0x400)
        diff -= 0x800;
    if (diff < 0)
    {
        if (diff < -100)
            p_vehicle->Speed = 0;
        if (diff > -50)
            p_veh->AngleDY = -diff * 2;
        else
            p_veh->AngleDY = (0x800 - (p_vehicle->Speed >> 3)) >> 2;
    }
    else if (diff > 0)
    {
        if (diff > 100)
            p_vehicle->Speed = 0;
        if (diff < 50)
            p_veh->AngleDY = -(diff * 2);
        else
            p_veh->AngleDY = -((0x800 - (p_vehicle->Speed >> 3)) >> 2);
    }
}

void train_unkn_st18_func_2(struct Thing *p_vehicle)
{
    FNCHK_VOID("train_unkn_st18_func_2", train_unkn_st18_func_2_asm(p_vehicle),
      train_unkn_st18_func_2_c(p_vehicle));
}

#ifdef SW_FNCHK
static int train_unkn_st21_exit_func_1_asm(struct Thing *p_vehicle)
{
    int ret;
    asm volatile ("call ASM_train_unkn_st21_exit_func_1\n"
        : "=a" (ret) : "0" (p_vehicle) : "cc", "memory");
    return ret;
}

static void train_unkn_st20_func_1_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_train_unkn_st20_func_1\n"
        : : "a" (p_vehicle) : "cc", "memory");
}
#endif

/** A random point (in 1/16 steps) inside the triangle of the first three
 * points of a quad face, in map coordinates.  16-bit arithmetic as in the
 * original.  Remade from set_xz_for_unkn3. */
static void set_xz_for_unkn3(struct SingleObjectFace4 *p_face, s32 *p_x, s32 *p_z)
{
    struct SingleObject *p_obj;
    struct SinglePoint *p_pt0, *p_pt1, *p_pt2;
    ushort r1, r2;
    short sx, sz;

    r1 = LbRandomAnyShort() & 0xF;
    r2 = LbRandomAnyShort() & 0xF;
    p_obj = &game_objects[(ushort)p_face->Object];
    p_pt1 = &game_object_points[(ushort)p_face->PointNo[1]];
    p_pt0 = &game_object_points[(ushort)p_face->PointNo[0]];
    p_pt2 = &game_object_points[(ushort)p_face->PointNo[2]];
    sx = (short)((ushort)(p_pt1->X - p_pt0->X) * r1 + (ushort)(p_pt2->X - p_pt0->X) * (16 - r1));
    sz = (short)((ushort)(p_pt1->Z - p_pt0->Z) * r2 + (ushort)(p_pt2->Z - p_pt0->Z) * (16 - r2));
    sx = (short)((sx >> 4) + p_pt0->X + p_obj->MapX);
    sz = (short)((sz >> 4) + p_pt0->Z + p_obj->MapZ);
    *p_x = sx;
    *p_z = sz;
}

/** Train at a station: every passenger gets out onto a random spot of the
 * platform face (ComCur).  Returns 0.  Remade from train_unkn_st21_exit_func_1. */
static int train_unkn_st21_exit_func_1_c(struct Thing *p_vehicle)
{
    while (p_vehicle->U.UVehicle.PassengerHead != 0)
    {
        struct Thing *p_person;
        s32 x, z;
        short face;

        p_person = &things[(ushort)p_vehicle->U.UVehicle.PassengerHead];
        p_person->Flag &= ~0x12000000;
        p_vehicle->U.UVehicle.PassengerHead = p_person->U.UPerson.LinkPassenger;
        face = p_vehicle->U.UVehicle.ComCur;
        set_xz_for_unkn3(&game_object_faces4[face], &x, &z);
        p_person->X = x << 8;
        p_person->Z = z << 8;
        p_person->U.UPerson.OnFace = -face;
        set_thing_height_on_face_quad(p_person, x << 8, z << 8, face);
        p_person->Flag |= 0x0004;
        add_node_thing((ushort)p_person->ThingOffset);
        p_person->U.UPerson.Flag3 |= 0x80;
    }
    return 0;
}

int train_unkn_st21_exit_func_1(struct Thing *p_vehicle)
{
    int ret;

    FNCHK_RET("train_unkn_st21_exit_func_1", ret, train_unkn_st21_exit_func_1_asm(p_vehicle),
      train_unkn_st21_exit_func_1_c(p_vehicle));
    return ret;
}

/** First person standing on the station face `face` who is waiting for
 * a train (or riding with a group on one), or 0.
 * Remade from train_unkn_func_02. */
static ThingIdx train_unkn_func_02(ushort face)
{
    ThingIdx thing;

    for (thing = same_type_head[1]; thing != 0; thing = things[(ushort)thing].LinkSame)
    {
        struct Thing *p_person, *p_owner;
        short on_face;

        p_person = &things[(ushort)thing];
        on_face = p_person->U.UPerson.OnFace;
        if (on_face >= 0)
            continue;
        if (-on_face != face)
            continue;
        if ((p_person->Flag & 0x10000000) != 0)
            continue;
        if ((p_person->Flag & 0x00080000) != 0)
        {
            p_owner = &things[(ushort)p_person->Owner];
            if ((p_owner->Flag & 0x10000000) == 0)
                continue;
            if (things[p_owner->U.UPerson.Vehicle].SubType != 0x1D)
                continue;
            return thing;
        }
        if ((p_person->Flag & 0x00002000) != 0)
        {
            if ((p_person->U.UPerson.Flag3 & 0x20) != 0)
                return thing;
            if ((ushort)p_person->State != 0x2B)
                continue;
            p_owner = &things[(ushort)p_person->Owner];
            if ((p_owner->U.UPerson.Flag3 & 0x20) != 0)
                return thing;
            if ((p_owner->Flag & 0x10000000) == 0)
                continue;
            if (things[p_owner->U.UPerson.Vehicle].SubType != 0x1D)
                continue;
            return thing;
        }
        if ((p_person->U.UPerson.Flag3 & 0x80) == 0)
            return thing;
    }
    return 0;
}

/** Puts a person into the train: first in its passenger list, off the
 * map, at the train position.  Remade from
 * person_clear_route_and_teleport_to_vehicle. */
static void person_clear_route_and_teleport_to_vehicle(struct Thing *p_vehicle,
  struct Thing *p_person)
{
    short old_head;

    old_head = p_vehicle->U.UVehicle.PassengerHead;
    p_vehicle->U.UVehicle.PassengerHead = p_person->ThingOffset;
    p_person->U.UPerson.OnFace = 0;
    p_person->U.UPerson.LinkPassenger = old_head;
    p_person->U.UPerson.Flag3 &= ~0x20;
    p_person->Flag |= 0x12000000;
    delete_node(p_person);
    p_person->U.UPerson.Vehicle = p_vehicle->ThingOffset;
    p_person->X = p_vehicle->X;
    p_person->Y = p_vehicle->Y >> 3;
    p_person->Z = p_vehicle->Z;
}

/** Train waiting at a station: takes in the people waiting on the platform
 * faces (ComHead and the faces walkable from it); when nobody came for
 * Timer2 turns, it leaves (state 0x12).  Remade from train_unkn_st20_func_1. */
static void train_unkn_st20_func_1_c(struct Thing *p_vehicle)
{
    ushort found;

    found = 0;
    for (;;)
    {
        if (p_vehicle->U.UVehicle.ComHead != 0)
        {
            struct WalkHeader *p_wh;
            ushort i;

            found = 0;
            p_wh = &game_walk_headers[(short)game_object_faces4[(short)p_vehicle->U.UVehicle.ComHead].WalkHeader];
            for (i = p_wh->StartItem; i < p_wh->StartItem + p_wh->Count; i++)
            {
                struct WalkHeader *p_wh2;
                short item;
                ushort k;

                item = game_walk_items[i];
                found = train_unkn_func_02((ushort)-item);
                if (found != 0)
                    break;
                p_wh2 = &game_walk_headers[(short)game_object_faces4[-item].WalkHeader];
                for (k = p_wh2->StartItem; k < p_wh2->StartItem + p_wh2->Count; k++)
                {
                    found = train_unkn_func_02((ushort)-game_walk_items[k]);
                    if (found != 0)
                        break;
                }
                if (found != 0)
                    break;
            }
            if (found == 0)
                found = train_unkn_func_02(p_vehicle->U.UVehicle.ComHead);
        }
        if (found == 0)
            break;
        person_clear_route_and_teleport_to_vehicle(p_vehicle, &things[found]);
        p_vehicle->U.UVehicle.Timer2 = 100;
    }
    p_vehicle->U.UVehicle.Timer2--;
    if (p_vehicle->U.UVehicle.Timer2 <= 0)
    {
        p_vehicle->State = 0x12;
        play_dist_sample(p_vehicle, 0x61, 0x7F, 0x40, 100, 0, 1);
    }
}

void train_unkn_st20_func_1(struct Thing *p_vehicle)
{
    FNCHK_VOID("train_unkn_st20_func_1", train_unkn_st20_func_1_asm(p_vehicle),
      train_unkn_st20_func_1_c(p_vehicle));
}

#ifdef SW_FNCHK
static void process_ship_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_ship\n"
        : : "a" (p_vehicle) : "cc", "memory");
}
#endif

/** Random state of the ships' route choice (asm data). */
extern u32 rand_d;
/** Count, then up to 16 traffic nodes where ships appear (asm data). */
extern s32 data_1c84f4[17];

/** Next traffic node of a ship from node `tnode`: a random link with link
 * flags (bits 0xC0) equal to `want` (12 tries), else the first such link.
 * Returns 0 when there is none.  Remade from find_next_ship_tnode. */
static short find_next_ship_tnode(short tnode, ubyte want)
{
    struct TrafficNode *p_tnode;
    int n;

    for (n = 0; n < 12; n++)
    {
        u32 r;
        int i;

        r = rand_d;
        rand_d = ((r >> 7) | (r << 25)) + 0x16365267;
        i = r & 3;
        p_tnode = &game_traffic_nodes[tnode];
        if ((p_tnode->UTraffic.Link[i] != 0) && ((p_tnode->UTraffic.Flags[i] & 0xC0) == want))
            return -p_tnode->UTraffic.Link[i];
    }
    for (n = 0; n < 4; n++)
    {
        p_tnode = &game_traffic_nodes[tnode];
        if ((p_tnode->UTraffic.Link[n] != 0) && ((p_tnode->UTraffic.Flags[n] & 0xC0) == want))
            return -p_tnode->UTraffic.Link[n];
    }
    return 0;
}

/** A stopping ship: no wanted speed, turning eases off by 4. */
static void ship_stop_turning(struct TngUVehicle *p_veh)
{
    p_veh->ReqdSpeed = 0;
    if (p_veh->AngleDY > 0)
        p_veh->AngleDY -= 4;
    if (p_veh->AngleDY < 0)
        p_veh->AngleDY += 4;
}

/** A ship sails along traffic nodes: steers to the node, at a route end
 * (node flag 0x80) jumps to a random start node, takes the next free node
 * in state 0x40 (marking it taken with 0x40), waits at docks (flag 0x10,
 * LeisurePlace is the wait timer) and stops behind another ship.
 * Remade from process_ship. */
static void process_ship_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    struct TrafficNode *p_tnode;
    struct M33 *p_mat;
    s32 dx, dz, d2;
    int a, i;

    p_veh = &p_vehicle->U.UVehicle;
    p_tnode = &game_traffic_nodes[p_veh->TNode];
    dx = p_tnode->X - (p_vehicle->X >> 8);
    dz = p_tnode->Z - (p_vehicle->Z >> 8);
    a = (ushort)arctan(dx, dz) - p_veh->AngleY;
    if (a > 0x400)
        a -= 0x7FF;
    if (a < -0x400)
        a += 0x7FF;
    if (a > 0xA0)
        a = 0xA0;
    if (a < -0xA0)
        a = -0xA0;
    if (abs(a) > 40)
        a >>= 2;
    if (p_veh->AngleDY > a)
        p_veh->AngleDY--;
    if (p_veh->AngleDY < a)
        p_veh->AngleDY++;
    p_veh->ReqdSpeed = 300 - (abs(p_veh->AngleDY) << 2);
    d2 = (s32)((u32)dx * (u32)dx + (u32)dz * (u32)dz);
    if (d2 < 0x100000)
    {
        p_tnode = &game_traffic_nodes[p_veh->TNode];
        if ((p_tnode->Flags & 0x80) != 0)
        {
            if (d2 < 0x20000)
            {
                struct TrafficNode *p_start;
                s32 px, pz;
                ushort rnd;

                p_tnode->Flags &= ~0x40;
                rnd = LbRandomAnyShort();
                p_veh->TNode = (short)data_1c84f4[1 + rnd % data_1c84f4[0]];
                p_start = &game_traffic_nodes[p_veh->TNode];
                px = p_start->X << 8;
                pz = p_start->Z << 8;
                move_mapwho(p_vehicle, px, p_vehicle->Y, pz);
                p_start = &game_traffic_nodes[p_veh->TNode];
                for (i = 0; i < 4; i++)
                {
                    struct TrafficNode *p_next;

                    if (p_start->UTraffic.Link[i] == 0)
                        continue;
                    if ((p_start->UTraffic.Flags[i] & 0xC0) != 0)
                        continue;
                    p_next = &game_traffic_nodes[-p_start->UTraffic.Link[i]];
                    dx = (p_next->X << 8) - px;
                    dz = (p_next->Z << 8) - pz;
                    break;
                }
                p_veh->AngleDY = 0;
                p_veh->AngleY = arctan(dx, dz);
            }
        }
        else if ((ushort)p_vehicle->State == 0x3F)
        {
            if (p_tnode->GateLink != 0)
                p_veh->TNode = p_tnode->GateLink;
            else
                ship_stop_turning(p_veh);
        }
        else if ((ushort)p_vehicle->State == 0x40)
        {
            short next;

            next = find_next_ship_tnode(p_veh->TNode, 0);
            if (next == 0)
            {
                p_veh->ReqdSpeed = 0;
            }
            else if ((game_traffic_nodes[next].Flags & 0x40) != 0)
            {
                ship_stop_turning(p_veh);
            }
            else
            {
                game_traffic_nodes[p_veh->TNode].Flags &= ~0x40;
                p_veh->TNode = next;
                game_traffic_nodes[p_veh->TNode].Flags |= 0x40;
                if ((game_traffic_nodes[p_veh->TNode].Flags & 0x810) != 0)
                    p_veh->LeisurePlace = 200;
            }
        }
    }
    p_tnode = &game_traffic_nodes[p_veh->TNode];
    for (i = 0; i < 4; i++)
    {
        if ((p_tnode->UTraffic.Flags[i] & 0x40) == 0)
            continue;
        if ((game_traffic_nodes[-p_tnode->UTraffic.Link[i]].Flags & 0x40) == 0)
            continue;
        ship_stop_turning(p_veh);
        break;
    }
    if (p_veh->LeisurePlace != 0)
    {
        ship_stop_turning(p_veh);
        p_veh->LeisurePlace--;
    }
    if (((game_traffic_nodes[p_veh->TNode].Flags & 0x10) != 0) && (p_veh->LeisurePlace != 0))
    {
        ship_stop_turning(p_veh);
        p_veh->LeisurePlace--;
        if ((p_veh->LeisurePlace == 0) && (p_veh->PassengerHead == 0))
            p_veh->LeisurePlace = 150;
    }
    if (p_vehicle->Speed < p_veh->ReqdSpeed)
        p_vehicle->Speed++;
    if (p_vehicle->Speed > p_veh->ReqdSpeed)
        p_vehicle->Speed -= 10;
    p_veh->AngleY = (p_veh->AngleY + (p_veh->AngleDY >> 4)) & 0x7FF;
    p_mat = &local_mats[p_veh->MatrixIndex];
    rotate_obj(0, -p_veh->AngleY, 0, p_mat);
    p_vehicle->VX = (-p_mat->R[0][2] * p_vehicle->Speed) >> 10;
    p_vehicle->VY = (-p_mat->R[1][2] * p_vehicle->Speed) >> 10;
    p_vehicle->VZ = (-p_mat->R[2][2] * p_vehicle->Speed) >> 10;
    move_mapwho(p_vehicle, p_vehicle->X + p_vehicle->VX, p_vehicle->Y + p_vehicle->VY,
      p_vehicle->Z + p_vehicle->VZ);
}

void process_ship(struct Thing *p_vehicle)
{
    FNCHK_VOID("process_ship", process_ship_asm(p_vehicle), process_ship_c(p_vehicle));
}

#ifdef SW_FNCHK
static void process_mech_unknown1_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_mech_unknown1\n"
        : : "a" (p_vehicle) : "cc", "memory");
}
#endif

/* The asm sees the end of unkn_mech_struc3 (from offset 0x54) as pairs,
 * one per leg: s32 timer at 0x54, s32 animation at 0x5C, s32 frame
 * (unkn_mech_struc1 index) at 0x64, s32 sequence (unkn_mech_struc2 index)
 * at 0x6C and the "playing" byte at 0x74.  Read through memcpy, as the
 * structures are packed. */
static s32 mech_rd32(const void *p, int offs)
{
    s32 v;

    memcpy(&v, (const ubyte *)p + offs, 4);
    return v;
}

static void mech_wr32(void *p, int offs, s32 v)
{
    memcpy((ubyte *)p + offs, &v, 4);
}

/** Starts animation `anim` (an unkn_mech_arr2 entry) in slot `slot` (0 or 1)
 * of the mech of thing `owntng`: the animation, its first step (from
 * unkn_mech_arr1) and the "playing" flag.  Remade from mech_unkn_func_05. */
void mech_unkn_func_05_c(int owntng, int slot, int anim)
{
    ubyte *p_m3, *p_m1;
    s32 step;

    p_m3 = (ubyte *)&unkn_mech_arr3[owntng];
    step = unkn_mech_arr2[anim].field_0;
    p_m1 = (ubyte *)&unkn_mech_arr1[step];
    mech_wr32(p_m3, 0x6C + slot * 4, anim);
    mech_wr32(p_m3, 0x54 + slot * 4, mech_rd32(p_m1, 0xD));
    mech_wr32(p_m3, 0x5C + slot * 4, mech_rd32(p_m1, 0x9));
    mech_wr32(p_m3, 0x64 + slot * 4, step);
    p_m3[0x74 + slot] = 1;
}

/** Copies a matrix transposed.  Remade from matrix_transpose_cp. */
static void matrix_transpose_cp(struct M33 *p_dst, struct M33 *p_src)
{
    p_dst->R[0][0] = p_src->R[0][0];
    p_dst->R[0][1] = p_src->R[1][0];
    p_dst->R[0][2] = p_src->R[2][0];
    p_dst->R[1][0] = p_src->R[0][1];
    p_dst->R[1][1] = p_src->R[1][1];
    p_dst->R[1][2] = p_src->R[2][1];
    p_dst->R[2][0] = p_src->R[0][2];
    p_dst->R[2][1] = p_src->R[1][2];
    p_dst->R[2][2] = p_src->R[2][2];
}

/** Three angles (0..0x7FF) one step of `steps` towards the target ones,
 * each the short way round.  Remade from mech_unkn_func_20. */
static void mech_unkn_func_20(const s32 *p_cur, const s32 *p_tgt, s32 steps, s32 *p_out)
{
    s32 d[3];
    int a;

    for (a = 0; a < 3; a++)
    {
        s32 dd, sign, other;

        dd = p_tgt[a] - p_cur[a];
        if (dd < 0)
            sign = -1;
        else
            sign = (dd != 0);
        other = -((0x800 - abs(dd)) * sign);
        if (abs(other) < abs(dd))
            dd = other;
        d[a] = dd;
    }
    if (steps != 0) {
        for (a = 0; a < 3; a++)
            d[a] /= steps;
    }
    for (a = 0; a < 3; a++)
        p_out[a] = (p_cur[a] + d[a]) & 0x7FF;
}

/** Moves every part of a mech one step towards the current frame of the
 * animation of leg 0: angles (where the frame has the part) and position.
 * Remade from mech_unkn_func_19. */
static void mech_unkn_func_19(ushort mechno)
{
    struct unkn_mech_struc3 *p_m3;
    s32 angles[17][3];
    u32 k, nparts;

    p_m3 = &unkn_mech_arr3[mechno];
    nparts = p_m3->mech3_unkn_fld_8;
    for (k = 0; k < nparts; k++)
    {
        struct unkn_mech_struc4 *p_m4;

        p_m4 = &unkn_mech_arr4[p_m3->mech3_unkn_arr4_idx + k];
        angles[k][0] = p_m4->angle_49;
        angles[k][1] = p_m4->angle_4D;
        angles[k][2] = p_m4->angle_51;
    }
    for (k = 0; k < nparts; k++)
    {
        struct unkn_mech_struc4 *p_m4;
        ubyte *p_m5;
        ushort part;
        s32 steps, cur[3], tgt[3], d[3];
        int a;

        p_m4 = &unkn_mech_arr4[p_m3->mech3_unkn_arr4_idx + k];
        p_m5 = (ubyte *)&unkn_mech_arr5[mech_rd32(p_m3, 0x5C)];
        part = p_m4->field_23;
        if (p_m5[0x198 + part] == 0)
            continue;
        steps = mech_rd32(p_m3, 0x54);
        cur[0] = p_m4->angle_49;
        cur[1] = p_m4->angle_4D;
        cur[2] = p_m4->angle_51;
        for (a = 0; a < 3; a++)
            tgt[a] = mech_rd32(p_m5, 0xC + 24 * part + 4 * a);
        mech_unkn_func_20(cur, tgt, steps, angles[k]);
        for (a = 0; a < 3; a++)
            d[a] = mech_rd32(p_m5, 24 * part + 4 * a) - p_m4->field_6D.R[a];
        if (steps != 0) {
            for (a = 0; a < 3; a++)
                d[a] /= steps;
        }
        for (a = 0; a < 3; a++)
            p_m4->field_6D.R[a] += d[a];
    }
    for (k = 0; k < nparts; k++)
    {
        struct unkn_mech_struc4 *p_m4;

        p_m4 = &unkn_mech_arr4[p_m3->mech3_unkn_arr4_idx + k];
        p_m4->angle_49 = angles[k][0];
        p_m4->angle_4D = angles[k][1];
        p_m4->angle_51 = angles[k][2];
    }
}

/** Advances the animation of both legs of a mech: a leg waits its timer,
 * then goes to the next frame (or stops at the end of its sequence), then
 * the parts move.  Returns the event of the last new frame (leg 0 last),
 * 0x7FFFFFFF when none.  Remade from mech_unkn_func_08. */
static u32 mech_unkn_func_08(ushort mechno)
{
    struct unkn_mech_struc3 *p_m3;
    s32 ret;
    int leg;

    ret = 0x7FFFFFFF;
    p_m3 = &unkn_mech_arr3[mechno];
    for (leg = 1; leg >= 0; leg--)
    {
        ubyte *p_leg;
        s32 timer, frame;
        struct unkn_mech_struc2 *p_m2;

        p_leg = (ubyte *)p_m3 + 4 * leg;
        if (*((ubyte *)p_m3 + 0x74 + leg) == 0)
            continue;
        timer = mech_rd32(p_leg, 0x54);
        if (timer > 0) {
            mech_wr32(p_leg, 0x54, timer - 1);
            continue;
        }
        frame = unkn_mech_arr1[mech_rd32(p_leg, 0x64)].field_5;
        p_m2 = &unkn_mech_arr2[mech_rd32(p_leg, 0x6C)];
        if ((frame == p_m2->field_0) && (p_m2->field_88 == 0))
        {
            *((ubyte *)p_m3 + 0x74 + leg) = 0;
            mech_wr32(p_leg, 0x54, 0);
            continue;
        }
        mech_wr32(p_leg, 0x64, frame);
        mech_wr32(p_leg, 0x54, unkn_mech_arr1[frame].field_D);
        ret = unkn_mech_arr1[frame].field_9;
        mech_wr32(p_leg, 0x5C, ret);
    }
    mech_unkn_func_19(mechno);
    return (u32)ret;
}

/** Mech animation each turn: the mech's matrix (transposed) goes to its
 * animation state, the legs move, and frame events set the step flags in
 * TNode; a mech hit while walking to a point stops (state 0x33).
 * Remade from process_mech_unknown1. */
static void process_mech_unknown1_c(struct Thing *p_vehicle)
{
    struct TngUVehicle *p_veh;
    ushort mechno;
    u32 event;

    p_veh = &p_vehicle->U.UVehicle;
    mech_last_cor_x = p_vehicle->X >> 8;
    mech_last_cor_y = p_vehicle->Y >> 8;
    mech_last_cor_z = mech_last_cor_x;
    mech_unkn_tile_y2 = mech_last_cor_x;
    mech_unkn_tile_x3 = mech_last_cor_y;
    mech_unkn_tile_y3 = mech_unkn_tile_y2;
    mechno = (ushort)p_vehicle->Owner;
    matrix_transpose_cp(&unkn_mech_arr3[mechno].mech3_unkn_mat_C, &local_mats[p_veh->MatrixIndex]);
    event = mech_unkn_func_08(mechno);
    /* step flags in the low byte of TNode */
    switch (event)
    {
    case 0:
        p_veh->TNode |= 0x04;
        break;
    case 6:
        p_veh->TNode = (p_veh->TNode | 0x02) & ~0x04;
        break;
    case 8:
        p_veh->TNode = (p_veh->TNode | 0x01) & ~0x04;
        break;
    case 0xE:
        p_veh->TNode = (p_veh->TNode | 0x08) & ~0x04;
        break;
    default:
        break;
    }
    if (((ushort)p_vehicle->State == 0x32) && (p_veh->ZebraOldHealth != p_vehicle->Health))
    {
        remove_path(p_vehicle);
        p_veh->PathIndex = 0;
        p_vehicle->State = 0x33;
    }
    p_veh->ZebraOldHealth = p_vehicle->Health;
}

void process_mech_unknown1(struct Thing *p_vehicle)
{
    FNCHK_VOID("process_mech_unknown1", process_mech_unknown1_asm(p_vehicle),
      process_mech_unknown1_c(p_vehicle));
}

#ifdef SW_FNCHK
static void init_mech_explode_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_init_mech_explode\n"
        : : "a" (p_vehicle) : "cc", "memory");
}
#endif

/** A mech blows up: its crew is thrown out dead, bangs and a crater, the
 * mech is removed, and all mech parts (things of type 35) fly apart from
 * where it stood.  Remade from init_mech_explode. */
static void init_mech_explode_c(struct Thing *p_vehicle)
{
    s32 ox, oz;
    ThingIdx thing;

    ox = p_vehicle->X;
    oz = p_vehicle->Z;
    if ((p_vehicle->Type != TT_VEHICLE) || (p_vehicle->SubType != 0x3B))
        return;
    while (p_vehicle->U.UVehicle.PassengerHead != 0)
    {
        struct Thing *p_person;

        p_person = &things[(ushort)p_vehicle->U.UVehicle.PassengerHead];
        p_vehicle->U.UVehicle.PassengerHead = p_person->U.UPerson.LinkPassenger;
        p_person->Flag &= ~0x12000000;
        add_node_thing((ushort)p_person->ThingOffset);
        set_person_dead(p_person, 0xC);
    }
    bang_new4(p_vehicle->X, p_vehicle->Y, p_vehicle->Z, 10);
    bang_new4(p_vehicle->X, p_vehicle->Y, p_vehicle->Z, 0x73);
    bang_new4(p_vehicle->X, p_vehicle->Y, p_vehicle->Z, 0x2D);
    quick_crater(p_vehicle->X >> 16, p_vehicle->Z >> 16, 2);
    p_vehicle->Flag = 0x0002;
    p_vehicle->State = 0x0D;
    remove_thing((ushort)p_vehicle->ThingOffset);
    delete_node(p_vehicle);
    for (thing = things_used_head; thing > 0; )
    {
        struct Thing *p_part;

        p_part = &things[thing];
        thing = p_part->LinkChild;
        if (p_part->Type != TT_UNKN35)
            continue;
        p_part->State = 0x0D;
        p_part->Flag |= 0x0002;
        p_part->X <<= 8;
        p_part->VY = 1000;
        p_part->Z <<= 8;
        p_part->VX = (p_part->X - ox) >> 4;
        p_part->Y <<= 8;
        p_part->VZ = (p_part->Z - oz) >> 4;
        add_node_thing((ushort)p_part->ThingOffset);
    }
}

extern ushort game_level_unique_id;

/** Creates one mech part: a thing of type 35 with its own model (prim
 * object `prim`) and a copy of the matrix.  Returns the thing index.
 * Remade from init_unkn1thing. */
static ThingIdx init_unkn1thing(struct M33 *p_mat, s32 x, s32 y, s32 z,
  ushort upx, ushort upy, ushort upz, short prim)
{
    struct Thing *p_part;
    ThingIdx thing;

    thing = get_new_thing();
    p_part = &things[(ushort)thing];
    p_part->VX = 0;
    p_part->VZ = 0;
    p_part->VY = 0;
    p_part->U.UVehicle.AngleX = 0;
    p_part->U.UVehicle.AngleY = 0;
    p_part->U.UVehicle.AngleZ = 0;
    p_part->Type = TT_UNKN35;
    p_part->Radius = 150;
    p_part->Parent = 0;
    p_part->Speed = 0;
    p_part->StartFrame = 0;
    p_part->State = 0;
    p_part->Flag = 0;
    p_part->X = x;
    p_part->U.UVehicle.Object = next_object;
    p_part->U.UVehicle.MatrixIndex = next_local_mat;
    p_part->Z = z;
    p_part->Y = y;
    next_local_mat++;
    matrix_identity_fill(&local_mats[p_part->U.UVehicle.MatrixIndex]);
    local_mats[p_part->U.UVehicle.MatrixIndex] = *p_mat;
    p_part->Flag |= 0x04000000;
    p_part->U.UVehicle.UniqueID = game_level_unique_id;
    unkn2_pos_x = upx;
    unkn2_pos_y = upy;
    game_level_unique_id++;
    unkn2_pos_z = upz;
    copy_prim_obj_to_game_object((ushort)(x >> 8), (ushort)(z >> 8),
      (short)(-prim_unknprop01 - prim), (ushort)(y >> 8));
    unkn_object_shift_03(next_object - 1);
    game_objects[next_object - 1].ThingNo = thing;
    return thing;
}

/** Offsets of the 12 mech parts from their parent, *4 (z negated); the asm
 * data tables data_1554f8, data_155528 and data_155558. */
static const s32 mech_part_ofs_x[12] = {0, 0, -70, -10, 70, 10, 0, -40, 40, 40, -40, 0};
static const s32 mech_part_ofs_z[12] = {0, -10, 0, 75, 0, 75, 70, 20, 20, 20, 20, 65};
static const s32 mech_part_ofs_y[12] = {0, 0, 0, 10, 0, 10, -10, -40, -40, 40, 40, -10};

/** The 12 parts: prim object, part id, field_79, children (part numbers). */
static const struct {
    short prim;
    short id;
    ubyte flag;
    ubyte nchild;
    ubyte child[4];
} mech_parts[12] = {
    {0x1A, 0, 1, 3, {1, 6, 11, 0}},
    {0x00, 1, 0, 2, {2, 4, 0, 0}},
    {0x24, 3, 1, 1, {3, 0, 0, 0}},
    {0x26, 5, 1, 0, {0, 0, 0, 0}},
    {0x25, 4, 1, 1, {5, 0, 0, 0}},
    {0x27, 6, 1, 0, {0, 0, 0, 0}},
    {0x00, 2, 0, 4, {7, 8, 9, 10}},
    {0x22, 7, 1, 0, {0, 0, 0, 0}},
    {0x23, 8, 1, 0, {0, 0, 0, 0}},
    {0x21, 9, 1, 0, {0, 0, 0, 0}},
    {0x20, 10, 1, 0, {0, 0, 0, 0}},
    {0x1B, 11, 1, 0, {0, 0, 0, 0}},
};

/** Builds a mech from the vehicle thing: a new animation state (the next
 * unkn_mech_arr3 entry, the mech's Owner) and its 12 parts as type 35
 * things linked in unkn_mech_arr4 as a tree.  Remade from mech_unkn_func_09. */
static void mech_unkn_func_09_c(ThingIdx thing)
{
    struct Thing *p_mech;
    struct unkn_mech_struc3 *p_m3;
    struct M33 *p_mat;
    s32 base;
    int k;

    p_mech = &things[thing];
    base = unkn_mech_var10;
    p_mat = &local_mats[p_mech->U.UVehicle.MatrixIndex];
    p_m3 = &unkn_mech_arr3[unkn_mech_var11];
    p_m3->mech3_arr4_ptr = &unkn_mech_arr4[unkn_mech_var10];
    p_m3->mech3_unkn_arr4_idx = unkn_mech_var10;
    p_m3->mech3_unkn_mat_C = unkn_mech_mat8;
    memset((ubyte *)p_m3 + 0x3C, 0, 24);
    p_m3->mech3_unkn_X = p_mech->X;
    p_m3->mech3_unkn_Y = p_mech->Y;
    p_m3->mech3_unkn_Z = p_mech->Z;
    matrix_identity_fill(&local_mats[p_mech->U.UVehicle.MatrixIndex]);
    p_mech->Flag = 0x04000000;
    p_mech->U.UVehicle.TNode = 4;
    p_mech->Owner = unkn_mech_var11;
    VNAV_unkn_func_207(p_mech);

    for (k = 0; k < 12; k++)
    {
        struct unkn_mech_struc4 *p_m4;
        ThingIdx part;
        int c;

        part = init_unkn1thing(p_mat, 0, 0, 0, 0x40, 0x40, 0x40, mech_parts[k].prim);
        p_m4 = &unkn_mech_arr4[unkn_mech_var10];
        p_m4->thing = part;
        p_m4->field_23 = mech_parts[k].id;
        p_m4->field_79 = mech_parts[k].flag;
        p_m4->field_61.R[0] = mech_part_ofs_x[k] * 4;
        p_m4->field_61.R[1] = mech_part_ofs_z[k] * -4;
        p_m4->field_61.R[2] = mech_part_ofs_y[k] * 4;
        p_m4->field_2[0] = mech_parts[k].nchild;
        for (c = 0; c < mech_parts[k].nchild; c++)
        {
            struct unkn_mech_struc4 *p_child;

            p_child = &unkn_mech_arr4[base + mech_parts[k].child[c]];
            memcpy(&p_m4->field_2[1 + 4 * c], &p_child, sizeof(p_child));
        }
        unkn_mech_var10++;
    }
    unkn_mech_arr3[unkn_mech_var11].mech3_unkn_fld_8 = 12;
    unkn_mech_var11++;
}

void init_mech_explode(struct Thing *p_vehicle)
{
    FNCHK_VOID("init_mech_explode", init_mech_explode_asm(p_vehicle),
      init_mech_explode_c(p_vehicle));
}

void process_train(struct Thing *p_vehicle)
{
    int dtvel;

    dtvel = p_vehicle->U.UVehicle.ReqdSpeed - p_vehicle->Speed;
    if (abs(dtvel) >= 4) {
        p_vehicle->Speed += dtvel >> 1;
    } else {
        p_vehicle->Speed = p_vehicle->U.UVehicle.ReqdSpeed;
    }

    if (p_vehicle->State == VehSt_UNKN_45)
    {
          p_vehicle->Y <<= 3;
          if (process_my_crashing_vehicle(p_vehicle))
          {
              move_flying_vehicle(p_vehicle);
              p_vehicle->Y >>= 3;
              set_passengers_location(p_vehicle);
          }
          return;
    }

    if (p_vehicle->U.UVehicle.TNode != 0)
    {
        struct Thing *p_station;

        assert(p_vehicle->SubType == SubTT_VEH_TRAIN); // only trains have stations
        assert(p_vehicle->U.UVehicle.TNode > 0); // trains stations have positive indexes
        p_station = &things[p_vehicle->U.UVehicle.TNode];
        if ((p_station->Type != TT_BUILDING) || (p_station->Flag & TngF_Destroyed) != 0)
        {
            p_vehicle->Flag |= TngF_Destroyed;
            start_crashing(p_vehicle);
            p_vehicle->U.UVehicle.ReqdSpeed = 0;
            return;
        }
    }

    switch (p_vehicle->State)
    {
    case VehSt_UNKN_12:
        if (p_vehicle->U.UVehicle.TNode != 0)
        {
            if ((p_vehicle->Flag & TngF_Unkn08000000) != 0)
                train_unkn_st18_func_1(p_vehicle);
            else
                train_unkn_st18_func_2(p_vehicle);
        }
        set_passengers_location(p_vehicle);
        move_vehicle(p_vehicle);
        break;
    case VehSt_UNKN_13:
        if (!train_unkn_st21_exit_func_1(p_vehicle))
        {
            p_vehicle->State = VehSt_UNKN_14;
            p_vehicle->U.UVehicle.Timer2 = 100;
            play_dist_sample(p_vehicle, 225 + (LbRandomAnyShort() % 4), FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 1);
        }
        break;
    case VehSt_UNKN_14:
        train_unkn_st20_func_1(p_vehicle);
        set_passengers_location(p_vehicle);
        break;
    case VehSt_UNKN_15:
        train_unkn_st21_exit_func_1(p_vehicle);
        if (p_vehicle->U.UVehicle.GotoX != 0 && things[p_vehicle->U.UVehicle.GotoX].State == VehSt_UNKN_12) {
            p_vehicle->State = VehSt_UNKN_12;
        }
        else if (p_vehicle->Owner != 0 && things[p_vehicle->Owner].State == VehSt_UNKN_12) {
            p_vehicle->State = VehSt_UNKN_12;
        }
        set_passengers_location(p_vehicle);
        break;
    default:
        LOGERR("Shagged %s %d state %d",
          vehicle_type_name(p_vehicle->SubType), (int)p_vehicle->ThingOffset,
          (int)p_vehicle->State);
        break;
    }
}

void process_vehicle_goto_point_liftoff(struct Thing *p_vehicle)
{
    int i;

    set_passengers_location(p_vehicle);
    i = alt_at_point(PRCCOORD_TO_MAPCOORD(p_vehicle->X), PRCCOORD_TO_MAPCOORD(p_vehicle->Z));
    if (p_vehicle->Y < MAPCOORD_TO_PRCCOORD(400, 0) + (i << 3)) {
        p_vehicle->Speed = 0;
        p_vehicle->Y += MAPCOORD_TO_PRCCOORD(8, 0);
    } else {
        p_vehicle->SubState = 6;
    }
}

void process_veh_ground(struct Thing *p_vehicle)
{
    int dtvel;

    dtvel = p_vehicle->U.UVehicle.ReqdSpeed - p_vehicle->Speed;
    if (dtvel != 0)
    {
        if (dtvel > 100)
            p_vehicle->Speed += 100;
        else if (dtvel < -400)
            p_vehicle->Speed -= 400;
        else
            p_vehicle->Speed = p_vehicle->U.UVehicle.ReqdSpeed;
    }

    switch (p_vehicle->State)
    {
    case VehSt_UNKN_3A:
    case VehSt_UNKN_42:
    case VehSt_UNKN_43:
        p_vehicle->Y <<= 3;
        process_my_flying_vehicle(p_vehicle);
        move_flying_vehicle(p_vehicle);
        set_passengers_location(p_vehicle);
        p_vehicle->Y >>= 3;
        break;
    case VehSt_UNKN_3E:
        process_unplacing(p_vehicle);
        move_vehicle(p_vehicle);
        set_passengers_location(p_vehicle);
        break;
    case VehSt_UNKN_41:
        p_vehicle->U.UVehicle.ReqdSpeed = 0;
        p_vehicle->U.UVehicle.AngleDY = 0;
        p_vehicle->Y <<= 3;
        process_hovering_vehicle(p_vehicle);
        move_flying_vehicle(p_vehicle);
        set_passengers_location(p_vehicle);
        p_vehicle->Y >>= 3;
        break;
    case VehSt_UNKN_45:
        p_vehicle->Y <<= 3;
        if (process_my_crashing_vehicle(p_vehicle)) {
            move_flying_vehicle(p_vehicle);
            set_passengers_location(p_vehicle);
        }
        p_vehicle->Y >>= 3;
        break;
    case VehSt_UNKN_39:
    case VehSt_UNKN_44:
        p_vehicle->Y <<= 3;
        process_my_takeoff(p_vehicle);
        move_flying_vehicle(p_vehicle);
        set_passengers_location(p_vehicle);
        p_vehicle->Y >>= 3;
        break;
    case VehSt_FLY_LANDING:
        p_vehicle->Y <<= 3;
        process_my_land_vehicle(p_vehicle);
        move_flying_vehicle(p_vehicle);
        set_passengers_location(p_vehicle);
        p_vehicle->Y >>= 3;
        break;
    case VehSt_UNKN_3C:
        // Pretend we are wandering
        p_vehicle->State = VehSt_WANDER;
        if (p_vehicle->U.UVehicle.TNode != 0)
              process_next_tnode(p_vehicle);
        move_vehicle(p_vehicle);
        set_passengers_location(p_vehicle);
        // Get back to specifics of this state
        p_vehicle->State = VehSt_UNKN_3C;
        process_stop_as_soon_as_you_can(p_vehicle);
        break;
    case VehSt_NONE:
    case VehSt_PARKED_PERPN:
    case VehSt_PARKED_PARAL:
        if (p_vehicle->SubType == SubTT_VEH_FLYING)
            process_parked_flyer(p_vehicle);
        break;
    case VehSt_UNKN_27:
        switch (p_vehicle->SubState)
        {
        case 5:
            process_vehicle_goto_point_liftoff(p_vehicle);
            move_vehicle(p_vehicle);
            set_passengers_location(p_vehicle);
            break;
        case 6:
            process_vehicle_goto_point_fly(p_vehicle);
            move_vehicle(p_vehicle);
            set_passengers_location(p_vehicle);
            break;
        case 7:
            process_vehicle_goto_point_land(p_vehicle);
            move_vehicle(p_vehicle);
            set_passengers_location(p_vehicle);
            break;
        default:
            move_vehicle(p_vehicle);
            set_passengers_location(p_vehicle);
            break;
        }
        break;
    case VehSt_UNKN_36:
        process_stopping(p_vehicle);
        move_vehicle(p_vehicle);
        set_passengers_location(p_vehicle);
        break;
    case VehSt_UNKN_38:
        process_unstopping(p_vehicle);
        move_vehicle(p_vehicle);
        set_passengers_location(p_vehicle);
        break;
    case VehSt_WANDER:
    case VehSt_GOTO_LOC:
    case VehSt_UNKN_33:
    case VehSt_UNKN_34:
        if (p_vehicle->U.UVehicle.TNode != 0)
            process_next_tnode(p_vehicle);
        move_vehicle(p_vehicle);
        set_passengers_location(p_vehicle);
        break;
    default:
        LOGERR("Unexpected %s %d state %d",
          vehicle_type_name(p_vehicle->SubType), (int)p_vehicle->ThingOffset,
          (int)p_vehicle->State);
        break;
    }

    if ((p_vehicle->SubType != SubTT_VEH_FLYING)
     && ((p_vehicle->Flag & TngF_Destroyed) == 0)) {
        set_vehicle_alt(p_vehicle);
    }
}

void vehicle_check_outside_map(struct Thing *p_vehicle)
{
    if ((p_vehicle->X <= MAPCOORD_TO_PRCCOORD(0,0))
      || (p_vehicle->X >= MAPCOORD_TO_PRCCOORD(MAP_COORD_WIDTH,0))
      || (p_vehicle->Z <= MAPCOORD_TO_PRCCOORD(0,0))
      || (p_vehicle->Z >= MAPCOORD_TO_PRCCOORD(MAP_COORD_HEIGHT,0)))
    {
        if (p_vehicle->State != VehSt_UNKN_45)
            start_crashing(p_vehicle);
    }
}

void process_vehicle(struct Thing *p_vehicle)
{
#if 0
    asm volatile ("call ASM_process_vehicle\n"
        : : "a" (p_vehicle));
    return;
#endif
    if ((p_vehicle->Flag & TngF_Destroyed) == 0)
        p_vehicle->OldTarget = 0;
    if (p_vehicle->U.UVehicle.RecoilTimer > 0)
        p_vehicle->U.UVehicle.RecoilTimer--;
    if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_Unkn0080) != 0)
    {
        p_vehicle->Health -= 16;
        if (p_vehicle->Health < 0)
            start_crashing(p_vehicle);
        if (((gameturn & 7) == 0) && (LbRandomAnyShort() & 7) == 0)
            p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_Unkn0080;
    }

    switch (p_vehicle->SubType)
    {
    case SubTT_VEH_TRAIN:
        if (p_vehicle->State == VehSt_UNKN_D)
            break;
        process_train(p_vehicle);
        break;
    case SubTT_VEH_SHUTTLE_POD:
        process_shuttle_pod(p_vehicle);
        break;
    case SubTT_VEH_GROUND:
    case SubTT_VEH_UNKN43:
        process_veh_ground(p_vehicle);
        break;
    case SubTT_VEH_FLYING:
        vehicle_check_outside_map(p_vehicle);
        process_veh_ground(p_vehicle);
        break;
    case SubTT_VEH_TANK:
        if (p_vehicle->State == VehSt_UNKN_45) {
            bang_new4(p_vehicle->X, p_vehicle->Y, p_vehicle->Z, 10);
            init_vehicle_explode(p_vehicle);
        } else if (p_vehicle->State == VehSt_PARKED_PARAL) {
            process_tank_stationary(p_vehicle);
        } else {
            process_tank(p_vehicle);
            set_passengers_location(p_vehicle);
        }
        process_tank_turret(p_vehicle);
        break;
    case SubTT_VEH_SHIP:
        process_ship(p_vehicle);
        set_passengers_location(p_vehicle);
        break;
    case SubTT_VEH_MECH:
        unkn_path_func_001(p_vehicle, 0);
        if (p_vehicle->State == VehSt_PARKED_PARAL)
            process_mech_stationary(p_vehicle);
        else
            process_mech(p_vehicle);
        process_mech_unknown1(p_vehicle);
        set_passengers_location(p_vehicle);
        break;
    default:
        break;
    }
}

void preprogress_trains_turns(ulong nturns)
{
    ulong turns;

    for (turns = nturns; turns > 0; turns--)
    {
        ThingIdx thing;
        int tng_remain;
        struct Thing *p_thing;

        thing = things_used_head;
        tng_remain = things_used;
        while (thing > 0)
        {
            if (tng_remain == 0)
                break;
            p_thing = &things[thing];
            thing = p_thing->LinkChild;
            tng_remain--;

            if ((p_thing->Type == TT_VEHICLE)
              && (p_thing->SubType == SubTT_VEH_TRAIN))
                process_vehicle(p_thing);
        }
    }
}

int vehicle_hit_by_bullet(struct Thing *p_vehicle, short hp,
  int vx, int vy, int vz, struct Thing *p_attacker, ushort type)
{
    int health_decr;

    if ((p_vehicle->Flag & TngF_Destroyed) != 0) {
        return 0;
    }

    p_vehicle->Flag |= TngF_Unkn01000000;
    if (p_vehicle->SubType == SubTT_VEH_TANK)
        hp = hp >> 1;
    if (p_vehicle->SubType == SubTT_VEH_MECH)
        hp >>= 2;

    play_dist_sample(p_vehicle, 65, FULL_VOL, EQUL_PAN, NORM_PTCH, 0, 1);

    if (type == DMG_UZI || type == DMG_MINIGUN)
        hp >>= 1;
    health_decr = hp >> 1;

    p_vehicle->Health -= health_decr;
    p_vehicle->U.UVehicle.RecoilTimer = 5;
    if (p_vehicle->Health > 0) {
        return health_decr;
    }

    p_vehicle->OldTarget = p_attacker->ThingOffset;
    if (p_vehicle->SubType == SubTT_VEH_MECH)
    {
        p_vehicle->Flag |= TngF_Destroyed;
        init_mech_explode(p_vehicle);
    }
    else
    {
        p_vehicle->Flag |= TngF_Destroyed;
        start_crashing(p_vehicle);
        play_dist_sample(p_vehicle, 95, FULL_VOL, EQUL_PAN, NORM_PTCH, 0, 1);
    }
    return p_vehicle->Health + health_decr;
}

/******************************************************************************/
