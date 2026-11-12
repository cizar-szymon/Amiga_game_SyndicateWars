/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file lvendian.h
 *     Byte order of map and level files on big endian CPUs.
 * @par Purpose:
 *     Map (.mad, .dat) and level files are little endian memory images.
 *     These functions convert them in place right after reading (and before
 *     writing). On little endian CPUs they do nothing.
 * @par Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef LVENDIAN_H
#define LVENDIAN_H

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Thing;
struct SimpleThing;

/** Convert things; the union part is chosen by Type/SubType (bytes). */
void thing_fix_endian(struct Thing *p_thing, ulong count);
/** Convert simple things; the union part is chosen by Type. */
void sthing_fix_endian(struct SimpleThing *p_sthing, ulong count);
/** Convert a whole .mad map buffer in place (see load_mad_pc_buffer()). */
void mad_buffer_fix_endian(ubyte *buf, long size);

#ifdef __cplusplus
};
#endif

#endif
/******************************************************************************/
