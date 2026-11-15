/* Stack size for the Amiga build (bebbo gcc / libnix).
 * __stack alone does not enlarge the stack: libnix swaps stacks only when
 * swapstack.o (__stkinit) is linked, so it is referenced here. Without it a
 * start from the icon runs with the icon's stack (see PORTING_PLAN.md). */
unsigned long __stack = 512 * 1024;
extern void __stkinit(void);
void (*const amiga_stack_swap)(void) = __stkinit;

/* Version string for the Shell `Version` command; the number and the date
 * come from the build (tools/amiga/build_amiga.sh, build/amiga/sw_version.h). */
#include "sw_version.h"
const char amiga_version_tag[] = "$VER: SyndWars " SW_PORT_VERSION " (" SW_PORT_DATE ")";
const char amiga_port_version[] = SW_PORT_VERSION;
