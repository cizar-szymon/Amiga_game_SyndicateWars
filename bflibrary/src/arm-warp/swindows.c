/******************************************************************************/
// Bullfrog Engine Emulation Library - Warp ARM backend
/******************************************************************************/
/** @file swindows.c
 *     Library init and the event loop: keyboard and mouse events the 68k
 *     host sends over the link (its IDCMP messages), quit.
 * @par Comment:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "bfwindows.h"
#include "bfscreen.h"
#include "bfscrsurf.h"
#include "bfkeybd.h"
#include "bfmouse.h"
#include "bfmemut.h"
#include "bfplanar.h"
#include "warp_priv.h"
#include "wlibc.h"
#include "wlink.h"

volatile TbBool lbLibInitialised = false;
volatile TbBool lbUserQuit = false;
volatile TbBool lbAppActive;

TbIdleControl lbIdleHandlers[LB_IDLE_HANDLERS_MAX] = {0};

void LbRegisterStandardVideoModes(void);

TbResult LbBaseInitialise(void)
{
    lbScreenInitialised = false;
    lbScreenSurface = NULL;
    lbDrawSurface = NULL;
    LbMemorySet(lbIdleHandlers, 0, sizeof(lbIdleHandlers));
    lbAppActive = true;
    LbMouseChangeMoveRatio(NORMAL_MOUSE_MOVE_RATIO, NORMAL_MOUSE_MOVE_RATIO);
    if (lbScreenModeInfoNum == 0) {
        LbRegisterStandardVideoModes();
    }
    lbLibInitialised = true;
    return Lb_SUCCESS;
}

TbResult LbInputRestate(void)
{
    return Lb_SUCCESS;
}

static TbKeyMods amiga_key_mods(unsigned qual, TbKeyCode code)
{
    TbKeyMods keymod = KMod_NONE;

    switch (code)
    {
    case KC_LSHIFT: case KC_RSHIFT: case KC_LCONTROL: case KC_RCONTROL:
    case KC_LALT: case KC_RALT: case KC_LWIN: case KC_RWIN:
        return KMod_DONTCARE;
    default:
        break;
    }
    if (qual & SWL_QUAL_CONTROL)
        keymod |= KMod_CONTROL;
    if (qual & (SWL_QUAL_LSHIFT | SWL_QUAL_RSHIFT))
        keymod |= KMod_SHIFT;
    if (qual & (SWL_QUAL_LALT | SWL_QUAL_RALT))
        keymod |= KMod_ALT;
    return keymod;
}

void warp_process_events(void)
{
    WlEvent e;
    struct TbPoint pos;

    wl_poll();
    if (wl_quit)
        lbUserQuit = true;
    while (wl_next_event(&e))
    {
        switch (e.type)
        {
        case SWL_KEY:
            if ((e.a & 0x7F) < 0x68) {
                TbKeyCode kc = amiga_rawkey_to_kc(e.a & 0x7F);
                if (kc != KC_UNASSIGNED)
                    keyboardControl((e.a & 0x80) ? KActn_KEYUP : KActn_KEYDOWN,
                      kc, amiga_key_mods((unsigned)e.b, kc));
            }
            break;
        case SWL_MOUSE:
            pos.x = e.a;
            pos.y = e.b;
            switch (e.c)
            {
            case SWL_MS_MOVE: mouseControl(MActn_MOUSEMOVE, &pos); break;
            case SWL_MS_LDOWN: mouseControl(MActn_LBUTTONDOWN, &pos); break;
            case SWL_MS_LUP: mouseControl(MActn_LBUTTONUP, &pos); break;
            case SWL_MS_RDOWN: mouseControl(MActn_RBUTTONDOWN, &pos); break;
            case SWL_MS_RUP: mouseControl(MActn_RBUTTONUP, &pos); break;
            case SWL_MS_MDOWN: mouseControl(MActn_MBUTTONDOWN, &pos); break;
            case SWL_MS_MUP: mouseControl(MActn_MBUTTONUP, &pos); break;
            }
            break;
        case SWL_ACTIVE:
            lbAppActive = e.a != 0;
            break;
        }
    }
}

/** As Delay(1) on the Amiga: 20 ms, the link kept served. */
void LbDoMultitasking(void)
{
    uint32_t end = w_millis() + 20;
    while ((int32_t)(w_millis() - end) < 0)
        wl_poll();
    LbWindowsControl();
}

TbBool LbWindowsControl(void)
{
    int n;

    warp_process_events();
    for (n = 0; n < LB_IDLE_HANDLERS_MAX; n++) {
        TbIdleControl cb;

        cb = lbIdleHandlers[n];
        if (cb == NULL)
            break;
        cb();
    }
    return !lbUserQuit;
}

TbResult LbRegisterIdleHandler(TbIdleControl cb)
{
    int n;

    for (n = 0; ; n++) {
        if (n >= LB_IDLE_HANDLERS_MAX)
            return Lb_FAIL;
        if (lbIdleHandlers[n] == NULL)
            break;
    }
    lbIdleHandlers[n] = cb;
    return Lb_SUCCESS;
}

TbResult LbBaseReset(void)
{
    lbLibInitialised = false;
    LbMemorySet(lbIdleHandlers, 0, sizeof(lbIdleHandlers));
    return Lb_SUCCESS;
}
/******************************************************************************/
