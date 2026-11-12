/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file pathtrig.h
 *     Header file for pathtrig.c.
 * @par Purpose:
 *     Pathfinding triangulation support required by the 3D engine.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     29 Sep 2023 - 02 Jan 2024
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef PATHTRIG_H
#define PATHTRIG_H

#include "bftypes.h"
#include "trstate.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
#pragma pack(1)

struct Thing;

struct WayPoint {
    s32 x8;
    s32 y8;
};

struct Path { // sizeof=0x814
    s32 ax8;
    s32 ay8;
    s32 bx8;
    s32 by8;
    s32 PathLength;
    struct WayPoint WayPoints[256];
};

#pragma pack()
/******************************************************************************/

void triangulation_clear(void);
void triangulation_allocate(int tgnNo, int maxTrigs);

void triangulate_map(ubyte *p_map);
void triangulate_area(ubyte *p_map, int x1, int x2, int y1, int y2);

void generate_ground_map(void);

void thin_wall_around_object_rm(ushort obj, ushort colt);
void generate_map_triangulation(void);

void triangulation_unkn_func_002(int x1, int z1, int x2, int z2);

/** Print triangulation arrays into log file, for debug.
 */
void print_triangulation(void);

/** Print walk items into log file, for debug.
 */
void print_walk_items(void);

/** Print collision columns into log file, for debug.
 */
void print_mapel_collision_columns(void);

/** Print collision vectors into log file, for debug.
 */
void print_collision_vects(void);

void unkn_path_func_001(struct Thing *p_thing, ubyte a2);

/** Route through triangles between triA and triB, the shorter of the two
 * search directions; returns its length or -1.
 */
s32 ma_triangle_route_3(int triA, int triB, s32 *p_route, s32 *p_cost);
int path_init8_thing(struct Path *p_path, int ax8, int ay8, int bx8, int by8, int a6);
void fatter_than_nothing(struct Path *p_src, struct Path *p_dst, int fat);
int tri_path_init8(int triA, int triB, struct Path *p_path, int ax8,
  int ay8, int bx8, int by8, int a8);

/** Checks whether line segment (x1,y1)-(x2,y2) intersects segment (x3,y3)-(x4,y4).
 */
TbBool two4_line_intersection(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4);

void init_col_vects_linked_list(void);

/** Update assigned ColVectList range within each Thing.
 *
 * The ColVectList instances contain references to things containing geometry
 * they own, but we also want Things instances to contain references to the
 * ColVectList, to speed up access in objects which change them dynamically.
 *
 * This function resets said references within all Object things.
 */
void reset_things_col_vect_range(void);

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
