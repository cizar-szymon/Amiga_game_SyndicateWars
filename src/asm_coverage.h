/******************************************************************************/
// Syndicate Wars Fan Expansion, Amiga port - diagnostics.
/******************************************************************************/
/** @file asm_coverage.h
 *     Coverage of the remaining x86 assembly, per phase of the game loop.
 *     See asm_coverage.c and tools/amiga/asm_cov_filter.py.
 */
/******************************************************************************/
#ifndef ASM_COVERAGE_H
#define ASM_COVERAGE_H

enum AsmCovPhase {
    ACP_INIT = 0,   /**< before the game loop: loading, mission setup */
    ACP_INPUT,      /**< input(), explosions progress */
    ACP_DRAW,       /**< draw_game(): mission view / menu drawing */
    ACP_THINGS,     /**< process_things(): world simulation */
    ACP_OTHER,      /**< packets, screen swap, the rest of the turn */
    ACP_MENU,       /**< draw_game() while in the purple menu */
    ACP_COUNT,
};

#ifdef SW_ASM_COVERAGE
void asm_cov_phase(int phase);
#elif defined(SW_AMIGA)
/* Amiga port bring-up: trace of the game loop phases (platform/amiga) */
void amiga_phase_trace(int phase);
#define asm_cov_phase(phase) amiga_phase_trace(phase)
#define AMIGA_STEP(n) amiga_phase_trace(100 + (n))
void amiga_prof_begin(int slot);
void amiga_prof_end(int slot);
extern int amiga_prof_level; /* platform/amiga/prof.c */
#define AMIGA_PROF_BEGIN(n) do { if (amiga_prof_level) amiga_prof_begin(n); } while (0)
#define AMIGA_PROF_END(n) do { if (amiga_prof_level) amiga_prof_end(n); } while (0)
#else
#define asm_cov_phase(phase) ((void)0)
#endif
#ifndef AMIGA_STEP
#define AMIGA_STEP(n) ((void)0)
#define AMIGA_PROF_BEGIN(n) ((void)0)
#define AMIGA_PROF_END(n) ((void)0)
#endif

#endif
/******************************************************************************/
