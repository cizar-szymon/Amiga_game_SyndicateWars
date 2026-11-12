/******************************************************************************/
// Bullfrog Engine Emulation Library - AmigaOS backend
/******************************************************************************/
/** @file sscreen.c
 *     Screen: RTG 8-bit (CLUT) screen through CyberGraphX/Picasso96, palette,
 *     off-screen surfaces.
 * @par Comment:
 *     The game draws into its own buffer, lbDisplay.WScreen, in Fast RAM.
 *     LbScreenSwap() copies it to the locked screen bitmap on the graphics
 *     card with long word writes only (never reads VRAM).
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfx.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/dos.h>
#include <proto/cybergraphics.h>

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
#include "amiga_priv.h"
#include "privbflog.h"

struct Library *CyberGfxBase;

struct Screen *amiga_screen;
struct Window *amiga_window;
static UWORD *amiga_blank_pointer;

volatile TbBool lbScreenDirectAccessActive = false;
long lbMinScreenSurfaceDimension = 1;
TbSurfaceDimensions lbScreenSurfaceDimensions = { 320, 200 };
TbBool lbHasSecondSurface = false;
TbBool lbDoubleBufferingRequested = false;
ushort lbEngineBPP = 8;

OSSurfaceHandle lbScreenSurface;
OSSurfaceHandle lbDrawSurface;

ubyte lbPalette[PALETTE_8b_SIZE];
ubyte *lbVesaData;

unsigned long amiga_fps_last;
static unsigned long fps_frames;
static TbClockMSec fps_start;
static TbClockMSec fps_log_start;
static unsigned long fps_log_frames;
static int fps_log_enabled = -1;

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

static ULONG amiga_find_mode(long width, long height)
{
    if (CyberGfxBase == NULL)
        CyberGfxBase = OpenLibrary((STRPTR)"cybergraphics.library", 40);
    if (CyberGfxBase == NULL)
        return INVALID_ID;
    return BestCModeIDTags(CYBRBIDTG_NominalWidth, width,
      CYBRBIDTG_NominalHeight, height, CYBRBIDTG_Depth, 8, TAG_DONE);
}

TbBool LbHwCheckIsModeAvailable(TbScreenMode mode)
{
    TbScreenModeInfo *mdinfo;

    mdinfo = LbScreenGetModeInfo(mode);
    if (mdinfo->BitsPerPixel != 8)
        return false;
    return (amiga_find_mode(mdinfo->Width, mdinfo->Height) != INVALID_ID);
}

static void amiga_close_display(void)
{
    if (amiga_window != NULL) {
        ClearPointer(amiga_window);
        CloseWindow(amiga_window);
        amiga_window = NULL;
    }
    if (amiga_screen != NULL) {
        CloseScreen(amiga_screen);
        amiga_screen = NULL;
    }
    if (amiga_blank_pointer != NULL) {
        FreeMem(amiga_blank_pointer, 6 * sizeof(UWORD));
        amiga_blank_pointer = NULL;
    }
}

static void amiga_exit_cleanup(void)
{
    amiga_close_display();
    if (CyberGfxBase != NULL) {
        CloseLibrary(CyberGfxBase);
        CyberGfxBase = NULL;
    }
}

static TbResult amiga_open_display(long width, long height)
{
    static int atexit_set;
    ULONG modeid;

    if (!atexit_set) {
        atexit(amiga_exit_cleanup);
        atexit_set = 1;
    }
    if ((amiga_screen != NULL) && (amiga_screen->Width == width)
      && (amiga_screen->Height == height))
        return Lb_SUCCESS;
    amiga_close_display();

    modeid = amiga_find_mode(width, height);
    if (modeid == INVALID_ID)
        return Lb_FAIL;
    amiga_screen = OpenScreenTags(NULL,
      SA_DisplayID, modeid, SA_Width, width, SA_Height, height, SA_Depth, 8,
      SA_Quiet, TRUE, SA_ShowTitle, FALSE, SA_Type, CUSTOMSCREEN,
      SA_Title, (ULONG)"Syndicate Wars", TAG_DONE);
    if (amiga_screen == NULL)
        return Lb_FAIL;
    amiga_window = OpenWindowTags(NULL,
      WA_CustomScreen, (ULONG)amiga_screen, WA_Left, 0, WA_Top, 0,
      WA_Width, width, WA_Height, height,
      WA_Backdrop, TRUE, WA_Borderless, TRUE, WA_Activate, TRUE,
      WA_RMBTrap, TRUE, WA_ReportMouse, TRUE, WA_NoCareRefresh, TRUE,
      WA_SimpleRefresh, TRUE, WA_Title, (ULONG)"Syndicate Wars",
      WA_IDCMP, IDCMP_RAWKEY | IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS
        | IDCMP_ACTIVEWINDOW | IDCMP_INACTIVEWINDOW,
      TAG_DONE);
    if (amiga_window == NULL) {
        amiga_close_display();
        return Lb_FAIL;
    }
    // The game draws its own pointer into the frame
    amiga_blank_pointer = AllocMem(6 * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
    if (amiga_blank_pointer != NULL)
        SetPointer(amiga_window, amiga_blank_pointer, 1, 16, 0, 0);
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
    if (amiga_open_display(mdinfo->Width, mdinfo->Height) != Lb_SUCCESS)
        return Lb_FAIL;

    lbScreenSurfaceDimensions.Width = mdinfo->Width;
    lbScreenSurfaceDimensions.Height = mdinfo->Height;
    lbScreenSurface = (OSSurfaceHandle)amiga_screen;
    lbDrawSurface = (OSSurfaceHandle)amiga_screen;
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

TbResult LbScreenClear(TbPixel colour)
{
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
// Frame rate display: 3x5 digits, drawn into the card's bitmap after the copy

static const UBYTE fps_font[10][5] = {
    {7,5,5,5,7}, {2,6,2,2,7}, {7,1,7,4,7}, {7,1,7,1,7}, {5,5,7,1,1},
    {7,4,7,1,7}, {7,4,7,5,7}, {7,1,1,1,1}, {7,5,7,5,7}, {7,5,7,1,7},
};

static void fps_draw(UBYTE *base, ULONG bpr, unsigned long value)
{
    char txt[8];
    int i, n, y, x;

    n = snprintf(txt, sizeof(txt), "%lu", value);
    for (i = 0; i < n; i++)
    {
        const UBYTE *g = fps_font[txt[i] - '0'];
        for (y = 0; y < 5; y++)
        {
            UBYTE *row = base + (2 + y * 2) * bpr + 2 + i * 8;
            for (x = 0; x < 3; x++)
            {
                UBYTE c = (g[y] & (4 >> x)) ? 255 : 0;
                row[x * 2] = c; row[x * 2 + 1] = c;
                row[bpr + x * 2] = c; row[bpr + x * 2 + 1] = c;
            }
        }
    }
}

void amiga_prof_begin(int slot);
void amiga_prof_end(int slot);
void amiga_prof_frame(void);

static void fps_count(void)
{
    TbClockMSec now;

    static unsigned long total_frames;
    static TbClockMSec diag_last;

    now = LbTimerClock();
    fps_frames++;
    total_frames++;
    if ((now - diag_last >= 5000) || (total_frames <= 3)) {
        LOGWARN("present %lu, clock %lu ms, fps %lu", total_frames,
          (unsigned long)now, amiga_fps_last);
        diag_last = now;
    }
    if (now - fps_start >= 1000) {
        amiga_fps_last = (fps_frames * 1000 + (now - fps_start) / 2) / (now - fps_start);
        fps_frames = 0;
        fps_start = now;
    }
    if (fps_log_enabled < 0) {
        char buf[4];
        fps_log_enabled = (GetVar((STRPTR)"SyndWarsFPS", (STRPTR)buf, sizeof(buf), 0) >= 0);
        fps_log_start = now;
    }
    if (fps_log_enabled) {
        fps_log_frames++;
        if (now - fps_log_start >= 5000) {
            FILE *f = fopen("RAM:SyndWars-fps.txt", "a");
            if (f != NULL) {
                unsigned long t10 = (fps_log_frames * 10000) / (now - fps_log_start);
                fprintf(f, "%lu ms: %lu.%lu fps\n", (unsigned long)now, t10 / 10, t10 % 10);
                fclose(f);
            }
            fps_log_frames = 0;
            fps_log_start = now;
        }
    }
}

/** Copy WScreen to the card. Only long word writes to VRAM. */
static TbResult amiga_present(void)
{
    APTR lock;
    UBYTE *base = NULL;
    ULONG bpr = 0;
    ULONG w, h, y;
    const UBYTE *src;

    if ((amiga_screen == NULL) || (lbDisplay.WScreen == NULL))
        return Lb_FAIL;
    w = lbDisplay.GraphicsScreenWidth;
    h = lbDisplay.GraphicsScreenHeight;
    if (w > (ULONG)amiga_screen->Width)
        w = amiga_screen->Width;
    if (h > (ULONG)amiga_screen->Height)
        h = amiga_screen->Height;
    fps_count();
    amiga_prof_frame();
    amiga_prof_begin(30);
    lock = LockBitMapTags(amiga_screen->RastPort.BitMap,
      LBMI_BASEADDRESS, (ULONG)&base, LBMI_BYTESPERROW, (ULONG)&bpr, TAG_DONE);
    if (lock == NULL)
        return Lb_FAIL;
    src = lbDisplay.WScreen;
    {
        static const UBYTE *logged_src;
        if (src != logged_src) {
            logged_src = src;
            LbWarnLog("present: WScreen %08lx (mod 16 = %lu), VRAM %08lx, bpr %lu\n",
              (ULONG)src, (ULONG)src & 15, (ULONG)base, bpr);
        }
    }
    if ((bpr == w) && ((ULONG)lbDisplay.GraphicsScreenWidth == w))
    {
        // One block: a large CopyMem bursts (55 MB/s on a Warp, WarpBench),
        // much faster than a copy loop per row
        CopyMemQuick((APTR)src, (APTR)base, w * h);
        h = 0;
    }
    for (y = 0; y < h; y++)
    {
        const ULONG *s = (const ULONG *)src;
        ULONG *d = (ULONG *)(base + y * bpr);
        ULONG n;

        for (n = w >> 4; n != 0; n--) {
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
            d += 4; s += 4;
        }
        for (n = (w >> 2) & 3; n != 0; n--)
            *d++ = *s++;
        src += lbDisplay.GraphicsScreenWidth;
    }
    fps_draw(base, bpr, amiga_fps_last);
    UnLockBitMap(lock);
    amiga_prof_end(30);
    return Lb_SUCCESS;
}

TbResult LbScreenSwap(void)
{
    TbResult ret;

    LbMouseOnBeginSwap();
    ret = amiga_present();
    LbMouseOnEndSwap();
    return ret;
}

TbResult LbScreenSwapClear(TbPixel colour)
{
    TbResult ret;

    ret = LbScreenSwap();
    amiga_prof_begin(31);
    LbScreenClear(colour);
    amiga_prof_end(31);
    return ret;
}

TbResult LbScreenWaitVbi(void)
{
    if (amiga_screen != NULL)
        WaitTOF();
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
    static ULONG table[1 + PALETTE_8b_COLORS * 3 + 1];
    ULONG i;

    if (!lbScreenInitialised || (palette == NULL))
        return Lb_FAIL;
    table[0] = ((ULONG)PALETTE_8b_COLORS << 16) | 0;
    for (i = 0; i < PALETTE_8b_COLORS * 3; i++)
    {
        ULONG v6, v8;

        // palette and lbPalette may be the same buffer
        v6 = palette[i] & 0x3F;
        lbPalette[i] = v6;
        v8 = (v6 << 2) | (v6 >> 4);
        table[1 + i] = v8 * 0x01010101UL;
    }
    table[1 + PALETTE_8b_COLORS * 3] = 0;
    if (amiga_screen != NULL)
        LoadRGB32(&amiga_screen->ViewPort, table);
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
