/* Smacker videos on the Amiga and the Warp ARM (intro, elections, outros):
 * play_smk() of bfsmacklib with the decoder libsmacker (third_party/
 * libsmacker, LGPL 2.1) in place of RAD's x86 library.
 *
 * As the PC's play_smk_direct(): each frame decoded and put into the middle
 * of WScreen, the palette set when it changes, the screen shown; the video's
 * own rate kept by the clock; Esc, Return, Space or the left mouse button
 * end it (unless SMK_NoStopOnUserInput).  No sound yet.
 *
 * ENV:SyndWarsSmkLog (68k) / the error.log line at the end: frames, how many
 * were late, the decode and copy time per frame - to see how a 68060 does. */
#include <stdio.h>
#include <string.h>

#include "bftypes.h"
#include "bfscreen.h"
#include "bfpalette.h"
#include "bfkeybd.h"
#include "bfmouse.h"
#include "bftime.h"
#include "bflog.h"
#include "bfsmack.h"

#include "../../third_party/libsmacker/smacker.h"

extern int game_hacky_update(void);

static void put_frame(const unsigned char *frame, unsigned long w, unsigned long h,
  unsigned char yscale)
{
    long dst_w = lbDisplay.GraphicsScreenWidth;
    long dst_h = lbDisplay.GraphicsScreenHeight;
    long out_h = (yscale != SMK_FLAG_Y_NONE) ? (long)h * 2 : (long)h;
    long x, y, cw, ch, sy;
    TbPixel *dst;

    if (lbDisplay.WScreen == NULL)
        return;
    cw = ((long)w < dst_w) ? (long)w : dst_w;
    ch = (out_h < dst_h) ? out_h : dst_h;
    x = (dst_w - cw) / 2;
    y = (dst_h - ch) / 2;
    dst = lbDisplay.WScreen + y * dst_w + x;
    for (sy = 0; sy < ch; sy++) {
        long src_line = (yscale != SMK_FLAG_Y_NONE) ? (sy >> 1) : sy;
        if ((yscale == SMK_FLAG_Y_INTERLACE) && (sy & 1))
            memset(dst, 0, cw);
        else
            memcpy(dst, frame + src_line * w, cw);
        dst += dst_w;
    }
}

static TbBool skip_pressed(ushort plyflags)
{
    if ((plyflags & SMK_NoStopOnUserInput) != 0)
        return false;
    return lbKeyOn[KC_ESCAPE] || lbKeyOn[KC_RETURN] || lbKeyOn[KC_SPACE]
      || lbDisplay.MLeftButton;
}

/** Microseconds a frame, from the header's frame rate (as libsmacker reads
 * it, but without a double: they did not come through the 68k build). */
static ulong smk_frame_us(const char *fname)
{
    unsigned char hd[20];
    long rate;
    FILE *f = fopen(fname, "rb");

    if (f == NULL)
        return 100000;
    if (fread(hd, 1, sizeof(hd), f) != sizeof(hd)) {
        fclose(f);
        return 100000;
    }
    fclose(f);
    rate = (long)((ulong)hd[16] | ((ulong)hd[17] << 8) | ((ulong)hd[18] << 16)
      | ((ulong)hd[19] << 24));
    if (rate > 0)
        return (ulong)rate * 1000;
    if (rate < 0)
        return (ulong)(-rate) * 10;
    return 100000;
}

TbResult play_smk(char *fname, u32 smkflags, ushort plyflags)
{
    smk s;
    unsigned long frames, w, h, n;
    unsigned char yscale;
    ulong usf;
    ubyte pal[PALETTE_8b_SIZE];
    const unsigned char *spal;
    TbClockMSec start, now, t0, due;
    ulong late = 0, decode_ms = 0, show_ms = 0, shown = 0;
    TbBool finish = false;
    int i;

    (void)smkflags;
    lbDisplay.MLeftButton = 0;
    s = smk_open_file(fname, SMK_MODE_DISK);
    if (s == NULL) {
        LbWarnLog("smk: cannot open %s\n", fname);
        return Lb_FAIL;
    }
    smk_info_all(s, NULL, &frames, NULL);
    usf = smk_frame_us(fname);
    smk_info_video(s, &w, &h, &yscale);
    smk_enable_all(s, SMK_VIDEO_TRACK);
    LbWarnLog("smk: %s, %lux%lu, %lu frames, %ld us a frame\n", fname, w, h,
      frames, (long)usf);

    LbScreenClear(0);
    start = LbTimerClock();
    t0 = start;
    if (smk_first(s) < 0) {
        smk_close(s);
        return Lb_FAIL;
    }
    for (n = 0; (n < frames) && !finish; n++)
    {
        now = LbTimerClock();
        decode_ms += now - t0;
        t0 = now;
        spal = smk_get_palette(s);
        if (spal != NULL) {
            // libsmacker's palette is 8 bits a colour, the game's 6
            for (i = 0; i < PALETTE_8b_SIZE; i++)
                pal[i] = spal[i] >> 2;
        }
        due = start + (TbClockMSec)(((n + 1) * (usf / 10)) / 100);
        // a frame decoded after its time is not shown: the next comes
        // sooner (frames of Smacker build on the ones before, so all are
        // decoded)
        if ((now <= due) || ((n & 7) == 7)) {
            put_frame(smk_get_video(s), w, h, yscale);
            LbPaletteSet(pal);
            LbScreenSwap();
            shown++;
        } else {
            late++;
        }
        now = LbTimerClock();
        show_ms += now - t0;
        while (LbTimerClock() < due) {
            game_hacky_update();
            if (skip_pressed(plyflags)) {
                finish = true;
                break;
            }
        }
        if (skip_pressed(plyflags))
            finish = true;
        t0 = LbTimerClock();
        if (smk_next(s) < 0)
            break;
    }
    smk_close(s);
    now = LbTimerClock();
    LbWarnLog("smk: %lu of %lu frames in %lu ms, %lu not shown (late); "
      "decode %lu ms, copy+show %lu ms a frame\n", n, frames,
      (ulong)(now - start), late, (n > 0) ? decode_ms / n : 0,
      (shown > 0) ? show_ms / shown : 0);
    lbDisplay.MLeftButton = 0;
    return Lb_SUCCESS;
}
