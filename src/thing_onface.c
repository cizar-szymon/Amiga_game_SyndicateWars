/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file thing_onface.c
 *     Support for checking if thing is on 3D face and putting it on a face.
 * @par Purpose:
 *     Check thing position to determine whether it is placed on a 3D face.
 *     Adjust thing properties to put it on the 3D face (triangle or quad).
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     19 Aug 2025 - 17 Jan 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "thing_onface.h"

#include <assert.h>
#include "bfendian.h"
#include "bfscreen.h"
#include "poly.h"

#include "enginpriobjs.h"
#include "enginsngobjs.h"
#include "engintrns.h"

#include "bigmap.h"
#include "game.h"
#include "packet.h"
#include "player.h"
#include "swlog.h"
#include "thing.h"
#include "fncheck.h"
/******************************************************************************/

struct Thing thing_on_face;

/******************************************************************************/

#ifdef SW_FNCHK
static ubyte check_big_point_triangle_asm(int x, int y, int ux, int uy, int vx, int vy, int wx, int wy)
{
    ubyte ret;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[4];

    stkargs[0] = (int)(intptr_t)vx;
    stkargs[1] = (int)(intptr_t)vy;
    stkargs[2] = (int)(intptr_t)wx;
    stkargs[3] = (int)(intptr_t)wy;

    asm volatile (
      "push 12(%5)\n"
      "push 8(%5)\n"
      "push 4(%5)\n"
      "push 0(%5)\n"
      "call ASM_check_big_point_triangle\n"
        : "=a" (ret)
        : "a" (x), "d" (y), "b" (ux), "c" (uy), "S" (stkargs)
        : "cc", "memory");
    return ret;
}
#endif

/** Check whether point (x,y) lies within triangle (u,v,w).
 *
 * Computes the barycentric coordinates of the point in 8.8 fixed point:
 * t along edge u->v and s along edge u->w; the point is inside if both are
 * non-negative and their sum is below 1.0 (256). Products and shifts wrap
 * at 32 bits and divisions truncate toward zero, like the original.
 */
static ubyte check_big_point_triangle_c(int x, int y, int ux, int uy, int vx, int vy, int wx, int wy)
{
    s32 dvx, dvy, dwx, dwy;
    s32 num, den, t, s;

    dvx = vx - ux;
    dvy = vy - uy;
    dwx = wx - ux;
    dwy = wy - uy;

    num = (s32)((u32)(y - uy) * (u32)dwx + (u32)(ux - x) * (u32)dwy);
    den = (s32)((u32)dvy * (u32)dwx - (u32)dvx * (u32)dwy);
    if (den == 0)
        return 0;
    t = (s32)((u32)num << 8) / den;
    if (t < 0)
        return 0;

    if (dwx != 0) {
        num = (s32)(((u32)x << 8) - ((u32)ux << 8) - (u32)dvx * (u32)t);
        s = num / dwx;
    } else {
        num = (s32)(((u32)y << 8) - ((u32)uy << 8) - (u32)dvy * (u32)t);
        s = num / dwy;
    }
    if (s < 0)
        return 0;
    if (s + t >= 256)
        return 0;
    return 1;
}

ubyte check_big_point_triangle(int x, int y, int ux, int uy, int vx, int vy, int wx, int wy)
{
    ubyte ret;
    FNCHK_PURE("check_big_point_triangle", ret,
      check_big_point_triangle_asm(x, y, ux, uy, vx, vy, wx, wy),
      check_big_point_triangle_c(x, y, ux, uy, vx, vy, wx, wy));
    return ret;
}

ushort set_thing_height_on_face_tri(struct Thing *p_thing, int x, int z, short face)
{
    ushort ret;
    asm volatile (
      "call ASM_set_thing_height_on_face_tri\n"
        : "=r" (ret) : "a" (p_thing), "d" (x), "b" (z), "c" (face));
    return ret;
}

ushort set_thing_height_on_face_quad(struct Thing *p_thing, int x, int z, short face)
{
    ushort ret;
    asm volatile (
      "call ASM_set_thing_height_on_face_quad\n"
        : "=r" (ret) : "a" (p_thing), "d" (x), "b" (z), "c" (face));
    return ret;
}

ushort set_thing_height_on_face(struct Thing *p_thing, int x, int z, short face)
{
    if (face > 0) {
        return set_thing_height_on_face_tri(p_thing, x, z, face);
    } else if (face < 0) {
        return set_thing_height_on_face_quad(p_thing, x, z, -face);
    }
    return 0;
}

short find_and_set_connected_face(struct Thing *p_thing, int x, int z, short face)
{
    short ret;
    asm volatile (
      "call ASM_find_and_set_connected_face\n"
        : "=r" (ret) : "a" (p_thing), "d" (x), "b" (z), "c" (face));
    return ret;
}

void check_mouse_over_face(struct PolyPoint *p_pt0, struct PolyPoint *p_pt1,
  struct PolyPoint *p_pt2, int face, int type)
{
#if 0
    asm volatile (
      "push %4\n"
      "call ASM_check_mouse_over_face\n"
        : : "a" (p_pt0), "d" (p_pt1), "b" (p_pt2), "c" (face), "g" (type));
    return;
#endif
    struct SingleObjectFace3 *p_face3;
    struct SingleObjectFace4 *p_face4;
    struct SingleObject *p_sobj;
    struct SinglePoint *p_point0;
    struct SinglePoint *p_point1;
    struct SinglePoint *p_point2;
    int ms_x, ms_y;

    if (pktrec_mode == PktR_PLAYBACK) {
        return;
    }

    ms_x = lbDisplay.MMouseX;
    ms_y = lbDisplay.MMouseY;

    if (!check_big_point_triangle(ms_x, ms_y, p_pt0->X, p_pt0->Y,
        p_pt1->X, p_pt1->Y, p_pt2->X, p_pt2->Y)) {
        return;
    }

    switch (type)
    {
    case 1:
        players[local_player_no].Target = face;
        p_face3 = &game_object_faces3[face];
        p_sobj = &game_objects[p_face3->Object];
        p_point2 = &game_object_points[p_face3->PointNo[0]];
        p_point0 = &game_object_points[p_face3->PointNo[2]];
        p_point1 = &game_object_points[p_face3->PointNo[1]];
        break;
    case 2:
        players[local_player_no].Target = -face;
        p_face4 = &game_object_faces4[face];
        p_sobj = &game_objects[p_face4->Object];
        p_point2 = &game_object_points[p_face4->PointNo[0]];
        p_point0 = &game_object_points[p_face4->PointNo[2]];
        p_point1 = &game_object_points[p_face4->PointNo[1]];
        break;
    case 3:
        players[local_player_no].Target = -face;
        p_face4 = &game_object_faces4[face];
        p_sobj = &game_objects[p_face4->Object];
        p_point2 = &game_object_points[p_face4->PointNo[3]];
        p_point0 = &game_object_points[p_face4->PointNo[1]];
        p_point1 = &game_object_points[p_face4->PointNo[2]];
        break;
    default:
        LOGERR("Invalid type=%d", (int)type);
        return;
    }

    players[local_player_no].TargetType = TrgTp_Unkn3;

    int ms_cor_x, ms_cor_z;
    ms_cor_x = p_sobj->MapX;
    ms_cor_z = p_sobj->MapZ;

    if (get_mapcoord_on_face_points(&ms_cor_x, &ms_cor_z, p_pt0, p_pt1, p_pt2,
          p_point0, p_point1, p_point2, ms_x, ms_y))
    {
        int prc_x, prc_z;

        prc_x = MAPCOORD_TO_PRCCOORD(ms_cor_x, 0);
        prc_z = MAPCOORD_TO_PRCCOORD(ms_cor_z, 0);
        switch (type)
        {
        case 1:
            set_thing_height_on_face_tri(&thing_on_face, prc_x, prc_z, face);
            break;
        case 2:
        case 3:
            set_thing_height_on_face_quad(&thing_on_face, prc_x, prc_z, face);
            break;
        }
        mouse_map_x = ms_cor_x;
        mouse_map_z = ms_cor_z;
    }
}

/******************************************************************************/
