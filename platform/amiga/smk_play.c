/* Smacker videos on the Amiga and the Warp ARM (intro, elections, outros):
 * play_smk() of bfsmacklib with the decoder libsmacker (third_party/
 * libsmacker, LGPL 2.1) in place of RAD's x86 library.
 *
 * As the PC's play_smk_direct(): each frame decoded and put into the middle
 * of WScreen, the palette set when it changes, the screen shown; the video's
 * own rate kept by the clock; Esc, Return, Space or the left mouse button
 * end it (unless SMK_NoStopOnUserInput).
 *
 * Sound (68k): the video's track 0 (16-bit, 22 kHz) made 8-bit and put in a
 * queue, which the music voices of the sound engine take as a stream (Paula
 * or AHI); the game's music is put aside meanwhile.  The Warp ARM: no sound
 * of the videos yet.
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

#if !defined(SW_WARP)
#include "snd_engine.h"

extern short startscr_samplevol;
#define STARTSCR_VOLUME_MAX 322

/* the queue of the video's sound: 8-bit signed, left and right */
#define AQ_SIZE 65536
#define AQ_MASK (AQ_SIZE - 1)
#define AQ_BLOCK 2048
static signed char aq_l[AQ_SIZE], aq_r[AQ_SIZE];
static volatile ulong aq_w, aq_r_pos;
static ulong aq_under;

static void aq_push(smk s, unsigned char ch, unsigned char bits)
{
    const unsigned char *a = smk_get_audio(s, 0);
    ulong size = smk_get_audio_size(s, 0);
    ulong n, i, w = aq_w;

    if ((a == NULL) || (size == 0))
        return;
    if (bits == 16) {
        const short *v = (const short *)a;
        n = size / (2 * ch);
        for (i = 0; i < n; i++, w++) {
            if (w - aq_r_pos >= AQ_SIZE)
                break;          // full: the rest dropped
            aq_l[w & AQ_MASK] = (signed char)(v[i * ch] >> 8);
            aq_r[w & AQ_MASK] = (signed char)(v[i * ch + ch - 1] >> 8);
        }
    } else {
        // 8-bit Smacker sound is unsigned
        n = size / ch;
        for (i = 0; i < n; i++, w++) {
            if (w - aq_r_pos >= AQ_SIZE)
                break;
            aq_l[w & AQ_MASK] = (signed char)(a[i * ch] ^ 0x80);
            aq_r[w & AQ_MASK] = (signed char)(a[i * ch + ch - 1] ^ 0x80);
        }
    }
    aq_w = w;
}

/** The engine's stream source: a block of the queue, silence when empty. */
static void aq_fill(UBYTE *dst, ULONG block, void *ctx)
{
    ulong i, r = aq_r_pos;

    (void)ctx;
    for (i = 0; i < block; i++) {
        if (r != aq_w) {
            dst[i] = (UBYTE)aq_l[r & AQ_MASK];
            dst[block + i] = (UBYTE)aq_r[r & AQ_MASK];
            r++;
        } else {
            dst[i] = 0;
            dst[block + i] = 0;
            aq_under++;
        }
    }
    aq_r_pos = r;
}
#endif

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
#if !defined(SW_WARP)
    unsigned char amask, ach[7], abits[7];
    unsigned long arate[7];
    struct SeMusicState mstate;
    TbBool sound = false;
#endif

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
#if !defined(SW_WARP)
    smk_info_audio(s, &amask, ach, abits, arate);
    if ((amask & SMK_AUDIO_TRACK_0) && (se_backend() != SE_NONE) &&
      (ach[0] >= 1) && (ach[0] <= 2) && ((abits[0] == 8) || (abits[0] == 16)))
        sound = true;
    smk_enable_all(s, SMK_VIDEO_TRACK | (sound ? SMK_AUDIO_TRACK_0 : 0));
#else
    smk_enable_all(s, SMK_VIDEO_TRACK);
#endif
    LbWarnLog("smk: %s, %lux%lu, %lu frames, %ld us a frame\n", fname, w, h,
      frames, (long)usf);

    LbScreenClear(0);
    start = LbTimerClock();
    t0 = start;
    if (smk_first(s) < 0) {
        smk_close(s);
        return Lb_FAIL;
    }
#if !defined(SW_WARP)
    if (sound) {
        aq_w = aq_r_pos = 0;
        aq_under = 0;
        aq_push(s, ach[0], abits[0]);
        se_music_suspend(&mstate);
        if (se_stream_open(arate[0], AQ_BLOCK, aq_fill, NULL))
            se_music_play((UWORD)(64L * startscr_samplevol / STARTSCR_VOLUME_MAX));
        else
            sound = false;
    }
#endif
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
#if !defined(SW_WARP)
            if (sound)
                se_music_update();
#endif
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
#if !defined(SW_WARP)
        if (sound) {
            aq_push(s, ach[0], abits[0]);
            se_music_update();
        }
#endif
    }
    smk_close(s);
#if !defined(SW_WARP)
    if (sound) {
        se_music_close();
        se_music_resume(&mstate);
        LbWarnLog("smk: sound %lu Hz, %u ch, %u bit; %lu samples short\n",
          arate[0], ach[0], abits[0], aq_under);
    }
#endif
    now = LbTimerClock();
    LbWarnLog("smk: %lu of %lu frames in %lu ms, %lu not shown (late); "
      "decode %lu ms, copy+show %lu ms a frame\n", n, frames,
      (ulong)(now - start), late, (n > 0) ? decode_ms / n : 0,
      (shown > 0) ? show_ms / shown : 0);
    lbDisplay.MLeftButton = 0;
    return Lb_SUCCESS;
}
