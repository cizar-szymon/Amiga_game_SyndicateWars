/******************************************************************************/
// Bullfrog Engine Emulation Library - AmigaOS backend
/******************************************************************************/
/** @file swindows.c
 *     Library init and the event loop: IDCMP keyboard and mouse messages,
 *     Ctrl-C (quit, needed by AmiAgent stop).
 * @par Comment:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include <exec/types.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <devices/inputevent.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/dos.h>
#include "bfwindows.h"
#include "bfscreen.h"
#include "bfscrsurf.h"
#include "bfkeybd.h"
#include "bfmouse.h"
#include "bfmemut.h"
#include "bfplanar.h"
#include "amiga_priv.h"

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

static TbKeyMods amiga_key_mods(UWORD qual, TbKeyCode code)
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
    if (qual & IEQUALIFIER_CONTROL)
        keymod |= KMod_CONTROL;
    if (qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT))
        keymod |= KMod_SHIFT;
    if (qual & (IEQUALIFIER_LALT | IEQUALIFIER_RALT))
        keymod |= KMod_ALT;
    return keymod;
}

void amiga_process_events(void)
{
    struct IntuiMessage *msg;
    int n;

    if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
        lbUserQuit = true;
    if (amiga_window == NULL)
        return;
    for (n = 256; n != 0; n--)
    {
        ULONG cls;
        UWORD code, qual;
        WORD mx, my;
        struct TbPoint pos;

        msg = (struct IntuiMessage *)GetMsg(amiga_window->UserPort);
        if (msg == NULL)
            break;
        cls = msg->Class;
        code = msg->Code;
        qual = msg->Qualifier;
        mx = msg->MouseX;
        my = msg->MouseY;
        ReplyMsg((struct Message *)msg);

        switch (cls)
        {
        case IDCMP_RAWKEY:
            if ((code & 0x7F) < 0x68) {
                TbKeyCode kc = amiga_rawkey_to_kc(code & 0x7F);
                if (kc != KC_UNASSIGNED)
                    keyboardControl((code & IECODE_UP_PREFIX) ? KActn_KEYUP : KActn_KEYDOWN,
                      kc, amiga_key_mods(qual, kc));
            }
            break;
        case IDCMP_MOUSEMOVE:
            pos.x = mx;
            pos.y = my;
            mouseControl(MActn_MOUSEMOVE, &pos);
            break;
        case IDCMP_MOUSEBUTTONS:
            pos.x = mx;
            pos.y = my;
            switch (code)
            {
            case SELECTDOWN: mouseControl(MActn_LBUTTONDOWN, &pos); break;
            case SELECTUP: mouseControl(MActn_LBUTTONUP, &pos); break;
            case MENUDOWN: mouseControl(MActn_RBUTTONDOWN, &pos); break;
            case MENUUP: mouseControl(MActn_RBUTTONUP, &pos); break;
            case MIDDLEDOWN: mouseControl(MActn_MBUTTONDOWN, &pos); break;
            case MIDDLEUP: mouseControl(MActn_MBUTTONUP, &pos); break;
            }
            break;
        case IDCMP_ACTIVEWINDOW:
            lbAppActive = true;
            break;
        case IDCMP_INACTIVEWINDOW:
            lbAppActive = false;
            break;
        }
    }
}

void LbDoMultitasking(void)
{
    Delay(1);
    LbWindowsControl();
}

TbBool LbWindowsControl(void)
{
    int n;

    amiga_process_events();
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
