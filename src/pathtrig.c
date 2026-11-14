/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file pathtrig.c
 *     Pathfinding triangulation support required by the 3D engine.
 * @par Purpose:
 *     Implement functions for handling pathfinding triangulation.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     29 Sep 2023 - 02 Jan 2024
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "pathtrig.h"

#include "bfkeybd.h"
#include "bfmath.h"
#include "bfmemory.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

#include "drawshape.h"
#include "engincolour.h"

#include "bigmap.h"
#include "building.h"
#include "enginsngobjs.h"
#include "game.h"
#include "game_data.h"
#include "hud_draw.h"
#include "lvwalk.h"
#include "thing.h"
#include "tngcolisn.h"
#include "triangls.h"
#include "tringops.h"
#include "trpoints.h"
#include "trstate.h"
#include "trfringe.h"
#include "delaunay.h"
#include "swlog.h"
#include "fncheck.h"
/******************************************************************************/
s32 Ex[100];
s32 Ey[100];
s32 ixE = 0;
s32 thin_wall_x1, thin_wall_y1;
s32 thin_wall_x2, thin_wall_y2;
s32 tree_dad[8000];
s32 tree_val[8000];
ubyte routeTags[8000];
s32 tree_Ax8;
s32 tree_Ay8;
s32 tree_Bx8;
s32 tree_By8;
s32 tree_altA;
s32 tree_altB;
s32 tree_triA;
s32 tree_triB;
s32 tree_CentreDest;
s32 tree_routelen;
s32 tree_routecost;
s32 tree_route[3000];
s32 route_bak[3000];


extern short link__MapColListEmptyHead;
extern short link__MapColVectEmptyHead;

/** Map of the ground surface for triangluation purposes, 2x2 per tile.
 */
ubyte *ground_map = NULL;

enum ThinWallType {
    THIN_BLOCK = 0,
    THIN_PASS = 1,
};

extern const int MOD3[];

void unkn_path_func_001(struct Thing *p_thing, ubyte a2)
{
    int cor_beg_x, cor_beg_z;
    ushort pt, pt_max;
    ushort path;

    if (a2 == 1)
        return;

    path = p_thing->U.UPerson.PathIndex;
    pt = p_thing->PathOffset;
    pt_max = my_paths[path].Flag;
    cor_beg_x = PRCCOORD_TO_MAPCOORD(p_thing->X);
    cor_beg_z = PRCCOORD_TO_MAPCOORD(p_thing->Z);
    while (path > 0)
    {
        int cor_end_x, cor_end_y, cor_end_z;
        int cor_beg_y;

        if (pt >= pt_max)
            break;
        cor_end_x = my_paths[path].X[pt & 3];
        cor_end_z = my_paths[path].Z[pt & 3];
        cor_beg_y = alt_at_point(cor_beg_x, cor_beg_z) >> 5;
        cor_end_y = alt_at_point(cor_end_x, cor_end_z) >> 5;

        if (lbShift == KMod_SHIFT) {
            draw_mapcoord_line_directed_to_target(cor_beg_x, cor_beg_y, cor_beg_z,
              cor_end_x, cor_end_y, cor_end_z, colour_lookup[2]);
        } else {
            draw_mapcoord_line_col(cor_beg_x, cor_beg_y, cor_beg_z,
              cor_end_x, cor_end_y, cor_end_z, colour_lookup[2]);
        }
        cor_beg_x = cor_end_x;
        cor_beg_z = cor_end_z;

        pt++;
        if ((pt & 3) == 0)
            path = my_paths[path].Next;
    }
}

//TODO temp copy of static func
static sbyte path_compare_multiplications(long mul1a, long mul1b, long mul2a, long mul2b)
{
    long long mul1,mul2;
    mul1 = (long long)mul1a * (long long)mul1b;
    mul2 = (long long)mul2a * (long long)mul2b;
    if (mul1 > mul2)
        return 1;
    if (mul1 < mul2)
        return -1;
    return 0;
}

#ifdef SW_FNCHK
static TbBool triangle_contains8_asm(int tri, int x, int y)
{
    TbBool ret;
    asm volatile ("call ASM_triangle_contains8\n"
        : "=r" (ret) : "a" (tri), "d" (x), "b" (y));
    return ret;
}
#endif

/** Difference of two 32-bit values, wrapping as the original. */
static inline s32 sub32w(s32 a, s32 b)
{
    return (s32)((u32)a - (u32)b);
}

/** Whether point (x,y) (coordinates * 256) lies inside the triangle or on
 * its edge: for each edge the 64-bit cross product must not be positive.
 */
static TbBool triangle_contains8_c(int tri, int x, int y)
{
    struct TrTriangle *p_tri;
    struct TrPoint *p_pt;
    s32 x0, y0, x1, y1, x2, y2;

    p_tri = &triangulation[0].Triangles[tri];
    p_pt = &triangulation[0].Points[p_tri->point[0]];
    x0 = (s32)((u32)p_pt->x << 8);
    y0 = (s32)((u32)p_pt->y << 8);
    p_pt = &triangulation[0].Points[p_tri->point[1]];
    x1 = (s32)((u32)p_pt->x << 8);
    y1 = (s32)((u32)p_pt->y << 8);
    p_pt = &triangulation[0].Points[p_tri->point[2]];
    x2 = (s32)((u32)p_pt->x << 8);
    y2 = (s32)((u32)p_pt->y << 8);

    if (path_compare_multiplications(sub32w(x1, x0), sub32w(y, y1),
      sub32w(y1, y0), sub32w(x, x1)) > 0)
        return false;
    if (path_compare_multiplications(sub32w(x2, x1), sub32w(y, y2),
      sub32w(y2, y1), sub32w(x, x2)) > 0)
        return false;
    if (path_compare_multiplications(sub32w(x0, x2), sub32w(y, y0),
      sub32w(y0, y2), sub32w(x, x0)) > 0)
        return false;
    return true;
}

TbBool triangle_contains8(int tri, int x, int y)
{
    TbBool ret;

    FNCHK_PURE("triangle_contains8", ret, triangle_contains8_asm(tri, x, y),
      triangle_contains8_c(tri, x, y));
    return ret;
}

#ifdef SW_FNCHK
static int pointed_at8_asm(TrFineCoord pt_x, TrFineCoord pt_y, int *r_tri, int *r_cor)
{
    int ret;
    asm volatile ("call ASM_pointed_at8\n"
        : "=r" (ret) : "a" (pt_x), "d" (pt_y), "b" (r_tri), "c" (r_cor)
        : "cc", "memory");
    return ret;
}
#endif

/** Walks around the corner `*r_cor` of triangle `*r_tri` (coordinates of
 * the corner fixed), from one triangle sharing it to the next, until the one
 * whose angle at the corner contains the direction to point (pt_x,pt_y).
 * Sets that triangle and corner and returns the next corner index, or -1
 * when the walk reaches an edge with no neighbour (the outputs unchanged).
 */
static int pointed_at8_c(TrFineCoord pt_x, TrFineCoord pt_y, int *r_tri, int *r_cor)
{
    struct TrTriangle *p_tri;
    struct TrPoint *p_pt;
    s32 cx, cy;
    int tri, cor;

    tri = *r_tri;
    cor = *r_cor;
    p_tri = &triangulation[0].Triangles[tri];
    p_pt = &triangulation[0].Points[p_tri->point[cor]];
    cx = (s32)((u32)p_pt->x << 8);
    cy = (s32)((u32)p_pt->y << 8);
    while (1)
    {
        s32 nx, ny, mx, my;
        TbBool rel_m, rel_n;
        int lnk, k;

        p_tri = &triangulation[0].Triangles[tri];
        p_pt = &triangulation[0].Points[p_tri->point[MOD3[cor+1]]];
        nx = (s32)((u32)p_pt->x << 8);
        ny = (s32)((u32)p_pt->y << 8);
        p_pt = &triangulation[0].Points[p_tri->point[MOD3[cor+2]]];
        mx = (s32)((u32)p_pt->x << 8);
        my = (s32)((u32)p_pt->y << 8);
        rel_m = path_compare_multiplications(sub32w(mx, cx), sub32w(pt_y, my),
          sub32w(my, cy), sub32w(pt_x, mx)) > 0;
        rel_n = path_compare_multiplications(sub32w(nx, cx), sub32w(pt_y, ny),
          sub32w(ny, cy), sub32w(pt_x, nx)) > 0;
        if (rel_m && !rel_n)
        {
            *r_tri = tri;
            *r_cor = cor;
            return MOD3[cor+1];
        }
        lnk = p_tri->tri[cor];
        if (lnk == -1)
            return -1;
        p_tri = &triangulation[0].Triangles[lnk];
        if (p_tri->tri[0] == tri)
            k = 0;
        else if (p_tri->tri[1] == tri)
            k = 1;
        else if (p_tri->tri[2] == tri)
            k = 2;
        else
            k = -1;
        cor = MOD3[k+1];
        tri = lnk;
    }
}

int pointed_at8(TrFineCoord pt_x, TrFineCoord pt_y, int *r_tri, int *r_cor)
{
    int ret;

    FNCHK_RET("pointed_at8", ret, pointed_at8_asm(pt_x, pt_y, r_tri, r_cor),
      pointed_at8_c(pt_x, pt_y, r_tri, r_cor));
    return ret;
}

int triangle_find8(TrFineCoord pt_x, TrFineCoord pt_y)
{
#if 0
    int ret;
    asm volatile ("call ASM_triangle_find8\n"
        : "=r" (ret) : "a" (pt_x), "d" (pt_y));
    return ret;
#else
    int tri; //TODO switch type to TrTriangId
    int remain;

    tri = triangulation[0].last_tri;

    if (!tri_is_allocated(tri))
    {
        for (tri = 0; tri < triangulation[0].ix_Triangles; tri++)
        {
            if (tri_is_allocated(tri) && triangle_contains8(tri, pt_x, pt_y))
                break;
        }
        if (tri >= triangulation[0].ix_Triangles) {
            tri = -1;
        }
        triangulation[0].last_tri = tri;
        return tri;
    }

    remain = 2000;
    while (tri >= 0)
    {
        struct TrTriangle *p_tri;
        TrFineCoord pt0_x, pt0_y;
        TrFineCoord pt1_x, pt1_y;
        TrFineCoord pt2_x, pt2_y;
        TbBool eqA, eqB, eqC;

        remain--;
        if (remain <= 0) {
            tri = -1;
            LOGERR("Cannot find (%d,%d)", pt_x, pt_y);
            break;
        }
        p_tri = &triangulation[0].Triangles[tri];
        {
            struct TrPoint *p_pt;
            p_pt = &triangulation[0].Points[p_tri->point[0]];
            pt0_x = p_pt->x << 8;
            pt0_y = p_pt->y << 8;
            p_pt = &triangulation[0].Points[p_tri->point[1]];
            pt1_x = p_pt->x << 8;
            pt1_y = p_pt->y << 8;
            p_pt = &triangulation[0].Points[p_tri->point[2]];
            pt2_x = p_pt->x << 8;
            pt2_y = p_pt->y << 8;
        }
        eqA = path_compare_multiplications(pt1_x - pt0_x, pt_y - pt1_y, pt_x - pt1_x, pt1_y - pt0_y) > 0;
        eqB = path_compare_multiplications(pt2_x - pt1_x, pt_y - pt2_y, pt_x - pt2_x, pt2_y - pt1_y) > 0;
        eqC = path_compare_multiplications(pt0_x - pt2_x, pt_y - pt0_y, pt_x - pt0_x, pt0_y - pt2_y) > 0;
        if (!eqA && !eqB && !eqC)
            break;

        if (eqA && !eqB && !eqC && p_tri->tri[0] >= 0) {
            tri = p_tri->tri[0];
        } else if (eqB && !eqC && !eqA && p_tri->tri[1] >= 0) {
            tri = p_tri->tri[1];
        } else if (eqC && !eqA && !eqB && p_tri->tri[2] >= 0) {
            tri = p_tri->tri[2];
        } else if (eqA && eqB) {
            int cor, rcor;
            cor = 1;
            rcor = pointed_at8(pt_x, pt_y, &tri, &cor);
            if (rcor < 0) tri = -1;
        } else if (eqB && eqC) {
            int cor, rcor;
            cor = 2;
            rcor = pointed_at8(pt_x, pt_y, &tri, &cor);
            if (rcor < 0) tri = -1;
        } else if (eqC && eqA) {
            int cor, rcor;
            cor = 0;
            rcor = pointed_at8(pt_x, pt_y, &tri, &cor);
            if (rcor < 0) tri = -1;
        }
    }
    if (tri >= 0 && !triangle_contains8(tri, pt_x, pt_y)) {
        tri = -1;
    }
    triangulation[0].last_tri = tri;
    return tri;
#endif
}

#ifdef SW_FNCHK
static int triangle_findSE8_asm(int x, int y)
{
    int ret;
    asm volatile ("call ASM_triangle_findSE8\n"
        : "=r" (ret) : "a" (x), "d" (y) : "cc", "memory");
    return ret;
}
#endif

/** Triangle containing point (x,y) (coordinates * 256), with points on an
 * edge or a corner resolved towards the south-east: on a corner the
 * triangle around it pointed at by (0x10100,0x10100), on an edge the
 * neighbour across it when (0x10100,0x10100) lies on its side.
 */
static int triangle_findSE8_c(int x, int y)
{
    struct TrTriangle *p_tri;
    struct TrPoint *p_pt;
    int tri, cor;

    tri = triangle_find8(x, y);
    if (tri < 0)
        return tri;
    p_tri = &triangulation[0].Triangles[tri];
    for (cor = 0; cor < 3; cor++)
    {
        p_pt = &triangulation[0].Points[p_tri->point[cor]];
        if ((s32)((u32)p_pt->x << 8) != x)
            continue;
        if ((s32)((u32)p_pt->y << 8) != y)
            continue;
        pointed_at8(0x10100, 0x10100, &tri, &cor);
        return tri;
    }
    for (cor = 0; cor < 3; cor++)
    {
        s32 px, py, qx, qy;

        p_pt = &triangulation[0].Points[p_tri->point[cor]];
        px = (s32)((u32)p_pt->x << 8);
        py = (s32)((u32)p_pt->y << 8);
        p_pt = &triangulation[0].Points[p_tri->point[MOD3[cor+1]]];
        qx = (s32)((u32)p_pt->x << 8);
        qy = (s32)((u32)p_pt->y << 8);
        if (path_compare_multiplications(sub32w(qx, px), sub32w(y, qy),
          sub32w(qy, py), sub32w(x, qx)) != 0)
            continue;
        if (path_compare_multiplications(sub32w(qx, px), sub32w(0x10100, qy),
          sub32w(qy, py), sub32w(0x10100, qx)) <= 0)
            continue;
        return p_tri->tri[cor];
    }
    return tri;
}

int triangle_findSE8(int x, int y)
{
    int ret;

    FNCHK_RET("triangle_findSE8", ret, triangle_findSE8_asm(x, y),
      triangle_findSE8_c(x, y));
    return ret;
}

void make_edge(int x1, int y1, int x2, int y2)
{
    asm volatile (
      "call ASM_make_edge\n"
        : : "a" (x1), "d" (y1), "b" (x2), "c" (y2));
}

int edge_find(int x1, int y1, int x2, int y2, int *ntri1, int *ncor1)
{
    int ret;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[2];

    stkargs[0] = (int)(intptr_t)ntri1;
    stkargs[1] = (int)(intptr_t)ncor1;

    asm volatile (
      "push 4(%5)\n"
      "push 0(%5)\n"
      "call ASM_edge_find\n"
        : "=r" (ret)
        : "a" (x1), "d" (y1), "b" (x2), "c" (y2), "S" (stkargs)
        : "cc", "memory");
    return ret;
}

#ifdef SW_FNCHK
static TbBool two4_line_intersection_asm(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4)
{
    TbBool ret;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[4];

    stkargs[0] = (int)(intptr_t)x3;
    stkargs[1] = (int)(intptr_t)y3;
    stkargs[2] = (int)(intptr_t)x4;
    stkargs[3] = (int)(intptr_t)y4;

    asm volatile (
      "push 12(%5)\n"
      "push 8(%5)\n"
      "push 4(%5)\n"
      "push 0(%5)\n"
      "call ASM_two4_line_intersection\n"
        : "=r" (ret)
        : "a" (x1), "d" (y1), "b" (x2), "c" (y2), "S" (stkargs)
        : "cc", "memory");
    return ret;
}
#endif

/** Product of two 32-bit values, wrapping as the original imul. */
static inline s32 mul32w(s32 a, s32 b)
{
    return (s32)((u32)a * (u32)b);
}

/** Segment (x1,y1)-(x2,y2) against segment (x3,y3)-(x4,y4): 0 - no
 * intersection, 1 - the segments are parallel (and their bounding boxes
 * overlap), 2 - they intersect. The original compares the bounding boxes on
 * the low 16 bits of the coordinates, products are 32-bit.
 */
static TbBool two4_line_intersection_c(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4)
{
    s32 ax, ay, bx, by, cx, cy, d, e, f;
    short lo, hi;

    ax = (s32)((u32)x2 - (u32)x1);
    bx = (s32)((u32)x3 - (u32)x4);
    if (ax < 0) {
        lo = x2; hi = x1;
    } else {
        lo = x1; hi = x2;
    }
    if (bx > 0) {
        if ((hi < (short)x4) || ((short)x3 < lo))
            return 0;
    } else {
        if ((hi < (short)x3) || ((short)x4 < lo))
            return 0;
    }
    ay = (s32)((u32)y2 - (u32)y1);
    by = (s32)((u32)y3 - (u32)y4);
    if (ay < 0) {
        hi = y1; lo = y2;
    } else {
        hi = y2; lo = y1;
    }
    if (by > 0) {
        if ((hi < (short)y4) || (lo > (short)y3))
            return 0;
    } else {
        if ((hi < (short)y3) || (lo > (short)y4))
            return 0;
    }
    cx = (s32)((u32)x1 - (u32)x3);
    cy = (s32)((u32)y1 - (u32)y3);
    d = (s32)((u32)mul32w(by, cx) - (u32)mul32w(bx, cy));
    f = (s32)((u32)mul32w(ay, bx) - (u32)mul32w(by, ax));
    if (f > 0) {
        if ((d < 0) || (d > f))
            return 0;
    } else {
        if ((d > 0) || (d < f))
            return 0;
    }
    e = (s32)((u32)mul32w(cy, ax) - (u32)mul32w(ay, cx));
    if (f > 0) {
        if ((e < 0) || (e > f))
            return 0;
    } else {
        if ((e > 0) || (e < f))
            return 0;
    }
    return (f == 0) ? 1 : 2;
}

TbBool two4_line_intersection(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4)
{
    TbBool ret;

    FNCHK_PURE("two4_line_intersection", ret,
      two4_line_intersection_asm(x1, y1, x2, y2, x3, y3, x4, y4),
      two4_line_intersection_c(x1, y1, x2, y2, x3, y3, x4, y4));
    return ret;
}

/**
 * Finds point with given coords in list of triangles.
 * @param pt_x
 * @param pt_y
 * @param r_tri Output pointer for triangle index.
 * @param r_cor Output corner/tip index within the triangle.
 * @return Gives true if a point was found and output pointers set.
 */
TbBool point_find(TrCoord pt_x, TrCoord pt_y, int *r_tri, int *r_cor)
// TODO change types to TrTriangId *r_tri, TrTipId *r_cor
{
#if 0
    int ret;
    asm volatile (
      "call ASM_point_find\n"
        : "=r" (ret) : "a" (pt_x), "d" (pt_y), "b" (r_tri), "c" (r_cor));
    return ret;
#else
    TrTriangId tri;
    TrTipId cor;

    tri = triangle_find8(pt_x << 8, pt_y << 8);
    if (tri == -1) {
        LOGERR("triangle not found at (%d,%d)", (int)pt_x, (int)pt_y);
        return false;
    }

    for (cor = 0; cor < 3; cor++)
    {
        if (triangle_tip_equals(tri, cor, pt_x, pt_y))
        {
            *r_tri = tri;
            *r_cor = cor;
            return true;
        }
    }
    return false;
#endif
}

void make_clip_list(int x1, int y1, int x2, int y2)
{
    asm volatile (
      "call ASM_make_clip_list\n"
        : : "a" (x1), "d" (y1), "b" (x2), "c" (y2));
}

void make_clipped_edges(int en1, int en2)
{
    asm volatile (
      "call ASM_make_clipped_edges\n"
        : : "a" (en1), "d" (en2));
}

void thin_wall(int x1, int y1, int x2, int y2, ubyte en1, ubyte en2)
{
#if 0
    asm volatile (
      "push %5\n"
      "push %4\n"
      "call ASM_thin_wall\n"
        : : "a" (x1), "d" (y1), "b" (x2), "c" (y2), "g" (en1), "g" (en2));
#else
    thin_wall_x1 = x1;
    thin_wall_y1 = y1;
    thin_wall_x2 = x2;
    thin_wall_y2 = y2;
    if (!insert_point(x1, y1)) {
        LOGERR("point1 (%d,%d) insertion failed", x1, y1);
        return;
    }
    if (!insert_point(x2, y2)) {
        LOGERR("point2 (%d,%d) insertion failed", x2, y2);
        return;
    }
    if (x1 != x2 || y1 != y2)
    {
        ixE = 0;
        make_clip_list(x1, y1, x2, y2);
        make_clipped_edges(en1, en2);
    }
#endif
}


#ifdef SW_FNCHK
static s32 route_to_path_asm(int ax8, int ay8, int bx8, int by8, s32 *p_treert,
  s32 treert_len, struct Path *p_path, int *p_len2)
{
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[4];
    s32 ret;

    stkargs[0] = (int)(intptr_t)p_treert;
    stkargs[1] = (int)(intptr_t)treert_len;
    stkargs[2] = (int)(intptr_t)p_path;
    stkargs[3] = (int)(intptr_t)p_len2;

    asm volatile (
      "push 12(%5)\n"
      "push 8(%5)\n"
      "push 4(%5)\n"
      "push 0(%5)\n"
      "call ASM_route_to_path\n"
        : "=r" (ret) : "a" (ax8), "d" (ay8), "b" (bx8), "c" (by8), "S" (stkargs)
        : "cc", "memory");

    return ret;
}
#endif

/** Point of a triangle corner, coordinates * 256. */
static void tri_corner_point8(struct TrTriangle *p_tri, int cor, s32 *p_x, s32 *p_y)
{
    struct TrPoint *p_pt;

    p_pt = &triangulation[0].Points[p_tri->point[cor]];
    *p_x = (s32)((u32)p_pt->x << 8);
    *p_y = (s32)((u32)p_pt->y << 8);
}

/** Ends of the edge of triangle `tri` through which the route goes on to
 * triangle `nxt` (a neighbour, or the jump triangle when the edge allows
 * entering it): (x1,y1) the end on the left, (x2,y2) on the right.
 * Returns 0, or -42 when `nxt` is not reachable from `tri`.
 * Remade from edge_points8.
 */
static int edge_points8(int tri, int nxt, s32 *p_x1, s32 *p_y1, s32 *p_x2, s32 *p_y2)
{
    struct TrTriangle *p_tri;
    int cor;

    p_tri = &triangulation[0].Triangles[tri];
    for (cor = 0; cor < 3; cor++)
    {
        if ((p_tri->tri[cor] == nxt) ||
          ((p_tri->jump == nxt) && ((p_tri->enter & (0x10 << cor)) != 0)))
        {
            tri_corner_point8(p_tri, MOD3[cor+1], p_x1, p_y1);
            tri_corner_point8(p_tri, cor, p_x2, p_y2);
            return 0;
        }
    }
    return -42;
}

/** Where point (x,y) lies against the funnel from the current point p[0..1]
 * through the left p[2..3] and right p[4..5] points: -1 - outside on the
 * left side, 1 - outside on the right side, 0 - inside.
 * Remade from fov_region.
 */
static int fov_region(s32 x, s32 y, s32 *p)
{
    if (path_compare_multiplications(sub32w(p[2], p[0]), sub32w(y, p[3]),
      sub32w(p[3], p[1]), sub32w(x, p[2])) < 0)
        return -1;
    if (path_compare_multiplications(sub32w(p[4], p[0]), sub32w(y, p[5]),
      sub32w(p[5], p[1]), sub32w(x, p[4])) > 0)
        return 1;
    return 0;
}

/** Distance between two points, as the original: squares summed in 32 bits. */
static s32 route_dist8(s32 x1, s32 y1, s32 x2, s32 y2)
{
    u32 dx, dy;

    dx = (u32)x2 - (u32)x1;
    dy = (u32)y2 - (u32)y1;
    return LbSqrL(dy * dy + dx * dx);
}

/** Turns a route through triangles into a path of way points from A to B:
 * the funnel along the crossed edges, a corner of it becomes a way point
 * where the route turns. Returns the number of way points, -1 on error
 * (an unreachable step, a loop, more than 256 points); *p_len2 gets the
 * path length.
 */
static s32 route_to_path_c(int ax8, int ay8, int bx8, int by8, s32 *p_treert,
  s32 treert_len, struct Path *p_path, int *p_len2)
{
    // current point, left and right funnel points (as fov_region() reads them)
    s32 fp[6];
    s32 nlx, nly, nrx, nry;
    int npts, i, li, ri;

    p_path->ax8 = ax8;
    p_path->ay8 = ay8;
    p_path->bx8 = bx8;
    p_path->by8 = by8;
    *p_len2 = 0;
    npts = 0;
    fp[0] = ax8;
    fp[1] = ay8;
    if (treert_len == 0)
    {
        *p_len2 += route_dist8(fp[0], fp[1], bx8, by8);
        p_path->PathLength = 1;
        p_path->WayPoints[0].x8 = bx8;
        p_path->WayPoints[0].y8 = by8;
        return 1;
    }
    li = 0;
    ri = 0;
    if (edge_points8(p_treert[0], p_treert[1], &fp[2], &fp[3], &fp[4], &fp[5]) == -42)
        return -1;
    i = 1;
    while (1)
    {
        int r1, r2;

        if (i < treert_len)
        {
            if (edge_points8(p_treert[i], p_treert[i+1], &nlx, &nly, &nrx, &nry) == -42)
                return -1;
            r1 = fov_region(nlx, nly, fp);
            r2 = fov_region(nrx, nry, fp);
        }
        else
        {
            r1 = fov_region(bx8, by8, fp);
            r2 = r1;
            if (r1 == 0)
            {
                if (npts == 256)
                    return -1;
                *p_len2 += route_dist8(fp[0], fp[1], bx8, by8);
                p_path->WayPoints[npts].x8 = bx8;
                p_path->WayPoints[npts].y8 = by8;
                npts++;
                p_path->PathLength = npts;
                return npts;
            }
            if ((npts >= 3) &&
              (p_path->WayPoints[npts-1].x8 == p_path->WayPoints[npts-3].x8) &&
              (p_path->WayPoints[npts-1].y8 == p_path->WayPoints[npts-3].y8))
                return -1;
        }
        if (r1 == 0) {
            fp[2] = nlx;
            fp[3] = nly;
            li = i;
        }
        if (r2 == 0) {
            fp[4] = nrx;
            fp[5] = nry;
            ri = i;
        }
        if (r2 == -1)
        {
            if (npts == 256)
                return -1;
            *p_len2 += route_dist8(fp[0], fp[1], fp[2], fp[3]);
            p_path->WayPoints[npts].x8 = fp[2];
            p_path->WayPoints[npts].y8 = fp[3];
            fp[0] = fp[2];
            fp[1] = fp[3];
            i = li;
            npts++;
            if (edge_points8(p_treert[i], p_treert[i+1], &fp[2], &fp[3], &fp[4], &fp[5]) == -42)
                return -1;
        }
        else if (r1 == 1)
        {
            if (npts == 256)
                return -1;
            *p_len2 += route_dist8(fp[0], fp[1], fp[4], fp[5]);
            p_path->WayPoints[npts].x8 = fp[4];
            p_path->WayPoints[npts].y8 = fp[5];
            fp[0] = fp[4];
            fp[1] = fp[5];
            i = ri;
            npts++;
            if (edge_points8(p_treert[i], p_treert[i+1], &fp[2], &fp[3], &fp[4], &fp[5]) == -42)
                return -1;
        }
        i++;
    }
}

s32 route_to_path(int ax8, int ay8, int bx8, int by8, s32 *p_treert,
  s32 treert_len, struct Path *p_path, int *p_len2)
{
    s32 ret;

    FNCHK_RET("route_to_path", ret,
      route_to_path_asm(ax8, ay8, bx8, by8, p_treert, treert_len, p_path, p_len2),
      route_to_path_c(ax8, ay8, bx8, by8, p_treert, treert_len, p_path, p_len2));
    return ret;
}

/* Route search data, in the asm data: a heap of triangles (index 1 is the
 * top, 256 the last usable place) and a spare path. */
extern s32 route_heap[257];
extern s32 route_heap_end;
extern struct Path route_fwd_path;
extern ubyte tag_current;

static inline void route_heap_swap(int a, int b)
{
    s32 t;

    t = route_heap[a];
    route_heap[a] = route_heap[b];
    route_heap[b] = t;
}

/** Puts a triangle on the route heap and moves it up, as the original:
 * at each step the smallest of the place, its sibling and their parent goes
 * to the parent. With a full heap the last place is overwritten.
 */
static void route_heap_push(s32 tri)
{
    int i;

    if (route_heap_end < 256)
        route_heap_end++;
    i = route_heap_end;
    route_heap[i] = tri;
    if (i == 1)
        return;
    if ((i & 1) == 0)
    {
        int p = i >> 1;
        if (tree_val[route_heap[i]] > tree_val[route_heap[p]])
            return;
        route_heap_swap(i, p);
        i = p;
    }
    while (i != 1)
    {
        int p, sb, m;
        s32 vi;

        p = i >> 1;
        sb = i ^ 1;
        vi = tree_val[route_heap[i]];
        if (vi < tree_val[route_heap[sb]])
            m = (vi < tree_val[route_heap[p]]) ? i : p;
        else
            m = (tree_val[route_heap[sb]] < tree_val[route_heap[p]]) ? sb : p;
        if (m == p)
            return;
        route_heap_swap(p, m);
        i = p;
    }
}

/** Removes the top of the route heap (the caller has read it): the last
 * place goes to the top and moves down, as the original.
 */
static void route_heap_pop(void)
{
    int i, end;

    end = route_heap_end;
    route_heap[1] = route_heap[end];
    route_heap_end = end - 1;
    route_heap[end] = -1;
    if (route_heap_end == 0)
        return;
    i = 1;
    while (1)
    {
        int c, sb, m;
        s32 vi;

        c = 2 * i;
        sb = c ^ 1;
        end = route_heap_end;
        if (sb > end)
        {
            if (i == end)
                return;
            if (tree_val[route_heap[i]] < tree_val[route_heap[end]])
                return;
            route_heap_swap(end, i);
            return;
        }
        vi = tree_val[route_heap[i]];
        if (vi < tree_val[route_heap[c]])
            m = (vi >= tree_val[route_heap[sb]]) ? sb : i;
        else
            m = (tree_val[route_heap[c]] >= tree_val[route_heap[sb]]) ? sb : c;
        if (m == i)
            return;
        route_heap_swap(m, i);
        i = m;
    }
}

static inline s32 abs32w(s32 a)
{
    return (a < 0) ? (s32)(0u - (u32)a) : a;
}

/** Estimated distance from a triangle to point (tree_Ax8,tree_Ay8):
 * the smallest of the octile distances from its corners.
 */
static s32 route_tri_dist_to_A(int tri)
{
    struct TrTriangle *p_tri;
    s32 ax, ay, best;
    int cor;

    ax = tree_Ax8 >> 8;
    ay = tree_Ay8 >> 8;
    p_tri = &triangulation[0].Triangles[tri];
    best = 0;
    for (cor = 0; cor < 3; cor++)
    {
        struct TrPoint *p_pt;
        s32 dx, dy, d;

        p_pt = &triangulation[0].Points[(ushort)p_tri->point[cor]];
        dx = abs32w(sub32w(ax, p_pt->x));
        dy = abs32w(sub32w(ay, p_pt->y));
        if (dx > dy)
            d = dx + (dy >> 1);
        else
            d = dy + (dx >> 1);
        if ((cor == 0) || (d < best))
            best = d;
    }
    return best;
}

/** Adds to the search the not yet visited triangles which can be entered
 * from a triangle (its neighbours and the jump triangle). */
static void route_expand_tri(s32 tri)
{
    struct TrTriangle *p_tri;
    int k;

    for (k = 0; k < 4; k++)
    {
        s32 n;

        p_tri = &triangulation[0].Triangles[tri];
        if ((p_tri->enter & (1 << k)) == 0)
            continue;
        n = (k < 3) ? p_tri->tri[k] : p_tri->jump;
        if (n == -1)
            continue;
        if (routeTags[n] == tag_current)
            continue;
        tree_val[n] = route_tri_dist_to_A(n);
        routeTags[n] = tag_current;
        tree_dad[n] = tri;
        route_heap_push(n);
    }
}

/** Best-first search of a route through triangles from triB to triA,
 * guided by the distance to point A. The route is stored from triA back to
 * triB; returns its length (number of steps) or -1.
 * Remade from triangle_route_do_unkn5 and triangle_route_do_unkn6 (the same
 * code twice in the original).
 */
static s32 triangle_route_do_unkn5(s32 triA, s32 triB, s32 *p_route)
{
    s32 cur, nxt;

    tag_current++;
    route_heap[1] = -1;
    route_heap_end = 0;
    if (tag_current == 0) {
        memset(routeTags, 0, 8000);
        tag_current = 1;
    }
    tree_val[triB] = 1;
    tree_dad[triB] = triB;
    routeTags[triB] = tag_current;
    route_heap_push(triB);

    while (1)
    {
        cur = route_heap[1];
        if ((cur == triA) || (cur == -1))
            break;
        route_heap_pop();
        // the original takes two triangles from the heap before expanding
        nxt = route_heap[1];
        if (nxt != -1)
        {
            if (nxt == triA)
                break;
            route_heap_pop();
        }
        route_expand_tri(cur);
        if (nxt != -1)
            route_expand_tri(nxt);
    }

    if (route_heap[1] == -1)
        return -1;
    {
        s32 n, count;

        n = triA;
        count = 0;
        while (1)
        {
            p_route[count] = n;
            if (n == triB)
                break;
            count++;
            n = tree_dad[n];
        }
        return count;
    }
}

#ifdef SW_FNCHK
static s32 ma_triangle_route_3_asm(int triA, int triB, s32 *p_route, s32 *p_cost)
{
    s32 ret;

    asm volatile ("call ASM_ma_triangle_route_3\n"
        : "=a" (ret), "+d" (triB), "+b" (p_route), "+c" (p_cost)
        : "a" (triA)
        : "cc", "memory");
    return ret;
}
#endif

/** Route through triangles between triangles A and B, searched both ways
 * (from A to B with the points swapped, and from B to A): the one giving
 * the shorter path is stored. Returns its length (number of steps) or -1.
 */
static s32 ma_triangle_route_3_c(int triA, int triB, s32 *p_route, s32 *p_cost)
{
    s32 t, len_bak, len_fwd;
    int cost_bak, cost_fwd;

    t = tree_Ax8; tree_Ax8 = tree_Bx8; tree_Bx8 = t;
    t = tree_Ay8; tree_Ay8 = tree_By8; tree_By8 = t;
    len_bak = triangle_route_do_unkn5(triB, triA, route_bak);
    if (len_bak == -1)
        return -1;
    if (route_to_path(tree_Ax8, tree_Ay8, tree_Bx8, tree_By8, route_bak,
      len_bak, &route_fwd_path, &cost_bak) == -1)
        return -1;
    t = tree_Ax8; tree_Ax8 = tree_Bx8; tree_Bx8 = t;
    t = tree_Ay8; tree_Ay8 = tree_By8; tree_By8 = t;
    len_fwd = triangle_route_do_unkn5(triA, triB, p_route);
    if (len_fwd == -1)
    {
        cost_fwd = 0x7FFFFFFF;
    }
    else
    {
        if (route_to_path(tree_Ax8, tree_Ay8, tree_Bx8, tree_By8, p_route,
          len_fwd, &route_fwd_path, &cost_fwd) == -1)
            return -1;
    }
    if (cost_fwd < cost_bak)
        return len_fwd;
    {
        int k;
        for (k = len_bak; k >= 0; k--)
            p_route[len_bak - k] = route_bak[k];
    }
    return len_bak;
}

s32 ma_triangle_route_3(int triA, int triB, s32 *p_route, s32 *p_cost)
{
    s32 ret;

    FNCHK_RET("ma_triangle_route_3", ret,
      ma_triangle_route_3_asm(triA, triB, p_route, p_cost),
      ma_triangle_route_3_c(triA, triB, p_route, p_cost));
    return ret;
}

/** Like triangle_route_do_unkn5, but one triangle at a time, and the search
 * goes on for up to 29 more triangles after reaching triA, which may then
 * get a better way in (else its first one is restored).
 * Remade from triangle_route_do_unkn7.
 */
static s32 triangle_route_do_unkn7(s32 triA, s32 triB, s32 *p_route)
{
    s32 cur, saved_dad;
    int counter;

    saved_dad = 0;
    counter = 0;
    tag_current++;
    route_heap[1] = -1;
    route_heap_end = 0;
    if (tag_current == 0) {
        memset(routeTags, 0, 8000);
        tag_current = 1;
    }
    tree_val[triB] = 1;
    tree_dad[triB] = triB;
    routeTags[triB] = tag_current;
    route_heap_push(triB);

    while (1)
    {
        cur = route_heap[1];
        if (cur == -1)
            break;
        if (cur == triA)
        {
            if (counter != 0)
                break;
            saved_dad = tree_dad[triA];
            routeTags[triA] = (ubyte)(tag_current + 1);
            counter = 30;
            route_heap_pop();
            continue;
        }
        if (counter > 1)
        {
            counter--;
            if (counter == 1) {
                tree_dad[triA] = saved_dad;
                break;
            }
        }
        route_heap_pop();
        route_expand_tri(cur);
    }

    if (route_heap[1] == -1)
        return -1;
    {
        s32 n, count;

        n = triA;
        count = 0;
        while (1)
        {
            p_route[count] = n;
            if (n == triB)
                break;
            count++;
            n = tree_dad[n];
        }
        return count;
    }
}

#ifdef SW_FNCHK
static s32 ma_triangle_route_4_asm(int triA, int triB, s32 *p_treert)
{
    s32 ret;
    asm volatile ("call ASM_ma_triangle_route_4\n"
        : "=a" (ret), "+d" (triB), "+b" (p_treert) : "a" (triA) : "cc", "memory");
    return ret;
}
#endif

/** Route through triangles from triangle A to B, searched backwards from B
 * with the points swapped.  Returns its length (number of steps) or -1.
 * Remade from ma_triangle_route_4.
 */
static s32 ma_triangle_route_4_c(int triA, int triB, s32 *p_treert)
{
    s32 t, len;
    int cost;

    t = tree_Ax8; tree_Ax8 = tree_Bx8; tree_Bx8 = t;
    t = tree_Ay8; tree_Ay8 = tree_By8; tree_By8 = t;
    len = triangle_route_do_unkn7(triB, triA, route_bak);
    if (len == -1)
        return -1;
    if (route_to_path(tree_Ax8, tree_Ay8, tree_Bx8, tree_By8, route_bak,
      len, &route_fwd_path, &cost) == -1)
        return -1;
    t = tree_Ax8; tree_Ax8 = tree_Bx8; tree_Bx8 = t;
    t = tree_Ay8; tree_Ay8 = tree_By8; tree_By8 = t;
    {
        int k;
        for (k = len; k >= 0; k--)
            p_treert[len - k] = route_bak[k];
    }
    return len;
}

s32 ma_triangle_route_4(int triA, int triB, s32 *p_treert)
{
    s32 ret;
    FNCHK_RET("ma_triangle_route_4", ret, ma_triangle_route_4_asm(triA, triB, p_treert),
      ma_triangle_route_4_c(triA, triB, p_treert));
    return ret;
}

/** The `solid` byte of a triangle, sign extended, as the original reads it;
 * the original does this also for triangle -1 (the bytes before the array).
 */
static s32 tri_solid_sx(int tri)
{
    sbyte *p = (sbyte *)triangulation[0].Triangles + (s32)((u32)tri << 4);
    return p[offsetof(struct TrTriangle, solid)];
}

#ifdef SW_FNCHK
static int path_init8_thing_asm(struct Path *p_path, int ax8, int ay8, int bx8, int by8, int a6)
{
    int stkargs[2];
    int ret;

    stkargs[0] = by8;
    stkargs[1] = a6;
    asm volatile (
      "push 4(%5)\n"
      "push 0(%5)\n"
      "call ASM_path_init8_thing\n"
        : "=a" (ret), "+d" (ax8), "+b" (ay8), "+c" (bx8)
        : "a" (p_path), "S" (stkargs)
        : "cc", "memory");
    return ret;
}

static int tri_path_init8_asm(int triA, int triB, struct Path *p_path, int ax8,
  int ay8, int bx8, int by8, int a8)
{
    int stkargs[4];
    int ret;

    stkargs[0] = ay8;
    stkargs[1] = bx8;
    stkargs[2] = by8;
    stkargs[3] = a8;
    asm volatile (
      "push 12(%5)\n"
      "push 8(%5)\n"
      "push 4(%5)\n"
      "push 0(%5)\n"
      "call ASM_tri_path_init8\n"
        : "=a" (ret), "+d" (triB), "+b" (p_path), "+c" (ax8)
        : "a" (triA), "S" (stkargs)
        : "cc", "memory");
    return ret;
}
#endif

/** Path from point A to point B (coordinates * 256) for a thing: triangles
 * of both points, a route between them and its way points.
 * Returns the number of way points or -1.
 */
static int path_init8_thing_c(struct Path *p_path, int ax8, int ay8, int bx8, int by8, int a6)
{
    s32 triB;
    int len2;

    navi_onscreen_debug(0);
    tree_Ax8 = ax8;
    tree_Ay8 = ay8;
    tree_Bx8 = bx8;
    tree_By8 = by8;
    p_path->PathLength = 0;
    p_path->ax8 = ax8;
    p_path->ay8 = ay8;
    p_path->bx8 = bx8;
    p_path->by8 = by8;
    tree_routelen = -1;
    tree_triA = triangle_findSE8(ax8, ay8);
    triB = triangle_findSE8(bx8, by8);
    if (tree_triA < 0)
        return -1;
    tree_altA = tri_solid_sx(tree_triA);
    tree_altB = tri_solid_sx(triB);
    tree_triB = triB;
    if (triB == -1)
        return -1;
    tree_routelen = ma_triangle_route_3(tree_triA, triB, tree_route, &tree_routecost);
    if (tree_routelen == -1)
        return -1;
    p_path->PathLength = route_to_path(ax8, ay8, bx8, by8, tree_route,
      tree_routelen, p_path, &len2);
    return p_path->PathLength;
}

/** Path from point A to point B (coordinates * 256) with known triangles of
 * the points (-1 - not known, found from the point).
 * Returns the number of way points or -1.
 */
static int tri_path_init8_c(int triA, int triB, struct Path *p_path, int ax8,
  int ay8, int bx8, int by8, int a8)
{
    s32 tA, tB;
    int len2;

    navi_onscreen_debug(0);
    tree_Ax8 = ax8;
    tree_Ay8 = ay8;
    tree_Bx8 = bx8;
    tree_By8 = by8;
    p_path->ax8 = ax8;
    p_path->ay8 = ay8;
    p_path->bx8 = bx8;
    p_path->by8 = by8;
    p_path->PathLength = 0;
    tree_routelen = -1;
    tA = (short)triA;
    if (tA == -1)
        tA = triangle_findSE8(ax8, ay8);
    tree_triA = tA;
    tB = (short)triB;
    if (tB == -1)
    {
        tB = triangle_findSE8(bx8, by8);
        if (tB == -1)
            return -1;
    }
    tree_altA = tri_solid_sx(tA);
    tree_altB = tri_solid_sx(tB);
    tree_triB = tB;
    // with no triangle of A the original returns what is left in its register
    if (tA == -1)
        return tree_altB;
    tree_routelen = ma_triangle_route_3(tA, tB, tree_route, &tree_routecost);
    if (tree_routelen == -1)
        return -1;
    p_path->PathLength = route_to_path(ax8, ay8, bx8, by8, tree_route,
      tree_routelen, p_path, &len2);
    return p_path->PathLength;
}

int path_init8_thing(struct Path *p_path, int ax8, int ay8, int bx8, int by8, int a6)
{
    int ret;

    FNCHK_RET("path_init8_thing", ret,
      path_init8_thing_asm(p_path, ax8, ay8, bx8, by8, a6),
      path_init8_thing_c(p_path, ax8, ay8, bx8, by8, a6));
    return ret;
}

/** Best-first search of a route through triangles from triB to triA, like
 * triangle_route_do_unkn5 but through the three side neighbours only, one
 * triangle at a time. Triangles with solid flag 8 are not left (forward
 * search) or not entered (`check_next`, the backward search).
 * Remade from triangle_route_do_given and triangle_route_do_bak.
 */
static s32 triangle_route_do_wide(s32 triA, s32 triB, s32 *p_route, TbBool check_next)
{
    s32 cur;

    tag_current++;
    route_heap[1] = -1;
    route_heap_end = 0;
    if (tag_current == 0) {
        memset(routeTags, 0, 8000);
        tag_current = 1;
    }
    tree_val[triB] = 1;
    tree_dad[triB] = triB;
    routeTags[triB] = tag_current;
    route_heap_push(triB);

    while (1)
    {
        int k;

        cur = route_heap[1];
        if ((cur == triA) || (cur == -1))
            break;
        route_heap_pop();
        for (k = 0; k < 3; k++)
        {
            struct TrTriangle *p_tri;
            s32 n;

            p_tri = &triangulation[0].Triangles[cur];
            n = p_tri->tri[k];
            if (n == -1)
                continue;
            if (routeTags[n] == tag_current)
                continue;
            if (check_next)
                p_tri = &triangulation[0].Triangles[n];
            if ((p_tri->solid & 0x08) != 0)
                continue;
            tree_val[n] = route_tri_dist_to_A(n);
            routeTags[n] = tag_current;
            tree_dad[n] = cur;
            route_heap_push(n);
        }
    }

    if (route_heap[1] == -1)
        return -1;
    {
        s32 n, count;

        n = triA;
        count = 0;
        while (1)
        {
            p_route[count] = n;
            if (n == triB)
                break;
            count++;
            n = tree_dad[n];
        }
        return count;
    }
}

/** Route through triangles between A and B searched both ways, the one
 * with the shorter path kept. Unlike ma_triangle_route_3, a failed search
 * either way fails (with points A and B left swapped if the second one
 * fails, as in the original).  Remade from ma_triangle_route. */
static s32 ma_triangle_route(s32 triA, s32 triB, s32 *p_route)
{
    s32 t, len_fwd, len_bak;
    int cost_fwd, cost_bak;

    len_fwd = triangle_route_do_wide(triA, triB, p_route, false);
    if (len_fwd == -1)
        return -1;
    if (route_to_path(tree_Ax8, tree_Ay8, tree_Bx8, tree_By8, p_route,
      len_fwd, &route_fwd_path, &cost_fwd) == -1)
        return -1;
    t = tree_Ax8; tree_Ax8 = tree_Bx8; tree_Bx8 = t;
    t = tree_Ay8; tree_Ay8 = tree_By8; tree_By8 = t;
    len_bak = triangle_route_do_wide(triB, triA, route_bak, true);
    if (len_bak == -1)
        return -1;
    if (route_to_path(tree_Ax8, tree_Ay8, tree_Bx8, tree_By8, route_bak,
      len_bak, &route_fwd_path, &cost_bak) == -1)
        return -1;
    t = tree_Ax8; tree_Ax8 = tree_Bx8; tree_Bx8 = t;
    t = tree_Ay8; tree_Ay8 = tree_By8; tree_By8 = t;
    if (cost_fwd < cost_bak)
        return len_fwd;
    {
        int k;
        for (k = len_bak; k >= 0; k--)
            p_route[len_bak - k] = route_bak[k];
    }
    return len_bak;
}

/** Path for a flyer through the triangulation selected by the caller,
 * between two points; PathLength stays 0 when there is none.
 * Remade from path_init8_wide. */
void path_init8_wide(struct Path *p_path, s32 ax8, s32 ay8, s32 bx8, s32 by8)
{
    int len2;

    navi_onscreen_debug(0);
    tree_Ax8 = ax8;
    p_path->PathLength = 0;
    tree_Ay8 = ay8;
    p_path->ax8 = ax8;
    tree_Bx8 = bx8;
    p_path->ay8 = ay8;
    tree_By8 = by8;
    p_path->bx8 = bx8;
    p_path->by8 = by8;
    tree_routelen = -1;
    tree_triA = triangle_findSE8(ax8, ay8);
    tree_triB = triangle_findSE8(bx8, by8);
    if ((tree_triA < 0) || (tree_triB < 0))
        return;
    tree_altA = tri_solid_sx(tree_triA);
    tree_altB = tri_solid_sx(tree_triB);
    tree_routelen = ma_triangle_route(tree_triA, tree_triB, tree_route);
    if (tree_routelen == -1)
        return;
    p_path->PathLength = route_to_path(ax8, ay8, bx8, by8, tree_route,
      tree_routelen, p_path, &len2);
}

int tri_path_init8(int triA, int triB, struct Path *p_path, int ax8,
  int ay8, int bx8, int by8, int a8)
{
    int ret;

    FNCHK_RET("tri_path_init8", ret,
      tri_path_init8_asm(triA, triB, p_path, ax8, ay8, bx8, by8, a8),
      tri_path_init8_c(triA, triB, p_path, ax8, ay8, bx8, by8, a8));
    return ret;
}

/** Octile length of a vector, as the original: the longer component and
 * half of the shorter one. */
static s32 oct_len(s32 x, s32 y)
{
    s32 ax, ay;

    ax = abs32w(x);
    ay = abs32w(y);
    if (ay >= ax)
        return ay + (ax >> 1);
    return ax + (ay >> 1);
}

/** Vector scaled to length 256 (octile), unchanged when it has no length. */
static void oct_unit(s32 *p_x, s32 *p_y)
{
    s32 len;

    len = oct_len(*p_x, *p_y);
    if (len == 0)
        return;
    *p_x = (s32)((u32)*p_x << 8) / len;
    *p_y = (s32)((u32)*p_y << 8) / len;
}

#ifdef SW_FNCHK
static void fatter_than_nothing_asm(struct Path *p_src, struct Path *p_dst, int fat)
{
    asm volatile ("call ASM_fatter_than_nothing\n"
        : "+a" (p_src), "+d" (p_dst), "+b" (fat)
        :
        : "cc", "memory");
}
#endif

/** Copies a path moving its way points away from the corners by `fat`
 * (the path of a wider walker): the inner points go along the bisector of
 * the turn, a sharp turn gets an extra point.
 */
static void fatter_than_nothing_c(struct Path *p_src, struct Path *p_dst, int fat)
{
    s32 prev_x, prev_y;
    int k, n;

    p_dst->ax8 = p_src->ax8;
    p_dst->ay8 = p_src->ay8;
    p_dst->bx8 = p_src->bx8;
    p_dst->by8 = p_src->by8;
    p_dst->PathLength = p_src->PathLength;
    prev_x = p_src->ax8;
    prev_y = p_src->ay8;
    n = 0;
    for (k = 0; k < p_src->PathLength - 1; k++)
    {
        struct WayPoint *p_wp, *p_nx;
        s32 u1x, u1y, u2x, u2y, dot, cross, len;

        p_wp = &p_src->WayPoints[k];
        p_nx = &p_src->WayPoints[k+1];
        u1x = sub32w(p_wp->x8, prev_x);
        u1y = sub32w(p_wp->y8, prev_y);
        u2x = sub32w(p_nx->x8, p_wp->x8);
        u2y = sub32w(p_nx->y8, p_wp->y8);
        len = oct_len(u1x, u1y);
        if (len == 0)
            len = 1;
        u1x = (s32)((u32)u1x << 8) / len;
        u1y = (s32)((u32)u1y << 8) / len;
        len = oct_len(u2x, u2y);
        if (len == 0)
            len = 1;
        u2x = (s32)((u32)u2x << 8) / len;
        u2y = (s32)((u32)u2y << 8) / len;
        dot = (s32)((u32)mul32w(u1x, u2x) + (u32)mul32w(u1y, u2y));
        cross = (s32)((u32)mul32w(u1x, u2y) - (u32)mul32w(u1y, u2x));

        if (dot < -20000)
        {
            s32 a, b;

            // a turn back: two points, around the corner
            if (cross > 0) {
                a = sub32w(-u2y, u2x);
                b = sub32w(u2x, u2y);
            } else {
                a = sub32w(u1x, u1y);
                b = (s32)((u32)u1x + (u32)u1y);
            }
            oct_unit(&a, &b);
            a = mul32w(a, fat);
            b = mul32w(b, fat);
            p_dst->WayPoints[n].x8 = (s32)((u32)p_wp->x8 + (u32)a);
            p_dst->WayPoints[n].y8 = (s32)((u32)p_wp->y8 + (u32)b);
            n++;
            p_dst->PathLength++;
            if (cross < 0) {
                a = sub32w(-u2y, u2x);
                b = sub32w(u2x, u2y);
            } else {
                a = sub32w(u1x, u1y);
                b = (s32)((u32)u1y + (u32)u1x);
            }
            oct_unit(&a, &b);
            a = mul32w(a, fat);
            b = mul32w(b, fat);
            p_dst->WayPoints[n].x8 = (s32)((u32)p_wp->x8 + (u32)a);
            p_dst->WayPoints[n].y8 = (s32)((u32)p_wp->y8 + (u32)b);
        }
        else
        {
            s32 a, b, nx, ny;
            struct MyMapElement *p_mapel;

            a = sub32w(u1x, u2x);
            b = sub32w(u1y, u2y);
            oct_unit(&a, &b);
            a = mul32w(a, fat);
            b = mul32w(b, fat);
            nx = (s32)((u32)p_wp->x8 + (u32)a);
            ny = (s32)((u32)p_wp->y8 + (u32)b);
            p_mapel = &game_my_big_map[((ny >> 16) << 7) + (nx >> 16)];
            if ((p_mapel->Flags2 & 0x04) != 0) {
                // half the distance; the original adds the x offset to y too
                a >>= 1;
                p_dst->WayPoints[n].x8 = (s32)((u32)p_wp->x8 + (u32)a);
                p_dst->WayPoints[n].y8 = (s32)((u32)p_wp->y8 + (u32)a);
            } else {
                p_dst->WayPoints[n].x8 = nx;
                p_dst->WayPoints[n].y8 = ny;
            }
        }
        prev_x = p_wp->x8;
        prev_y = p_wp->y8;
        n++;
    }
    p_dst->WayPoints[n].x8 = p_src->WayPoints[k].x8;
    p_dst->WayPoints[n].y8 = p_src->WayPoints[k].y8;
}

void fatter_than_nothing(struct Path *p_src, struct Path *p_dst, int fat)
{
    FNCHK_VOID("fatter_than_nothing", fatter_than_nothing_asm(p_src, p_dst, fat),
      fatter_than_nothing_c(p_src, p_dst, fat));
}

void path_init8_unkn3(struct Path *p_path, int ax8, int ay8, int bx8, int by8, int a6)
{
#if 0
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[2];

    stkargs[0] = (int)(intptr_t)by8;
    stkargs[1] = (int)(intptr_t)a6;

    asm volatile (
      "push 4(%4)\n"
      "push 0(%4)\n"
      "call ASM_path_init8_unkn3\n"
        : : "a" (p_path), "d" (ax8), "b" (ay8), "c" (bx8), "S" (stkargs)
        : "cc", "memory");
    return;
#endif
    navi_onscreen_debug(0);

    p_path->PathLength = 0;
    p_path->ax8 = ax8;
    p_path->ay8 = ay8;
    p_path->by8 = by8;
    p_path->bx8 = bx8;

    tree_Ax8 = ax8;
    tree_Ay8 = ay8;
    tree_Bx8 = bx8;
    tree_By8 = by8;
    tree_routelen = -1;

    tree_triA = triangle_findSE8(ax8, ay8);
    if (tree_triA != -1) {
        struct TrTriangle *p_tri;

        p_tri = &triangulation[0].Triangles[tree_triA];
        tree_altA = p_tri->solid;
    } else {
        tree_altA = 0;
    }

    tree_triB = triangle_findSE8(bx8, by8);
    if (tree_triB != -1) {
        struct TrTriangle *p_tri;

        p_tri = &triangulation[0].Triangles[tree_triB];
        tree_altB = p_tri->solid;
    } else {
        tree_altB = 0;
    }

    if ((tree_triA != -1) && (tree_triB != -1))
    {
        s32 len2;

        tree_routelen = ma_triangle_route_4(tree_triA, tree_triB, tree_route);
        if (tree_routelen != -1) {
            p_path->PathLength = route_to_path(ax8, ay8, bx8, by8,
              tree_route, tree_routelen, p_path, &len2);
        }
    }
}

int unused_func_115(int X1, int Z1, int X2, int Z2)
{
    int ret;
    asm volatile ("call ASM_unused_func_115\n"
        : "=r" (ret) : "a" (X1), "d" (Z1), "b" (X2), "c" (Z2));
    return ret;
}

short collide_coords(short *p_X, short *p_Y, short *p_Z)
{
    short ret;
    asm volatile ("call ASM_collide_coords\n"
        : "=r" (ret) : "a" (p_X), "d" (p_Y), "b" (p_Z));
    return ret;
}

void thin_wall_at_line(int X1, int Y1, int Z1, int X2, int Y2, int Z2, short face, ushort colt, ubyte pass)
{
    short cX1, cY1, cZ1;
    short cX2, cY2, cZ2;

    if (!unused_func_115(X1, Z1, X2, Z2))
        return;

    if (X1 < 0 || X1 > 32768)
        return;
    if (X2 < 0 || X2 > 32768)
        return;
    if (Z1 < 0 || Z1 > 32768)
        return;
    if (Z2 < 0 || Z2 > 32768)
        return;

    cX1 = X1;
    cY1 = Y1;
    cZ1 = Z1;

    cX2 = X2;
    cY2 = Y2;
    cZ2 = Z2;

    if (colt != 2)
    {
        collide_coords(&cX1, &cY1, &cZ1);
        collide_coords(&cX2, &cY2, &cZ2);
    }
    thin_wall(cX1, cZ1, cX2, cZ2, pass, pass);
}

/* Needed only for ASM - remove pending.
*/
void thin_wall_at_line_rm(int X1, int Y1, int Z1, int X2, int Y2, int Z2, short face, ushort colt)
{
    thin_wall_at_line(X1, Y1, Z1, X2, Y2, Z2, face, colt, THIN_PASS);
}

static ubyte face_is_blocking(ushort obj, short face, ushort colt)
{
    if (face < 0)
    {
        struct SingleObjectFace4 *p_face;
        p_face = &game_object_faces4[-face];
        if ((p_face->GFlags & FGFlg_Unkn10) != 0)
            return THIN_PASS;
        return THIN_BLOCK;
    }
    else if (face > 0)
    {
        struct SingleObjectFace3 *p_face;
        p_face = &game_object_faces3[face];
        if ((p_face->GFlags & FGFlg_Unkn10) != 0)
            return THIN_PASS;
        return THIN_BLOCK;
    }
    return THIN_PASS;
}

#define TOLERANCE 150

void thin_wall_around_face3(short obj_x, short obj_y, short obj_z, short face, ushort colt, ubyte pass)
{
    struct SingleObjectFace3 *p_face;
    int alt_cor[4];
    int x_cor[4];
    int y_cor[4];
    int z_cor[4];
    short cor;

    p_face = &game_object_faces3[face];

    for (cor = 0; cor < 3; cor++) {
        struct SinglePoint *p_pt;
        p_pt = &game_object_points[p_face->PointNo[cor]];
        x_cor[cor] = obj_x + p_pt->X;
        y_cor[cor] = obj_y + p_pt->Y;
        z_cor[cor] = obj_z + p_pt->Z;
    }
    for (cor = 0; cor < 3; cor++) {
        int alt;
        alt = alt_at_point(x_cor[cor], z_cor[cor]);
        alt_cor[cor] = PRCCOORD_TO_YCOORD(alt);
    }
    if (alt_cor[0] - TOLERANCE < y_cor[0] && alt_cor[0] + TOLERANCE > y_cor[0]
      && alt_cor[1] - TOLERANCE < y_cor[1] && alt_cor[1] + TOLERANCE > y_cor[1]) {
        thin_wall_at_line(x_cor[0], y_cor[0], z_cor[0],
          x_cor[1], y_cor[1], z_cor[1], face, colt, pass);
    }
    if (alt_cor[0] - TOLERANCE < y_cor[0] && alt_cor[0] + TOLERANCE > y_cor[0]
      && alt_cor[2] - TOLERANCE < y_cor[2] && alt_cor[2] + TOLERANCE > y_cor[2]) {
        thin_wall_at_line(x_cor[0], y_cor[0], z_cor[0],
          x_cor[2], y_cor[2], z_cor[2], face, colt, pass);
    }
    if (alt_cor[1] - TOLERANCE < y_cor[1] && alt_cor[1] + TOLERANCE > y_cor[1]
      && alt_cor[2] - TOLERANCE < y_cor[2] && alt_cor[2] + TOLERANCE > y_cor[2]) {
        thin_wall_at_line(x_cor[1], y_cor[1], z_cor[1],
          x_cor[2], y_cor[2], z_cor[2], face, colt, pass);
    }
}

void thin_wall_around_face4(short obj_x, short obj_y, short obj_z, short face, ushort colt, ubyte pass)
{
    struct SingleObjectFace4 *p_face;
    int alt_cor[4];
    int x_cor[4];
    int y_cor[4];
    int z_cor[4];
    short cor;

    p_face = &game_object_faces4[face];

    for (cor = 0; cor < 4; cor++) {
        struct SinglePoint *p_pt;
        p_pt = &game_object_points[p_face->PointNo[cor]];
        x_cor[cor] = obj_x + p_pt->X;
        y_cor[cor] = obj_y + p_pt->Y;
        z_cor[cor] = obj_z + p_pt->Z;
    }
    for (cor = 0; cor < 4; cor++) {
        int alt;
        alt = alt_at_point(x_cor[cor], z_cor[cor]);
        alt_cor[cor] = PRCCOORD_TO_YCOORD(alt);
    }
    if (alt_cor[0] - TOLERANCE < y_cor[0] && alt_cor[0] + TOLERANCE > y_cor[0]
      && alt_cor[1] - TOLERANCE < y_cor[1] && alt_cor[1] + TOLERANCE > y_cor[1]) {
        thin_wall_at_line(x_cor[0], y_cor[0], z_cor[0],
          x_cor[1], y_cor[1], z_cor[1], -face, colt, pass);
    }
    if (alt_cor[1] - TOLERANCE < y_cor[1] && alt_cor[1] + TOLERANCE > y_cor[1]
      && alt_cor[3] - TOLERANCE < y_cor[3] && alt_cor[3] + TOLERANCE > y_cor[3]) {
        thin_wall_at_line(x_cor[1], y_cor[1], z_cor[1],
          x_cor[3], y_cor[3], z_cor[3], -face, colt, pass);
    }
    if (alt_cor[3] - TOLERANCE < y_cor[3] && alt_cor[3] + TOLERANCE > y_cor[3]
      && alt_cor[2] - TOLERANCE < y_cor[2] && alt_cor[2] + TOLERANCE > y_cor[2]) {
        thin_wall_at_line(x_cor[3], y_cor[3], z_cor[3],
          x_cor[2], y_cor[2], z_cor[2], -face, colt, pass);
    }
    if (alt_cor[2] - TOLERANCE < y_cor[2] && alt_cor[2] + TOLERANCE > y_cor[2]
      && alt_cor[0] - TOLERANCE < y_cor[0] && alt_cor[0] + TOLERANCE > y_cor[0]) {
        thin_wall_at_line(x_cor[2], y_cor[2], z_cor[2],
          x_cor[0], y_cor[0], z_cor[0], -face, colt, pass);
    }
}

#undef TOLERANCE

void thin_wall_around_object(ushort obj, ushort colt)
{
    short obj_x, obj_y, obj_z;
    short startface3, endface3;
    short startface4, endface4;
    short face;

    {
        struct SingleObject *p_obj;

        p_obj = &game_objects[obj];
        obj_x = p_obj->MapX;
        obj_y = p_obj->OffsetY;
        obj_z = p_obj->MapZ;
        startface3 = p_obj->StartFace;
        endface3 = startface3 + p_obj->NumbFaces;
        startface4 = p_obj->StartFace4;
        endface4 = startface4 + p_obj->NumbFaces4;
    }
    for (face = startface3; face < endface3; face++)
    {
        ubyte pass;
        pass = face_is_blocking(obj, face, colt);
        thin_wall_around_face3(obj_x, obj_y, obj_z, face, colt, pass);
    }
    for (face = startface4; face < endface4; face++)
    {
        ubyte pass;
        pass = face_is_blocking(obj, -face, colt);
        thin_wall_around_face4(obj_x, obj_y, obj_z, face, colt, pass);
    }
}

void thin_wall_around_object_rm(ushort obj, ushort colt)
{
#if 0
    asm volatile (
      "call ASM_thin_wall_around_object_rm\n"
        : : "a" (obj), "d" (colt));
#else
    short obj_x, obj_y, obj_z;
    short startface3, endface3;
    short startface4, endface4;
    short face;

    {
        struct SingleObject *p_obj;

        p_obj = &game_objects[obj];
        obj_x = p_obj->MapX;
        obj_y = p_obj->OffsetY;
        obj_z = p_obj->MapZ;
        startface3 = p_obj->StartFace;
        endface3 = startface3 + p_obj->NumbFaces;
        startface4 = p_obj->StartFace4;
        endface4 = startface4 + p_obj->NumbFaces4;
    }
    for (face = startface3; face < endface3; face++)
    {
        thin_wall_around_face3(obj_x, obj_y, obj_z, face, colt, THIN_PASS);
    }
    for (face = startface4; face < endface4; face++)
    {
        thin_wall_around_face4(obj_x, obj_y, obj_z, face, colt, THIN_PASS);
    }
#endif
}

void brute_fill_rectangle(int x1, int y1, int x2, int y2, ubyte solid)
{
#if 0
    asm volatile (
      "push %4\n"
      "call ASM_brute_fill_rectangle\n"
        : : "a" (x1), "d" (y1), "b" (x2), "c" (y2), "g" (solid));
#else
    int tri;

    for (tri = 0; tri < triangulation[0].ix_Triangles; tri++)
    {
        struct TrTriangle *p_tri;

        if (!tri_is_allocated(tri))
            continue;

        if (!triangle_contained_within_rect_coords(tri, x1, y1, x2, y2))
            continue;

        p_tri = &triangulation[0].Triangles[tri];
        p_tri->solid = solid;
    }
#endif
}

void fill_rectangle(int x1, int y1, int x2, int y2, ubyte solid)
{
#if 0
    asm volatile (
      "push %4\n"
      "call ASM_fill_rectangle\n"
        : : "a" (x1), "d" (y1), "b" (x2), "c" (y2), "g" (solid));
#else
    int tri1, tri2, tri3, tri4, ucor;
    int area_r, area_t;

    area_r = (y2 - y1) * 2 * (x2 - x1);
    area_t = 0;

    edge_find(x1, y1, x1, y2, &tri1, &ucor);
    {
        struct TrTriangle *p_tri;
        p_tri = &triangulation[0].Triangles[tri1];
        p_tri->solid = solid;
        area_t += triangle_area1(tri1);
    }
    if (area_t == area_r)
        return;

    edge_find(x2, y2, x2, y1, &tri2, &ucor);
    if (tri2 != tri1)
    {
        struct TrTriangle *p_tri;
        p_tri = &triangulation[0].Triangles[tri2];
        p_tri->solid = solid;
        area_t += triangle_area1(tri2);
    }
    if (area_t == area_r)
        return;

    edge_find(x2, y1, x1, y1, &tri3, &ucor);
    if (tri3 != tri1 && tri3 != tri2)
    {
        struct TrTriangle *p_tri;
        p_tri = &triangulation[0].Triangles[tri3];
        p_tri->solid = solid;
        area_t += triangle_area1(tri3);
    }
    if (area_t == area_r)
        return;

    edge_find(x1, y2, x2, y2, &tri4, &ucor);
    if (tri4 != tri1 && tri4 != tri2 && tri4 != tri3)
    {
        struct TrTriangle *p_tri;
        p_tri = &triangulation[0].Triangles[tri4];
        p_tri->solid = solid;
        area_t += triangle_area1(tri4);
    }
    if (area_t == area_r)
        return;

    brute_fill_rectangle(x1, y1, x2, y2, solid);
#endif
}

void tri_set_rectangle(int x1, int y1, int x2, int y2, ubyte solid)
{
#if 0
    asm volatile (
      "push %4\n"
      "call ASM_tri_set_rectangle\n"
        : : "a" (x1), "d" (y1), "b" (x2), "c" (y2), "g" (solid));
#else
    int sx1, sy1, sx2, sy2;

    sx1 = x1;
    sy1 = y1;
    sx2 = x2;
    sy2 = y2;
    if (x1 > x2) {
        sx1 = x2;
        sx2 = x1;
    }
    if (y1 > y2) {
        sy1 = y2;
        sy2 = y1;
    }
    insert_point(sx1, sy1);
    insert_point(sx2, sy1);
    make_edge(sx1, sy1, sx2, sy1);
    insert_point(sx2, sy2);
    make_edge(sx2, sy1, sx2, sy2);
    insert_point(sx1, sy2);
    make_edge(sx2, sy2, sx1, sy2);
    make_edge(sx1, sy2, sx1, sy1);
    fill_rectangle(sx1, sy1, sx2, sy2, solid);
#endif
}

void triangulate_set_rectangle(int x1, int y1, int x2, int y2, ubyte solid)
{
    tri_set_rectangle(x1, y1, x2, y2, solid);
}

void triangulation_initxy(int x1, int y1, int x2, int y2)
{
#if 0
    asm volatile (
      "call ASM_triangulation_initxy\n"
        : : "a" (x1), "d" (y1), "b" (x2), "c" (y2));
#else
    int tri, pt;
    struct TrTriangle *p_tri;
    struct TrPoint *p_point;

    triangulation[0].tri_initialised = 1;

    for (tri = 0; tri < triangulation[0].max_Triangles; tri++)
    {
        tri_clear(tri);
    }
    triangulation[0].free_Triangles = -1;

    for (pt = 0; pt < triangulation[0].max_Points; pt++)
    {
        point_clear(pt);
    }
    triangulation[0].free_Points = -1;

    p_point = &triangulation[0].Points[0];
    p_point->x = x1;
    p_point->y = y1;
    p_point = &triangulation[0].Points[1];
    p_point->x = x2;
    p_point->y = y1;
    p_point = &triangulation[0].Points[2];
    p_point->x = x2;
    p_point->y = y2;
    p_point = &triangulation[0].Points[3];
    p_point->x = x1;
    p_point->y = y2;
    triangulation[0].ix_Points = 4;
    triangulation[0].count_Points = 4;

    p_tri = &triangulation[0].Triangles[0];
    p_tri->point[0] = 3;
    p_tri->point[1] = 1;
    p_tri->point[2] = 0;
    p_tri->tri[0] = 1;
    p_tri->tri[1] = -1;
    p_tri->tri[2] = -1;
    p_tri->solid = 0;
    p_tri->enter = 7;
    p_tri = &triangulation[0].Triangles[1];
    p_tri->point[0] = 1;
    p_tri->point[1] = 3;
    p_tri->point[2] = 2;
    p_tri->tri[0] = 0;
    p_tri->tri[1] = -1;
    p_tri->tri[2] = -1;
    p_tri->solid = 0;
    p_tri->enter = 7;
    triangulation[0].ix_Triangles = 2;
    triangulation[0].count_Triangles = 2;
#endif
}

void triangulation_init(void)
{
    int dim_lo, dim_hi;
    dim_lo = -4096;
    dim_hi = 32768+4096+512;

    triangulation[0].triangle_top = triangulation[0].max_Triangles;
    triangulation[0].point_top = triangulation[0].max_Points;
    triangulation_initxy(dim_lo, dim_lo, dim_hi, dim_hi);
    triangulation[0].last_tri = -1;
}

void triangulation_init_edges(void)
{
#if 0
    asm volatile ("call ASM_triangulation_init_edges\n"
        :  :  : "eax" );
#else
    int dim_lo, dim_hi;
    dim_lo = -4096+256;
    dim_hi = 32768+4096+256;

    insert_point(dim_lo, dim_lo);
    insert_point(dim_hi, dim_lo);
    insert_point(dim_lo, dim_hi);
    insert_point(dim_hi, dim_hi);
    make_edge(dim_lo, dim_lo, dim_hi, dim_lo);
    make_edge(dim_hi, dim_lo, dim_hi, dim_hi);
    make_edge(dim_hi, dim_hi, dim_lo, dim_hi);
    make_edge(dim_lo, dim_hi, dim_lo, dim_lo);
#endif
}

void triangulation_allocate(int tgnNo, int maxTrigs)
{
    asm volatile (
      "call ASM_triangulation_allocate\n"
        : : "a" (tgnNo), "d" (maxTrigs));
}

void triangulate_area(ubyte *p_map, int x1, int y1, int x2, int y2)
{
#if 0
    asm volatile (
      "push %4\n"
      "call ASM_triangulate_area\n"
        : : "a" (p_map), "d" (x1), "b" (y1), "c" (x2), "g" (y2));
#else
    ubyte solid;

    if (!triangulation[0].tri_initialised)
    {
        triangulation[0].tri_initialised = 1;
        triangulation_init();
    }
    tri_set_rectangle(x1 << 7, y1 << 7, x2 << 7, y2 << 7, 0);
    fringe_init(p_map, x1, y1, x2, y2);

    while (fringe_get_rectangle(&x1, &y1, &x2, &y2, &solid))
    {
        if (solid == 0)
            continue;
        tri_set_rectangle(x1 << 7, y1 << 7, x2 << 7, y2 << 7, solid);
    }
#endif
}

void triangulate_map(ubyte *p_map)
{
#if 0
    asm volatile (
      "call ASM_triangulate_map\n"
        : : "a" (p_map));
#else
    triangulate_area(p_map, 0, 0, 256, 256);
#endif
}

void triangulation_clear(void)
{
    triangulation_init();
    triangulation_init_edges();
}

void init_col_vects_linked_list(void)
{
    int i, limit;

    link__MapColListEmptyHead = 0;
    link__MapColVectEmptyHead = 0;

    // TODO why this amount?
    limit = 1000;//get_memory_ptr_allocated_count((void **)&game_col_vects_list);
    for (i = 0; i < limit; i++)
    {
        struct ColVectList *p_cvlist;

        p_cvlist = &game_col_vects_list[next_vects_list + i];
        p_cvlist->NextColList = link__MapColListEmptyHead;
        link__MapColListEmptyHead = i + next_vects_list;
    }

    // TODO why this amount?
    limit = 600;//get_memory_ptr_allocated_count((void **)&game_col_vects);
    for (i = 0; i < limit; i++)
    {
        struct ColVect *p_colvect;

        p_colvect = &game_col_vects[next_col_vect + i];
        p_colvect->Face = link__MapColVectEmptyHead;
        link__MapColVectEmptyHead = i + next_col_vect;
    }
}

void init_collision_vects(void)
{
    ushort tile_x, tile_z;
    int i, limit;

    limit = get_memory_ptr_allocated_count((void **)&game_col_vects_list);
    for (i = 0; i < limit; i++) {
        struct ColVectList *p_cvlist;

        p_cvlist = &game_col_vects_list[i];
        p_cvlist->Vect = 0;
    }
    next_col_vect = 1;
    next_vects_list = 1;
    for (tile_x = 0; tile_x < MAP_TILE_WIDTH; tile_x++)
    {
        for (tile_z = 0; tile_z < MAP_TILE_HEIGHT; tile_z++)
        {
            struct MyMapElement *p_mapel;

            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];
            p_mapel->ColHead = 0;
        }
    }
}

void reset_things_col_vect_range(void)
{
    struct Thing *p_thing;
    ThingIdx thing;
    ushort vl;
    short i;
    ushort count;

    thing = get_thing_same_type_head(TT_BUILDING, -1);
    for (i = 0; thing > 0; thing = p_thing->LinkSame, i++)
    {
        if (i >= THINGS_LIMIT) {
            LOGERR("Infinite loop in same type things list");
            break;
        }
        p_thing = &things[thing];
        p_thing->U.UObject.BuildStartVect = 0;
        p_thing->U.UObject.BuildNumbVect = 0;
    }

    thing = 0;
    count = 0;
    for (vl = 0; vl < next_vects_list; vl++)
    {
        struct ColVectList *p_cvlist;

        p_cvlist = &game_col_vects_list[vl];
        if (p_cvlist->Object == thing)
        {
            count++;
            continue;
        }
        if (thing > 0)
        {
            struct Thing *p_thing;
            p_thing = &things[thing];
            if (p_thing->Type == TT_BUILDING)
            {
                if (p_thing->SubType == SubTT_BLD_DOME) {
                    // Dome has collision vectors spread evenly around; we want
                    // to make toggleable only a little above half of them,
                    // as only half of the dome is being opened
                    count = count * 5 / 9;
                } else {
                    // Other building types have no toggleable collision vectors
                    count = 0;
                }
                p_thing->U.UObject.BuildStartVect = vl - count;
                p_thing->U.UObject.BuildNumbVect = count;
            }
        }
        thing = p_cvlist->Object;
        count = 1;
    }
    { // Setting properties in the last thing after we've reeached end of vects_list
        if (thing > 0)
        {
            struct Thing *p_thing;
            p_thing = &things[thing];
            if (p_thing->Type == TT_BUILDING) {
                p_thing->U.UObject.BuildStartVect = vl - count;
                p_thing->U.UObject.BuildNumbVect = count;
            }
        }
    }
}

/** Adds given walk face to a list of walk items of another face.
 *
 * Can only be called continously for one face, until another face gats it walk head created.
 */
TbBool face_has_walk_item(short face, short walk_face)
{
    ushort wlkhead;

    if (face < 0)
    {
        struct SingleObjectFace4 *p_face;
        p_face = &game_object_faces4[-face];
        wlkhead = p_face->WalkHeader;
    }
    else if (face > 0)
    {
        struct SingleObjectFace3 *p_face;
        p_face = &game_object_faces3[face];
        wlkhead = p_face->WalkHeader;
    } else
    {
        return false;
    }

    return walk_face_is_in_list(wlkhead, walk_face);
}

/** Adds given walk face to a list of walk items of another face.
 *
 * Can only be called continously for one face, until another face gets its walk head created.
 */
void add_face_walk_item(short face, short walk_face)
{
    ushort wlkhead;

    if (face_has_walk_item(face, walk_face)) {
        return;
    }

    if (face < 0)
    {
        struct SingleObjectFace4 *p_face;
        p_face = &game_object_faces4[-face];
        wlkhead = p_face->WalkHeader;
    }
    else if (face > 0)
    {
        struct SingleObjectFace3 *p_face;
        p_face = &game_object_faces3[face];
        wlkhead = p_face->WalkHeader;
    } else
    {
        LOGERR("no walk head assigned to face %d", (int)face);
        return;
    }

    add_walk_face_to_list(wlkhead, walk_face);
}

int face_to_object_position(short face, short *x, short *y, short *z)
{
    if (face < 0)
    {
        struct SingleObjectFace4 *p_face;
        struct SingleObject *p_obj;

        p_face = &game_object_faces4[-face];
        p_obj = &game_objects[p_face->Object];
        *x = p_obj->MapX;
        *y = p_obj->OffsetY;
        *z = p_obj->MapZ;
        return 4;
    }
    else if (face > 0)
    {
        struct SingleObjectFace3 *p_face;
        struct SingleObject *p_obj;

        p_face = &game_object_faces3[face];
        p_obj = &game_objects[p_face->Object];
        *x = p_obj->MapX;
        *y = p_obj->OffsetY;
        *z = p_obj->MapZ;
        return 3;
    }
    return 0;
}

int add_walk_items_for_face_object(short face, short obj)
{
    short fcobj_x, fcobj_y, fcobj_z;
    short gvobj_x, gvobj_y, gvobj_z;
    short num_points;
    short cface;
    short startface3, endface3;
    short startface4, endface4;
    short cor;

    num_points = face_to_object_position(face, &fcobj_x, &fcobj_y, &fcobj_z);
    {
        struct SingleObject *p_obj;

        p_obj = &game_objects[obj];
        gvobj_x = p_obj->MapX;
        gvobj_y = p_obj->OffsetY;
        gvobj_z = p_obj->MapZ;
        startface3 = p_obj->StartFace;
        endface3 = startface3 + p_obj->NumbFaces;
        startface4 = p_obj->StartFace4;
        endface4 = startface4 + p_obj->NumbFaces4;
    }

    for (cor = 0; cor < num_points; cor++)
    {
        short fcpt_x, fcpt_y, fcpt_z;

        if (face < 0)
        {
            struct SingleObjectFace4 *p_face;
            struct SinglePoint *p_pt;

            p_face = &game_object_faces4[-face];
            p_pt = &game_object_points[p_face->PointNo[cor]];
            fcpt_x = fcobj_x + p_pt->X;
            fcpt_y = fcobj_y + p_pt->Y;
            fcpt_z = fcobj_z + p_pt->Z;
        }
        else if (face > 0)
        {
            struct SingleObjectFace3 *p_face;
            struct SinglePoint *p_pt;

            p_face = &game_object_faces3[face];
            p_pt = &game_object_points[p_face->PointNo[cor]];
            fcpt_x = fcobj_x + p_pt->X;
            fcpt_y = fcobj_y + p_pt->Y;
            fcpt_z = fcobj_z + p_pt->Z;
        }
        else
        {
            LOGERR("walk on face zero");
            fcpt_x = fcobj_x;
            fcpt_y = fcobj_y;
            fcpt_z = fcobj_z;
        }

        for (cface = startface3; cface < endface3; cface++)
        {
            struct SingleObjectFace3 *p_face;
            short ccor;
            p_face = &game_object_faces3[cface];
            if ((p_face->GFlags & FGFlg_Unkn04) == 0) {
                continue;
            }
            if (cface == face) {
                continue;
            }
            if (game_normals[p_face->FaceNormal].NY == 0) {
                continue;
            }
            for (ccor = 0; ccor < 3; ccor++)
            {
                struct SinglePoint *p_pt;
                int delta_x, delta_y, delta_z;

                p_pt = &game_object_points[p_face->PointNo[ccor]];
                delta_x = (gvobj_x + p_pt->X) - fcpt_x;
                delta_y = (gvobj_y + p_pt->Y) - fcpt_y;
                delta_z = (gvobj_z + p_pt->Z) - fcpt_z;

                if (delta_z * delta_z + delta_y * delta_y + delta_x * delta_x < 1900)
                {
                    add_face_walk_item(face, cface);
                    break;
                }
            }
        }

        for (cface = startface4; cface < endface4; cface++)
        {
            struct SingleObjectFace4 *p_face;
            short ccor;

            if (-cface == face) {
                continue;
            }
            p_face = &game_object_faces4[cface];
            if ((p_face->GFlags & FGFlg_Unkn04) == 0) {
                continue;
            }
            if (game_normals[p_face->FaceNormal].NY == 0) {
                continue;
            }
            for (ccor = 0; ccor < 4; ccor++)
            {
                struct SinglePoint *p_pt;
                int delta_x, delta_y, delta_z;

                p_pt = &game_object_points[p_face->PointNo[ccor]];
                delta_x = (gvobj_x + p_pt->X) - fcpt_x;
                delta_y = (gvobj_y + p_pt->Y) - fcpt_y;
                delta_z = (gvobj_z + p_pt->Z) - fcpt_z;

                if (delta_z * delta_z + delta_y * delta_y + delta_x * delta_x < 1900)
                {
                    add_face_walk_item(face, -cface);
                    break;
                }
            }
        }
    }
    return 0;
}

void add_walk_items_for_face_thing_objects(short face, struct Thing *p_thing)
{
    ushort beg_obj, end_obj;
    ushort obj;

    beg_obj = p_thing->U.UObject.Object;
    end_obj = beg_obj + p_thing->U.UObject.NumbObjects;
    for (obj = beg_obj; obj < end_obj; obj++) {
        add_walk_items_for_face_object(face, obj);
    }
}

void add_walk_items_for_face_things_near(short x, short y, short z, short radius, short face)
{
    short shift_x, shift_z;
    short range;

    range = MAPCOORD_TO_TILE(radius + 127);

    for (shift_x = -range; shift_x <= range; shift_x++)
    {
        for (shift_z = -range; shift_z <= range; shift_z++)
        {
            int tile_x, tile_z;
            struct MyMapElement *p_mapel;
            ThingIdx thing;
            int i;

            tile_x = MAPCOORD_TO_TILE(x) + shift_x;
            tile_z = MAPCOORD_TO_TILE(z) + shift_z;
            if (tile_x < 0 || tile_x >= MAP_TILE_WIDTH)
                continue;
            if (tile_z < 0 || tile_z >= MAP_TILE_HEIGHT)
                continue;
            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];
            thing = p_mapel->Child;
            for (i = 0; thing != 0 && i < MAX_THINGS_ON_TILE; i++)
            {
                if (thing <= 0) {
                    struct SimpleThing *p_sthing;
                    p_sthing = &sthings[thing];
                    thing = p_sthing->Next;
                } else {
                    struct Thing *p_thing;
                    p_thing = &things[thing];
                    if (p_thing->Type == TT_BUILDING)
                        add_walk_items_for_face_thing_objects(face, p_thing);
                    thing = p_thing->Next;
                }
            }
        }
    }
}

/** Reserves WalkHeader and assigns it to given face.
 *
 * The WalkHeader is initialized to have 0 items starting at given item index.
 */
TbBool prepare_face_for_having_walk_items(short face)
{
    ushort wlkhead;

    wlkhead = create_walk_list();

    if (face > 0)
    {
        struct SingleObjectFace3 *p_face;
        p_face = &game_object_faces3[face];
        p_face->WalkHeader = wlkhead;
    }
    else if (face < 0)
    {
        struct SingleObjectFace4 *p_face;
        p_face = &game_object_faces4[-face];
        p_face->WalkHeader = wlkhead;
    } else
    {
        destroy_walk_list(wlkhead);
        return false;
    }
    return true;
}

void add_walk_items_for_face(short face)
{
    short obj_x, obj_y, obj_z;

    prepare_face_for_having_walk_items(face);
    face_to_object_position(face, &obj_x, &obj_y, &obj_z);
    add_walk_items_for_face_things_near(obj_x, obj_y, obj_z, 31 << 8, face);
}

void generate_walk_items(void)
{
    short face;

    reset_all_walk_lists();

    for (face = 1; face < next_object_face3; face++)
    {
        if ((game_object_faces3[face].GFlags & FGFlg_Unkn04) != 0)
            add_walk_items_for_face(face);
    }
    for (face = 1; face < next_object_face4; face++)
    {
        if ((game_object_faces4[face].GFlags & FGFlg_Unkn04) != 0)
            add_walk_items_for_face(-face);
    }
}

#define FACE_SWEEP_STEPS 256

/** Function to find the divisor of N closest to the target
 */
int divisor_of_a_number_not_greater_than_given_target(int N, int target)
{
    int closest = -1;
    int diff = INT_MAX;

    // Iterate till square root of N
    for (int i = 1; i <= LbSqrL(N); i++)
    {
        if (N % i == 0)
        {
            // Check if divisors are equal
            if (N / i == i) {
                // Check if i is the closest
                if (abs(target - i) < diff) {
                    diff = abs(target - i);
                    closest = i;
                }
            } else {
                // Check if i is the closest
                if (abs(target - i) < diff) {
                    diff = abs(target - i);
                    closest = i;
                }
                // Check if n / i is the closest
                if (abs(target - N / i) < diff) {
                    diff = abs(target - N / i);
                    closest = N / i;
                }
            }
        }
    }
    return closest;
}

void update_mapel_collision_columns_around_triangle(short fcobj_x, short fcobj_y, short fcobj_z,
  struct SinglePoint *p_pt0, struct SinglePoint *p_pt1, struct SinglePoint *p_pt2, ushort flags)
{
    int dist_B, dist_A;
    int incr_B, incr_A;
    int sh_A, sh_B;
    int basept_x, basept_y, basept_z;
    int delta1_x, delta1_y, delta1_z;
    int delta2_x, delta2_y, delta2_z;

    basept_x = fcobj_x + p_pt0->X;
    basept_y = fcobj_y + p_pt0->Y;
    basept_z = fcobj_z + p_pt0->Z;
    delta1_x = p_pt1->X - p_pt0->X;
    delta1_y = p_pt1->Y - p_pt0->Y;
    delta1_z = p_pt1->Z - p_pt0->Z;
    delta2_x = p_pt2->X - p_pt0->X;
    delta2_y = p_pt2->Y - p_pt0->Y;
    delta2_z = p_pt2->Z - p_pt0->Z;
    // Distance between pt0 and pt1, in 1/4 of a tile
    dist_A = LbSqrL(delta1_x * delta1_x + delta1_y * delta1_y + delta1_z * delta1_z) >> 6;
    if (dist_A < 1)
        dist_A = 1;
    incr_A = divisor_of_a_number_not_greater_than_given_target(FACE_SWEEP_STEPS, FACE_SWEEP_STEPS / dist_A);
    // Distance between pt0 and pt2, in 1/4 of a tile
    dist_B = LbSqrL(delta2_x * delta2_x + delta2_y * delta2_y + delta2_z * delta2_z) >> 6;
    if (dist_B < 1)
        dist_B = 1;
    incr_B = divisor_of_a_number_not_greater_than_given_target(FACE_SWEEP_STEPS, FACE_SWEEP_STEPS / dist_B);
    if ((incr_A <= 0) || (incr_B <= 0)) {
        LOGERR("bad increment");
        return;
    }

    for (sh_A = 0; sh_A <= FACE_SWEEP_STEPS; sh_A += incr_A)
    {
        for (sh_B = 0; sh_B <= FACE_SWEEP_STEPS; sh_B += incr_B)
        {
            struct MyMapElement *p_mapel;
            int ccx, ccy, ccz;
            short tile_x, tile_z;
            ushort qb;

            ccx = basept_x + (delta2_x * sh_B + delta1_x * sh_A) / FACE_SWEEP_STEPS;
            ccy = basept_y + (delta2_y * sh_B + delta1_y * sh_A) / FACE_SWEEP_STEPS;
            ccz = basept_z + (delta2_z * sh_B + delta1_z * sh_A) / FACE_SWEEP_STEPS;
            tile_x = MAPCOORD_TO_TILE(ccx);
            tile_z = MAPCOORD_TO_TILE(ccz);
            if (tile_x < 1 || tile_x >= MAP_TILE_WIDTH)
                continue;
            if (tile_z < 1 || tile_z >= MAP_TILE_HEIGHT)
                continue;

            p_mapel = &game_my_big_map[tile_x + tile_z * MAP_TILE_WIDTH];
            if ((p_mapel->Alt - 20 <= ccy) && flags) {
                p_mapel->Texture |= 0x8000;
            }
            qb = map_coord_to_collision_qbit_index(ccx, ccz);
            set_mapel_col_columns(p_mapel, (ccy - p_mapel->Alt) >> 7, qb);
        }
    }
}

void update_mapel_collision_columns_around_face(short face, ushort flags)
{
    struct SinglePoint *p_pt0;
    struct SinglePoint *p_pt1;
    struct SinglePoint *p_pt2;
    short fcobj_x, fcobj_y, fcobj_z;
    short num_points;
    short tri;

    num_points = face_to_object_position(face, &fcobj_x, &fcobj_y, &fcobj_z);

    // Do it once for triangular, twice for quad surface
    for (tri = 0; tri < num_points - 2; tri++)
    {
        if (face <= 0)
        {
            struct SingleObjectFace4 *p_face;

            p_face = &game_object_faces4[-face];
            p_pt0 = &game_object_points[p_face->PointNo[0]];
            p_pt1 = &game_object_points[p_face->PointNo[tri+1]];
            p_pt2 = &game_object_points[p_face->PointNo[tri+2]];
        }
        else
        {
            struct SingleObjectFace3 *p_face;

            p_face = &game_object_faces3[face];
            p_pt0 = &game_object_points[p_face->PointNo[0]];
            p_pt1 = &game_object_points[p_face->PointNo[1]];
            p_pt2 = &game_object_points[p_face->PointNo[2]];
        }

        update_mapel_collision_columns_around_triangle(fcobj_x, fcobj_y, fcobj_z,
          p_pt0, p_pt1, p_pt2, flags);
    }
}

void update_mapel_collision_columns_around_object(ushort obj, ushort flags)
{
    short startface3, endface3;
    short startface4, endface4;
    short face;

    {
        struct SingleObject *p_obj;

        p_obj = &game_objects[obj];
        startface3 = p_obj->StartFace;
        endface3 = startface3 + p_obj->NumbFaces;
        startface4 = p_obj->StartFace4;
        endface4 = startface4 + p_obj->NumbFaces4;
    }
    for (face = startface3; face < endface3; face++) {
        update_mapel_collision_columns_around_face(face, flags);
    }
    for (face = startface4; face < endface4; face++) {
        update_mapel_collision_columns_around_face(-face, flags);
    }
}

void update_mapel_collision_columns_around_thing_objects(struct Thing *p_thing, ushort flags)
{
    ushort beg_obj, end_obj;
    ushort obj;

    beg_obj = p_thing->U.UObject.Object;
    end_obj = beg_obj + p_thing->U.UObject.NumbObjects;
    for (obj = beg_obj; obj < end_obj; obj++) {
        update_mapel_collision_columns_around_object(obj, flags);
    }
}

void clear_mapel_collision_columns(void)
{
    ushort tile_x, tile_z;

    next_col_column = 1;
    for (tile_x = 0; tile_x < MAP_TILE_WIDTH; tile_x++)
    {
        for (tile_z = 0; tile_z < MAP_TILE_HEIGHT; tile_z++)
        {
            struct MyMapElement *p_mapel;

            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];
            p_mapel->ColumnHead = 0;
        }
    }
}

void update_mapel_collision_columns(void)
{
    ushort tile_x, tile_z;

    clear_mapel_collision_columns();

    for (tile_x = 0; tile_x < MAP_TILE_WIDTH; tile_x++)
    {
        for (tile_z = 0; tile_z < MAP_TILE_HEIGHT; tile_z++)
        {
            struct MyMapElement *p_mapel;
            ThingIdx thing;
            int i;

            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];
            thing = p_mapel->Child;
            for (i = 0; thing != 0 && i < MAX_THINGS_ON_TILE; i++)
            {
                if (thing <= 0) {
                    struct SimpleThing *p_sthing;
                    p_sthing = &sthings[thing];
                    thing = p_sthing->Next;
                } else {
                    struct Thing *p_thing;
                    p_thing = &things[thing];
                    if (p_thing->Type == TT_BUILDING)
                        update_mapel_collision_columns_around_thing_objects(p_thing, 0);
                    thing = p_thing->Next;
                }
            }
        }
    }
}

void add_next_col_vect_to_vects_list(short x, short z, ThingIdx thing, short face, ushort vect, ubyte flags)
{
    short tile_x, tile_z;
    struct MyMapElement *p_mapel;
    struct ColVectList *p_cvlist;

    tile_x = MAPCOORD_TO_TILE(x);
    tile_z = MAPCOORD_TO_TILE(z);
    if (tile_x < 0 || tile_x >= MAP_TILE_WIDTH) {
        return;
    }
    if (tile_z < 0 || tile_z >= MAP_TILE_HEIGHT) {
        return;
    }

    p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];
    p_cvlist = &game_col_vects_list[p_mapel->ColHead];

    if (vect != p_cvlist->Vect)
    {
        ushort next_vl;

        next_vl = next_vects_list;
        next_vects_list++;
        p_cvlist = &game_col_vects_list[next_vl];
        p_cvlist->Vect = vect;
        p_cvlist->Object = thing;
        p_cvlist->NextColList = p_mapel->ColHead;
        p_mapel->ColHead = next_vl;
        if (flags & 0x01)
        {
            if (face != 0)
                p_mapel->Texture |= 0x8000;
        }
    }
}

int new_col_vect(short x1, short y1, short z1, short x2, short y2, short z2, short face)
{
    struct ColVect *p_colvect;
    int vect;
    int limit;

    limit = get_memory_ptr_allocated_count((void **)&game_col_vects);
    if (next_col_vect >= limit) {
        LOGERR("Reached limit of col_vect items");
        return 0;
    }

    vect = next_col_vect;
    next_col_vect++;
    p_colvect = &game_col_vects[vect];

    p_colvect->X1 = x1;
    p_colvect->Y1 = y1;
    p_colvect->Z1 = z1;
    p_colvect->X2 = x2;
    p_colvect->Y2 = y2;
    p_colvect->Z2 = z2;
    p_colvect->Face = face;

    return vect;
}

void add_obj_face_to_col_vect(short x1, short y1, short z1, short x2, short y2, short z2, ThingIdx thing, short face, ushort flags)
{
    int vect, limit;

    limit = get_memory_ptr_allocated_count((void **)&game_col_vects_list);
    if (next_vects_list >= limit) {
        return;
    }

    //TODO why generating thin walls here? we have a separate higher level call for that
    //thin_wall(x1 >> 7, z1 >> 7, x2 >> 7, z2 >> 7, 1, 1);
    vect = new_col_vect(x1, y1, z1, x2, y2, z2, face);
    if (vect <= 0)
        return;

    int delta_x, delta_z, dist;
    int x, z, step_x, step_z;

    delta_x = x2 - x1;
    delta_z = z2 - z1;
    dist = LbSqrL(delta_x * delta_x + delta_z * delta_z);
    if (dist < 1)
        dist = 1;

    x = x1 << 10;
    z = z1 << 10;
    step_x = (delta_x << 10) / dist;
    step_z = (delta_z << 10) / dist;
    for (; dist >= 0; dist--)
    {
        add_next_col_vect_to_vects_list(x >> 10, z >> 10, thing, face, vect, flags);
        x += step_x;
        z += step_z;
    }
}

#define TOLERANCE 10

/** Adds a face to col_vect lists in nearby MapElements, if the face has two sibling points close to the ground.
 */
void add_object_face3_to_col_vect(short obj_x, short obj_y, short obj_z, ThingIdx thing, short face, ushort a2)
{
    int alt_cor[4];
    int x_cor[4];
    int y_cor[4];
    int z_cor[4];
    struct SingleObjectFace3 *p_face;
    int cor;

    // Fill arrays with face coordinates
    p_face = &game_object_faces3[face];
    for (cor = 0; cor < 3; cor++) {
        struct SinglePoint *p_pt;
        p_pt = &game_object_points[p_face->PointNo[cor]];
        x_cor[cor] = obj_x + p_pt->X;
        y_cor[cor] = (obj_y + p_pt->Y) >> 3; // TODO why divide by 8?
        z_cor[cor] = obj_z + p_pt->Z;
    }
    // Fill array with altitudes
    for (cor = 0; cor < 3; cor++) {
        int alt;
        alt = alt_at_point(x_cor[cor], z_cor[cor]);
        alt_cor[cor] = alt >> 8;
    }
    // Add only if coords from two sibling points are very close to ground
    if (y_cor[0] > alt_cor[0] - TOLERANCE && y_cor[0] < alt_cor[0] + TOLERANCE
      && y_cor[1] > alt_cor[1] - TOLERANCE && y_cor[1] < alt_cor[1] + TOLERANCE) {
        add_obj_face_to_col_vect(x_cor[0], y_cor[0], z_cor[0],
          x_cor[1], y_cor[1], z_cor[1], thing, face, a2);
    }
    if (y_cor[0] > alt_cor[0] - TOLERANCE && y_cor[0] < alt_cor[0] + TOLERANCE
      && y_cor[2] > alt_cor[2] - TOLERANCE && y_cor[2] < alt_cor[2] + TOLERANCE) {
        add_obj_face_to_col_vect(x_cor[0], y_cor[0], z_cor[0],
          x_cor[2], y_cor[2], z_cor[2], thing, face, a2);
    }
    if (y_cor[1] > alt_cor[1] - TOLERANCE && y_cor[1] < alt_cor[1] + TOLERANCE
      && y_cor[2] > alt_cor[2] - TOLERANCE && y_cor[2] < alt_cor[2] + TOLERANCE) {
        add_obj_face_to_col_vect(x_cor[1], y_cor[1], z_cor[1],
          x_cor[2], y_cor[2], z_cor[2], thing, face, a2);
    }
}

/** Adds a face to col_vect lists in nearby MapElements, if the face has two sibling points close to the ground.
 */
void add_object_face4_to_col_vect(short obj_x, short obj_y, short obj_z, ThingIdx thing, short face, ushort a2)
{
    int alt_cor[4];
    int x_cor[4];
    int y_cor[4];
    int z_cor[4];
    struct SingleObjectFace4 *p_face;
    int cor;

    // Fill arrays with face coordinates
    p_face = &game_object_faces4[face];
    for (cor = 0; cor < 4; cor++) {
        struct SinglePoint *p_pt;
        p_pt = &game_object_points[p_face->PointNo[cor]];
        x_cor[cor] = obj_x + p_pt->X;
        y_cor[cor] = obj_y + p_pt->Y; // TODO why not divide by 8?
        z_cor[cor] = obj_z + p_pt->Z;
    }
    // Fill array with altitudes
    for (cor = 0; cor < 4; cor++) {
        int alt;
        alt = alt_at_point(x_cor[cor], z_cor[cor]);
        alt_cor[cor] = alt >> 8;
    }
    // Add only if coords from two sibling points are very close to ground
    if (y_cor[0] > alt_cor[0] - TOLERANCE && y_cor[0] < alt_cor[0] + TOLERANCE
      && y_cor[1] > alt_cor[1] - TOLERANCE && y_cor[1] < alt_cor[1] + TOLERANCE) {
        add_obj_face_to_col_vect(x_cor[0], y_cor[0], z_cor[0],
          x_cor[1], y_cor[1], z_cor[1], thing, -face, a2);
    }
    if (y_cor[1] > alt_cor[1] - TOLERANCE && y_cor[1] < alt_cor[1] + TOLERANCE
      && y_cor[3] > alt_cor[3] - TOLERANCE && y_cor[3] < alt_cor[3] + TOLERANCE) {
        add_obj_face_to_col_vect(x_cor[1], y_cor[1], z_cor[1],
          x_cor[3], y_cor[3], z_cor[3], thing, -face, a2);
    }
    if (y_cor[3] > alt_cor[3] - TOLERANCE && y_cor[3] < alt_cor[3] + TOLERANCE
      && y_cor[2] > alt_cor[2] - TOLERANCE && y_cor[2] < alt_cor[2] + TOLERANCE) {
        add_obj_face_to_col_vect(x_cor[3], y_cor[3], z_cor[3],
          x_cor[2], y_cor[2], z_cor[2], thing, -face, a2);
    }
    if (y_cor[2] > alt_cor[2] - TOLERANCE && y_cor[2] < alt_cor[2] + TOLERANCE
      && y_cor[0] > alt_cor[0] - TOLERANCE && y_cor[0] < alt_cor[0] + TOLERANCE) {
        add_obj_face_to_col_vect(x_cor[2], y_cor[2], z_cor[2],
          x_cor[0], y_cor[0], z_cor[0], thing, -face, a2);
    }
}

#undef TOLERANCE

void add_all_object_faces_to_col_vect(ThingIdx thing, ushort obj, ushort a2)
{
    short face;
    short startface3, endface3;
    short startface4, endface4;
    short obj_x, obj_y, obj_z;

    {
        struct SingleObject *p_obj;

        p_obj = &game_objects[obj];
        obj_x = p_obj->MapX;
        obj_z = p_obj->MapZ;
        obj_y = p_obj->OffsetY;
        startface3 = p_obj->StartFace;
        endface3 = startface3 + p_obj->NumbFaces;
        startface4 = p_obj->StartFace4;
        endface4 = startface4 + p_obj->NumbFaces4;
    }
    for (face = startface3; face < endface3; face++)
    {
        add_object_face3_to_col_vect(obj_x, obj_y, obj_z, thing, face, a2);
    }
    for (face = startface4; face < endface4; face++)
    {
        add_object_face4_to_col_vect(obj_x, obj_y, obj_z, thing, face, a2);
    }
}

void add_all_thing_objects_faces_to_col_vect(struct Thing *p_thing, ushort flags)
{
    ushort beg_obj, end_obj;
    ushort obj;

    beg_obj = p_thing->U.UObject.Object;
    end_obj = beg_obj + p_thing->U.UObject.NumbObjects;
    for (obj = beg_obj; obj < end_obj; obj++) {
        add_all_object_faces_to_col_vect(p_thing->ThingOffset, obj, flags);
    }
}

void generate_collision_vects(void)
{
    ushort tile_x, tile_z;

    for (tile_x = 0; tile_x < MAP_TILE_WIDTH; tile_x++)
    {
        for (tile_z = 0; tile_z < MAP_TILE_HEIGHT; tile_z++)
        {
            ThingIdx thing;
            int i;

            thing = get_mapwho_thing_index(tile_x, tile_z);
            for (i = 0; thing != 0 && i < MAX_THINGS_ON_TILE; i++)
            {
                if (thing <= 0) {
                    struct SimpleThing *p_sthing;
                    p_sthing = &sthings[thing];
                    thing = p_sthing->Next;
                } else {
                    struct Thing *p_thing;
                    p_thing = &things[thing];
                    if (p_thing->Type == TT_BUILDING)
                        add_all_thing_objects_faces_to_col_vect(p_thing, 0);
                    thing = p_thing->Next;
                }
            }
        }
    }
}

void thin_wall_around_thing_objects(struct Thing *p_thing, ubyte colt)
{
    ushort beg_obj, end_obj;
    ushort obj;

    beg_obj = p_thing->U.UObject.Object;
    end_obj = beg_obj + p_thing->U.UObject.NumbObjects;
    for (obj = beg_obj; obj < end_obj; obj++) {
        thin_wall_around_object(obj, colt);
    }
}

void generate_thin_walls(void)
{
    ushort tile_x, tile_z;

    for (tile_x = 0; tile_x < MAP_TILE_WIDTH; tile_x++)
    {
        for (tile_z = 0; tile_z < MAP_TILE_HEIGHT; tile_z++)
        {
            ThingIdx thing;
            int i;

            thing = get_mapwho_thing_index(tile_x, tile_z);
            for (i = 0; thing != 0 && i < MAX_THINGS_ON_TILE; i++)
            {
                if (thing <= 0) {
                    struct SimpleThing *p_sthing;
                    p_sthing = &sthings[thing];
                    thing = p_sthing->Next;
                } else {
                    struct Thing *p_thing;
                    p_thing = &things[thing];
                    if (p_thing->Type == TT_BUILDING)
                        thin_wall_around_thing_objects(p_thing, 0);
                    thing = p_thing->Next;
                }
            }
        }
    }
}

/** Divides triangles adding line segment where entrances to thin paths ought to be.
 */
void thin_paths_entrance_on_vectlist(ushort vl_head)
{
    struct ColVectList *p_cvlist;
    ushort vl;

    for (vl = vl_head; vl != 0; vl = p_cvlist->NextColList & 0x7FFF)
    {
        struct ColVect *p_colvect;
        int sx1, sx2, sy1, sy2;

        p_cvlist = &game_col_vects_list[vl];
        p_colvect = &game_col_vects[p_cvlist->Vect];

        sx1 = p_colvect->X1;
        sy1 = p_colvect->Z1;
        sx2 = p_colvect->X2;
        sy2 = p_colvect->Z2;

        insert_point(sx1, sy1);
        insert_point(sx2, sy2);
    }
}

void generate_thin_paths_entrance(void)
{
    ushort tile_x, tile_z;

    for (tile_x = 0; tile_x < MAP_TILE_WIDTH; tile_x++)
    {
        for (tile_z = 0; tile_z < MAP_TILE_HEIGHT; tile_z++)
        {
            struct MyMapElement *p_mapel;

            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];
            thin_paths_entrance_on_vectlist(p_mapel->ColHead);
        }
    }
}

void thin_paths_on_vectlist(ushort vl_head,
  TrTriangId *faces3_added, TrTriangId *faces4_added)
{
    struct ColVectList *p_cvlist;
    ushort vl;

    for (vl = vl_head; vl != 0; vl = p_cvlist->NextColList & 0x7FFF)
    {
        struct ColVect *p_colvect;
        int sx1, sx2, sy1, sy2;

        p_cvlist = &game_col_vects_list[vl];
        p_colvect = &game_col_vects[p_cvlist->Vect];

        int face = p_colvect->Face;

        if (face_is_blocking_walk(face))
            continue;

        sx1 = p_colvect->X1;
        sy1 = p_colvect->Z1;
        sx2 = p_colvect->X2;
        sy2 = p_colvect->Z2;

        make_edge(sx1, sy1, sx2, sy2);

        // The lower edge is done, now we need to make the
        // remaining point and edges

        //TODO finish implementation
    }
}

void thin_path_on_thing_objects(struct Thing *p_thing,
  TrTriangId *faces3_added, TrTriangId *faces4_added)
{
    ushort vl;
    for (vl = 0; vl < next_vects_list; vl++)
    {
        struct ColVectList *p_cvlist;

        p_cvlist = &game_col_vects_list[vl];
        if (p_cvlist->Object == p_thing->ThingOffset)
        {
            //TODO finish implementation
        }
    }
}

void generate_thin_paths(void)
{
    ushort tile_x, tile_z;
    // Array for mapping each SingleObjectFace3 into corresponding TrTriangle
    TrTriangId *faces3_added;
    // Array for mapping each SingleObjectFace4 into two TrTriangles
    TrTriangId *faces4_added;

    faces3_added = LbMemoryAlloc(next_object_face3 * sizeof(TrTriangId));
    faces4_added = LbMemoryAlloc(next_object_face4 * sizeof(TrTriangId) * 2);

    // Add faces which directly touch the ground
    for (tile_x = 0; tile_x < MAP_TILE_WIDTH; tile_x++)
    {
        for (tile_z = 0; tile_z < MAP_TILE_HEIGHT; tile_z++)
        {
            struct MyMapElement *p_mapel;

            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];
            thin_paths_on_vectlist(p_mapel->ColHead, faces3_added, faces4_added);
        }
    }

    // Add the rest of faces for each object
    for (tile_x = 0; tile_x < MAP_TILE_WIDTH; tile_x++)
    {
        for (tile_z = 0; tile_z < MAP_TILE_HEIGHT; tile_z++)
        {
            ThingIdx thing;
            int i;

            thing = get_mapwho_thing_index(tile_x, tile_z);
            for (i = 0; thing != 0 && i < MAX_THINGS_ON_TILE; i++)
            {
                if (thing <= 0) {
                    struct SimpleThing *p_sthing;
                    p_sthing = &sthings[thing];
                    thing = p_sthing->Next;
                } else {
                    struct Thing *p_thing;
                    p_thing = &things[thing];
                    if (p_thing->Type == TT_BUILDING)
                        thin_path_on_thing_objects(p_thing, faces3_added, faces4_added);
                    thing = p_thing->Next;
                }
            }
        }
    }

    LbMemoryFree(faces3_added);
    LbMemoryFree(faces4_added);
}

#ifdef SW_FNCHK
static void triangulation_unkn_func_002_asm(int x1, int z1, int x2, int z2)
{
    asm volatile (
      "call ASM_triangulation_unkn_func_002\n"
        : "+a" (x1), "+d" (z1), "+b" (x2), "+c" (z2) : : "cc", "memory");
}
#endif

/** True unless the tile of (x1,z1) has a collision vector between (x1,z1)
 * and (x2,z2), either way, within 32 (at most 200 entries are looked at).
 * Remade from func_5fcf4. */
static TbBool tri_edge_has_no_col_vect(s32 x1, s32 z1, s32 x2, s32 z2)
{
    ushort cl;
    int n;

    if (((x1 >> 8) < 0) || ((x1 >> 8) >= 128) || ((z1 >> 8) < 0) || ((z1 >> 8) >= 128))
        return false;
    n = 0;
    for (cl = game_my_big_map[((z1 >> 8) << 7) + (x1 >> 8)].ColHead; cl != 0;
      cl = game_col_vects_list[cl].NextColList & 0x7FFF)
    {
        struct ColVect *p_cvect;

        if (n++ >= 200)
            break;
        p_cvect = &game_col_vects[game_col_vects_list[cl].Vect];
        if ((abs(p_cvect->X1 - x1) < 32) && (abs(p_cvect->X2 - x2) < 32) &&
          (abs(p_cvect->Z2 - z2) < 32) && (abs(p_cvect->Z1 - z1) < 32))
            return false;
        if ((abs(p_cvect->X1 - x2) < 32) && (abs(p_cvect->X2 - x1) < 32) &&
          (abs(p_cvect->Z2 - z1) < 32) && (abs(p_cvect->Z1 - z2) < 32))
            return false;
    }
    return true;
}

/** Opens edge `edge` of triangle `tri` (and the matching edge of its
 * neighbour) when the neighbour is walkable. */
static void tri_open_edge(int tri, int edge)
{
    struct TrTriangle *p_tri, *p_nbr;
    int nbr, k;

    p_tri = &triangulation[0].Triangles[tri];
    nbr = p_tri->tri[edge];
    p_nbr = &triangulation[0].Triangles[nbr];
    if ((p_nbr->solid & 0x06) != 0)
        return;
    p_tri->enter |= (1 << edge);
    for (k = 0; k < 3; k++)
    {
        if (p_nbr->tri[k] == tri) {
            p_nbr->enter |= (1 << k);
            break;
        }
    }
}

/** After something in the tile rectangle (x1,z1)-(x2,z2) went away (a
 * building blown up), opens the edges of walkable triangles there which
 * have no collision vector any more.  A triangle is looked at only when
 * its point 0 or 1 is inside.  Remade from triangulation_unkn_func_002. */
static void triangulation_unkn_func_002_c(int x1, int z1, int x2, int z2)
{
    s32 bx1, bz1, bx2, bz2;
    int tri;

    bx1 = x1 << 8;
    bz1 = z1 << 8;
    bx2 = (x2 + 1) << 8;
    bz2 = (z2 + 1) << 8;
    for (tri = 0; tri < triangulation[0].ix_Triangles; tri++)
    {
        struct TrTriangle *p_tri;
        struct TrPoint *p_pt[3];
        ubyte in;
        int k;

        p_tri = &triangulation[0].Triangles[tri];
        if ((p_tri->solid & 0x06) != 0)
            continue;
        in = 0;
        for (k = 0; k < 3; k++)
        {
            p_pt[k] = &triangulation[0].Points[(ushort)p_tri->point[k]];
            if ((p_pt[k]->x >= bx1) && (p_pt[k]->x <= bx2) &&
              (p_pt[k]->y >= bz1) && (p_pt[k]->y <= bz2))
                in |= (1 << k);
            if ((k == 1) && (in == 0))
                break;
        }
        if (in == 0)
            continue;
        if (((in & 0x03) == 0x03) && ((p_tri->enter & 0x01) == 0) &&
          tri_edge_has_no_col_vect(p_pt[0]->x, p_pt[0]->y, p_pt[1]->x, p_pt[1]->y))
            tri_open_edge(tri, 0);
        if (((in & 0x06) == 0x06) && ((p_tri->enter & 0x02) == 0) &&
          tri_edge_has_no_col_vect(p_pt[1]->x, p_pt[1]->y, p_pt[2]->x, p_pt[2]->y))
            tri_open_edge(tri, 1);
        if (((in & 0x05) == 0x05) && ((p_tri->enter & 0x04) == 0) &&
          tri_edge_has_no_col_vect(p_pt[2]->x, p_pt[2]->y, p_pt[0]->x, p_pt[0]->y))
            tri_open_edge(tri, 2);
    }
}

void triangulation_unkn_func_002(int x1, int z1, int x2, int z2)
{
    FNCHK_VOID("triangulation_unkn_func_002", triangulation_unkn_func_002_asm(x1, z1, x2, z2),
      triangulation_unkn_func_002_c(x1, z1, x2, z2));
}

int fringe_at_tile(short tile_x, short tile_z)
{
    struct MyMapElement *p_mapel;

    if ((tile_x < 0) || (tile_x >= MAP_TILE_WIDTH))
        return 0;
    if ((tile_z < 0) || (tile_z >= MAP_TILE_HEIGHT))
        return 0;

    p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];

    return (p_mapel->Flags2 & MEF2_Unkn04);
}

void fill_ground_map(ubyte *p_map)
{
    ushort tile_x, tile_z;

    for (tile_x = 0; tile_x < MAP_TILE_WIDTH; tile_x++)
    {
        for (tile_z = 0; tile_z < MAP_TILE_HEIGHT; tile_z++)
        {
            ubyte *p_solid;
            int n;

            p_solid = &p_map[2*MAP_TILE_WIDTH * 2*tile_z + 2*tile_x];
            n = fringe_at_tile(tile_x, tile_z);
            p_solid[0] = n;
            p_solid[1] = n;
            p_solid += 2*MAP_TILE_WIDTH;
            p_solid[0] = n;
            p_solid[1] = n;
        }
    }
}

void generate_ground_map(void)
{
    if (ground_map == NULL) {
        ground_map = LbMemoryAlloc(MAP_TILE_WIDTH * MAP_TILE_HEIGHT);
    }
    fill_ground_map(ground_map);
    triangulate_map(ground_map);
}

void generate_map_triangulation(void)
{
#if 0 // This is done earlier, in triangulation_clear()
    triangulation_init();
    // TODO should this be replaced by triangulation_init_edges()?
    thin_wall(0, 0, 255, 0, 1, 1);
    thin_wall(255, 0, 255, 255, 1, 1);
    thin_wall(255, 255, 0, 255, 1, 1);
    thin_wall(0, 255, 0, 0, 1, 1);
#endif
    init_collision_vects();
    generate_walk_items();
    update_mapel_collision_columns();
    generate_collision_vects();
    generate_ground_map();
    generate_thin_walls();
    // Add points where jumps to thin_paths will be. These should be
    // added here to allow best results from Delaunay optimization
    generate_thin_paths_entrance();
    switch (selected_triangulation_no)
    {
    case 1:
        triangulation_clear_enter_into_solid_gnd();
        break;
    case 2:
        triangulation_clear_enter_into_solid_air();
        break;
    }
    delaunay_step();
    generate_thin_paths();
}

/******************************************************************************/
