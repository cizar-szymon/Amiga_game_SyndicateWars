/* swhost_snd.h - the sound of the ARM game in its 68k host (swhost_snd.c). */
#ifndef SWHOST_SND_H
#define SWHOST_SND_H

#include <exec/types.h>

/** Handles a SWL_SND_* / SWL_MUS_* record: 0 not one of them, 1 done,
 * 2 done and RESULT res, extra is to be sent. */
int hsnd_handle(UWORD type, const UBYTE *p, UWORD len, LONG *res, LONG *extra);
/** Keeps the voices and the music fed; call after every wake-up. */
void hsnd_service(BOOL timer_signalled);
/** Signal of the 20 ms timer to wait for (0 when no sound). */
ULONG hsnd_wait_mask(void);
void hsnd_close(void);

#endif
