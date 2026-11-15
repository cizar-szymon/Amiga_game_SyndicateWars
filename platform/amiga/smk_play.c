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
#include <stdlib.h>
#if !defined(SW_WARP)
#include <proto/dos.h>
#endif

#include "bftypes.h"
#include "bfscreen.h"
#include "bfpalette.h"
#include "bfkeybd.h"
#include "bfmouse.h"
#include "bftime.h"
#include "bflog.h"
#include "bfsmack.h"
#include "bfmemory.h"

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
/* all the sound of the video is in the queue: an empty queue is its end */
static volatile int aq_done;

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
            if (!aq_done)
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

/* frames decoded ahead: on the 68060 enough for the slow parts of the
 * 600x300 videos (2.9 MB of them); the ARM decodes much faster and has
 * little memory */
#if defined(SW_WARP)
#define FQ_N 4
#else
#define FQ_N 16
#endif

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
    TbClockMSec start, now, t0;
    ulong late = 0, decode_ms = 0, show_ms = 0, shown = 0, decode_max = 0;
    ulong decoded;
    ubyte *fq_pix[FQ_N];
    static ubyte fq_pal[FQ_N][PALETTE_8b_SIZE];
    int fq_n, fq_first, fq_cnt;
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
    {
        // test: ENV:SyndWarsSmkTest=file plays that file instead (e.g. in
        // place of the intro), to measure the videos of the campaign's end
#if !defined(SW_WARP)
        static char t[64];
        if (GetVar((STRPTR)"SyndWarsSmkTest", (STRPTR)t, sizeof(t), 0) > 0)
            fname = t;
#endif
    }
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

    // a queue of decoded frames: decoding goes ahead while there is time,
    // each frame is shown at its time - a slow frame (a cut, up to 170 ms
    // of 600x300 on a 68060) takes from the queue instead of being dropped
    for (i = 0; i < FQ_N; i++) {
        fq_pix[i] = LbMemoryAlloc(w * h);
        if (fq_pix[i] == NULL)
            break;
    }
    fq_n = i;
    if (fq_n == 0) {
        smk_close(s);
        return Lb_FAIL;
    }

    LbScreenClear(0);
    t0 = LbTimerClock();
    if (smk_first(s) < 0) {
        for (i = 0; i < fq_n; i++)
            LbMemoryFree(fq_pix[i]);
        smk_close(s);
        return Lb_FAIL;
    }
    decoded = 0;
    fq_first = fq_cnt = 0;
#if !defined(SW_WARP)
    aq_w = aq_r_pos = 0;
    aq_under = 0;
    aq_done = 0;
#endif
    for (;;) {
        // into the queue: the picture and the palette of the frame decoded
        int q = (fq_first + fq_cnt) % fq_n;
        memcpy(fq_pix[q], smk_get_video(s), w * h);
        spal = smk_get_palette(s);
        if (spal != NULL)
            memcpy(fq_pal[q], spal, PALETTE_8b_SIZE);
        fq_cnt++;
        decoded++;
#if !defined(SW_WARP)
        if (sound)
            aq_push(s, ach[0], abits[0]);
#endif
        now = LbTimerClock();
        decode_ms += now - t0;
        if (now - t0 > decode_max)
            decode_max = now - t0;
        // the whole queue before the start (and its sound: more than the
        // first blocks the voices take)
        if ((fq_cnt >= fq_n) || (decoded >= frames))
            break;
        t0 = now;
        if (smk_next(s) < 0)
            break;
    }
#if !defined(SW_WARP)
    if (sound) {
        se_music_suspend(&mstate);
        if (se_stream_open(arate[0], AQ_BLOCK, aq_fill, NULL))
            se_music_play((UWORD)(64L * startscr_samplevol / STARTSCR_VOLUME_MAX));
        else
            sound = false;
    }
#endif
    start = LbTimerClock();
    n = 0;
    while ((n < frames) && !finish)
    {
        TbClockMSec due = start + (TbClockMSec)((n * (usf / 10)) / 100);
        TbClockMSec next = start + (TbClockMSec)(((n + 2) * (usf / 10)) / 100);

        now = LbTimerClock();
        if ((fq_cnt > 0) && (now >= due)) {
            // frame n: shown (late too), unless more than a frame behind
            // with the next ones here - then dropped, to catch up
            if ((now >= next) && (fq_cnt > 1)) {
                late++;
            } else {
                for (i = 0; i < PALETTE_8b_SIZE; i++)
                    pal[i] = fq_pal[fq_first][i] >> 2;
                put_frame(fq_pix[fq_first], w, h, yscale);
                LbPaletteSet(pal);
                LbScreenSwap();
                show_ms += LbTimerClock() - now;
                shown++;
            }
            fq_first = (fq_first + 1) % fq_n;
            fq_cnt--;
            n++;
            continue;
        }
        if ((fq_cnt < fq_n) && (decoded < frames)) {
            int q;
            t0 = LbTimerClock();
            if (smk_next(s) < 0)
                break;
            q = (fq_first + fq_cnt) % fq_n;
            memcpy(fq_pix[q], smk_get_video(s), w * h);
            spal = smk_get_palette(s);
            if (spal != NULL)
                memcpy(fq_pal[q], spal, PALETTE_8b_SIZE);
            fq_cnt++;
            decoded++;
#if !defined(SW_WARP)
            if (sound) {
                aq_push(s, ach[0], abits[0]);
                se_music_update();
            }
#endif
            now = LbTimerClock();
            decode_ms += now - t0;
            if (now - t0 > decode_max)
                decode_max = now - t0;
            if (skip_pressed(plyflags))
                finish = true;
            continue;
        }
        if ((fq_cnt == 0) && (decoded >= frames))
            break;
#if !defined(SW_WARP)
        if (decoded >= frames)
            aq_done = 1;
#endif
        // nothing to do until the next frame's time
#if !defined(SW_WARP)
        if (sound)
            se_music_update();
#endif
        game_hacky_update();
        if (skip_pressed(plyflags))
            finish = true;
    }
    // the sound to its end (the last frame's)
#if !defined(SW_WARP)
    if (sound && !finish) {
        TbClockMSec end = start + (TbClockMSec)((frames * (usf / 10)) / 100);
        while ((LbTimerClock() < end) && !skip_pressed(plyflags)) {
            se_music_update();
            game_hacky_update();
        }
    }
#endif
    smk_close(s);
    for (i = 0; i < fq_n; i++)
        LbMemoryFree(fq_pix[i]);
#if !defined(SW_WARP)
    if (sound) {
        se_music_close();
        se_music_resume(&mstate);
        LbWarnLog("smk: sound %lu Hz, %u ch, %u bit; %lu samples in the video, "
          "%lu short\n", arate[0], ach[0], abits[0], (ulong)aq_w, aq_under);
    }
#endif
    now = LbTimerClock();
    LbWarnLog("smk: %lu of %lu frames in %lu ms, %lu not shown (late), queue %d; "
      "decode %lu ms (most %lu), copy+show %lu ms a frame\n", n, frames,
      (ulong)(now - start), late, fq_n, (decoded > 0) ? decode_ms / decoded : 0,
      decode_max, (shown > 0) ? show_ms / shown : 0);
    lbDisplay.MLeftButton = 0;
    return Lb_SUCCESS;
}
