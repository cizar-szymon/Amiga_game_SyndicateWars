/* Data symbols of the x86 asm files which gen_asm_data.py cannot convert
 * (pointers in the data, private struct types). Amiga build only. */
#include "asm_data_types.h"

/* swars.sx: .long aNoObjective */
const char *scrollinfo_text = "NO OBJECTIVE";

/* bat_s.sx: Breakout minigame levels; the minigame is not part of the
 * Amiga build, its data is left empty (struct BreakoutLevel is private
 * to bat.c, 4-byte name pointer + 120 bricks each). */
ubyte BAT_levels[16 * (4 + 12 * 10)];
