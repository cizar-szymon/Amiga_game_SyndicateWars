/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file lvendian.c
 *     Byte order of map and level files on big endian CPUs.
 * @par Purpose:
 *     See lvendian.h. Struct layouts come from data_layouts.h, generated
 *     by tools/amiga/struct_layout.py from the struct definitions;
 *     lb_le_fix_struct() checks each layout against sizeof().
 * @par Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "lvendian.h"

#include <assert.h>
#include "bfendian.h"
#include "matrix.h"
#include "bigmap.h"
#include "building.h"
#include "data_layouts.h"
#include "lvfiles.h"
#include "swlog.h"
#include "thing.h"
#include "trstate.h"
#include "triangls.h"
#include "trpoints.h"
/******************************************************************************/

extern struct QuickLoad quick_load_pc[];

static const char *thing_layout(const struct Thing *p_thing)
{
    switch (p_thing->Type)
    {
    case TT_PERSON:
        return LAYOUT_Thing_UPerson;
    case TT_VEHICLE:
        return LAYOUT_Thing_UVehicle;
    case TT_BUILDING:
        if (p_thing->SubType == SubTT_BLD_MGUN)
            return LAYOUT_Thing_UMGun;
        return LAYOUT_Thing_UObject;
    default:
        return LAYOUT_Thing_UEffect;
    }
}

void thing_fix_endian(struct Thing *p_thing, ulong count)
{
    if (!LB_BIG_ENDIAN)
        return;
    for (; count != 0; count--, p_thing++)
        lb_le_fix_struct(p_thing, 1, sizeof(struct Thing), thing_layout(p_thing));
}

static const char *sthing_layout(const struct SimpleThing *p_sthing)
{
    switch (p_sthing->Type)
    {
    case SmTT_TRAFFIC:
        return LAYOUT_SimpleThing_UTraffic;
    default:
        // all other variants are 16-bit fields at the same places
        return LAYOUT_SimpleThing_UEffect;
    }
}

void sthing_fix_endian(struct SimpleThing *p_sthing, ulong count)
{
    if (!LB_BIG_ENDIAN)
        return;
    for (; count != 0; count--, p_sthing++)
        lb_le_fix_struct(p_sthing, 1, sizeof(struct SimpleThing), sthing_layout(p_sthing));
}

/** Layouts of the quick_load_pc[] arrays, in the same order. */
static const char *const quick_load_layouts[] = {
    LAYOUT_MyMapElement,
    LAYOUT_SingleFloorTexture,
    LAYOUT_SingleTexture,
    LAYOUT_SinglePoint,
    LAYOUT_SingleObjectFace3,
    LAYOUT_SingleObject,
    LAYOUT_QuickLight,
    LAYOUT_FullLight,
    LAYOUT_Normal,
    LAYOUT_SingleObjectFace4,
    LAYOUT_AnimTmap,
    LAYOUT_TrafficNode,
    "36b",  // LightCommand: kept as bytes, fields read as little endian
    LAYOUT_ColVectList,
    LAYOUT_ColVect,
    LAYOUT_WalkHeader,
    "w",    // walk items: shorts
    LAYOUT_ColColumn,
};

void mad_buffer_fix_endian(ubyte *buf, long size)
{
    ubyte *p;
    ushort counts[sizeof(quick_load_layouts) / sizeof(quick_load_layouts[0])];
    ushort num;
    int i, k;

    if (!LB_BIG_ENDIAN)
        return;
    p = buf;
    lb_le_fix32_array(p, 1); // format version
    p += 4;

    for (i = 0; quick_load_pc[i].Size != 0; i++)
    {
        assert(i < (int)(sizeof(counts) / sizeof(counts[0])));
        counts[i] = 0;
        if (quick_load_pc[i].Numb != NULL) {
            lb_le_fix16_array(p, 1);
            counts[i] = *(ushort *)p;
            p += 2;
        }
    }
    for (i = 0; quick_load_pc[i].Size != 0; i++)
    {
        ulong entsize, nentries;

        if (quick_load_pc[i].Numb != NULL) {
            entsize = quick_load_pc[i].Size;
            nentries = quick_load_pc[i].Extra + counts[i];
        } else {
            // the whole map, given as Size blocks of Extra bytes
            entsize = 18;
            nentries = (ulong)quick_load_pc[i].Size * quick_load_pc[i].Extra / 18;
        }
        lb_le_fix_struct(p, nentries, entsize, quick_load_layouts[i]);
        p += nentries * entsize;
        p += 2; // unused 2 bytes after each array
    }

    lb_le_fix32_array(p, 2); // selected_triangulation_no, triangulation_initied
    p += 8;
    {
        struct Triangulation *p_trgl;
        long max_tri[4], max_pt[4];

        p_trgl = (struct Triangulation *)p;
        lb_le_fix_struct(p, 4, sizeof(struct Triangulation), LAYOUT_Triangulation);
        for (k = 0; k < 4; k++) {
            max_tri[k] = p_trgl[k].max_Triangles;
            max_pt[k] = p_trgl[k].max_Points;
        }
        p += 4 * sizeof(struct Triangulation);
        for (k = 0; k < 4; k++) {
            lb_le_fix_struct(p, max_tri[k], sizeof(struct TrTriangle), LAYOUT_TrTriangle);
            p += max_tri[k] * sizeof(struct TrTriangle);
        }
        for (k = 0; k < 4; k++) {
            lb_le_fix_struct(p, max_pt[k], sizeof(struct TrPoint), LAYOUT_TrPoint);
            p += max_pt[k] * sizeof(struct TrPoint);
        }
    }

    lb_le_fix16_array(p, 1);
    num = *(ushort *)p;
    p += 2;
    sthing_fix_endian((struct SimpleThing *)p, num);
    p += num * sizeof(struct SimpleThing);

    lb_le_fix16_array(p, 1);
    num = *(ushort *)p;
    p += 2;
    for (k = 0; k < num; k++)
    {
        struct Thing *p_thing;

        p_thing = (struct Thing *)p;
        thing_fix_endian(p_thing, 1);
        p += sizeof(struct Thing);
        if (p_thing->U.UObject.Object <= 0)
            continue;
        if ((p_thing->SubType == SubTT_BLD_MOVN_ROTOR) || (p_thing->SubType == SubTT_BLD_37)) {
            lb_le_fix_struct(p, 1, sizeof(struct M33), LAYOUT_M33);
            p += sizeof(struct M33);
        }
    }
    if (p - buf > size)
        LOGERR("map buffer conversion went %ld bytes past the end", (long)(p - buf - size));
}
/******************************************************************************/
