/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file matrix.c
 *     Routines implementing matrix support.
 * @par Purpose:
 *     Implement functions for handling multi-dimensional matrices.
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
#include "matrix.h"
#include "swlog.h"
#include "fncheck.h"
#include "bfmath.h"
/******************************************************************************/

struct M33 local_mats[LOCAL_MATS_COUNT];
ushort next_local_mat = 1;

/******************************************************************************/

void matrix_identity_fill(struct M33 *p_mat)
{
#if 0
    asm volatile ("call ASM_matrix_identity_fill\n"
        : : "a" (p_mat));
#endif
    p_mat->R[0][0] = 0x4000; p_mat->R[0][1] = 0; p_mat->R[0][2] = 0;
    p_mat->R[1][0] = 0; p_mat->R[1][1] = 0x4000; p_mat->R[1][2] = 0;
    p_mat->R[2][0] = 0; p_mat->R[2][1] = 0; p_mat->R[2][2] = 0x4000;
}

void matrix_transpose_copy(struct M33 *p_mat1, struct M33 *p_mat2)
{
#if 0
    asm volatile ("call ASM_matrix_transpose_copy\n"
        :  : "a" (p_mat1),  "d" (p_mat2));
#endif
    int row, col;

    for (row = 0; row < 3; row++)
        for (col = 0; col < 3; col++)
            p_mat2->R[col][row] = p_mat1->R[row][col];
}

void matrix_rotate_yaw_copy(const struct M33 *p_src, struct M33 *p_dst,
  int cos_v, int sin_v)
{
    int row;
    for (row = 0; row < 3; row++)
    {
        int a, c;
        a = p_src->R[row][0];
        c = p_src->R[row][2];
        p_dst->R[row][0] = (int)((((s64)a * sin_v) >> 16)
          + (((s64)c * cos_v) >> 16));
        p_dst->R[row][1] = p_src->R[row][1];
        p_dst->R[row][2] = (int)((((s64)c * sin_v) >> 16)
          - (((s64)a * cos_v) >> 16));
    }
}

void matrix_rotate_pitch_copy(const struct M33 *p_src, struct M33 *p_dst,
  int cos_v, int sin_v)
{
    int row;
    for (row = 0; row < 3; row++)
    {
        int a, b;
        a = p_src->R[row][0];
        b = p_src->R[row][1];
        p_dst->R[row][0] = (int)((((s64)a * cos_v) >> 16)
          - (((s64)b * sin_v) >> 16));
        p_dst->R[row][1] = (int)((((s64)b * cos_v) >> 16)
          + (((s64)a * sin_v) >> 16));
        p_dst->R[row][2] = p_src->R[row][2];
    }
}

void matrix_rotate_roll_copy(const struct M33 *p_src, struct M33 *p_dst,
  int cos_v, int sin_v)
{
    int row;
    for (row = 0; row < 3; row++)
    {
        int b, c;
        b = p_src->R[row][1];
        c = p_src->R[row][2];
        p_dst->R[row][0] = p_src->R[row][0];
        p_dst->R[row][1] = (int)((((s64)b * sin_v) >> 16)
          + (((s64)c * cos_v) >> 16));
        p_dst->R[row][2] = (int)((((s64)c * sin_v) >> 16)
          - (((s64)b * cos_v) >> 16));
    }
}

/* 32-bit products wrapping as the original's IMUL */
static inline s32 mul32w(s32 a, s32 b)
{
    return (s32)((u32)a * (u32)b);
}

/** Signed division by 512 rounding towards zero, as the original. */
static inline s32 div512(s32 v)
{
    return v / 512;
}

#ifdef SW_FNCHK
static void rotate_object_axis_asm(struct M33 *p_base, short xangle, short yangle, short zangle)
{
    asm volatile ("call ASM_rotate_object_axis\n"
        : : "a" (p_base), "d" (xangle), "b" (yangle), "c" (zangle) : "cc", "memory");
}

static void vec_cross_prod_asm(struct M31 *vecr, struct M31 *vec1, struct M31 *vec2)
{
    asm volatile ("call ASM_vec_cross_prod\n"
        : : "a" (vecr), "d" (vec1), "b" (vec2) : "cc", "memory");
}

static void object_vec_normalisation_asm(struct M33 *vec, ubyte col)
{
    asm volatile ("call ASM_object_vec_normalisation\n"
        : : "a" (vec), "d" (col) : "cc", "memory");
}
#endif

/** Remade from vec_cross_prod. */
static void vec_cross_prod_c(struct M31 *vecr, struct M31 *vec1, struct M31 *vec2)
{
    s32 r0, r1, r2;

    r0 = mul32w(vec1->R[1], vec2->R[2]) - mul32w(vec2->R[1], vec1->R[2]);
    r1 = mul32w(vec1->R[2], vec2->R[0]) - mul32w(vec1->R[0], vec2->R[2]);
    r2 = mul32w(vec1->R[0], vec2->R[1]) - mul32w(vec2->R[0], vec1->R[1]);
    vecr->R[0] = r0;
    vecr->R[1] = r1;
    vecr->R[2] = r2;
}

/** Scales column col of the matrix to the length 0x4000.
 * Remade from object_vec_normalisation. */
static void object_vec_normalisation_c(struct M33 *vec, ubyte col)
{
    s32 x, y, z, len;

    x = vec->R[0][col];
    y = vec->R[1][col];
    z = vec->R[2][col];
    len = LbSqrL((u32)(mul32w(x, x) + mul32w(y, y) + mul32w(z, z))) + 1;
    vec->R[0][col] = (s32)((u32)x << 14) / len;
    vec->R[1][col] = (s32)((u32)y << 14) / len;
    vec->R[2][col] = (s32)((u32)z << 14) / len;
}

/** Turns the object matrix a little around its axes (angles in 1/512
 * steps) and makes it orthonormal again.  Remade from rotate_object_axis.
 */
static void rotate_object_axis_c(struct M33 *p_base, short xangle, short yangle, short zangle)
{
    struct M33 *m;
    struct M31 a, b, r;
    int i;

    m = p_base;
    if ((xangle == 0) && (yangle == 0) && (zangle == 0))
        return;
    if (xangle != 0)
    {
        for (i = 0; i < 3; i++)
            m->R[i][2] += div512(mul32w(m->R[i][0] - m->R[i][2], xangle));
    }
    if (yangle != 0)
    {
        for (i = 0; i < 3; i++)
            m->R[i][2] += div512(mul32w(m->R[i][1] - m->R[i][2], yangle));
    }
    if (zangle != 0)
    {
        for (i = 0; i < 3; i++)
            m->R[i][0] += div512(mul32w(m->R[i][1] - m->R[i][0], zangle));
    }
    object_vec_normalisation(m, 2);
    for (i = 0; i < 3; i++) {
        a.R[i] = m->R[i][2];
        b.R[i] = m->R[i][0];
    }
    vec_cross_prod(&r, &a, &b);
    for (i = 0; i < 3; i++)
        m->R[i][1] = r.R[i] >> 14;
    object_vec_normalisation(m, 1);
    for (i = 0; i < 3; i++) {
        a.R[i] = m->R[i][2];
        b.R[i] = m->R[i][1];
    }
    vec_cross_prod(&r, &b, &a);
    for (i = 0; i < 3; i++)
        m->R[i][0] = r.R[i] >> 14;
    object_vec_normalisation(m, 0);
}

void rotate_object_axis(struct M33 *p_base, short xangle, short yangle, short zangle)
{
    FNCHK_VOID("rotate_object_axis", rotate_object_axis_asm(p_base, xangle, yangle, zangle),
      rotate_object_axis_c(p_base, xangle, yangle, zangle));
}

void vec_cross_prod(struct M31 *vecr, struct M31 *vec1, struct M31 *vec2)
{
    FNCHK_VOID("vec_cross_prod", vec_cross_prod_asm(vecr, vec1, vec2),
      vec_cross_prod_c(vecr, vec1, vec2));
}

void object_vec_normalisation(struct M33 *vec, ubyte col)
{
    FNCHK_VOID("object_vec_normalisation", object_vec_normalisation_asm(vec, col),
      object_vec_normalisation_c(vec, col));
}

void matrix_transform(struct M31 *p_result, struct M33 *p_trans, struct M31 *p_source)
{
    p_result->R[0] = p_trans->R[0][2] * p_source->R[2] + p_trans->R[0][0] * p_source->R[0] + p_source->R[1] * p_trans->R[0][1];
    p_result->R[1] = p_trans->R[1][2] * p_source->R[2] + p_trans->R[1][0] * p_source->R[0] + p_source->R[1] * p_trans->R[1][1];
    p_result->R[2] = p_trans->R[2][2] * p_source->R[2] + p_trans->R[2][1] * p_source->R[1] + p_source->R[0] * p_trans->R[2][0];
}

/******************************************************************************/
