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
#else
#define asm_cov_phase(phase) ((void)0)
#endif

#endif
/******************************************************************************/
