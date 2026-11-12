/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file gendianfix.c
 *     Conversion of little endian data file structures on big endian CPUs.
 * @par Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "bfendian.h"
#include "privbflog.h"

void lb_le_fix16_array(void *data, ulong count)
{
#if LB_BIG_ENDIAN
    ubyte *p = (ubyte *)data;
    for (; count != 0; count--, p += 2) {
        ubyte t = p[0]; p[0] = p[1]; p[1] = t;
    }
#else
    (void)data; (void)count;
#endif
}

void lb_le_fix32_array(void *data, ulong count)
{
#if LB_BIG_ENDIAN
    ubyte *p = (ubyte *)data;
    for (; count != 0; count--, p += 4) {
        ubyte t0 = p[0], t1 = p[1];
        p[0] = p[3]; p[1] = p[2]; p[2] = t1; p[3] = t0;
    }
#else
    (void)data; (void)count;
#endif
}

/** Walk the layout string; with `p` == NULL only measure the size. */
static ulong layout_apply(ubyte *p, const char *layout)
{
    ulong off = 0;
    const char *s = layout;

    while (*s != '\0')
    {
        ulong rep = 0;
        while ((*s >= '0') && (*s <= '9'))
            rep = rep * 10 + (*s++ - '0');
        if (rep == 0)
            rep = 1;
        switch (*s)
        {
        case 'b':
            off += rep;
            break;
        case 'w':
            if (p != NULL) lb_le_fix16_array(p + off, rep);
            off += 2 * rep;
            break;
        case 'd':
            if (p != NULL) lb_le_fix32_array(p + off, rep);
            off += 4 * rep;
            break;
        case ' ':
            break;
        default:
            return (ulong)-1;
        }
        s++;
    }
    return off;
}

TbBool lb_le_fix_struct(void *data, ulong count, ulong elem_size, const char *layout)
{
    ubyte *p;

    if (layout_apply(NULL, layout) != elem_size) {
        LOGERR("layout \"%s\" does not match struct size %lu", layout, elem_size);
        return false;
    }
#if LB_BIG_ENDIAN
    for (p = (ubyte *)data; count != 0; count--, p += elem_size)
        layout_apply(p, layout);
#else
    (void)p; (void)count; (void)data;
#endif
    return true;
}
/******************************************************************************/
