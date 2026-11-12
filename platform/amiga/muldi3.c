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

/* 64-bit division. libgcc ___divdi3 / ___moddi3 use the 64/32 DIVU.L, which
 * the 68060 also traps on. The game divides s64 values that nearly always
 * fit in 32 bits (triangle setup): those take the hardware 32-bit DIVS.L.
 * Others go through a shift-and-subtract loop (no 64-bit divide).
 * Same results as C division (quotient rounded toward zero, remainder with
 * the sign of the dividend); PC test: 5M random pairs, 0 differences. */
/* (u1:u0) / v for u1 < v, quotient fits in 32 bits. Two 32/32 hardware
 * divides on 16-bit digits (long division as in Hacker's Delight, "divlu"). */
static uint32_t amiga_divlu(uint32_t u1, uint32_t u0, uint32_t v, uint32_t *r)
{
    const uint32_t b = 0x10000;
    uint32_t un1, un0, vn1, vn0, q1, q0, un32, un21, un10, rhat;
    int s;

    s = __builtin_clz(v);
    v <<= s;
    vn1 = v >> 16;
    vn0 = v & 0xFFFF;
    un32 = (u1 << s) | (s ? (u0 >> (32 - s)) : 0);
    un10 = u0 << s;
    un1 = un10 >> 16;
    un0 = un10 & 0xFFFF;

    q1 = un32 / vn1;
    rhat = un32 - q1 * vn1;
    while ((q1 >= b) || (q1 * vn0 > b * rhat + un1)) {
        q1--;
        rhat += vn1;
        if (rhat >= b)
            break;
    }
    un21 = un32 * b + un1 - q1 * v;

    q0 = un21 / vn1;
    rhat = un21 - q0 * vn1;
    while ((q0 >= b) || (q0 * vn0 > b * rhat + un0)) {
        q0--;
        rhat += vn1;
        if (rhat >= b)
            break;
    }
    *r = (un21 * b + un0 - q0 * v) >> s;
    return q1 * b + q0;
}

static unsigned long long amiga_udivmod64(unsigned long long n,
  unsigned long long d, unsigned long long *rem)
{
    unsigned long long q = 0;
    int s;

    if (d == 0) {
        *rem = 0;
        return 0;
    }
    if ((d >> 32) == 0) {
        uint32_t nh = n >> 32, nl = (uint32_t)n, dl = (uint32_t)d;
        uint32_t qh, ql, r;

        if (nh == 0) {
            *rem = nl % dl;
            return nl / dl;
        }
        /* 64/32: high word first, then the 64/32 step with nh < d */
        qh = nh / dl;
        ql = amiga_divlu(nh - qh * dl, nl, dl, &r);
        *rem = r;
        return ((unsigned long long)qh << 32) | ql;
    }
    if (n < d) {
        *rem = n;
        return 0;
    }
    s = __builtin_clzll(d) - __builtin_clzll(n);
    d <<= s;
    for (; s >= 0; s--) {
        q <<= 1;
        if (n >= d) {
            n -= d;
            q |= 1;
        }
        d >>= 1;
    }
    *rem = n;
    return q;
}

static inline int amiga_fits32(long long a, long long b)
{
    return (a == (int32_t)a) && (b == (int32_t)b)
      && !((a == INT32_MIN) && (b == -1));
}

long long __divdi3(long long a, long long b)
{
    unsigned long long q, r;

    if (amiga_fits32(a, b))
        return (int32_t)a / (int32_t)b;
    q = amiga_udivmod64((a < 0) ? -(unsigned long long)a : (unsigned long long)a,
      (b < 0) ? -(unsigned long long)b : (unsigned long long)b, &r);
    return ((a < 0) != (b < 0)) ? -(long long)q : (long long)q;
}

long long __moddi3(long long a, long long b)
{
    unsigned long long r;

    if (amiga_fits32(a, b))
        return (int32_t)a % (int32_t)b;
    amiga_udivmod64((a < 0) ? -(unsigned long long)a : (unsigned long long)a,
      (b < 0) ? -(unsigned long long)b : (unsigned long long)b, &r);
    return (a < 0) ? -(long long)r : (long long)r;
}
