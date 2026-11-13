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
    {
        cov_frames++;
        // also written every 500 frames: a run stopped by a timeout leaves it too
        if ((cov_frames % 500) == 0)
            cov_dump();
    }
}

#endif

#ifdef SW_PCPROF
/* PC frame profiler (build_x86.sh prof): slots as in platform/amiga/prof.c,
 * average per game turn written to error.log every 5 s. */
#include <windows.h>
#include "bflog.h"

#define PC_PROF_SLOTS 48

static const char *const pc_prof_names[PC_PROF_SLOTS] = {
    [0] = "frame",
    [1] = "input", [2] = "draw", [3] = "things", [4] = "other", [5] = "swap",
    [6] = "scan_blips", [7] = "scan_arcs", [8] = "scan_dots",
    [10] = "view_inputs", [11] = "scanner_zoom+cam", [12] = "prepare_drawlist",
    [13] = "mouse_on_map", [14] = "engine_unk3",
    [15] = "scanner_turn", [16] = "scanner_map", [17] = "scanner_signals",
    [18] = "weapons_panel", [19] = "chat+pickable",
    [20] = "camera_setup", [21] = "draw_things", [22] = "floor",
    [23] = "explode", [24] = "draw_screen", [25] = "hud",
    [26] = "hud_goto+lock", [27] = "hud_agents+target", [28] = "hud_panel", [29] = "scanner",
    [30] = "present_copy", [31] = "clear",
};

static LONGLONG pc_prof_start[PC_PROF_SLOTS];
static LONGLONG pc_prof_sum[PC_PROF_SLOTS];
static LONGLONG pc_prof_max[PC_PROF_SLOTS];
static LONGLONG pc_prof_report;
static unsigned long pc_prof_turns;

static LONGLONG pc_prof_now(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

void pc_prof_begin(int slot)
{
    if ((slot < 0) || (slot >= PC_PROF_SLOTS))
        return;
    pc_prof_start[slot] = pc_prof_now();
    if (slot == 0)
    {
        LARGE_INTEGER f;
        int i;

        QueryPerformanceFrequency(&f);
        if (pc_prof_report == 0)
            pc_prof_report = pc_prof_start[0];
        pc_prof_turns++;
        if (pc_prof_start[0] - pc_prof_report < 5 * f.QuadPart)
            return;
        LbWarnLog("prof: %lu turns in %lu ms\n", pc_prof_turns,
          (unsigned long)((pc_prof_start[0] - pc_prof_report) * 1000 / f.QuadPart));
        for (i = 0; i < PC_PROF_SLOTS; i++)
        {
            if (pc_prof_sum[i] == 0)
                continue;
            /* average and maximum per turn, in microseconds */
            LbWarnLog("prof:   %-18s %7lu us avg %7lu us max\n",
              pc_prof_names[i] ? pc_prof_names[i] : "?",
              (unsigned long)(pc_prof_sum[i] * 1000000 / f.QuadPart / pc_prof_turns),
              (unsigned long)(pc_prof_max[i] * 1000000 / f.QuadPart));
            pc_prof_sum[i] = 0;
            pc_prof_max[i] = 0;
        }
        pc_prof_turns = 0;
        pc_prof_report = pc_prof_start[0];
    }
}

void pc_prof_end(int slot)
{
    LONGLONG d;

    if ((slot < 0) || (slot >= PC_PROF_SLOTS) || (pc_prof_start[slot] == 0))
        return;
    d = pc_prof_now() - pc_prof_start[slot];
    pc_prof_sum[slot] += d;
    if (pc_prof_max[slot] < d)
        pc_prof_max[slot] = d;
}
#endif
/******************************************************************************/
