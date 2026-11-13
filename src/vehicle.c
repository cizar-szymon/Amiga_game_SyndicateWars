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
#include "game_speed.h"
#include "matrix.h"
#include "pathtrig.h"
#include "people.h"
#include "sound.h"
#include "thing.h"
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

void mech_unkn_func_09(ThingIdx thing)
{
    asm volatile ("call ASM_mech_unkn_func_09\n"
        : : "a" (thing));
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
static int vehicle_object_radius(ushort object)
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

static int ariadne_initialise_route_asm(struct Thing *p_thing, int x, int z)
{
    int ret;
    asm volatile ("call ASM_ariadne_initialise_route\n"
        : "=a" (ret), "+d" (x), "+b" (z) : "0" (p_thing) : "cc", "memory");
    return ret;
}

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
        if (ariadne_initialise_route_asm(p_vehicle, x, z) <= 0)
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

int check_for_a_vehicle_here(int x, int z, struct Thing *p_vehicle)
{
    int ret;
    asm volatile (
      "call ASM_check_for_a_vehicle_here\n"
        : "=r" (ret) : "a" (x), "d" (z), "b" (p_vehicle));
    return ret;
}

void person_hit_by_car(struct Thing *p_person, struct Thing *p_vehicle)
{
    asm volatile ("call ASM_person_hit_by_car\n"
        : : "a" (p_person), "d" (p_vehicle));
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

void start_crashing(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_start_crashing\n"
        : : "a" (p_vehicle));
}

void start_landing(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_start_landing\n"
        : : "a" (p_vehicle));
}

void process_shuttle_pod(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_shuttle_pod\n"
        : : "a" (p_vehicle));
}

void init_vehicle_explode(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_init_vehicle_explode\n"
        : : "a" (p_vehicle));
}

void process_tank_stationary(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_tank_stationary\n"
        : : "a" (p_vehicle));
}

void process_tank(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_tank\n"
        : : "a" (p_vehicle));
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

void move_flying_vehicle(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_move_flying_vehicle\n"
        : : "a" (p_vehicle));
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

void process_ship(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_ship\n"
        : : "a" (p_vehicle));
}

void process_mech_stationary(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_mech_stationary\n"
        : : "a" (p_vehicle));
}

void process_mech(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_mech\n"
        : : "a" (p_vehicle));
}

void process_mech_unknown1(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_mech_unknown1\n"
        : : "a" (p_vehicle));
}

int process_my_crashing_vehicle(struct Thing *p_vehicle)
{
    int ret;
    asm volatile ("call ASM_process_my_crashing_vehicle\n"
        : "=r" (ret) : "a" (p_vehicle));
    return ret;
}

int train_unkn_st21_exit_func_1(struct Thing *p_vehicle)
{
    int ret;
    asm volatile ("call ASM_train_unkn_st21_exit_func_1\n"
        : "=r" (ret) : "a" (p_vehicle));
    return ret;
}

void train_unkn_st18_func_1(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_train_unkn_st18_func_1\n"
        : : "a" (p_vehicle));
}

void train_unkn_st18_func_2(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_train_unkn_st18_func_2\n"
        : : "a" (p_vehicle));
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

void train_unkn_st20_func_1(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_train_unkn_st20_func_1\n"
        : : "a" (p_vehicle));
}

void process_my_flying_vehicle(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_my_flying_vehicle\n"
        : : "a" (p_vehicle));
}

void process_unplacing(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_unplacing\n"
        : : "a" (p_vehicle));
}

void process_stop_as_soon_as_you_can(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_stop_as_soon_as_you_can\n"
        : : "a" (p_vehicle));
}

void process_hovering_vehicle(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_hovering_vehicle\n"
        : : "a" (p_vehicle));
}

void process_my_takeoff(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_my_takeoff\n"
        : : "a" (p_vehicle));
}

void process_my_land_vehicle(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_my_land_vehicle\n"
        : : "a" (p_vehicle));
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

void process_unstopping(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_unstopping\n"
        : : "a" (p_vehicle));
}

void process_stopping(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_stopping\n"
        : : "a" (p_vehicle));
}

void process_vehicle_goto_point_land(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_vehicle_goto_point_land\n"
        : : "a" (p_vehicle));
}

void process_vehicle_goto_point_fly(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_vehicle_goto_point_fly\n"
        : : "a" (p_vehicle));
}

void process_parked_flyer(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_parked_flyer\n"
        : : "a" (p_vehicle));
}

void init_mech_explode(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_init_mech_explode\n"
        : : "a" (p_vehicle));
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
