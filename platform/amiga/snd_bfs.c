/* snd_bfs.c - bfsoundlib on the Amiga: the OPENAL_* and ogg_vorbis_stream_*
 * functions (in place of bfsoundlib/src/drv_oal.c and oggvorbis.c) done
 * with the sound engine (snd_engine.h).  The 68k game links the engine
 * itself (snd_engine.c), the Warp ARM game a proxy which sends the same
 * calls to its 68k host (platform/warp/snd_proxy.c).
 *
 * Each SNDSAMPLE of the digital driver is one engine slot.  Samples must be
 * 8-bit signed mono (tools/amiga/make_sound_bank.py); others are silent but
 * take their time.  Music: the CD tracks pre-converted to PCM
 * (tools/amiga/make_music.py, MUSIC_TRACK_EXT "smu"); MIDI is not played.
 * ENV:SyndWarsSound = PAULA (default), AHI or NONE; ENV:SyndWarsMusic = OFF
 * turns the music off; ENV:SyndWarsSoundLog writes counters to
 * RAM:SyndWars-sound.txt every 5 s. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "drv_oal.h"
#include "oggvorbis.h"
#include "aildebug.h"
#include "bffile.h"
#include "bfmemory.h"
#include "bflog.h"
#include "snd_engine.h"

/** How the sound backend and the music options are got: ENV: on the 68k,
 * the command line of the ARM core (snd_proxy.c). */
int amiga_sound_backend_wanted(void);
int amiga_music_wanted(void);

struct SmpState {
    ULONG pos_mark;             /**< pos[] as set by us; else restarted */
    UBYTE active;
    UBYTE forever;              /**< started with loop count 0 */
};

static struct SmpState smp_state[SE_SLOTS];

ULONG se_ms(void)
{
    return AIL_ms_count();
}

ulong amiga_sound_heap_max(void)
{
    return se_heap_max();
}

void *amiga_sound_heap_alloc(ulong size)
{
    return se_heap_alloc(size);
}

void amiga_sound_heap_free(void *ptr)
{
    se_heap_free(ptr);
}

int32_t OPENAL_startup(void)
{
    int want, got;

    if (se_backend() != SE_NONE)
        return 1;
    want = amiga_sound_backend_wanted();
    got = se_open(want);
    // in error.log, for the bug reports
    LbWarnLog("sound: wanted %s, opened %s, music %s\n",
      (want == SE_AHI) ? "AHI" : (want == SE_PAULA) ? "Paula" : "none",
      (got == SE_AHI) ? "AHI" : (got == SE_PAULA) ? "Paula" : "none",
      amiga_music_wanted() ? "on" : "off");
    return (got != SE_NONE) ? 1 : 0;
}

int32_t OPENAL_shutdown(void)
{
    se_close();
    return 1;
}

int32_t OPENAL_create_buffers(uint32_t n_sources)
{
    (void)n_sources;
    return 1;
}

int32_t OPENAL_free_buffers(uint32_t n_sources)
{
    (void)n_sources;
    return 1;
}

int32_t OPENAL_create_sources_for_samples(DIG_DRIVER *digdrv)
{
    int32_t i;

    if (digdrv->n_samples > SE_SLOTS)
        digdrv->n_samples = SE_SLOTS;
    for (i = 0; i < digdrv->n_samples; i++)
        memset(&smp_state[i], 0, sizeof(smp_state[i]));
    return (digdrv->n_samples > 0);
}

int32_t OPENAL_free_sources_for_samples(DIG_DRIVER *digdrv)
{
    int32_t i;

    for (i = 0; i < digdrv->n_samples; i++)
        OPENAL_stop_sample(&digdrv->samples[i]);
    return 1;
}

int32_t OPENAL_create_sources_for_sequences(MDI_DRIVER *mdidrv)
{
    return (mdidrv->n_sequences > 0);
}

int32_t OPENAL_free_sources_for_sequences(MDI_DRIVER *mdidrv)
{
    (void)mdidrv;
    return 1;
}

void OPENAL_stop_sequence(SNDSEQUENCE *seq)
{
    (void)seq;
}

void OPENAL_unqueue_finished_mdi_sequences(MDI_DRIVER *mdidrv)
{
    (void)mdidrv;
}

int32_t OPENAL_update_mdi_sequence(SNDSEQUENCE *seq)
{
    return (seq->status == SNDSEQ_DONE) ? 1 : 0;
}

/******************************************************************************/
/* Effects */

static int smp_slot(SNDSAMPLE *s)
{
    int n = s - s->driver->samples;
    if ((n < 0) || (n >= SE_SLOTS))
        return -1;
    return n;
}

static UWORD smp_volume(SNDSAMPLE *s)
{
    LONG v = s->volume * s->driver->master_volume;
    if (v < 0)
        v = 0;
    return (UWORD)((v * 64 + 8064) / 16129);
}

static LONG smp_rate(SNDSAMPLE *s)
{
    return (s->playback_rate > 100) ? s->playback_rate : 22050;
}

/** Starts the sample from the data in its current buffer. */
static void smp_begin(SNDSAMPLE *s, int slot)
{
    struct SmpState *st = &smp_state[slot];
    int cur = s->current_buffer;
    const UBYTE *data = (const UBYTE *)s->start[cur] + s->pos[cur];
    ULONG len = s->len[cur] - s->pos[cur];
    ULONG bps, pass_ms;

    s->pos[cur] = s->len[cur];
    st->pos_mark = s->len[cur];
    bps = 1;
    if (s->format & DIG_F_16BITS_MASK)
        bps *= 2;
    if (s->format & DIG_F_STEREO_MASK)
        bps *= 2;
    pass_ms = ((len / bps) * 10) / (smp_rate(s) / 100);
    st->active = 1;
    st->forever = (s->loop_count == 0);
    se_play(slot, (s->format == DIG_F_MONO_8) ? data : NULL, len, smp_rate(s),
      smp_volume(s), s->pan, s->loop_count, pass_ms);
}

static void smp_end(int slot)
{
    se_stop(slot);
    smp_state[slot].active = 0;
}

void OPENAL_stop_sample(SNDSAMPLE *s)
{
    int slot = smp_slot(s);
    if (slot >= 0)
        smp_end(slot);
}

void OPENAL_pause_sample(SNDSAMPLE *s)
{
    OPENAL_stop_sample(s);
}

void OPENAL_resume_sample(SNDSAMPLE *s)
{
    (void)s;
}

void OPENAL_unqueue_finished_dig_samples(DIG_DRIVER *digdrv)
{
    int32_t i;

    se_log("RAM:SyndWars-sound.txt");
    for (i = 0; i < digdrv->n_samples; i++) {
        SNDSAMPLE *s = &digdrv->samples[i];
        struct SmpState *st = &smp_state[i];

        if (!st->active)
            continue;
        if (s->status != SNDSMP_PLAYING) {
            smp_end(i);
            continue;
        }
        if (s->pos[s->current_buffer] != st->pos_mark) {
            /* started again */
            smp_end(i);
        }
    }
}

int32_t OPENAL_update_dig_sample(SNDSAMPLE *s)
{
    int slot = smp_slot(s);
    struct SmpState *st;

    if (slot < 0)
        return 0;
    st = &smp_state[slot];
    if (!st->active) {
        int cur = s->current_buffer;
        if (s->pos[cur] >= s->len[cur]) {
            /* go to the other buffer, if the application gave one */
            if ((s->done[cur ^ 1] == 0) && (s->len[cur ^ 1] > s->pos[cur ^ 1]))
                s->current_buffer = cur ^= 1;
            else {
                if (s->done[0] || s->done[1])
                    s->status = SNDSMP_DONE;
                return (s->status == SNDSMP_DONE) ? 1 : 0;
            }
        }
        smp_begin(s, slot);
        return 0;
    }

    se_set(slot, smp_rate(s), smp_volume(s), s->pan);
    if (st->forever && (s->loop_count != 0)) {
        /* ReleaseLoopedSample(): ends after the pass which plays now, as on
         * the PC, where the loop count is read before each buffer */
        st->forever = 0;
        se_release(slot);
    }
    if (se_done(slot)) {
        st->active = 0;
        /* the next buffer, if any, starts at the next update */
        if (!s->done[0] && !s->done[1])
            return 0;
        if ((s->done[s->current_buffer ^ 1] == 0) &&
          (s->len[s->current_buffer ^ 1] > s->pos[s->current_buffer ^ 1]))
            return 0;
        s->status = SNDSMP_DONE;
    }
    return (s->status == SNDSMP_DONE) ? 1 : 0;
}

/******************************************************************************/
/* Music */

static UWORD music_volume(OggVorbisStream *stream)
{
    LONG v = (LONG)(stream->gain * 64.f + 0.5f);
    if (v < 0)
        v = 0;
    if (v > 64)
        v = 64;
    return (UWORD)v;
}

bool ogg_vorbis_stream_init(OggVorbisStream *stream)
{
    memset(stream, 0, sizeof(*stream));
    stream->gain = 1.0f;
    if (!amiga_music_wanted())
        return false;
    return (se_backend() != SE_NONE);
}

void ogg_vorbis_stream_free(OggVorbisStream *stream)
{
    ogg_vorbis_stream_clear(stream);
    memset(stream, 0, sizeof(*stream));
}

void ogg_vorbis_stream_pause(OggVorbisStream *stream)
{
    stream->playing = false;
    se_music_pause();
}

void ogg_vorbis_stream_play(OggVorbisStream *stream)
{
    stream->playing = true;
    se_music_play(music_volume(stream));
}

void ogg_vorbis_stream_stop(OggVorbisStream *stream)
{
    if (!stream->playing)
        return;
    stream->playing = false;
    se_music_pause();
    ogg_vorbis_stream_restart(stream);
}

void ogg_vorbis_stream_clear(OggVorbisStream *stream)
{
    if (stream->file_name == NULL)
        return;
    ogg_vorbis_stream_stop(stream);
    se_music_close();
    LbMemoryFree(stream->file_name);
    stream->file_name = NULL;
}

bool ogg_vorbis_stream_open(OggVorbisStream *stream, const char *fname)
{
    char real_fname[FILENAME_MAX];
    const char *path = fname;
    uint32_t sz;

    ogg_vorbis_stream_clear(stream);
    if (se_backend() == SE_NONE)
        return false;
#if LB_FILENAME_TRANSFORM
    if (lbFileNameTransform != NULL) {
        lbFileNameTransform(real_fname, fname);
        path = real_fname;
    }
#else
    (void)real_fname;
#endif
    if (!se_music_open(path))
        return false;
    sz = strlen(fname) + 1;
    stream->file_name = LbMemoryAlloc(sz);
    if (stream->file_name != NULL)
        memcpy(stream->file_name, fname, sz);
    return true;
}

bool ogg_vorbis_stream_restart(OggVorbisStream *stream)
{
    if (stream->file_name == NULL)
        return false;
    se_music_rewind();
    return true;
}

void ogg_vorbis_stream_set_gain(OggVorbisStream *stream, float gain)
{
    stream->gain = gain;
    if (stream->playing)
        se_music_play(music_volume(stream));
}

float ogg_vorbis_stream_get_gain(OggVorbisStream *stream)
{
    return stream->gain;
}

bool ogg_vorbis_stream_update(OggVorbisStream *stream)
{
    if ((stream->file_name == NULL) || !stream->playing)
        return true;
    se_music_update();
    return true;
}
