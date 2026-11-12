/* Stack size for the Amiga build (bebbo gcc / libnix).
 * __stack alone does not enlarge the stack: libnix swaps stacks only when
 * swapstack.o (__stkinit) is linked, so it is referenced here. Without it a
 * start from the icon runs with the icon's stack (see PORTING_PLAN.md). */
unsigned long __stack = 512 * 1024;
extern void __stkinit(void);
void (*const amiga_stack_swap)(void) = __stkinit;

/* Version string for the Shell `Version` command. */
const char amiga_version_tag[] = "$VER: SyndWars 0.01 (12.11.2026)";
