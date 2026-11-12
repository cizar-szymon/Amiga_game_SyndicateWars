/* Stand-ins for x86 asm functions not remade in C yet (Amiga build).
 * tools/amiga/m68k_prep.py replaces each remaining x86 inline asm statement
 * with asm_stub_hit("ASM_name"); the first call of each name is logged.
 */
#ifndef SW_AMIGA_ASM_STUBS_H
#define SW_AMIGA_ASM_STUBS_H

void asm_stub_hit(const char *name);

#endif
