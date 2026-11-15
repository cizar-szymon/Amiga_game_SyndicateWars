/* The Smacker video player of the Amiga port (see smk_core.h): the decoder
 * libsmacker (third_party/libsmacker, LGPL 2.1, sped up for the 68060) in
 * place of RAD's x86 library of the PC.
 *
 * As the PC's play_smk_direct(): each frame into the middle of the screen,
 * the palette, the video's own rate by the clock; the player may end it.
 * Frames are decoded ahead into a queue while there is time and each is
 * shown at its time: some frames of the 600x300 videos take up to 175 ms on
 * a 68060 (66 ms a frame).  A frame is dropped only when more than one frame
 * behind.
 *
 * Sound: the video's track 0 (16-bit, 22 kHz) made 8-bit into a queue, which
 * the music voices of the sound engine take as a stream (Paula or AHI); the
 * game's music is put aside meanwhile and back where it was after. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <exec/types.h>
#include "snd_engine.h"
#include "smk_core.h"
#include "../../third_party/libsmacker/smacker.h"

/* frames decoded ahead (2.9 MB at 600x300) */
#define FQ_N 16

/* the queue of the video's sound: 8-bit signed, left and right */
#define AQ_SIZE 65536
#define AQ_MASK (AQ_SIZE - 1)
#define AQ_BLOCK 2048
static signed char aq_l[AQ_SIZE], aq_r[AQ_SIZE];
static unsigned long aq_w, aq_r_pos;
static unsigned long aq_under;
/* all the sound of the video is in the queue: an empty queue is its end */
static int aq_done;

static void aq_push(smk s, unsigned char ch, unsigned char bits)
{
    const unsigned char *a = smk_get_audio(s, 0);
    unsigned long size = smk_get_audio_size(s, 0);
    unsigned long n, i, w = aq_w;

    if ((a == NULL) || (size == 0))
        return;
    if (bits == 16) {
        const short *v = (const short *)a;
        n = size / (2 * ch);
        for (i = 0; i < n; i++, w++) {
            if (w - aq_r_pos >= AQ_SIZE)
                break;          /* full: the rest dropped */
            aq_l[w & AQ_MASK] = (signed char)(v[i * ch] >> 8);
            aq_r[w & AQ_MASK] = (signed char)(v[i * ch + ch - 1] >> 8);
        }
    } else {
        /* 8-bit Smacker sound is unsigned */
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
    unsigned long i, r = aq_r_pos;

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

/** Microseconds a frame, from the header's frame rate (as libsmacker reads
 * it, but without a double: they did not come through the 68k build). */
static unsigned long smk_frame_us(const char *fname)
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
    rate = (long)((unsigned long)hd[16] | ((unsigned long)hd[17] << 8) |
      ((unsigned long)hd[18] << 16) | ((unsigned long)hd[19] << 24));
    if (rate > 0)
        return (unsigned long)rate * 1000;
    if (rate < 0)
        return (unsigned long)(-rate) * 10;
    return 100000;
}

static unsigned long frame_time(unsigned long n, unsigned long usf)
{
    return (n * (usf / 10)) / 100;
}

int smk_core_play(const char *fname, struct SmkOut *o)
{
    smk s;
    unsigned long frames, w, h, n, usf, decoded;
    unsigned char yscale;
    unsigned char pal[768];
    const unsigned char *spal;
    unsigned long start, now, t0;
    unsigned long late = 0, decode_ms = 0, show_ms = 0, shown = 0, decode_max = 0;
    unsigned char *fq_pix[FQ_N];
    static unsigned char fq_pal[FQ_N][768];
    int fq_n, fq_first, fq_cnt, i, finish = 0, sound = 0;
    unsigned char amask, ach[7], abits[7];
    unsigned long arate[7];
    struct SeMusicState mstate;
    char line[200];

    s = smk_open_file(fname, SMK_MODE_DISK);
    if (s == NULL) {
        sprintf(line, "smk: cannot open %s\n", fname);
        o->log(line);
        return -1;
    }
    smk_info_all(s, NULL, &frames, NULL);
    usf = smk_frame_us(fname);
    smk_info_video(s, &w, &h, &yscale);
    smk_info_audio(s, &amask, ach, abits, arate);
    if (o->sound && (amask & SMK_AUDIO_TRACK_0) && (se_backend() != SE_NONE) &&
      (ach[0] >= 1) && (ach[0] <= 2) && ((abits[0] == 8) || (abits[0] == 16)))
        sound = 1;
    smk_enable_all(s, SMK_VIDEO_TRACK | (sound ? SMK_AUDIO_TRACK_0 : 0));
    sprintf(line, "smk: %s, %lux%lu, %lu frames, %lu us a frame\n", fname, w, h,
      frames, usf);
    o->log(line);

    for (i = 0; i < FQ_N; i++) {
        fq_pix[i] = malloc(w * h);
        if (fq_pix[i] == NULL)
            break;
    }
    fq_n = i;
    if (fq_n == 0) {
        o->log("smk: no memory for the frames\n");
        smk_close(s);
        return -1;
    }

    o->clear(o->ctx);
    t0 = o->clock_ms();
    if (smk_first(s) < 0) {
        o->log("smk: the first frame cannot be decoded\n");
        for (i = 0; i < fq_n; i++)
            free(fq_pix[i]);
        smk_close(s);
        return -1;
    }
    decoded = 0;
    fq_first = fq_cnt = 0;
    aq_w = aq_r_pos = 0;
    aq_under = 0;
    aq_done = 0;
    for (;;) {
        /* into the queue: the picture and the palette of the frame decoded */
        int q = (fq_first + fq_cnt) % fq_n;
        memcpy(fq_pix[q], smk_get_video(s), w * h);
        spal = smk_get_palette(s);
        if (spal != NULL)
            memcpy(fq_pal[q], spal, 768);
        fq_cnt++;
        decoded++;
        if (sound)
            aq_push(s, ach[0], abits[0]);
        now = o->clock_ms();
        decode_ms += now - t0;
        if (now - t0 > decode_max)
            decode_max = now - t0;
        /* the whole queue before the start (and its sound: more than the
         * first blocks the voices take) */
        if ((fq_cnt >= fq_n) || (decoded >= frames))
            break;
        t0 = now;
        if (smk_next(s) < 0)
            break;
    }
    if (sound) {
        se_music_suspend(&mstate);
        if (se_stream_open(arate[0], AQ_BLOCK, aq_fill, NULL))
            se_music_play(o->volume);
        else
            sound = 0;
    }
    start = o->clock_ms();
    n = 0;
    while ((n < frames) && !finish)
    {
        unsigned long due = start + frame_time(n, usf);
        unsigned long next = start + frame_time(n + 2, usf);

        now = o->clock_ms();
        if ((fq_cnt > 0) && (now >= due)) {
            /* frame n: shown (late too), unless more than a frame behind
             * with the next ones here - then dropped, to catch up */
            if ((now >= next) && (fq_cnt > 1)) {
                late++;
            } else {
                /* libsmacker's palette is 8 bits a colour, the game's 6 */
                for (i = 0; i < 768; i++)
                    pal[i] = fq_pal[fq_first][i] >> 2;
                o->show(fq_pix[fq_first], w, h, yscale, pal, o->ctx);
                show_ms += o->clock_ms() - now;
                shown++;
            }
            fq_first = (fq_first + 1) % fq_n;
            fq_cnt--;
            n++;
            continue;
        }
        if ((fq_cnt < fq_n) && (decoded < frames)) {
            int q;
            t0 = o->clock_ms();
            if (smk_next(s) < 0)
                break;
            q = (fq_first + fq_cnt) % fq_n;
            memcpy(fq_pix[q], smk_get_video(s), w * h);
            spal = smk_get_palette(s);
            if (spal != NULL)
                memcpy(fq_pal[q], spal, 768);
            fq_cnt++;
            decoded++;
            if (sound) {
                aq_push(s, ach[0], abits[0]);
                se_music_update();
            }
            now = o->clock_ms();
            decode_ms += now - t0;
            if (now - t0 > decode_max)
                decode_max = now - t0;
            if (o->idle(o->ctx))
                finish = 1;
            continue;
        }
        if ((fq_cnt == 0) && (decoded >= frames))
            break;
        if (decoded >= frames)
            aq_done = 1;
        /* nothing to do until the next frame's time */
        if (sound)
            se_music_update();
        if (o->idle(o->ctx))
            finish = 1;
    }
    /* the sound to its end (the last frame's) */
    if (sound && !finish) {
        unsigned long end = start + frame_time(frames, usf);
        aq_done = 1;
        while (o->clock_ms() < end) {
            se_music_update();
            if (o->idle(o->ctx))
                break;
        }
    }
    smk_close(s);
    for (i = 0; i < fq_n; i++)
        free(fq_pix[i]);
    if (sound) {
        se_music_close();
        se_music_resume(&mstate);
        sprintf(line, "smk: sound %lu Hz, %u ch, %u bit; %lu samples in the video, "
          "%lu short\n", arate[0], (unsigned)ach[0], (unsigned)abits[0], aq_w, aq_under);
        o->log(line);
    }
    now = o->clock_ms();
    sprintf(line, "smk: %lu of %lu frames in %lu ms, %lu not shown (late), queue %d; "
      "decode %lu ms (most %lu), copy+show %lu ms a frame\n", n, frames,
      now - start, late, fq_n, (decoded > 0) ? decode_ms / decoded : 0,
      decode_max, (shown > 0) ? show_ms / shown : 0);
    o->log(line);
    return 0;
}
