/******************************************************************************/
// Syndicate Wars Fan Expansion, Amiga port - diagnostics.
/******************************************************************************/
/** @file fncheck.c
 *     Differential check of a remade C function against the original asm.
 *     See fncheck.h.
 * @par Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 3 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "fncheck.h"

#ifdef SW_FNCHK
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bfmemory.h"
#include "game_data.h"
#include "thing.h"

/* Program sections (GNU ld PE script). */
extern char fnchk_data_start[] __asm__("__data_start__");
extern char fnchk_data_end[] __asm__("__data_end__");
extern char fnchk_bss_start[] __asm__("__bss_start__");
extern char fnchk_bss_end[] __asm__("__bss_end__");

extern void *engine_mem_alloc_ptr;
extern u32 engine_mem_alloc_size;

#define FNCHK_MAX_FUNCS 128
#define FNCHK_MAX_REPORTS 3   /* detailed reports per function */
#define FNCHK_MAX_RANGES 16   /* differing ranges listed per report */

struct FnStat {
    const char *name;
    unsigned long calls;
    unsigned long fails;
};

struct Region {
    char *base;
    size_t len;
    const char *name;
};

/* All mutable checker state lives on the heap, outside the compared memory,
 * except the pointer below and `depth`, which are equal in all compared states. */
struct FnChk {
    struct Region reg[3];
    int nreg;
    char *before[3];
    char *result[3];
    struct FnStat stat[FNCHK_MAX_FUNCS];
    int nstat;
    FILE *log;
};

static struct FnChk *chk;
static int depth;

static void fnchk_summary(void);

static void fnchk_init(void)
{
    int i;

    chk = calloc(1, sizeof(*chk));
    chk->log = fopen("fncheck.log", "w");
    atexit(fnchk_summary);
    fprintf(chk->log, "fncheck: data %p-%p bss %p-%p engine %p+%lu\n",
        fnchk_data_start, fnchk_data_end, fnchk_bss_start, fnchk_bss_end,
        engine_mem_alloc_ptr, (unsigned long)engine_mem_alloc_size);
    chk->reg[0].base = fnchk_data_start;
    chk->reg[0].len = fnchk_data_end - fnchk_data_start;
    chk->reg[0].name = "data";
    chk->reg[1].base = fnchk_bss_start;
    chk->reg[1].len = fnchk_bss_end - fnchk_bss_start;
    chk->reg[1].name = "bss";
    chk->nreg = 2;
    for (i = 0; i < chk->nreg; i++) {
        chk->before[i] = malloc(chk->reg[i].len);
        chk->result[i] = malloc(chk->reg[i].len);
    }
    fflush(chk->log);
}

/* The engine block is allocated after startup; add it once it exists. */
static void fnchk_update_regions(void)
{
    if (chk->nreg == 2 && engine_mem_alloc_ptr != NULL) {
        chk->reg[2].base = engine_mem_alloc_ptr;
        chk->reg[2].len = engine_mem_alloc_size;
        chk->reg[2].name = "engine";
        chk->before[2] = malloc(chk->reg[2].len);
        chk->result[2] = malloc(chk->reg[2].len);
        chk->nreg = 3;
    }
}

static struct FnStat *fnchk_stat(const char *name)
{
    int i;
    for (i = 0; i < chk->nstat; i++)
        if (strcmp(chk->stat[i].name, name) == 0)
            return &chk->stat[i];
    if (chk->nstat >= FNCHK_MAX_FUNCS)
        return &chk->stat[FNCHK_MAX_FUNCS - 1];
    chk->stat[chk->nstat].name = name;
    return &chk->stat[chk->nstat++];
}

TbBool fnchk_begin(void)
{
    int i;

    if (depth != 0)
        return false;
    if (chk == NULL)
        fnchk_init();
    fnchk_update_regions();
    depth = 1;
    for (i = 0; i < chk->nreg; i++)
        memcpy(chk->before[i], chk->reg[i].base, chk->reg[i].len);
    return true;
}

void fnchk_mid(void)
{
    int i;
    for (i = 0; i < chk->nreg; i++) {
        memcpy(chk->result[i], chk->reg[i].base, chk->reg[i].len);
        memcpy(chk->reg[i].base, chk->before[i], chk->reg[i].len);
    }
}

/* Describe an address inside the engine block: mem_game buffer, things, sthings. */
static void describe_engine(char *addr, char *out, size_t outlen)
{
    int i;
    char *b;

    b = (char *)things;
    if (b != NULL && addr >= b && addr < b + THINGS_LIMIT * sizeof(struct Thing)) {
        size_t off = addr - b;
        snprintf(out, outlen, "things[%u]+%u", (unsigned)(off / sizeof(struct Thing)),
            (unsigned)(off % sizeof(struct Thing)));
        return;
    }
    b = (char *)sthings;
    if (b != NULL && addr >= b && addr < b + STHINGS_LIMIT * sizeof(struct SimpleThing)) {
        size_t off = addr - b;
        snprintf(out, outlen, "sthings[%u]+%u", (unsigned)(off / sizeof(struct SimpleThing)),
            (unsigned)(off % sizeof(struct SimpleThing)));
        return;
    }
    for (i = 0; mem_game[i].Name != NULL; i++) {
        MemSystem *m = &mem_game[i];
        char *p = *(char **)m->BufferPtr;
        size_t len = (size_t)m->ESize * m->N;
        if (p != NULL && addr >= p && addr < p + len) {
            size_t off = addr - p;
            snprintf(out, outlen, "%s[%u]+%u", m->Name, (unsigned)(off / m->ESize),
                (unsigned)(off % m->ESize));
            return;
        }
    }
    snprintf(out, outlen, "engine+0x%x", (unsigned)(addr - (char *)engine_mem_alloc_ptr));
}

TbBool fnchk_end(const char *name, long ret_asm, long ret_c)
{
    struct FnStat *st;
    int i, nranges;
    unsigned long ndiff;
    TbBool report;

    st = fnchk_stat(name);
    st->calls++;
    ndiff = 0;
    nranges = 0;
    report = (st->fails < FNCHK_MAX_REPORTS);
    for (i = 0; i < chk->nreg; i++) {
        struct Region *r = &chk->reg[i];
        char *a = chk->result[i];
        char *c = r->base;
        size_t k, start;
        if (memcmp(a, c, r->len) == 0)
            continue;
        k = 0;
        while (k < r->len) {
            if (a[k] == c[k]) { k++; continue; }
            start = k;
            while (k < r->len && a[k] != c[k])
                k++;
            ndiff += k - start;
            if (report && nranges < FNCHK_MAX_RANGES) {
                char desc[64];
                if (i == 2)
                    describe_engine(r->base + start, desc, sizeof(desc));
                else
                    snprintf(desc, sizeof(desc), "%s", r->name);
                if (nranges == 0)
                    fprintf(chk->log, "MISMATCH %s call %lu:\n", name, st->calls);
                fprintf(chk->log, "  %p len %u %s asm:", (void *)(r->base + start),
                    (unsigned)(k - start), desc);
                {
                    size_t j;
                    for (j = start; j < k && j < start + 8; j++)
                        fprintf(chk->log, " %02x", (unsigned char)a[j]);
                    fprintf(chk->log, " c:");
                    for (j = start; j < k && j < start + 8; j++)
                        fprintf(chk->log, " %02x", (unsigned char)c[j]);
                }
                fprintf(chk->log, "\n");
            }
            nranges++;
        }
    }
    if (ret_asm != ret_c) {
        if (report)
            fprintf(chk->log, "MISMATCH %s call %lu: result asm %ld c %ld\n",
                name, st->calls, ret_asm, ret_c);
        ndiff++;
    }
    if (ndiff != 0) {
        st->fails++;
        if (report)
            fprintf(chk->log, "  %lu bytes in %d ranges differ\n", ndiff, nranges);
        fflush(chk->log);
    }
    depth = 0;
    return (ndiff == 0);
}

void fnchk_pure_result(const char *name, long ret_asm, long ret_c)
{
    struct FnStat *st;

    if (chk == NULL)
        fnchk_init();
    st = fnchk_stat(name);
    st->calls++;
    if (ret_asm != ret_c) {
        if (st->fails < FNCHK_MAX_REPORTS) {
            fprintf(chk->log, "MISMATCH %s call %lu: result asm %ld c %ld\n",
                name, st->calls, ret_asm, ret_c);
            fflush(chk->log);
        }
        st->fails++;
    }
}

static void fnchk_summary(void)
{
    int i;

    fprintf(chk->log, "SUMMARY\n");
    for (i = 0; i < chk->nstat; i++)
        fprintf(chk->log, "  %-40s calls %8lu  fails %lu\n", chk->stat[i].name,
            chk->stat[i].calls, chk->stat[i].fails);
    fclose(chk->log);
}

#endif
/******************************************************************************/
