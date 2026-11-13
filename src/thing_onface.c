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
#include <stdlib.h>
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
#include "lvwalk.h"
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

#ifdef SW_FNCHK
static ushort set_thing_height_on_face_quad_asm(struct Thing *p_thing, int x, int z, short face)
{
    ushort ret;
    asm volatile (
      "call ASM_set_thing_height_on_face_quad\n"
        : "=a" (ret) : "a" (p_thing), "d" (x), "b" (z), "c" (face) : "cc", "memory");
    return ret;
}
#endif

/** Sign of a*b - c*d, computed in 64 bits. */
static int sign_cross64(s32 a, s32 b, s32 c, s32 d)
{
    s64 v;

    v = (s64)a * b - (s64)c * d;
    return (v > 0) - (v < 0);
}

/** Is the map point (in 1/256 of map coordinates) inside the quad face,
 * seen from above: 1 in triangle 0-1-2, 2 in the other half, 0 outside.
 * Remade from inside_the_quad.
 */
static int inside_the_quad(struct SingleObjectFace4 *p_face4, int x, int z)
{
    struct SingleObject *p_obj;
    struct SinglePoint *p_pt;
    int x0, z0, x1, z1, x2, z2, x3, z3;
    int px, pz;

    p_obj = &game_objects[p_face4->Object];
    p_pt = &game_object_points[(ushort)p_face4->PointNo[0]];
    x0 = p_obj->MapX + p_pt->X; z0 = p_obj->MapZ + p_pt->Z;
    p_pt = &game_object_points[(ushort)p_face4->PointNo[1]];
    x1 = p_obj->MapX + p_pt->X; z1 = p_obj->MapZ + p_pt->Z;
    p_pt = &game_object_points[(ushort)p_face4->PointNo[2]];
    x2 = p_obj->MapX + p_pt->X; z2 = p_obj->MapZ + p_pt->Z;
    p_pt = &game_object_points[(ushort)p_face4->PointNo[3]];
    x3 = p_obj->MapX + p_pt->X; z3 = p_obj->MapZ + p_pt->Z;
    px = x >> 8;
    pz = z >> 8;

    if ((sign_cross64(x1 - x0, pz - z1, z1 - z0, px - x1) >= 0) &&
      (sign_cross64(x2 - x1, pz - z2, z2 - z1, px - x2) >= 0) &&
      (sign_cross64(x0 - x2, pz - z0, z0 - z2, px - x0) >= 0))
        return 1;
    if (sign_cross64(x2 - x3, pz - z2, z2 - z3, px - x2) < 0)
        return 0;
    if (sign_cross64(x1 - x2, pz - z1, z1 - z2, px - x1) < 0)
        return 0;
    if (sign_cross64(x3 - x1, pz - z3, z3 - z1, px - x3) < 0)
        return 0;
    return 2;
}

/** 16.16 fixed point product (the middle 32 bits of the 64-bit one). */
static s32 fix_mul16(s32 a, s32 b)
{
    return (s32)(((s64)a * b) >> 16);
}

/** 16.16 fixed point quotient. */
static s32 fix_div16(s32 a, s32 b)
{
    return (s32)(((s64)a << 16) / b);
}

/** Sets the height of the thing standing at (x,z) on the plane of the quad
 * face (points 0, 1, 2).  Returns 0 outside, else 1 or 2 for the half.
 * Remade from set_thing_height_on_face_quad64.
 */
static ushort set_thing_height_on_face_quad64(struct Thing *p_thing, int x, int z, ushort face)
{
    struct SingleObjectFace4 *p_face4;
    struct SingleObject *p_obj;
    struct SinglePoint *p_pt;
    int x0, y0, z0, x1, y1, z1, x2, y2, z2;
    s32 dx1, dy1, dz1, dx2, dy2, dz2, px, pz;
    s32 den, u, v, s, y;

    if (face > next_object_face4)
        return 0;
    p_face4 = &game_object_faces4[face];
    p_obj = &game_objects[p_face4->Object];
    p_pt = &game_object_points[(ushort)p_face4->PointNo[0]];
    x0 = p_obj->MapX + p_pt->X; y0 = p_obj->OffsetY + p_pt->Y; z0 = p_obj->MapZ + p_pt->Z;
    p_pt = &game_object_points[(ushort)p_face4->PointNo[1]];
    x1 = p_obj->MapX + p_pt->X; y1 = p_obj->OffsetY + p_pt->Y; z1 = p_obj->MapZ + p_pt->Z;
    p_pt = &game_object_points[(ushort)p_face4->PointNo[2]];
    x2 = p_obj->MapX + p_pt->X; y2 = p_obj->OffsetY + p_pt->Y; z2 = p_obj->MapZ + p_pt->Z;

    dx1 = (x1 - x0) << 8;
    dy1 = (y1 - y0) << 8;
    dz1 = (z1 - z0) << 8;
    dy2 = (y2 - y0) << 8;
    dx2 = (x2 - x0) << 8;
    dz2 = (z2 - z0) << 8;
    px = x - (x0 << 8);
    pz = z - (z0 << 8);

    den = fix_mul16(dz2, dx1) - fix_mul16(dx2, dz1);
    if (den == 0)
        den = 1;
    u = fix_div16(fix_mul16(px, dz2) - fix_mul16(pz, dx2), den);
    v = fix_div16(fix_mul16(pz, dx1) - fix_mul16(px, dz1), den);
    if ((u < 0) || (u > 0x10000) || (v < 0) || (v > 0x10000))
        return 0;
    s = u + v;
    if (s > 0x20000)
        return 0;
    y = (y0 << 8) + fix_mul16(u, dy1) + fix_mul16(v, dy2);
    p_thing->Y = y >> 3;
    return (s > 0x10000) ? 2 : 1;
}

/** Remade from set_thing_height_on_face_quad. */
static ushort set_thing_height_on_face_quad_c(struct Thing *p_thing, int x, int z, short face)
{
    int inside;

    inside = inside_the_quad(&game_object_faces4[(ushort)face], x, z);
    if (inside == 0)
        return 0;
    set_thing_height_on_face_quad64(p_thing, x, z, (ushort)face);
    return inside;
}

ushort set_thing_height_on_face_quad(struct Thing *p_thing, int x, int z, short face)
{
    ushort ret;
    FNCHK_RET("set_thing_height_on_face_quad", ret,
      set_thing_height_on_face_quad_asm(p_thing, x, z, face),
      set_thing_height_on_face_quad_c(p_thing, x, z, face));
    return ret;
}

/** Is the map point (in 1/256 of map coordinates) inside the triangle
 * face, seen from above?  Remade from inside_the_triangle.
 */
static int inside_the_triangle(struct SingleObjectFace3 *p_face3, int x, int z)
{
    struct SingleObject *p_obj;
    struct SinglePoint *p_pt;
    int x0, z0, x1, z1, x2, z2;
    int px, pz;

    p_obj = &game_objects[p_face3->Object];
    p_pt = &game_object_points[(ushort)p_face3->PointNo[0]];
    x0 = p_obj->MapX + p_pt->X; z0 = p_obj->MapZ + p_pt->Z;
    p_pt = &game_object_points[(ushort)p_face3->PointNo[1]];
    x1 = p_obj->MapX + p_pt->X; z1 = p_obj->MapZ + p_pt->Z;
    p_pt = &game_object_points[(ushort)p_face3->PointNo[2]];
    x2 = p_obj->MapX + p_pt->X; z2 = p_obj->MapZ + p_pt->Z;
    px = x >> 8;
    pz = z >> 8;

    if (sign_cross64(x1 - x0, pz - z1, z1 - z0, px - x1) < 0)
        return 0;
    if (sign_cross64(x2 - x1, pz - z2, z2 - z1, px - x2) < 0)
        return 0;
    if (sign_cross64(x0 - x2, pz - z0, z0 - z2, px - x0) < 0)
        return 0;
    return 1;
}

/** Sets the height of the thing standing at (x,z) on the plane of the
 * triangle face.  Returns 0 outside (or more than 0xA00 from point 0),
 * else 1.  Remade from set_thing_height_on_face_tri64.
 */
static ushort set_thing_height_on_face_tri64(struct Thing *p_thing, int x, int z, ushort face)
{
    struct SingleObjectFace3 *p_face3;
    struct SingleObject *p_obj;
    struct SinglePoint *p_pt;
    int x0, y0, z0, x1, y1, z1, x2, y2, z2;
    s32 dx1, dy1, dz1, dx2, dy2, dz2, px, pz;
    s32 den, u, v, y;
    int adx, adz, dist;

    if (face > next_object_face3)
        return 0;
    p_face3 = &game_object_faces3[face];
    p_obj = &game_objects[p_face3->Object];
    p_pt = &game_object_points[(ushort)p_face3->PointNo[0]];
    x0 = p_obj->MapX + p_pt->X; y0 = p_obj->OffsetY + p_pt->Y; z0 = p_obj->MapZ + p_pt->Z;
    p_pt = &game_object_points[(ushort)p_face3->PointNo[1]];
    x1 = p_obj->MapX + p_pt->X; y1 = p_obj->OffsetY + p_pt->Y; z1 = p_obj->MapZ + p_pt->Z;
    p_pt = &game_object_points[(ushort)p_face3->PointNo[2]];
    x2 = p_obj->MapX + p_pt->X; y2 = p_obj->OffsetY + p_pt->Y; z2 = p_obj->MapZ + p_pt->Z;

    dx1 = (x1 - x0) << 8;
    dy1 = (y1 - y0) << 8;
    dz1 = (z1 - z0) << 8;
    dx2 = (x2 - x0) << 8;
    dy2 = (y2 - y0) << 8;
    dz2 = (z2 - z0) << 8;
    px = x - (x0 << 8);
    pz = z - (z0 << 8);

    adx = abs(px >> 8);
    adz = abs(pz >> 8);
    if (adz >= adx)
        dist = adz + (adx >> 1);
    else
        dist = adx + (adz >> 1);
    if (dist > 0xA00)
        return 0;

    den = fix_mul16(dz2, dx1) - fix_mul16(dx2, dz1);
    if (den == 0)
        den = 1;
    u = fix_div16(fix_mul16(px, dz2) - fix_mul16(pz, dx2), den);
    v = fix_div16(fix_mul16(pz, dx1) - fix_mul16(px, dz1), den);
    if ((u < 0) || (u > 0x10000) || (v < 0) || (v > 0x10000))
        return 0;
    if (u + v > 0x10000)
        return 0;
    y = (y0 << 8) + fix_mul16(u, dy1) + fix_mul16(v, dy2);
    p_thing->Y = y >> 3;
    return 1;
}

#ifdef SW_FNCHK
static ushort set_thing_height_on_face_tri_asm(struct Thing *p_thing, int x, int z, short face)
{
    ushort ret;
    asm volatile (
      "call ASM_set_thing_height_on_face_tri\n"
        : "=a" (ret) : "a" (p_thing), "d" (x), "b" (z), "c" (face) : "cc", "memory");
    return ret;
}
#endif

/** Remade from set_thing_height_on_face_tri. */
static ushort set_thing_height_on_face_tri_c(struct Thing *p_thing, int x, int z, short face)
{
    int inside;

    inside = inside_the_triangle(&game_object_faces3[(ushort)face], x, z);
    if (inside == 0)
        return 0;
    set_thing_height_on_face_tri64(p_thing, x, z, (ushort)face);
    return inside;
}

ushort set_thing_height_on_face_tri(struct Thing *p_thing, int x, int z, short face)
{
    ushort ret;
    FNCHK_RET("set_thing_height_on_face_tri", ret,
      set_thing_height_on_face_tri_asm(p_thing, x, z, face),
      set_thing_height_on_face_tri_c(p_thing, x, z, face));
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

#ifdef SW_FNCHK
static short find_and_set_connected_face_asm(struct Thing *p_thing, int x, int z, short face)
{
    short ret;
    asm volatile (
      "call ASM_find_and_set_connected_face\n"
        : "=a" (ret) : "a" (p_thing), "d" (x), "b" (z), "c" (face) : "cc", "memory");
    return ret;
}
#endif

/** Height of a thing moving from its face (OnFace) onto one of the faces
 * connected to it.  Moves of more than 4000 in height are refused.
 * Returns the new face, or 0.  Remade from find_and_set_connected_face;
 * the face argument is not used.
 */
static short find_and_set_connected_face_c(struct Thing *p_thing, int x, int z, short face)
{
    struct WalkHeader *p_walk_head;
    ushort item;
    short wface;
    s32 old_y;

    if (p_thing->U.UPerson.OnFace > 0)
        p_walk_head = &game_walk_headers[(short)game_object_faces3[p_thing->U.UPerson.OnFace].WalkHeader];
    else
        p_walk_head = &game_walk_headers[(short)game_object_faces4[-p_thing->U.UPerson.OnFace].WalkHeader];

    for (item = p_walk_head->StartItem; ; item++)
    {
        if (item >= p_walk_head->Count + p_walk_head->StartItem)
            return 0;
        wface = game_walk_items[item];
        if (wface >= 0)
        {
            struct SingleObjectFace3 *p_face3;

            p_face3 = &game_object_faces3[wface];
            if (((p_face3->GFlags & 0x04) != 0) && (inside_the_triangle(p_face3, x, z) != 0))
                break;
        }
        else
        {
            struct SingleObjectFace4 *p_face4;
            int inside;

            p_face4 = &game_object_faces4[(ushort)-wface];
            if ((p_face4->GFlags & 0x04) == 0)
                continue;
            inside = inside_the_quad(p_face4, x, z);
            if (inside == 0)
                continue;
            if (inside == 2)
                p_thing->Flag |= 0x20;
            else
                p_thing->Flag &= ~0x20;
            break;
        }
    }

    old_y = p_thing->Y;
    wface = game_walk_items[item];
    if (wface > 0)
    {
        if (inside_the_triangle(&game_object_faces3[(ushort)wface], x, z) != 0)
            set_thing_height_on_face_tri64(p_thing, x, z, (ushort)wface);
    }
    else if (wface == 0)
    {
        return wface;
    }
    else
    {
        struct SingleObjectFace4 *p_face4;
        struct SingleObject *p_obj;
        ushort face4;
        int n;

        face4 = (ushort)-wface;
        p_face4 = &game_object_faces4[face4];
        p_obj = &game_objects[p_face4->Object];
        n = face4 - p_obj->StartFace4;
        if (((n == 0x3E) || (n == 9) || (n == 0x3F)) && (p_obj->NumbFaces == 0x15) &&
          (p_obj->NumbFaces4 == 0x40) && (p_obj->EndPoint - p_obj->StartPoint == 0x53))
            p_thing->Flag2 &= ~0x20000000;
        if (inside_the_quad(p_face4, x, z) != 0)
            set_thing_height_on_face_quad64(p_thing, x, z, face4);
    }
    if (abs((int)(old_y - p_thing->Y)) > 4000) {
        p_thing->Y = old_y;
        return 0;
    }
    p_thing->U.UPerson.OnFace = wface;
    return wface;
}

short find_and_set_connected_face(struct Thing *p_thing, int x, int z, short face)
{
    short ret;
    FNCHK_RET("find_and_set_connected_face", ret,
      find_and_set_connected_face_asm(p_thing, x, z, face),
      find_and_set_connected_face_c(p_thing, x, z, face));
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
