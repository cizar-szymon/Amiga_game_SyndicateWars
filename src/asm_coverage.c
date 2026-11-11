/******************************************************************************/
// Syndicate Wars Fan Expansion, Amiga port - diagnostics.
/******************************************************************************/
/** @file asm_coverage.c
 *     Coverage of the remaining x86 assembly, per phase of the game loop.
 * @par Purpose:
 *     Built with SW_ASM_COVERAGE and tools/amiga/asm_cov_filter.py as ASFILTER,
 *     every asm function label stores 1 into its entry when executed. This file
 *     collects the hits per phase and writes asm_coverage.txt on exit.
 *     Without SW_ASM_COVERAGE all functions are empty.
 * @par Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 3 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "asm_coverage.h"

#ifdef SW_ASM_COVERAGE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

struct AsmCovEntry {
    const char *name;
    volatile uint32_t hit;
    uint32_t mask;
    uint32_t pad;
};

/* The PE linker sorts .data$cova < .data$covb (entries from asm) < .data$covc. */
__attribute__((section(".data$cova"), aligned(16), used))
static struct AsmCovEntry cov_begin[1];
__attribute__((section(".data$covc"), aligned(16), used))
static struct AsmCovEntry cov_end[1];

static int cov_cur_phase = ACP_INIT;
static unsigned long cov_frames;

static const char *phase_names[ACP_COUNT] = {
    "init", "input", "draw", "things", "other", "menu",
};

static void cov_collect(int phase)
{
    struct AsmCovEntry *e;
    for (e = cov_begin + 1; e < cov_end; e++) {
        if (e->name != NULL && e->hit) {
            e->mask |= 1u << phase;
            e->hit = 0;
        }
    }
}

static void cov_dump(void)
{
    struct AsmCovEntry *e;
    FILE *f;
    int i;

    cov_collect(cov_cur_phase);
    f = fopen("asm_coverage.txt", "w");
    if (f == NULL)
        return;
    fprintf(f, "# frames %lu; phases:", cov_frames);
    for (i = 0; i < ACP_COUNT; i++)
        fprintf(f, " %d=%s", i, phase_names[i]);
    fprintf(f, "\n");
    for (e = cov_begin + 1; e < cov_end; e++) {
        if (e->name == NULL)
            continue;
        fprintf(f, "%s", e->name);
        for (i = 0; i < ACP_COUNT; i++)
            if (e->mask & (1u << i))
                fprintf(f, " %s", phase_names[i]);
        fprintf(f, "\n");
    }
    fclose(f);
}

void asm_cov_phase(int phase)
{
    static int registered;
    if (!registered) {
        registered = 1;
        atexit(cov_dump);
    }
    cov_collect(cov_cur_phase);
    cov_cur_phase = phase;
    if (phase == ACP_INPUT)
        cov_frames++;
}

#endif
/******************************************************************************/
