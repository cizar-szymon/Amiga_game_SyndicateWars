/* Paula sound driver for bfsoundlib on AmigaOS.
 *
 * Takes the place of bfsoundlib/src/drv_oal.c (OpenAL) and oggvorbis.c:
 * the same OPENAL_* and ogg_vorbis_stream_* functions, done with the 4
 * channels of audio.device.
 *
 * - Effects: each playing SNDSAMPLE gets a channel of its side (Amiga
 *   channels 0 and 3 are left, 1 and 2 right), a free one or the quietest
 *   one, if that is not louder.  A sample without a channel goes on
 *   silently and ends when its time is up, as if it played.  Samples must
 *   be 8-bit signed mono in Chip RAM (tools/amiga/make_sound_bank.py, the
 *   sound heap is in Chip RAM); others are silent.
 * - Music: the CD tracks pre-converted to 8-bit signed stereo PCM
 *   (tools/amiga/make_music.py), streamed from disk to channels 0 (left)
 *   and 1 (right); effects then have channels 2 and 3.
 * - MIDI sequences are not played.
 *
 * Panning follows the PC (OpenAL) build: pan above 64 is left, below 64
 * (but not 0) right, else the middle.
 * ENV:SyndWarsSound = PAULA (default) or NONE; with ENV:SyndWarsSoundLog
 * counters go to RAM:SyndWars-sound.txt every 5 s.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include <exec/exec.h>
#include <exec/memory.h>
#include <devices/audio.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "drv_oal.h"
#include "oggvorbis.h"
#include "aildebug.h"
#include "bffile.h"
#include "bfmemory.h"

#define NCH 4
#define NREQ 3
/* Longest write: the length register counts 16-bit words */
#define CHUNK_MAX 65536
#define MAX_SAMPLES 64
#define OWNER_MUSIC ((SNDSAMPLE *)1)

struct Chan {
    struct IOAudio *req[NREQ];  /**< writes, issued and done in order */
    struct IOAudio *pv;         /**< period/volume changes */
    UBYTE head;                 /**< oldest busy write */
    UBYTE nbusy;
    SNDSAMPLE *owner;           /**< effect playing, OWNER_MUSIC or NULL */
    UWORD vol, per;
};

struct SmpState {
    const UBYTE *beg;           /**< whole sample data */
    ULONG len;
    const UBYTE *data;          /**< data not yet sent to the channel */
    ULONG left;
    LONG loops;                 /**< repeats after this pass, -1 forever */
    ULONG end_ms;               /**< end if it plays without a channel */
    ULONG pos_mark;             /**< pos[] as set by us; else restarted */
    LONG rate;                  /**< rate of per_cache */
    UWORD per_cache;
    BYTE ch;                    /**< channel, -1 none */
    UBYTE active;
    UBYTE sent_whole;           /**< short sample: its one write is sent */
    UBYTE paula_ok;             /**< 8-bit mono data in Chip RAM */
};

static struct MsgPort *snd_port;
static struct IOAudio *alloc_req;
static bool paula_open;
static struct Chan chans[NCH];
static ULONG paula_clock = 3546895;
static struct SmpState smp_state[MAX_SAMPLES];
static bool music_owns;         /**< channels 0 and 1 are the music's */

/* diagnostics */
static struct {
    ULONG started, on_chan, stolen, no_chan, not_chip, not_8bit, music_blocks;
} snd_cnt;
static int snd_log = -1;
static ULONG snd_log_ms;

static void snd_log_write(void)
{
    char buf[16];
    BPTR fh;
    ULONG now;

    if (snd_log < 0)
        snd_log = (GetVar((STRPTR)"SyndWarsSoundLog", (STRPTR)buf, sizeof(buf), 0) >= 0);
    if (!snd_log)
        return;
    now = AIL_ms_count();
    if ((LONG)(now - snd_log_ms) < 5000)
        return;
    snd_log_ms = now;
    fh = Open((STRPTR)"RAM:SyndWars-sound.txt", MODE_NEWFILE);
    if (fh == 0)
        return;
    FPrintf(fh, (STRPTR)"paula %ld clock %lu ms %lu\n", (LONG)paula_open, paula_clock, now);
    FPrintf(fh, (STRPTR)"started %lu on_chan %lu stolen %lu no_chan %lu\n",
        snd_cnt.started, snd_cnt.on_chan, snd_cnt.stolen, snd_cnt.no_chan);
    FPrintf(fh, (STRPTR)"not_chip %lu not_8bit %lu music_blocks %lu music_owns %ld\n",
        snd_cnt.not_chip, snd_cnt.not_8bit, snd_cnt.music_blocks, (LONG)music_owns);
    Close(fh);
}

/******************************************************************************/

void *amiga_chip_alloc(ulong size)
{
    /* leave some Chip RAM for the system */
    if (AvailMem(MEMF_CHIP | MEMF_LARGEST) < size + 200000)
        return NULL;
    return AllocVec(size, MEMF_CHIP | MEMF_PUBLIC);
}

void amiga_chip_free(void *ptr)
{
    if (ptr != NULL)
        FreeVec(ptr);
}

static ULONG snd_ms(void)
{
    return AIL_ms_count();
}

/* BeginIO() keeps io_Flags (SendIO() clears ADIOF_PERVOL). */
static void begin_io(struct IORequest *r)
{
    register struct IORequest *a1 __asm("a1") = r;
    register struct Device *a6 __asm("a6") = r->io_Device;
    __asm volatile ("jsr -30(%%a6)"
        : "+r" (a1), "+r" (a6) : : "d0", "d1", "a0", "cc", "memory");
}

static void chan_reap(struct Chan *c)
{
    while (c->nbusy > 0) {
        struct IOAudio *r = c->req[c->head];
        if (!CheckIO((struct IORequest *)r))
            break;
        WaitIO((struct IORequest *)r);
        c->head = (c->head + 1) % NREQ;
        c->nbusy--;
    }
}

static void chan_write(struct Chan *c, const UBYTE *data, ULONG len,
    UWORD cycles)
{
    struct IOAudio *r = c->req[(c->head + c->nbusy) % NREQ];

    r->ioa_Request.io_Command = CMD_WRITE;
    r->ioa_Request.io_Flags = ADIOF_PERVOL;
    r->ioa_Data = (UBYTE *)data;
    r->ioa_Length = len;
    r->ioa_Period = c->per;
    r->ioa_Volume = c->vol;
    r->ioa_Cycles = cycles;
    begin_io((struct IORequest *)r);
    c->nbusy++;
}

/** Stops the channel at once; its owner (if an effect) goes on silently. */
static void chan_stop(int ch)
{
    struct Chan *c = &chans[ch];
    int k;

    for (k = c->nbusy - 1; k >= 0; k--) {
        struct IOAudio *r = c->req[(c->head + k) % NREQ];
        AbortIO((struct IORequest *)r);
        WaitIO((struct IORequest *)r);
    }
    c->nbusy = 0;
    c->head = 0;
    if ((c->owner != NULL) && (c->owner != OWNER_MUSIC)) {
        SNDSAMPLE *s = c->owner;
        DIG_DRIVER *digdrv = s->driver;
        int n = s - digdrv->samples;
        if ((n >= 0) && (n < MAX_SAMPLES))
            smp_state[n].ch = -1;
    }
    c->owner = NULL;
}

static void chan_set(struct Chan *c, UWORD per, UWORD vol)
{
    if ((c->per == per) && (c->vol == vol))
        return;
    c->per = per;
    c->vol = vol;
    if (c->nbusy == 0)
        return;
    c->pv->ioa_Request.io_Command = ADCMD_PERVOL;
    c->pv->ioa_Request.io_Flags = 0;
    c->pv->ioa_Period = per;
    c->pv->ioa_Volume = vol;
    DoIO((struct IORequest *)c->pv);
}

static UWORD rate_to_period(LONG rate)
{
    ULONG per;

    if (rate < 1000)
        rate = 22050;
    per = (paula_clock + rate - 1) / rate;
    if (per < 124)
        per = 124;
    if (per > 65535)
        per = 65535;
    return per;
}

/******************************************************************************/

static bool paula_wanted(void)
{
    char buf[16];

    if (GetVar((STRPTR)"SyndWarsSound", (STRPTR)buf, sizeof(buf), 0) < 0)
        return true;
    return (strcasecmp(buf, "NONE") != 0);
}

int32_t OPENAL_startup(void)
{
    static UBYTE all_chans = 0x0F;
    int ch, k;

    if (paula_open)
        return 1;
    if (!paula_wanted())
        return 0;
    if (SysBase->ex_EClockFrequency != 0)
        paula_clock = SysBase->ex_EClockFrequency * 5;

    snd_port = CreateMsgPort();
    if (snd_port == NULL)
        return 0;
    alloc_req = (struct IOAudio *)CreateIORequest(snd_port, sizeof(struct IOAudio));
    if (alloc_req == NULL)
        goto err;
    alloc_req->ioa_Request.io_Message.mn_Node.ln_Pri = 127;
    alloc_req->ioa_Request.io_Flags = ADIOF_NOWAIT;
    alloc_req->ioa_Data = &all_chans;
    alloc_req->ioa_Length = 1;
    if (OpenDevice((STRPTR)AUDIONAME, 0, (struct IORequest *)alloc_req, 0) != 0) {
        DeleteIORequest((struct IORequest *)alloc_req);
        alloc_req = NULL;
        goto err;
    }
    for (ch = 0; ch < NCH; ch++) {
        struct Chan *c = &chans[ch];
        for (k = 0; k <= NREQ; k++) {
            struct IOAudio *r;
            r = (struct IOAudio *)CreateIORequest(snd_port, sizeof(struct IOAudio));
            if (r == NULL)
                goto err;
            r->ioa_Request.io_Device = alloc_req->ioa_Request.io_Device;
            r->ioa_Request.io_Unit = (struct Unit *)(1UL << ch);
            r->ioa_AllocKey = alloc_req->ioa_AllocKey;
            if (k < NREQ)
                c->req[k] = r;
            else
                c->pv = r;
        }
        c->head = c->nbusy = 0;
        c->owner = NULL;
        c->vol = 0;
        c->per = 0;
    }
    paula_open = true;
    return 1;

err:
    OPENAL_shutdown();
    return 0;
}

int32_t OPENAL_shutdown(void)
{
    int ch, k;

    if (paula_open) {
        for (ch = 0; ch < NCH; ch++)
            chan_stop(ch);
    }
    paula_open = false;
    for (ch = 0; ch < NCH; ch++) {
        struct Chan *c = &chans[ch];
        for (k = 0; k < NREQ; k++) {
            if (c->req[k] != NULL)
                DeleteIORequest((struct IORequest *)c->req[k]);
            c->req[k] = NULL;
        }
        if (c->pv != NULL)
            DeleteIORequest((struct IORequest *)c->pv);
        c->pv = NULL;
    }
    if (alloc_req != NULL) {
        CloseDevice((struct IORequest *)alloc_req);
        DeleteIORequest((struct IORequest *)alloc_req);
        alloc_req = NULL;
    }
    if (snd_port != NULL)
        DeleteMsgPort(snd_port);
    snd_port = NULL;
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

    if (digdrv->n_samples > MAX_SAMPLES)
        digdrv->n_samples = MAX_SAMPLES;
    for (i = 0; i < digdrv->n_samples; i++) {
        memset(&smp_state[i], 0, sizeof(smp_state[i]));
        smp_state[i].ch = -1;
    }
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

static struct SmpState *smp_st(SNDSAMPLE *s)
{
    int n = s - s->driver->samples;
    if ((n < 0) || (n >= MAX_SAMPLES))
        return NULL;
    return &smp_state[n];
}

static UWORD smp_volume(SNDSAMPLE *s)
{
    LONG v = s->volume * s->driver->master_volume;
    if (v < 0)
        v = 0;
    return (UWORD)((v * 64 + 8064) / 16129);
}

/** Side of the sample: 0 left, 1 right, 2 middle (as the PC build). */
static int smp_side(SNDSAMPLE *s)
{
    if ((s->pan == 0) || (s->pan == 64))
        return 2;
    return (s->pan > 64) ? 0 : 1;
}

static bool chan_usable(int ch)
{
    return !(music_owns && (ch < 2));
}

/** A channel for the sample, or -1. */
static int pick_channel(SNDSAMPLE *s, UWORD vol)
{
    static const BYTE order[3][NCH] = {
        {3, 0, 2, 1},   /* left */
        {2, 1, 3, 0},   /* right */
        {3, 2, 0, 1},   /* middle */
    };
    const BYTE *ord = order[smp_side(s)];
    int k, best = -1;
    UWORD best_vol = 0xFFFF;

    if (!paula_open)
        return -1;
    for (k = 0; k < NCH; k++) {
        int ch = ord[k];
        if (chan_usable(ch) && (chans[ch].owner == NULL))
            return ch;
    }
    /* none free: the quietest, the own side first */
    for (k = 0; k < NCH; k++) {
        int ch = ord[k];
        if (!chan_usable(ch) || (chans[ch].owner == OWNER_MUSIC))
            continue;
        if (chans[ch].vol < best_vol) {
            best_vol = chans[ch].vol;
            best = ch;
        }
    }
    if ((best >= 0) && (best_vol <= vol)) {
        snd_cnt.stolen++;
        chan_stop(best);
        return best;
    }
    return -1;
}

/** Sends the sample's data to its channel, keeping 2 writes queued. */
static void smp_feed(struct SmpState *st)
{
    struct Chan *c = &chans[(int)st->ch];

    chan_reap(c);
    while (c->nbusy < 2) {
        ULONG n;

        if (st->left == 0) {
            if (st->loops == 0)
                break;
            if (st->loops > 0)
                st->loops--;
            st->data = st->beg;
            st->left = st->len;
        }
        if (st->len <= CHUNK_MAX) {
            /* whole sample in one write, the repeats done by Paula */
            UWORD cycles = 1;
            if (st->sent_whole)
                break;
            if (st->loops < 0)
                cycles = 0;
            else if (st->loops < 65535)
                cycles = st->loops + 1;
            chan_write(c, st->beg, st->len, cycles);
            st->sent_whole = 1;
            break;
        }
        n = st->left;
        if (n > CHUNK_MAX)
            n = CHUNK_MAX;
        chan_write(c, st->data, n, 1);
        st->data += n;
        st->left -= n;
    }
}

static void smp_to_channel(SNDSAMPLE *s, struct SmpState *st, int ch)
{
    struct Chan *c = &chans[ch];

    c->owner = s;
    c->per = st->per_cache;
    c->vol = smp_volume(s);
    st->ch = ch;
    st->sent_whole = 0;
    smp_feed(st);
}

/** Starts the sample from the data in its current buffer. */
static void smp_begin(SNDSAMPLE *s, struct SmpState *st)
{
    int cur = s->current_buffer;
    const UBYTE *data = s->start[cur];
    ULONG len = s->len[cur] - s->pos[cur];
    ULONG dur, bps;
    int ch;

    data += s->pos[cur];
    s->pos[cur] = s->len[cur];
    st->pos_mark = s->len[cur];
    st->paula_ok = (s->format == DIG_F_MONO_8);
    bps = 1;
    if (s->format & DIG_F_16BITS_MASK)
        bps *= 2;
    if (s->format & DIG_F_STEREO_MASK)
        bps *= 2;
    if ((ULONG)data & 1) {
        data++;
        len = (len > 0) ? len - 1 : 0;
    }
    len &= ~1UL;
    if (len < 2)
        st->paula_ok = false;
    if (s->format != DIG_F_MONO_8)
        snd_cnt.not_8bit++;
    if (st->paula_ok && !(TypeOfMem((APTR)data) & MEMF_CHIP)) {
        st->paula_ok = false;
        snd_cnt.not_chip++;
    }
    snd_cnt.started++;
    st->beg = st->data = data;
    st->len = st->left = len;
    st->loops = (s->loop_count == 0) ? -1 : s->loop_count - 1;
    if (st->rate != s->playback_rate || st->per_cache == 0) {
        st->rate = s->playback_rate;
        st->per_cache = rate_to_period(st->rate);
    }
    dur = len / bps;
    dur = (dur * 10) / ((st->rate > 100 ? st->rate : 22050) / 100);
    if (st->loops > 0)
        dur *= st->loops + 1;
    st->end_ms = snd_ms() + dur;
    st->active = 1;
    st->ch = -1;
    if (st->paula_ok) {
        ch = pick_channel(s, smp_volume(s));
        if (ch >= 0) {
            snd_cnt.on_chan++;
            smp_to_channel(s, st, ch);
        } else
            snd_cnt.no_chan++;
    }
}

static void smp_end(SNDSAMPLE *s, struct SmpState *st)
{
    if (st->ch >= 0)
        chan_stop(st->ch);
    st->active = 0;
    (void)s;
}

void OPENAL_stop_sample(SNDSAMPLE *s)
{
    struct SmpState *st = smp_st(s);
    if (st != NULL)
        smp_end(s, st);
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

    snd_log_write();
    for (i = 0; i < digdrv->n_samples; i++) {
        SNDSAMPLE *s = &digdrv->samples[i];
        struct SmpState *st = &smp_state[i];

        if (!st->active)
            continue;
        if (s->status != SNDSMP_PLAYING) {
            smp_end(s, st);
            continue;
        }
        if (s->pos[s->current_buffer] != st->pos_mark) {
            /* started again */
            smp_end(s, st);
        }
    }
}

int32_t OPENAL_update_dig_sample(SNDSAMPLE *s)
{
    struct SmpState *st = smp_st(s);
    bool finished;

    if (st == NULL)
        return 0;
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
        smp_begin(s, st);
        return 0;
    }

    if (st->rate != s->playback_rate) {
        st->rate = s->playback_rate;
        st->per_cache = rate_to_period(st->rate);
    }
    if (st->ch >= 0) {
        struct Chan *c = &chans[(int)st->ch];
        chan_set(c, st->per_cache, smp_volume(s));
        smp_feed(st);
        if (st->len <= CHUNK_MAX)
            finished = (c->nbusy == 0) && st->sent_whole;
        else
            finished = (c->nbusy == 0) && (st->left == 0) && (st->loops == 0);
        if (finished) {
            c->owner = NULL;
            st->ch = -1;
        }
    } else {
        if (st->loops < 0) {
            /* a looped sound takes a channel when one gets free */
            int ch;
            finished = false;
            if (st->paula_ok && paula_open) {
                for (ch = 0; ch < NCH; ch++) {
                    if (chan_usable(ch) && (chans[ch].owner == NULL)) {
                        st->data = st->beg;
                        st->left = st->len;
                        smp_to_channel(s, st, ch);
                        break;
                    }
                }
            }
        } else {
            finished = ((LONG)(snd_ms() - st->end_ms) >= 0);
        }
    }
    if (finished) {
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
/* Music: tracks of 8-bit signed stereo PCM, "SWMU" files */

struct MusicPriv {
    TbFileHandle fh;
    ULONG rate;
    ULONG block;                /**< bytes per channel in a block */
    ULONG nblocks;
    ULONG next_block;
    UBYTE *buf;                 /**< NREQ slots of 2 * block, Chip RAM */
    UWORD per;
};

static struct MusicPriv music;

static void music_release(void)
{
    if (!music_owns)
        return;
    chan_stop(0);
    chan_stop(1);
    music_owns = false;
}

static bool music_take(void)
{
    int ch;

    if (music_owns)
        return true;
    if (!paula_open)
        return false;
    for (ch = 0; ch < 2; ch++) {
        if (chans[ch].owner != NULL)
            chan_stop(ch);
        chans[ch].owner = OWNER_MUSIC;
    }
    music_owns = true;
    return true;
}

static UWORD music_volume(OggVorbisStream *stream)
{
    LONG v = (LONG)(stream->gain * 64.f + 0.5f);
    if (v < 0)
        v = 0;
    if (v > 64)
        v = 64;
    return v;
}

static void music_close(void)
{
    music_release();
    if (music.fh != INVALID_FILE && music.fh != 0)
        LbFileClose(music.fh);
    music.fh = INVALID_FILE;
    if (music.buf != NULL)
        FreeVec(music.buf);
    music.buf = NULL;
}

static u32 rd_be32(const UBYTE *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

bool ogg_vorbis_stream_init(OggVorbisStream *stream)
{
    memset(stream, 0, sizeof(*stream));
    stream->gain = 1.0f;
    music.fh = INVALID_FILE;
    return paula_open;
}

void ogg_vorbis_stream_free(OggVorbisStream *stream)
{
    ogg_vorbis_stream_clear(stream);
    memset(stream, 0, sizeof(*stream));
}

void ogg_vorbis_stream_pause(OggVorbisStream *stream)
{
    stream->playing = false;
    music_release();
}

void ogg_vorbis_stream_play(OggVorbisStream *stream)
{
    stream->playing = true;
}

void ogg_vorbis_stream_stop(OggVorbisStream *stream)
{
    if (!stream->playing)
        return;
    stream->playing = false;
    music_release();
    ogg_vorbis_stream_restart(stream);
}

void ogg_vorbis_stream_clear(OggVorbisStream *stream)
{
    if (stream->file_name == NULL)
        return;
    ogg_vorbis_stream_stop(stream);
    music_close();
    LbMemoryFree(stream->file_name);
    stream->file_name = NULL;
}

bool ogg_vorbis_stream_open(OggVorbisStream *stream, const char *fname)
{
    UBYTE head[16];
    uint32_t sz;

    ogg_vorbis_stream_clear(stream);
    music.fh = LbFileOpen(fname, Lb_FILE_MODE_READ_ONLY);
    if (music.fh == INVALID_FILE)
        return false;
    if ((LbFileRead(music.fh, head, 16) != 16) || (memcmp(head, "SWMU", 4) != 0))
        goto err;
    music.rate = rd_be32(head + 4);
    music.block = rd_be32(head + 8);
    music.nblocks = rd_be32(head + 12);
    if ((music.block < 256) || (music.block > CHUNK_MAX) || (music.block & 1))
        goto err;
    music.buf = AllocVec(NREQ * 2 * music.block, MEMF_CHIP | MEMF_PUBLIC);
    if (music.buf == NULL)
        goto err;
    music.next_block = 0;
    music.per = rate_to_period(music.rate);
    sz = strlen(fname) + 1;
    stream->file_name = LbMemoryAlloc(sz);
    if (stream->file_name != NULL)
        LbMemoryCopy(stream->file_name, fname, sz);
    return true;
err:
    music_close();
    return false;
}

bool ogg_vorbis_stream_restart(OggVorbisStream *stream)
{
    if ((stream->file_name == NULL) || (music.fh == INVALID_FILE))
        return false;
    music.next_block = 0;
    LbFileSeek(music.fh, 16, Lb_FILE_SEEK_BEGINNING);
    return true;
}

void ogg_vorbis_stream_set_gain(OggVorbisStream *stream, float gain)
{
    stream->gain = gain;
    if (music_owns) {
        UWORD vol = music_volume(stream);
        chan_set(&chans[0], music.per, vol);
        chan_set(&chans[1], music.per, vol);
    }
}

float ogg_vorbis_stream_get_gain(OggVorbisStream *stream)
{
    return stream->gain;
}

bool ogg_vorbis_stream_update(OggVorbisStream *stream)
{
    struct Chan *cl = &chans[0], *cr = &chans[1];
    UWORD vol;

    if ((stream->file_name == NULL) || !stream->playing || (music.buf == NULL))
        return true;
    vol = music_volume(stream);
    if (vol == 0) {
        music_release();
        return true;
    }
    if (!music_take())
        return false;
    chan_reap(cl);
    chan_reap(cr);
    cl->per = cr->per = music.per;
    cl->vol = cr->vol = vol;
    while ((cl->nbusy < NREQ) && (cr->nbusy < NREQ)) {
        UBYTE *slot = music.buf + ((cl->head + cl->nbusy) % NREQ) * 2 * music.block;
        if (music.next_block >= music.nblocks)
            ogg_vorbis_stream_restart(stream);
        if (LbFileRead(music.fh, slot, 2 * music.block) != (long)(2 * music.block)) {
            /* broken file: start it again */
            ogg_vorbis_stream_restart(stream);
            if (LbFileRead(music.fh, slot, 2 * music.block) != (long)(2 * music.block)) {
                stream->playing = false;
                music_release();
                return false;
            }
        }
        music.next_block++;
        snd_cnt.music_blocks++;
        chan_write(cl, slot, music.block, 1);
        chan_write(cr, slot + music.block, music.block, 1);
    }
    return true;
}
