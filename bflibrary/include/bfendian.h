/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file bfendian.h
 *     Byte order of the game data files (little endian, as on x86).
 * @par Purpose:
 *     Data structures read from files hold little endian values. On a big
 *     endian CPU (68k Amiga) every multi-byte field must be converted after
 *     reading and before writing. On little endian CPUs all of this compiles
 *     to nothing.
 * @par Comment:
 *     LE_FIX16/LE_FIX32 convert a field in place (the same operation converts
 *     both ways). le_fix_struct() converts an array of structs by a field
 *     size list.
 * @par Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef BFLIBRARY_BFENDIAN_H_
#define BFLIBRARY_BFENDIAN_H_

#include <stddef.h>
#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif

unsigned long blong(unsigned char *p);

unsigned long llong(unsigned char *p);

unsigned long bword(unsigned char *p);

unsigned long lword(unsigned char *p);

void toggle_flag_byte(unsigned char *flags, unsigned char mask);

void toggle_flag_dword(unsigned long *flags, unsigned long mask);

void set_flag_word(unsigned short *flags, unsigned short mask, short value);

void set_flag_byte(unsigned char *flags, unsigned char mask, short value);

void set_flag_dword(unsigned long *flags, unsigned long mask, short value);

int number_of_set_bits(unsigned long i);

u32 bw_rotl32(u32 n, ubyte c);

u32 bw_rotr32(u32 n, ubyte c);

/* Byte order of data files - added for the big endian (Amiga) port. */

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#  define LB_BIG_ENDIAN 1
#else
#  define LB_BIG_ENDIAN 0
#endif

static inline uint16_t lb_swap16(uint16_t v)
{
    return (uint16_t)((v >> 8) | (v << 8));
}

static inline u32 lb_swap32(u32 v)
{
    return (v >> 24) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | (v << 24);
}

#if LB_BIG_ENDIAN
#  define lb_le16(v) lb_swap16((uint16_t)(v))
#  define lb_le32(v) lb_swap32((u32)(v))
#else
#  define lb_le16(v) ((uint16_t)(v))
#  define lb_le32(v) ((u32)(v))
#endif

/** Convert a 16-bit field between file (LE) and CPU order, in place. */
#define LE_FIX16(field) do { if (LB_BIG_ENDIAN) \
    (field) = (__typeof__(field))lb_swap16((uint16_t)(field)); } while (0)
/** Convert a 32-bit field between file (LE) and CPU order, in place. */
#define LE_FIX32(field) do { if (LB_BIG_ENDIAN) \
    (field) = (__typeof__(field))lb_swap32((u32)(field)); } while (0)

/** Read little endian values from a byte buffer (any alignment). */
static inline uint16_t lb_read_le16(const void *p)
{
    const ubyte *b = (const ubyte *)p;
    return (uint16_t)(b[0] | (b[1] << 8));
}

static inline u32 lb_read_le32(const void *p)
{
    const ubyte *b = (const ubyte *)p;
    return (u32)b[0] | ((u32)b[1] << 8) | ((u32)b[2] << 16) | ((u32)b[3] << 24);
}

/** Convert 16-bit values of an array in place. */
void lb_le_fix16_array(void *data, ulong count);
/** Convert 32-bit values of an array in place. */
void lb_le_fix32_array(void *data, ulong count);

/** Convert an array of structs in place. `layout` is a string with one
 * character per field: 'b' 1 byte, 'w' 2 bytes, 'd' 4 bytes; a decimal
 * number before a character repeats it ("12b" = 12 bytes). The layout must
 * cover exactly `elem_size` bytes (checked; returns false otherwise). */
TbBool lb_le_fix_struct(void *data, ulong count, ulong elem_size, const char *layout);

#ifdef __cplusplus
};
#endif

#endif
/******************************************************************************/
