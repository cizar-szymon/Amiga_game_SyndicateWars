/* Bullfrog library, AmigaOS backend - shared private state. */
#ifndef BFLIB_AMIGA_PRIV_H
#define BFLIB_AMIGA_PRIV_H

#include <exec/types.h>
#include <intuition/intuition.h>

extern struct Screen *amiga_screen;
extern struct Window *amiga_window;

/** Process pending IDCMP messages (keyboard, mouse). */
void amiga_process_events(void);

/** Map an Amiga raw key code (0..0x7F) to a bflibrary KC_* code. */
unsigned char amiga_rawkey_to_kc(unsigned char raw);

/** Show the frame rate of the last second (set by sscreen.c). */
extern unsigned long amiga_fps_last;

#endif
