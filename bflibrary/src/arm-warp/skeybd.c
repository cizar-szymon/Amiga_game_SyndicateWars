/******************************************************************************/
// Bullfrog Engine Emulation Library - Warp ARM backend (raw Amiga key codes from the 68k host)
/******************************************************************************/
/** @file skeybd.c
 *     Keyboard: Amiga raw key codes mapped to bflibrary (PC scan code) keys.
 * @par Comment:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include <string.h>
#include "bfkeybd.h"
#include "warp_priv.h"

/** Amiga raw key code (US layout positions) -> KC_* */
static const unsigned char rawkey_map[128] = {
    /* 0x00 */ KC_GRAVE, KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7,
    /* 0x08 */ KC_8, KC_9, KC_0, KC_MINUS, KC_EQUALS, KC_BACKSLASH, 0, KC_NUMPAD0,
    /* 0x10 */ KC_Q, KC_W, KC_E, KC_R, KC_T, KC_Y, KC_U, KC_I,
    /* 0x18 */ KC_O, KC_P, KC_LBRACKET, KC_RBRACKET, 0, KC_NUMPAD1, KC_NUMPAD2, KC_NUMPAD3,
    /* 0x20 */ KC_A, KC_S, KC_D, KC_F, KC_G, KC_H, KC_J, KC_K,
    /* 0x28 */ KC_L, KC_SEMICOLON, KC_APOSTROPHE, KC_BACKSLASH, 0, KC_NUMPAD4, KC_NUMPAD5, KC_NUMPAD6,
    /* 0x30 */ KC_OEM_102, KC_Z, KC_X, KC_C, KC_V, KC_B, KC_N, KC_M,
    /* 0x38 */ KC_COMMA, KC_PERIOD, KC_SLASH, 0, KC_DECIMAL, KC_NUMPAD7, KC_NUMPAD8, KC_NUMPAD9,
    /* 0x40 */ KC_SPACE, KC_BACK, KC_TAB, KC_NUMPADENTER, KC_RETURN, KC_ESCAPE, KC_DELETE, KC_INSERT,
    /* 0x48 */ KC_PGUP, KC_PGDOWN, KC_SUBTRACT, KC_F11, KC_UP, KC_DOWN, KC_RIGHT, KC_LEFT,
    /* 0x50 */ KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6, KC_F7, KC_F8,
    /* 0x58 */ KC_F9, KC_F10, KC_NUMLOCK, KC_SCROLL, KC_DIVIDE, KC_MULTIPLY, KC_ADD, KC_F12,
    /* 0x60 */ KC_LSHIFT, KC_RSHIFT, KC_CAPITAL, KC_LCONTROL, KC_LALT, KC_RALT, KC_LWIN, KC_RWIN,
    /* 0x68 */ 0, 0, 0, KC_APPS, 0, KC_PAUSE, KC_PAUSE, KC_HOME,
    /* 0x70 */ KC_END, 0, 0, 0, 0, 0, 0, 0,
    /* 0x78 */ 0, 0, 0, 0, 0, 0, 0, 0,
};

unsigned char amiga_rawkey_to_kc(unsigned char raw)
{
    return rawkey_map[raw & 0x7F];
}

TbResult LbKeyboardOpen(void)
{
    return LbIKeyboardOpen();
}

TbResult LbKeyboardClose(void)
{
    return LbIKeyboardClose();
}

char LbKeyboard(void)
{
    if (lbInkey < 128)
        return lbInkeyToAscii[lbInkey];
    return 0;
}

TbResult LbIKeyboardOpen(void)
{
    memset(lbKeyOn, 0, 256);
    return 1;
}

TbResult LbIKeyboardClose(void)
{
    return 1;
}
/******************************************************************************/
