/* Amiga port: 64-bit multiply without the 64-bit MULU.L / MULS.L forms.
 *
 * The libgcc ___muldi3 of the m68k-amigaos toolchain multiplies the low
 * words with "mulu.l d,dh:dl" (32x32 -> 64). The 68060 does not implement
 * that form in hardware: each one traps into 68060.library (about 1.3 us
 * instead of a few cycles). The game multiplies s64 values in its renderer
 * (triangle setup, transformation, gpoly) thousands of times per frame.
 *
 * This file defines ___muldi3 itself, so the linker does not take the libgcc
 * member. Only 32x32 -> 32 multiplies are used, which the 68060 has.
 * The result is the exact low 64 bits of the product, as in libgcc. */
#include <stdint.h>

typedef union {
    long long ll;
    struct { uint32_t hi, lo; } s;  /* 68k is big endian */
} amiga_dword;

long long __muldi3(long long u, long long v)
{
    amiga_dword a, b, r;
    uint32_t a0, a1, b0, b1;
    uint32_t p00, p01, p10, p11, mid;

    a.ll = u;
    b.ll = v;
    a0 = a.s.lo & 0xFFFF;
    a1 = a.s.lo >> 16;
    b0 = b.s.lo & 0xFFFF;
    b1 = b.s.lo >> 16;
    p00 = a0 * b0;
    p01 = a0 * b1;
    p10 = a1 * b0;
    p11 = a1 * b1;
    /* low word x low word, 64-bit result from four 16x16 products */
    mid = (p00 >> 16) + (p01 & 0xFFFF) + (p10 & 0xFFFF);
    r.s.lo = (mid << 16) | (p00 & 0xFFFF);
    r.s.hi = p11 + (p01 >> 16) + (p10 >> 16) + (mid >> 16)
      /* cross terms only reach the high word */
      + a.s.hi * b.s.lo + a.s.lo * b.s.hi;
    return r.ll;
}
