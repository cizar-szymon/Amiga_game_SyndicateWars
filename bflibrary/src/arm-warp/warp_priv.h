/* Bullfrog library, Warp ARM backend - shared private state. */
#ifndef BFLIB_WARP_PRIV_H
#define BFLIB_WARP_PRIV_H

/** Handle what the 68k host sent (keyboard, mouse, quit). */
void warp_process_events(void);

/** Map an Amiga raw key code (0..0x7F) to a bflibrary KC_* code. */
unsigned char amiga_rawkey_to_kc(unsigned char raw);

#endif
