/******************************************************************************/
// Bullfrog Engine Emulation Library - Warp ARM backend
/******************************************************************************/
/** @file sscreen.c
 *     Screen on the Warp ARM: the 68k host opens the 8-bit screen, the frames
 *     and the palette go to it over the link; off-screen surfaces.
 * @par Comment:
 *     The game draws into its own buffer, lbDisplay.WScreen, in the ARM's
 *     SDRAM. LbScreenSwap() registers it for extFetch, sends FRAME and waits
 *     until the 68k has fetched it straight into the screen bitmap.
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "bfscreen.h"
#include "bfscrsurf.h"
#include "bfpalette.h"
#include "bfmemut.h"
#include "bfmouse.h"
#include "bftext.h"
#include "bfwindows.h"
#include "bftime.h"
#include "bfexe_key.h"
#include "bfplanar.h"
#include "svesa.h"
#include "warp_priv.h"
#include "wlibc.h"
#include "wlink.h"
#include "privbflog.h"

static long warp_screen_w, warp_screen_h;

volatile TbBool lbScreenDirectAccessActive = false;
long lbMinScreenSurfaceDimension = 1;
/** Game speed in % for the corner of the screen (src/game_speed.c), sent
 * with each frame to swhost; -1 hides it. */
long lbSpeedMeter = -1;
TbSurfaceDimensions lbScreenSurfaceDimensions = { 320, 200 };
TbBool lbHasSecondSurface = false;
TbBool lbDoubleBufferingRequested = false;
ushort lbEngineBPP = 8;

OSSurfaceHandle lbScreenSurface;
OSSurfaceHandle lbDrawSurface;

ubyte lbPalette[PALETTE_8b_SIZE];
ubyte *lbVesaData;

/******************************************************************************/
// Memory copy helpers (same as the other backends)

void *LbI_XMemCopy(void *dest, void *source, ulong len)
{
    ulong remain;
    ulong *s = (ulong *)source;
    ulong *d = (ulong *)dest;

    for (remain = len >> 2; remain != 0; remain--)
        *d++ = *s++;
    return dest;
}

/** Copies longs in reverse order, swapping bytes within each: the result is
 * the source byte sequence reversed (same as the x86 version). */
void *LbI_XMemCopyFlip(void *dest, void *source, ulong len)
{
    ubyte *s = (ubyte *)source + len;
    ubyte *d = (ubyte *)dest;
    ulong remain;

    for (remain = len & ~3UL; remain != 0; remain--)
        *d++ = *--s;
    return dest;
}

void *LbI_XMemCopyAndSet(void *dest, void *source, ulong val, ulong len)
{
    ulong remain;
    ulong *s = (ulong *)source;
    ulong *d = (ulong *)dest;

    for (remain = len >> 2; remain != 0; remain--) {
        *d++ = *s;
        *s++ = val;
    }
    return dest;
}

void *LbI_XMemRectCopy(void *dest, void *source, ulong lineLen,
  ulong width, ulong height)
{
    ulong remainH, remainW;
    ubyte *s = (ubyte *)source;
    ubyte *d = (ubyte *)dest;

    for (remainH = height; remainH != 0; remainH--)
    {
        for (remainW = width; remainW != 0; remainW--)
            *d++ = *s++;
        s += lineLen - width;
        d += lineLen - width;
    }
    return dest;
}

/******************************************************************************/
// Video modes

TbResult LbScreenSetMinScreenSurfaceDimension(long dim)
{
    lbMinScreenSurfaceDimension = dim;
    return Lb_SUCCESS;
}

void LbRegisterStandardVideoModes(void)
{
    lbScreenModeInfoNum = 0;
    LbRegisterVideoMode("INVALID",       0,    0,  0, Lb_VF_DEFAULT);
    LbRegisterVideoMode("320x200x8",   320,  200,  8, Lb_VF_PALETTE | 19);
    LbRegisterVideoMode("320x200x16",  320,  200, 16, Lb_VF_HICOLOUR);
    LbRegisterVideoMode("320x200x24",  320,  200, 24, Lb_VF_RGBCOLOUR);
    LbRegisterVideoMode("320x240x8",   320,  240,  8, Lb_VF_PALETTE);
    LbRegisterVideoMode("320x240x16",  320,  240, 16, Lb_VF_HICOLOUR);
    LbRegisterVideoMode("320x240x24",  320,  240, 24, Lb_VF_RGBCOLOUR);
    LbRegisterVideoMode("512x384x8",   512,  384,  8, Lb_VF_PALETTE);
    LbRegisterVideoMode("512x384x16",  512,  384, 16, Lb_VF_HICOLOUR);
    LbRegisterVideoMode("512x384x24",  512,  384, 24, Lb_VF_RGBCOLOUR | Lb_VF_VESA | 0);
    LbRegisterVideoMode("640x400x8",   640,  400,  8, Lb_VF_PALETTE);
    LbRegisterVideoMode("640x400x16",  640,  400, 16, Lb_VF_HICOLOUR);
    LbRegisterVideoMode("640x400x24",  640,  400, 24, Lb_VF_RGBCOLOUR | Lb_VF_VESA | 1);
    LbRegisterVideoMode("640x480x8",   640,  480,  8, Lb_VF_PALETTE);
    LbRegisterVideoMode("640x480x16",  640,  480, 16, Lb_VF_HICOLOUR);
    LbRegisterVideoMode("640x480x24",  640,  480, 24, Lb_VF_RGBCOLOUR | Lb_VF_VESA | 3);
    LbRegisterVideoMode("800x600x8",   800,  600,  8, Lb_VF_PALETTE);
    LbRegisterVideoMode("800x600x16",  800,  600, 16, Lb_VF_HICOLOUR);
    LbRegisterVideoMode("800x600x24",  800,  600, 24, Lb_VF_RGBCOLOUR | Lb_VF_VESA | 5);
    LbRegisterVideoMode("1024x768x8", 1024,  768,  8, Lb_VF_PALETTE);
    LbRegisterVideoMode("1024x768x16",1024,  768, 16, Lb_VF_HICOLOUR);
    LbRegisterVideoMode("1024x768x24",1024,  768, 24, Lb_VF_RGBCOLOUR | Lb_VF_VESA | 7);
}

TbBool LbHwCheckIsModeAvailable(TbScreenMode mode)
{
    TbScreenModeInfo *mdinfo;

    mdinfo = LbScreenGetModeInfo(mode);
    return (mdinfo->BitsPerPixel == 8);
}

static TbResult warp_open_display(long width, long height)
{
    uint16_t wh[2];

    if ((warp_screen_w == width) && (warp_screen_h == height))
        return Lb_SUCCESS;
    wh[0] = (uint16_t)width;
    wh[1] = (uint16_t)height;
    wl_send(SWL_SCREEN, wh, sizeof(wh));
    warp_screen_w = width;
    warp_screen_h = height;
    return Lb_SUCCESS;
}

TbResult LbScreenSetupAnyMode(TbScreenMode mode, TbScreenCoord width,
    TbScreenCoord height, ubyte *palette)
{
    TbScreenModeInfo *mdinfo;
    const struct TbSprite *msspr;
    long hot_x, hot_y;

    msspr = NULL;
    hot_x = hot_y = 0;
    LbExeReferenceNumber();
    if (!lbLibInitialised) {
        if (LbBaseInitialise() != Lb_SUCCESS)
            return Lb_FAIL;
    }
    if (lbDisplay.MouseSprite != NULL) {
        msspr = lbDisplay.MouseSprite;
        LbMouseGetSpriteOffset(&hot_x, &hot_y);
    }
    LbMouseChangeSprite(NULL);
    lbScreenInitialised = false;
    if (lbDisplay.OldVideoMode == 0)
        lbDisplay.OldVideoMode = 0xFF;

    mdinfo = LbScreenGetModeInfo(mode);
    if (!LbScreenIsModeAvailable(mode))
        return Lb_FAIL;
    if (warp_open_display(mdinfo->Width, mdinfo->Height) != Lb_SUCCESS)
        return Lb_FAIL;

    lbScreenSurfaceDimensions.Width = mdinfo->Width;
    lbScreenSurfaceDimensions.Height = mdinfo->Height;
    lbScreenSurface = (OSSurfaceHandle)&warp_screen_w;
    lbDrawSurface = (OSSurfaceHandle)&warp_screen_w;
    lbDisplay.VesaIsSetUp = false;
    lbDisplay.GraphicsScreenWidth = width;
    lbDisplay.GraphicsScreenHeight = height;
    lbDisplay.DrawFlags = 0;
    lbDisplay.DrawColour = 0;
#if defined(LB_ENABLE_SHADOW_COLOUR)
    lbDisplay.ShadowColour = 0;
#endif
    lbDisplay.PhysicalScreenWidth = mdinfo->Width;
    lbDisplay.PhysicalScreenHeight = mdinfo->Height;
    lbDisplay.ScreenMode = mode;
    lbDisplay.PhysicalScreen = NULL;
    lbScreenInitialised = true;

    if (palette != NULL) {
        if (LbPaletteSet(palette) != Lb_SUCCESS) {
            LbScreenReset();
            return Lb_FAIL;
        }
    }
    LbScreenSetGraphicsWindow(0, 0, mdinfo->Width, mdinfo->Height);
    LbTextSetWindow(0, 0, mdinfo->Width, mdinfo->Height);
    if (LbMouseIsInstalled())
    {
        LbMouseSetWindow(0, 0, lbDisplay.PhysicalScreenWidth, lbDisplay.PhysicalScreenHeight);
        LbMouseSetPosition(lbDisplay.PhysicalScreenWidth / 2, lbDisplay.PhysicalScreenHeight / 2);
        LbMouseChangeSpriteOffset(hot_x, hot_y);
        if (msspr != NULL)
          LbMouseChangeSprite(msspr);
    }
    return Lb_SUCCESS;
}

TbResult LbScreenClearGraphicsWindow(TbPixel colour)
{
    TbPixel *ptr;
    long h;

    ptr = lbDisplay.GraphicsWindowPtr;
    if (ptr == NULL)
        return Lb_FAIL;
    for (h = lbDisplay.GraphicsWindowHeight; h >= 0; h--)
    {
        LbMemorySet(ptr, colour, lbDisplay.GraphicsWindowWidth);
        ptr += lbDisplay.GraphicsScreenWidth;
    }
    return Lb_SUCCESS;
}

static TbBool warp_early_finish(void);

TbResult LbScreenClear(TbPixel colour)
{
    warp_early_finish();
    if (lbDisplay.WScreen == NULL)
        return Lb_FAIL;
    LbMemorySet(lbDisplay.WScreen, colour, lbDisplay.GraphicsScreenHeight *
      lbDisplay.GraphicsScreenWidth);
    return Lb_SUCCESS;
}

TbResult LbScreenReset(void)
{
    if (!lbScreenInitialised)
        return Lb_FAIL;
    LbMouseSuspend();
    lbScreenSurface = NULL;
    lbDrawSurface = NULL;
    lbScreenInitialised = false;
    // The screen stays open for the next mode setup; closed at exit.
    return Lb_SUCCESS;
}

TbBool LbScreenIsLocked(void)
{
    return (lbDisplay.WScreen != NULL) && lbScreenInitialised;
}

TbResult LbScreenLock(void)
{
    if (!lbScreenInitialised)
        return Lb_FAIL;
    return Lb_SUCCESS;
}

TbResult LbScreenUnlock(void)
{
    if (!lbScreenInitialised)
        return Lb_FAIL;
    return Lb_SUCCESS;
}

TbResult LbScreenSetDoubleBuffering(TbBool state)
{
    lbDoubleBufferingRequested = state;
    return Lb_SUCCESS;
}

TbBool LbScreenIsDoubleBufferred(void)
{
    return false;
}

ulong LbScreenSetWScreenInVideo(ulong flag)
{
    static ulong cur_flag = 0;
    ulong prev_flag;

    prev_flag = cur_flag;
    cur_flag = flag;
    return prev_flag;
}

/******************************************************************************/
// Present: the 68k fetches WScreen straight into its screen bitmap; the ARM
// waits for it (a copy to a second page costs as much as the fetch: the
// SDRAM moves ~50-60 MB/s either way).
// WScreen itself lives in the render area (warp_wscreen_relocate()): stores
// that miss the cache go straight out instead of reading the line first.

void amiga_prof_begin(int slot);
void amiga_prof_end(int slot);
void amiga_prof_frame(void);

uint32_t warp_fetch_us_sum, warp_fetch_frames;

static ubyte *warp_wscreen_game;

ubyte *warp_wscreen_relocate(ubyte *wscreen, ulong bytes)
{
    /* after the file transfer buffer at the start of the render area */
    ubyte *buf = (ubyte *)wl_env->renderArena + SWL_XFER_BYTES;

    if ((wscreen == NULL) || (SWL_XFER_BYTES + bytes > wl_env->renderArenaSize))
        return wscreen;
    warp_wscreen_game = wscreen;
    memset(buf, 0, bytes);
    LOGSYNC("WScreen %p -> render area %p, %lu bytes", wscreen, buf, (unsigned long)bytes);
    return buf;
}

ubyte *warp_wscreen_restore(void)
{
    ubyte *w = warp_wscreen_game;
    warp_wscreen_game = NULL;
    return (w != NULL) ? w : lbDisplay.WScreen;
}

static TbResult warp_present(int wait)
{
    static const ubyte *registered;
    static uint32_t registered_bytes;
    uint32_t bytes, pitch;
    long w, h;

    if ((warp_screen_w == 0) || (lbDisplay.WScreen == NULL))
        return Lb_FAIL;
    w = lbDisplay.GraphicsScreenWidth;
    h = lbDisplay.GraphicsScreenHeight;
    if (w > warp_screen_w)
        w = warp_screen_w;
    if (h > warp_screen_h)
        h = warp_screen_h;
    amiga_prof_frame();
    amiga_prof_begin(30);
    pitch = (uint32_t)lbDisplay.GraphicsScreenWidth;
    bytes = pitch * (uint32_t)h;
    if ((lbDisplay.WScreen != registered) || (bytes > registered_bytes)) {
        if (wl_env->bufRegister(SWL_BUF_SCREEN, lbDisplay.WScreen, (bytes + 15) & ~15u) != 0)
            LOGWARN("bufRegister screen %p %lu failed", lbDisplay.WScreen, (unsigned long)bytes);
        registered = lbDisplay.WScreen;
        registered_bytes = bytes;
    }
    wl_frame_send(SWL_BUF_SCREEN, w, h, pitch, lbSpeedMeter);
    if (wait)
        warp_fetch_us_sum += wl_frames_wait(0);
    warp_fetch_frames++;
    amiga_prof_end(30);
    return Lb_SUCCESS;
}

/* Early present (game loop, SW_WARP): the turn's frame goes to the 68k
 * right after it is drawn, and the 68k fetches it while the ARM runs the
 * simulation and waits for the next turn.  Nothing draws into WScreen in
 * between; the next swap only waits for the fetch to end (and clears). */
static int warp_early_pending;

void warp_present_early(void)
{
    if (warp_early_pending)
        return;
    LbMouseOnBeginSwap();
    if (warp_present(0) == Lb_SUCCESS)
        warp_early_pending = 1;
    else
        LbMouseOnEndSwap();
}

/** Ends an early present; returns true if there was one. */
static TbBool warp_early_finish(void)
{
    if (!warp_early_pending)
        return false;
    amiga_prof_begin(30);
    warp_fetch_us_sum += wl_frames_wait(0);
    amiga_prof_end(30);
    LbMouseOnEndSwap();
    warp_early_pending = 0;
    return true;
}

TbResult LbScreenSwap(void)
{
    TbResult ret;

    warp_early_finish();
    LbMouseOnBeginSwap();
    ret = warp_present(1);
    LbMouseOnEndSwap();
    return ret;
}

TbResult LbScreenSwapClear(TbPixel colour)
{
    TbResult ret;

    if (warp_early_finish())
        ret = Lb_SUCCESS;       /* this frame is on the screen already */
    else
        ret = LbScreenSwap();
    amiga_prof_begin(31);
    LbScreenClear(colour);
    amiga_prof_end(31);
    return ret;
}

TbResult LbScreenWaitVbi(void)
{
    return Lb_SUCCESS;
}

/******************************************************************************/
// Palette

TbResult LbPaletteGet(ubyte *palette)
{
    if (!lbScreenInitialised)
        return Lb_FAIL;
    if (lbDisplay.Palette == NULL)
        return Lb_FAIL;
    memcpy(palette, lbDisplay.Palette, PALETTE_8b_SIZE);
    return Lb_SUCCESS;
}

TbResult LbPaletteSet(const ubyte *palette)
{
    unsigned long i;

    if (!lbScreenInitialised || (palette == NULL))
        return Lb_FAIL;
    // palette and lbPalette may be the same buffer
    for (i = 0; i < PALETTE_8b_COLORS * 3; i++)
        lbPalette[i] = palette[i] & 0x3F;
    wl_send(SWL_PAL, lbPalette, PALETTE_8b_SIZE);
    lbDisplay.Palette = lbPalette;
    return Lb_SUCCESS;
}

/******************************************************************************/
// Off-screen surfaces: plain 8-bit buffers, blitted to/from WScreen

struct AmigaSurf {
    ulong w, h;
    ubyte *pixels;
};

void LbScreenSurfaceInit(struct SSurface *surf)
{
    surf->surf_data = NULL;
    surf->pitch = 0;
    surf->locks_count = 0;
}

TbResult LbScreenSurfaceCreate(struct SSurface *surf, ulong w, ulong h)
{
    struct AmigaSurf *as;

    as = malloc(sizeof(struct AmigaSurf) + w * h);
    if (as == NULL)
        return Lb_FAIL;
    as->w = w;
    as->h = h;
    as->pixels = (ubyte *)(as + 1);
    memset(as->pixels, 0, w * h);
    surf->surf_data = (OSSurfaceHandle)as;
    surf->locks_count = 0;
    surf->pitch = w;
    return Lb_SUCCESS;
}

TbResult LbScreenSurfaceRelease(struct SSurface *surf)
{
    if (surf->surf_data == NULL)
        return Lb_FAIL;
    free(surf->surf_data);
    surf->surf_data = NULL;
    return Lb_SUCCESS;
}

void *LbScreenSurfaceLock(struct SSurface *surf)
{
    struct AmigaSurf *as = (struct AmigaSurf *)surf->surf_data;
    if (as == NULL)
        return NULL;
    surf->locks_count++;
    return as->pixels;
}

TbResult LbScreenSurfaceUnlock(struct SSurface *surf)
{
    if (surf->locks_count > 0)
        surf->locks_count--;
    return Lb_SUCCESS;
}

static void amiga_blit_rect(ubyte *dst, long dpitch, long dw, long dh, long dx, long dy,
  const ubyte *src, long spitch, long sw, long sh, long sx, long sy,
  long w, long h, TbBool transp)
{
    long x, y;

    // clip to both buffers
    if (sx < 0) { w += sx; dx -= sx; sx = 0; }
    if (sy < 0) { h += sy; dy -= sy; sy = 0; }
    if (dx < 0) { w += dx; sx -= dx; dx = 0; }
    if (dy < 0) { h += dy; sy -= dy; dy = 0; }
    if (sx + w > sw) w = sw - sx;
    if (sy + h > sh) h = sh - sy;
    if (dx + w > dw) w = dw - dx;
    if (dy + h > dh) h = dh - dy;
    for (y = 0; y < h; y++)
    {
        const ubyte *s = src + (sy + y) * spitch + sx;
        ubyte *d = dst + (dy + y) * dpitch + dx;
        if (transp) {
            for (x = 0; x < w; x++)
                if (s[x] != 255)
                    d[x] = s[x];
        } else {
            memcpy(d, s, w);
        }
    }
}

TbResult LbScreenSurfaceBlit(struct SSurface *surf, ulong x, ulong y,
    struct TbRect *rect, ulong blflags)
{
    struct AmigaSurf *as = (struct AmigaSurf *)surf->surf_data;
    long w, h;

    if ((as == NULL) || (lbDisplay.WScreen == NULL))
        return Lb_FAIL;
    w = rect->right - rect->left;
    h = rect->bottom - rect->top;
    if ((blflags & SSBlt_TO_SCREEN) != 0) {
        amiga_blit_rect(lbDisplay.WScreen, lbDisplay.GraphicsScreenWidth,
          lbDisplay.GraphicsScreenWidth, lbDisplay.GraphicsScreenHeight, x, y,
          as->pixels, as->w, as->w, as->h, rect->left, rect->top, w, h,
          (blflags & SSBlt_TRANSPRN) != 0);
    } else {
        amiga_blit_rect(as->pixels, as->w, as->w, as->h, rect->left, rect->top,
          lbDisplay.WScreen, lbDisplay.GraphicsScreenWidth,
          lbDisplay.GraphicsScreenWidth, lbDisplay.GraphicsScreenHeight, x, y,
          w, h, (blflags & SSBlt_TRANSPRN) != 0);
    }
    return Lb_SUCCESS;
}

TbResult LbScreenSurfaceBlitScaled(struct SSurface *surf, struct TbRect *scrn_rect,
    struct TbRect *surf_rect, ulong blflags)
{
    // Scaling not supported yet: plain blit of the overlapping part
    return LbScreenSurfaceBlit(surf, scrn_rect->left, scrn_rect->top, surf_rect, blflags);
}

/******************************************************************************/
// VESA: not available

TbResult LbVesaGetGran(long mode) { (void)mode; return Lb_FAIL; }
TbResult LbVesaSetMode(long mode) { (void)mode; return Lb_FAIL; }
TbResult LbVesaSetPage(ushort pageNo) { (void)pageNo; return Lb_FAIL; }
TbResult LbVesaGetInfo(void) { return Lb_FAIL; }
TbBool LbVesaIsModeAvailable(long mode) { (void)mode; return false; }
/******************************************************************************/
