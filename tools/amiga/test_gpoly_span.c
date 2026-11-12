/* Test: span loops of poly_gp.c (new, gpoly_span_*_c) against the original
 * per-pixel functions (gpoly_pixel_shaded/noshade, copied from poly_gp.c).
 * Random start states and increments, random texture and fade tables.
 * Build: gcc -O2 -o test_gpoly_span test_gpoly_span.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef uint8_t ubyte;
typedef uint32_t u32;
typedef uint8_t TbPixel;
#define PITCH_SH 8

struct gpoly_factors { int S[4]; };
struct gpoly_blends { int B[3]; };
static ubyte vec_map[65536];
static struct { ubyte fade_table[65536]; } pixmap;

static inline ubyte __CFADDL__(long x, long y)
{
    return (unsigned long)(u32)(x) > (unsigned long)(u32)(x+y);
}

/* original, from poly_gp.c */
static TbPixel gpoly_pixel_shaded(struct gpoly_blends *p_bld, const struct gpoly_factors *p_inc)
{
    int loc_2d;
    ubyte a3b_h, a3b_l;
    ubyte loc_carry;
    ubyte ret_h, ret_l;

    ret_h = p_bld->B[2] >> 8;
    a3b_l = p_bld->B[0];
    a3b_h = p_bld->B[1] >> 8;
    loc_2d = p_bld->B[2];

    loc_carry = __CFADDL__(loc_2d, p_inc->S[1]);
    p_bld->B[2] = loc_2d + p_inc->S[1];
    loc_2d = p_bld->B[0] + loc_carry;

    ret_l = vec_map[(a3b_h << PITCH_SH) | a3b_l];

    loc_carry = __CFADDL__(loc_2d, p_inc->S[2]);
    p_bld->B[0] = loc_2d + p_inc->S[2];
    loc_2d = a3b_h + loc_carry;

    a3b_h = loc_2d + p_inc->S[3];

    p_bld->B[1] = (a3b_h << 8) | a3b_l;

    return pixmap.fade_table[(ret_h << 8) | ret_l];
}

static TbPixel gpoly_pixel_noshade(struct gpoly_blends *p_bld, const struct gpoly_factors *p_inc)
{
    int loc_2d;
    ubyte a3b_h, a3b_l;
    ubyte loc_carry;
    ubyte ret_l;

    a3b_l = p_bld->B[0];
    a3b_h = p_bld->B[1] >> 8;
    loc_2d = p_bld->B[2];

    loc_carry = __CFADDL__(loc_2d, p_inc->S[1]);
    p_bld->B[2] = loc_2d + p_inc->S[1];
    loc_2d = p_bld->B[0] + loc_carry;

    ret_l = vec_map[(a3b_h << PITCH_SH) | a3b_l];

    loc_carry = __CFADDL__(loc_2d, p_inc->S[2]);
    p_bld->B[0] = loc_2d + p_inc->S[2];
    loc_2d = a3b_h + loc_carry;

    a3b_h = loc_2d + p_inc->S[3];

    p_bld->B[1] = (a3b_h << 8) | a3b_l;

    return ret_l;
}

#define LOGERR(...) (void)0
#define LOGWARN(...) (void)0
#include "span_under_test.inc"

static u32 rnd(void)
{
    return ((u32)rand() << 17) ^ ((u32)rand() << 5) ^ (u32)rand();
}

int main(void)
{
    ubyte out1[700], out2[700];
    int test, i, fails = 0;

    srand(1234);
    for (i = 0; i < 65536; i++) { vec_map[i] = rnd(); pixmap.fade_table[i] = rnd(); }
    for (test = 0; test < 200000; test++)
    {
        struct gpoly_blends b, b2;
        struct gpoly_factors f;
        int n, shaded, k;

        for (k = 0; k < 3; k++) b.B[k] = rnd();
        for (k = 0; k < 4; k++) f.S[k] = rnd();
        if (test & 1) { b.B[0] |= 0xFFFFFF00; } // carry edge cases
        if (test & 2) { f.S[1] = 0xFFFFFFFF; }
        n = 1 + rnd() % 640;
        shaded = test & 4;
        b2 = b;
        for (i = 0; i < n; i++)
            out1[i] = shaded ? gpoly_pixel_shaded(&b2, &f) : gpoly_pixel_noshade(&b2, &f);
        if (shaded)
            gpoly_span_shaded_c(out2, n, &b, &f);
        else
            gpoly_span_noshade_c(out2, n, &b, &f);
        if (memcmp(out1, out2, n) != 0) {
            if (fails < 5) printf("test %d differs (shaded=%d n=%d)\n", test, !!shaded, n);
            fails++;
        }
    }
    printf("%d tests, %d failed\n", test, fails);
    return fails != 0;
}
