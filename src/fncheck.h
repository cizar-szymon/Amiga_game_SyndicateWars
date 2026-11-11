/******************************************************************************/
// Syndicate Wars Fan Expansion, Amiga port - diagnostics.
/******************************************************************************/
/** @file fncheck.h
 *     Differential check of a remade C function against the original asm.
 * @par Purpose:
 *     In a build with SW_FNCHK (tools/amiga/build_x86.sh check), a function
 *     wrapped with FNCHK_* runs the asm version first, saves the resulting
 *     game state, restores the state from before the call, runs the C version
 *     and compares both results byte by byte. Game state = engine memory block
 *     (all mem_game buffers, things, sthings) + .data + .bss of the program.
 *     Differences go to fncheck.log; a summary is written on exit.
 *     Without SW_FNCHK only the C version runs.
 * @par Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 3 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef FNCHECK_H
#define FNCHECK_H

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef SW_FNCHK

/** Returns true if a check may start now (not nested inside another check). */
TbBool fnchk_begin(void);
/** Asm version done: keep its result, restore the state from fnchk_begin(). */
void fnchk_mid(void);
/** C version done: compare with the asm result. Returns false on a difference. */
TbBool fnchk_end(const char *name, long ret_asm, long ret_c);
/** Count a call of a pure function and compare its results. */
void fnchk_pure_result(const char *name, long ret_asm, long ret_c);

/** Pure function (no side effects): both versions run, only results compared. */
#define FNCHK_PURE(name, res, asm_expr, c_expr) do { \
    long fnchk_ra_ = (long)(asm_expr); \
    res = (c_expr); fnchk_pure_result(name, fnchk_ra_, (long)(res)); \
} while (0)

/** Void function: asm statement, then C statement. */
#define FNCHK_VOID(name, asm_stmt, c_stmt) do { \
    if (fnchk_begin()) { \
        asm_stmt; fnchk_mid(); c_stmt; fnchk_end(name, 0, 0); \
    } else { c_stmt; } \
} while (0)

/** Function with a result: `res` gets the C result. */
#define FNCHK_RET(name, res, asm_expr, c_expr) do { \
    if (fnchk_begin()) { \
        long fnchk_ra_ = (long)(asm_expr); fnchk_mid(); \
        res = (c_expr); fnchk_end(name, fnchk_ra_, (long)(res)); \
    } else { res = (c_expr); } \
} while (0)

#else

#define FNCHK_VOID(name, asm_stmt, c_stmt) do { c_stmt; } while (0)
#define FNCHK_RET(name, res, asm_expr, c_expr) do { res = (c_expr); } while (0)
#define FNCHK_PURE(name, res, asm_expr, c_expr) do { res = (c_expr); } while (0)

#endif

#ifdef __cplusplus
};
#endif

#endif
/******************************************************************************/
