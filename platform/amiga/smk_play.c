/* Smacker videos of the game (intro, elections, outros): play_smk() of
 * bfsmacklib for the Amiga and the Warp ARM builds.
 *
 * 68k: the player smk_core.c (libsmacker) on bflibrary's screen - each frame
 * into the middle of WScreen, the palette, LbScreenSwap(); Esc, Return,
 * Space or the left mouse button end it (unless SMK_NoStopOnUserInput).
 * Warp ARM: the 68060 plays it - swhost gets SWL_SMK_PLAY and plays the
 * file with the same player on its screen and its sound engine, and the
 * game waits for its RESULT (the user's idea).
 *
 * ENV:SyndWarsSmkTest=file (68k, and the ARM's host) plays that file in
 * place of the one asked for (e.g. of the intro), to measure. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bftypes.h"
#include "bfscreen.h"
#include "bfpalette.h"
#include "bfkeybd.h"
#include "bfmouse.h"
#include "bflog.h"
#include "bfsmack.h"

extern short startscr_samplevol;
#define STARTSCR_VOLUME_MAX 322

static unsigned short video_volume(void)
{
    long v = 64L * startscr_samplevol / STARTSCR_VOLUME_MAX;
    if (v < 0)
        v = 0;
    if (v > 64)
        v = 64;
    return (unsigned short)v;
}

#if defined(SW_WARP)
/******************************************************************************/
#include "wlink.h"

TbResult play_smk(char *fname, u32 smkflags, ushort plyflags)
{
    uint16_t hd[2];
    int r;

    (void)smkflags;
    hd[0] = plyflags;
    hd[1] = video_volume();
    r = wl_request(SWL_SMK_PLAY, hd, sizeof(hd), fname, (unsigned)strlen(fname) + 1, NULL);
    lbDisplay.MLeftButton = 0;
    return (r == 0) ? Lb_SUCCESS : Lb_FAIL;
}

#else
/******************************************************************************/
#include <proto/dos.h>
#include "bftime.h"
#include "snd_engine.h"
#include "smk_core.h"

extern int game_hacky_update(void);

static void smk_show(const unsigned char *frame, unsigned long w, unsigned long h,
  unsigned char yscale, const unsigned char *pal6, void *ctx)
{
    long dst_w = lbDisplay.GraphicsScreenWidth;
    long dst_h = lbDisplay.GraphicsScreenHeight;
    long out_h = (yscale != 0) ? (long)h * 2 : (long)h;
    long x, y, cw, ch, sy;
    TbPixel *dst;

    (void)ctx;
    if (lbDisplay.WScreen == NULL)
        return;
    cw = ((long)w < dst_w) ? (long)w : dst_w;
    ch = (out_h < dst_h) ? out_h : dst_h;
    x = (dst_w - cw) / 2;
    y = (dst_h - ch) / 2;
    dst = lbDisplay.WScreen + y * dst_w + x;
    for (sy = 0; sy < ch; sy++) {
        long src_line = (yscale != 0) ? (sy >> 1) : sy;
        if ((yscale == 1) && (sy & 1))      // interlaced: every 2nd line black
            memset(dst, 0, cw);
        else
            memcpy(dst, frame + src_line * w, cw);
        dst += dst_w;
    }
    LbPaletteSet(pal6);
    LbScreenSwap();
}

static void smk_clear(void *ctx)
{
    (void)ctx;
    LbScreenClear(0);
}

static unsigned long smk_clock(void)
{
    return (unsigned long)LbTimerClock();
}

static int smk_idle(void *ctx)
{
    ushort plyflags = *(ushort *)ctx;

    game_hacky_update();
    if ((plyflags & SMK_NoStopOnUserInput) != 0)
        return 0;
    return lbKeyOn[KC_ESCAPE] || lbKeyOn[KC_RETURN] || lbKeyOn[KC_SPACE]
      || lbDisplay.MLeftButton;
}

static void smk_log(const char *text)
{
    LbWarnLog("%s", text);
}

TbResult play_smk(char *fname, u32 smkflags, ushort plyflags)
{
    struct SmkOut out;
    static char t[64];

    (void)smkflags;
    lbDisplay.MLeftButton = 0;
    // test: ENV:SyndWarsSmkTest=file plays that file instead
    if (GetVar((STRPTR)"SyndWarsSmkTest", (STRPTR)t, sizeof(t), 0) > 0)
        fname = t;
    out.show = smk_show;
    out.clear = smk_clear;
    out.clock_ms = smk_clock;
    out.idle = smk_idle;
    out.log = smk_log;
    out.ctx = &plyflags;
    out.sound = (se_backend() != SE_NONE);
    out.volume = video_volume();
    if (smk_core_play(fname, &out) < 0)
        return Lb_FAIL;
    lbDisplay.MLeftButton = 0;
    return Lb_SUCCESS;
}
#endif
