/* exthello - ARM half: smallest Warp ARM extension used to check the board.
 *
 * Sends the 68k a text with the environment (clock, memory, fast memory),
 * the time of two small benchmarks, registers buffer 0 (64 KB pattern) for
 * extFetch, then echoes every line it receives with each letter upper-cased
 * until it gets "QUIT" or a stop.  Exit code 42.
 * Built with Arm Compiler 6 as a bare-metal PIE (Makefile). */
#include <stdint.h>
#include "WarpExtEnv.h"

void __arm_relocate_pie_(void) { }   /* the firmware relocates */
void __arm_preinit_(void) { }        /* AC 6.13 linker wants it */

static const WarpExtEnv *env;
static char out[2048];
static unsigned outLen;

static void put(const char *s)
{
    while (*s && outLen < sizeof(out) - 1)
        out[outLen++] = *s++;
    out[outLen] = 0;
}

static void putu(uint32_t v)
{
    char b[12];
    int n = 0;
    do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) { char c[2] = { b[--n], 0 }; put(c); }
}

static void putx(uint32_t v)
{
    static const char hx[] = "0123456789abcdef";
    char b[11];
    int i;
    b[0] = '$';
    for (i = 0; i < 8; i++) b[1 + i] = hx[(v >> (28 - 4 * i)) & 15];
    b[9] = 0;
    put(b);
}

/* send all of buf, waiting while the ring is full */
static int send_all(const char *buf, uint32_t len)
{
    while (len) {
        uint32_t n = env->send(buf, len);
        buf += n; len -= n;
        if (env->stopRequested()) return 0;
    }
    return 1;
}

static uint32_t us(uint32_t cyc) { return (uint32_t)((uint64_t)cyc * 1000000u / env->cpuHz); }

int ext_main(const WarpExtEnv *e)
{
    uint8_t *buf;
    uint32_t i, t, acc, n;
    static char line[256];

    env = e;
    env->log("exthello: start\n");

    put("exthello: ARM alive\n");
    put("env version "); putu(env->version); put(", size "); putu(env->size);
    put(", cpu "); putu(env->cpuHz / 1000000u); put(" MHz\n");
    put("arena "); putx((uint32_t)env->arena); put(" "); putu(env->arenaSize / 1024u); put(" KB\n");
    put("render "); putx((uint32_t)env->renderArena); put(" ");
    putu(env->renderArenaSize / 1024u); put(" KB\n");
    if (env->version >= 4) {
        for (i = 0; i < env->fastMemCount; i++) {
            put("fastmem "); putx((uint32_t)env->fastMem[i].base); put(" ");
            putu(env->fastMem[i].size / 1024u); put(" KB flags "); putu(env->fastMem[i].flags);
            put("\n");
        }
    }
    put("args \""); put(env->args ? env->args : ""); put("\"\n");

    /* integer loop: 10M multiply-adds */
    t = env->cycles();
    acc = 1;
    for (i = 0; i < 10000000u; i++)
        acc = acc * 1664525u + 1013904223u;
    t = env->cycles() - t;
    put("10M mul-add: "); putu(us(t)); put(" us (acc "); putx(acc); put(")\n");

    /* 1 MB copy in the arena (SDRAM, cached) */
    {
        uint32_t *a = (uint32_t *)env->arena, *b = a + (1u << 18);
        for (i = 0; i < (1u << 18); i++) a[i] = i;
        t = env->cycles();
        for (i = 0; i < (1u << 18); i++) b[i] = a[i];
        t = env->cycles() - t;
        put("1 MB copy SDRAM: "); putu(us(t)); put(" us\n");
    }

    /* buffer 0: 64 KB pattern in the render area for extFetch */
    buf = (uint8_t *)env->renderArena;
    for (i = 0; i < 65536u; i++)
        buf[i] = (uint8_t)(i * 7u + (i >> 8));
    n = (uint32_t)env->bufRegister(0, buf, 65536u);
    put("bufRegister(0, 64 KB) -> "); putu(n); put("\n.\n");

    if (!send_all(out, outLen))
        return 1;
    env->log("exthello: info sent\n");

    /* echo: lines end with '\n' */
    n = 0;
    for (;;) {
        char c;
        if (env->stopRequested()) { env->log("exthello: stop\n"); return 2; }
        if (!env->waitIn(100)) continue;
        while (env->recv(&c, 1) == 1) {
            if (n < sizeof(line) - 1) line[n++] = c;
            if (c != '\n') continue;
            line[n] = 0;
            if (line[0] == 'Q' && line[1] == 'U' && line[2] == 'I' && line[3] == 'T') {
                send_all("bye\n", 4);
                env->log("exthello: quit\n");
                return 42;
            }
            for (i = 0; i < n; i++)
                if (line[i] >= 'a' && line[i] <= 'z') line[i] -= 32;
            send_all(line, n);
            n = 0;
        }
    }
}
