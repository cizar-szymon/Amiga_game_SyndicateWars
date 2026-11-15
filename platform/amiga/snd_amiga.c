/* Amiga sound driver for bfsoundlib: Paula (audio.device) or AHI.
 *
 * Takes the place of bfsoundlib/src/drv_oal.c (OpenAL) and oggvorbis.c:
 * the same OPENAL_* and ogg_vorbis_stream_* functions.
 *
 * - Effects: each playing SNDSAMPLE gets a voice, a free one or the
 *   quietest one, if that is not louder.  A sample without a voice goes on
 *   silently and ends when its time is up, as if it played (a looped one
 *   takes a voice when one gets free).  Samples are 8-bit signed mono
 *   (tools/amiga/make_sound_bank.py); others are silent.
 *   Paula: the 4 channels; a channel of the sample's side (0 and 3 left,
 *   1 and 2 right); the data must be in Chip RAM (the sound heap is there).
 *   AHI: 8 voices mixed by AHI with real panning; the sound heap (Fast RAM)
 *   is one AHI sound, samples are parts of it.
 * - Music: the CD tracks pre-converted to 8-bit signed stereo PCM
 *   (tools/amiga/make_music.py), streamed from disk to voices 0 (left) and
 *   1 (right); effects then have the other voices.
 * - MIDI sequences are not played.
 *
 * Panning follows the PC (OpenAL) build: pan above 64 is left, below 64
 * (but not 0) right, else the middle.
 * ENV:SyndWarsSound = PAULA (default), AHI or NONE; ENV:SyndWarsMusic = OFF
 * turns the music off; with
 * ENV:SyndWarsSoundLog counters go to RAM:SyndWars-sound.txt every 5 s.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include <exec/exec.h>
#include <exec/memory.h>
#include <devices/audio.h>
#include <devices/ahi.h>
#include <utility/hooks.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/ahi.h>

#include "drv_oal.h"
#include "oggvorbis.h"
#include "aildebug.h"
#include "bffile.h"
#include "bfmemory.h"

#define MAX_VOICES 8
#define PAULA_VOICES 4
#define AHI_VOICES 8
#define NREQ 3
/* Longest Paula write: the length register counts 16-bit words */
#define CHUNK_MAX 65536
#define MAX_SAMPLES 64
#define OWNER_MUSIC ((SNDSAMPLE *)1)

/* AHI sounds */
#define AHI_SND_HEAP 0
#define AHI_SND_MUSIC_L 1
#define AHI_SND_MUSIC_R 2

enum SndBackend { SND_NONE = 0, SND_PAULA, SND_AHI };

struct Voice {
    struct IOAudio *req[NREQ];  /**< Paula: writes, issued and done in order */
    struct IOAudio *pv;         /**< Paula: period/volume changes */
    UBYTE head;                 /**< Paula: oldest busy write */
    UBYTE nbusy;
    SNDSAMPLE *owner;           /**< effect playing, OWNER_MUSIC or NULL */
    UWORD vol;                  /**< 0..64 */
    UWORD per;                  /**< Paula period */
    LONG rate;                  /**< AHI frequency */
    LONG pan;                   /**< AHI pan, 0 left .. 0x10000 right */
};

struct SmpState {
    const UBYTE *beg;           /**< whole sample data */
    ULONG len;
    const UBYTE *data;          /**< Paula: data not yet sent */
    ULONG left;
    LONG loops;                 /**< repeats after this pass, -1 forever */
    ULONG end_ms;               /**< end (when not looped forever) */
    ULONG pos_mark;             /**< pos[] as set by us; else restarted */
    LONG rate;                  /**< rate of per_cache */
    UWORD per_cache;
    BYTE ch;                    /**< voice, -1 none */
    UBYTE active;
    UBYTE sent_whole;           /**< Paula, short sample: its write is sent */
    UBYTE playable;             /**< 8-bit mono data the backend can reach */
};

static enum SndBackend backend;
static int n_voices;
static struct Voice voices[MAX_VOICES];
static struct SmpState smp_state[MAX_SAMPLES];
static bool music_owns;         /**< voices 0 and 1 are the music's */

/* Paula */
static struct MsgPort *snd_port;
static struct IOAudio *alloc_req;
static ULONG paula_clock = 3546895;

/* AHI */
struct Library *AHIBase;
static struct MsgPort *ahi_port;
static struct AHIRequest *ahi_io;
static struct AHIAudioCtrl *ahi_ctrl;
static struct Hook ahi_hook;
static const UBYTE *ahi_heap;   /**< the sound heap, AHI sound 0 */
static ULONG ahi_heap_len;
volatile ULONG ahi_music_starts; /**< sounds started on voice 0 */

/* The sound function of AHI: counts the sounds started on voice 0.  It may
 * be called from the ahi_SetSound() of the game or from AHI's mixing. */
static ULONG ahi_hook_entry(struct Hook *hook __asm("a0"),
    struct AHIAudioCtrl *actrl __asm("a2"),
    struct AHISoundMessage *msg __asm("a1"))
{
    (void)hook;
    (void)actrl;
    if (msg->ahism_Channel == 0)
        ahi_music_starts++;
    return 0;
}

/* AHI calls with the UWORD arguments passed as whole 32-bit registers (the
 * inline macros set only the low word; this does not depend on how AHI
 * reads them). */
static ULONG ahi_LoadSound(ULONG sound, ULONG type, APTR info, struct AHIAudioCtrl *ac)
{
    register ULONG d0 __asm("d0") = sound;
    register ULONG d1 __asm("d1") = type;
    register APTR a0 __asm("a0") = info;
    register struct AHIAudioCtrl *a2 __asm("a2") = ac;
    register struct Library *a6 __asm("a6") = AHIBase;
    __asm volatile ("jsr -90(%%a6)"
        : "+r" (d0), "+r" (d1), "+r" (a0) : "r" (a2), "r" (a6) : "a1", "cc", "memory");
    return d0;
}

static void ahi_UnloadSound(ULONG sound, struct AHIAudioCtrl *ac)
{
    register ULONG d0 __asm("d0") = sound;
    register struct AHIAudioCtrl *a2 __asm("a2") = ac;
    register struct Library *a6 __asm("a6") = AHIBase;
    __asm volatile ("jsr -96(%%a6)"
        : "+r" (d0) : "r" (a2), "r" (a6) : "d1", "a0", "a1", "cc", "memory");
}

static void ahi_SetVol(ULONG ch, LONG vol, LONG pan, struct AHIAudioCtrl *ac, ULONG flags)
{
    register ULONG d0 __asm("d0") = ch;
    register LONG d1 __asm("d1") = vol;
    register LONG d2 __asm("d2") = pan;
    register ULONG d3 __asm("d3") = flags;
    register struct AHIAudioCtrl *a2 __asm("a2") = ac;
    register struct Library *a6 __asm("a6") = AHIBase;
    __asm volatile ("jsr -66(%%a6)"
        : "+r" (d0), "+r" (d1) : "r" (d2), "r" (d3), "r" (a2), "r" (a6)
        : "a0", "a1", "cc", "memory");
}

static void ahi_SetFreq(ULONG ch, ULONG freq, struct AHIAudioCtrl *ac, ULONG flags)
{
    register ULONG d0 __asm("d0") = ch;
    register ULONG d1 __asm("d1") = freq;
    register ULONG d2 __asm("d2") = flags;
    register struct AHIAudioCtrl *a2 __asm("a2") = ac;
    register struct Library *a6 __asm("a6") = AHIBase;
    __asm volatile ("jsr -72(%%a6)"
        : "+r" (d0), "+r" (d1) : "r" (d2), "r" (a2), "r" (a6)
        : "a0", "a1", "cc", "memory");
}

static void ahi_SetSound(ULONG ch, ULONG sound, ULONG offs, LONG len,
    struct AHIAudioCtrl *ac, ULONG flags)
{
    register ULONG d0 __asm("d0") = ch;
    register ULONG d1 __asm("d1") = sound;
    register ULONG d2 __asm("d2") = offs;
    register LONG d3 __asm("d3") = len;
    register ULONG d4 __asm("d4") = flags;
    register struct AHIAudioCtrl *a2 __asm("a2") = ac;
    register struct Library *a6 __asm("a6") = AHIBase;
    __asm volatile ("jsr -78(%%a6)"
        : "+r" (d0), "+r" (d1) : "r" (d2), "r" (d3), "r" (d4), "r" (a2), "r" (a6)
        : "a0", "a1", "cc", "memory");
}

/* the sound heap */
static void *heap_mem;
static ULONG heap_size;

/* diagnostics */
static struct {
    ULONG started, on_chan, stolen, no_chan, not_reach, not_8bit, music_blocks;
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
    FPrintf(fh, (STRPTR)"backend %ld voices %ld clock %lu ms %lu heap %lu\n",
        (LONG)backend, (LONG)n_voices, paula_clock, now, heap_size);
    FPrintf(fh, (STRPTR)"started %lu on_voice %lu stolen %lu no_voice %lu\n",
        snd_cnt.started, snd_cnt.on_chan, snd_cnt.stolen, snd_cnt.no_chan);
    FPrintf(fh, (STRPTR)"not_reachable %lu not_8bit %lu music_blocks %lu music_owns %ld ahi_starts %lu\n",
        snd_cnt.not_reach, snd_cnt.not_8bit, snd_cnt.music_blocks, (LONG)music_owns,
        ahi_music_starts);
    Close(fh);
}

static ULONG snd_ms(void)
{
    return AIL_ms_count();
}

/******************************************************************************/
/* The sound heap: in Chip RAM for Paula, anywhere for AHI */

static void ahi_heap_load(void)
{
    struct AHISampleInfo si;

    if ((ahi_ctrl == NULL) || (heap_mem == NULL))
        return;
    si.ahisi_Type = AHIST_M8S;
    si.ahisi_Address = heap_mem;
    si.ahisi_Length = heap_size;
    if (ahi_LoadSound(AHI_SND_HEAP, AHIST_SAMPLE, &si, ahi_ctrl) == AHIE_OK) {
        ahi_heap = heap_mem;
        ahi_heap_len = heap_size;
    }
}

static void ahi_heap_unload(void)
{
    int ch;

    if ((ahi_ctrl == NULL) || (ahi_heap == NULL))
        return;
    for (ch = 0; ch < n_voices; ch++) {
        if ((voices[ch].owner != NULL) && (voices[ch].owner != OWNER_MUSIC))
            ahi_SetSound(ch, AHI_NOSOUND, 0, 0, ahi_ctrl, AHISF_IMM);
    }
    ahi_UnloadSound(AHI_SND_HEAP, ahi_ctrl);
    ahi_heap = NULL;
    ahi_heap_len = 0;
}

ulong amiga_sound_heap_max(void)
{
    return (backend == SND_AHI) ? 1500000 : 800000;
}

void *amiga_sound_heap_alloc(ulong size)
{
    void *p;

    if (backend == SND_AHI) {
        p = AllocVec(size, MEMF_ANY);
    } else {
        /* leave some Chip RAM for the system */
        if (AvailMem(MEMF_CHIP | MEMF_LARGEST) < size + 200000)
            return NULL;
        p = AllocVec(size, MEMF_CHIP | MEMF_PUBLIC);
    }
    if (p != NULL) {
        heap_mem = p;
        heap_size = size;
        ahi_heap_load();
    }
    return p;
}

void amiga_sound_heap_free(void *ptr)
{
    if (ptr == NULL)
        return;
    if (ptr == heap_mem) {
        ahi_heap_unload();
        heap_mem = NULL;
        heap_size = 0;
    }
    FreeVec(ptr);
}

/******************************************************************************/
/* Paula voices */

/* BeginIO() keeps io_Flags (SendIO() clears ADIOF_PERVOL). */
static void begin_io(struct IORequest *r)
{
    register struct IORequest *a1 __asm("a1") = r;
    register struct Device *a6 __asm("a6") = r->io_Device;
    __asm volatile ("jsr -30(%%a6)"
        : "+r" (a1), "+r" (a6) : : "d0", "d1", "a0", "cc", "memory");
}

static void paula_reap(struct Voice *c)
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

static void paula_write(struct Voice *c, const UBYTE *data, ULONG len,
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

/** Stops the voice at once; its owner (if an effect) goes on silently. */
static void voice_stop(int ch)
{
    struct Voice *c = &voices[ch];
    int k;

    if (backend == SND_PAULA) {
        for (k = c->nbusy - 1; k >= 0; k--) {
            struct IOAudio *r = c->req[(c->head + k) % NREQ];
            AbortIO((struct IORequest *)r);
            WaitIO((struct IORequest *)r);
        }
        c->nbusy = 0;
        c->head = 0;
    } else if (backend == SND_AHI) {
        ahi_SetSound(ch, AHI_NOSOUND, 0, 0, ahi_ctrl, AHISF_IMM);
    }
    if ((c->owner != NULL) && (c->owner != OWNER_MUSIC)) {
        SNDSAMPLE *s = c->owner;
        int n = s - s->driver->samples;
        if ((n >= 0) && (n < MAX_SAMPLES))
            smp_state[n].ch = -1;
    }
    c->owner = NULL;
}

/** Sets the voice's rate, volume (0..64) and pan (AHI only). */
static void voice_set(int ch, LONG rate, UWORD per, UWORD vol, LONG pan)
{
    struct Voice *c = &voices[ch];

    if (backend == SND_PAULA) {
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
    } else if (backend == SND_AHI) {
        if (c->rate != rate) {
            c->rate = rate;
            ahi_SetFreq(ch, rate, ahi_ctrl, AHISF_IMM);
        }
        if ((c->vol != vol) || (c->pan != pan)) {
            c->vol = vol;
            c->pan = pan;
            ahi_SetVol(ch, (Fixed)vol << 10, pan, ahi_ctrl, AHISF_IMM);
        }
    }
}

/******************************************************************************/

static bool paula_startup(void)
{
    static UBYTE all_chans = 0x0F;
    int ch, k;

    if (SysBase->ex_EClockFrequency != 0)
        paula_clock = SysBase->ex_EClockFrequency * 5;
    snd_port = CreateMsgPort();
    if (snd_port == NULL)
        return false;
    alloc_req = (struct IOAudio *)CreateIORequest(snd_port, sizeof(struct IOAudio));
    if (alloc_req == NULL)
        return false;
    alloc_req->ioa_Request.io_Message.mn_Node.ln_Pri = 127;
    alloc_req->ioa_Request.io_Flags = ADIOF_NOWAIT;
    alloc_req->ioa_Data = &all_chans;
    alloc_req->ioa_Length = 1;
    if (OpenDevice((STRPTR)AUDIONAME, 0, (struct IORequest *)alloc_req, 0) != 0) {
        DeleteIORequest((struct IORequest *)alloc_req);
        alloc_req = NULL;
        return false;
    }
    for (ch = 0; ch < PAULA_VOICES; ch++) {
        struct Voice *c = &voices[ch];
        for (k = 0; k <= NREQ; k++) {
            struct IOAudio *r;
            r = (struct IOAudio *)CreateIORequest(snd_port, sizeof(struct IOAudio));
            if (r == NULL)
                return false;
            r->ioa_Request.io_Device = alloc_req->ioa_Request.io_Device;
            r->ioa_Request.io_Unit = (struct Unit *)(1UL << ch);
            r->ioa_AllocKey = alloc_req->ioa_AllocKey;
            if (k < NREQ)
                c->req[k] = r;
            else
                c->pv = r;
        }
    }
    n_voices = PAULA_VOICES;
    return true;
}

static void paula_shutdown(void)
{
    int ch, k;

    for (ch = 0; ch < PAULA_VOICES; ch++) {
        struct Voice *c = &voices[ch];
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
}

static bool music_ahi_setup(void);
static void music_ahi_free(void);

static bool ahi_startup(void)
{
    ahi_port = CreateMsgPort();
    if (ahi_port == NULL)
        return false;
    ahi_io = (struct AHIRequest *)CreateIORequest(ahi_port, sizeof(struct AHIRequest));
    if (ahi_io == NULL)
        return false;
    ahi_io->ahir_Version = 4;
    if (OpenDevice((STRPTR)AHINAME, AHI_NO_UNIT, (struct IORequest *)ahi_io, 0) != 0) {
        DeleteIORequest((struct IORequest *)ahi_io);
        ahi_io = NULL;
        return false;
    }
    AHIBase = (struct Library *)ahi_io->ahir_Std.io_Device;
    ahi_hook.h_Entry = (HOOKFUNC)ahi_hook_entry;
    ahi_ctrl = AHI_AllocAudio(
        AHIA_AudioID, AHI_DEFAULT_ID,
        AHIA_MixFreq, AHI_DEFAULT_FREQ,
        AHIA_Channels, AHI_VOICES,
        AHIA_Sounds, 3,
        AHIA_SoundFunc, (ULONG)&ahi_hook,
        TAG_DONE);
    if (ahi_ctrl == NULL)
        return false;
    /* sounds loaded before playing starts */
    if (!music_ahi_setup())
        return false;
    if (AHI_ControlAudio(ahi_ctrl, AHIC_Play, TRUE, TAG_DONE) != AHIE_OK)
        return false;
    n_voices = AHI_VOICES;
    return true;
}

static void ahi_shutdown(void)
{
    music_ahi_free();
    if (ahi_ctrl != NULL) {
        AHI_ControlAudio(ahi_ctrl, AHIC_Play, FALSE, TAG_DONE);
        AHI_FreeAudio(ahi_ctrl);
        ahi_ctrl = NULL;
    }
    ahi_heap = NULL;
    if (ahi_io != NULL) {
        if (AHIBase != NULL)
            CloseDevice((struct IORequest *)ahi_io);
        DeleteIORequest((struct IORequest *)ahi_io);
        ahi_io = NULL;
    }
    AHIBase = NULL;
    if (ahi_port != NULL)
        DeleteMsgPort(ahi_port);
    ahi_port = NULL;
}

static enum SndBackend backend_wanted(void)
{
    char buf[16];

    if (GetVar((STRPTR)"SyndWarsSound", (STRPTR)buf, sizeof(buf), 0) < 0)
        return SND_PAULA;
    if (strcasecmp(buf, "NONE") == 0)
        return SND_NONE;
    if (strcasecmp(buf, "AHI") == 0)
        return SND_AHI;
    return SND_PAULA;
}

int32_t OPENAL_startup(void)
{
    enum SndBackend want;
    int ch;

    if (backend != SND_NONE)
        return 1;
    for (ch = 0; ch < MAX_VOICES; ch++)
        memset(&voices[ch], 0, sizeof(voices[ch]));
    want = backend_wanted();
    if (want == SND_AHI) {
        if (ahi_startup()) {
            backend = SND_AHI;
            ahi_heap_load();
            return 1;
        }
        ahi_shutdown();
        want = SND_PAULA;  /* no AHI: Paula */
    }
    if (want == SND_PAULA) {
        if (paula_startup()) {
            backend = SND_PAULA;
            return 1;
        }
        paula_shutdown();
    }
    n_voices = 0;
    return 0;
}

int32_t OPENAL_shutdown(void)
{
    int ch;

    for (ch = 0; ch < n_voices; ch++)
        voice_stop(ch);
    music_owns = false;
    if (backend == SND_PAULA)
        paula_shutdown();
    else if (backend == SND_AHI)
        ahi_shutdown();
    backend = SND_NONE;
    n_voices = 0;
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
/* Effects */

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

/** AHI pan as the OpenAL position of the PC build. */
static LONG smp_ahi_pan(SNDSAMPLE *s)
{
    LONG p;

    if (s->pan == 0)
        return 0x8000;
    p = 0x8000 + (63 - s->pan) * 0x200;
    if (p < 0)
        p = 0;
    if (p > 0x10000)
        p = 0x10000;
    return p;
}

static bool voice_usable(int ch)
{
    return !(music_owns && (ch < 2));
}

/** A voice for the sample, or -1. */
static int pick_voice(SNDSAMPLE *s, UWORD vol)
{
    static const BYTE order[3][PAULA_VOICES] = {
        {3, 0, 2, 1},   /* left */
        {2, 1, 3, 0},   /* right */
        {3, 2, 0, 1},   /* middle */
    };
    static const BYTE ahi_order[AHI_VOICES] = {2, 3, 4, 5, 6, 7, 0, 1};
    const BYTE *ord;
    int k, best = -1;
    UWORD best_vol = 0xFFFF;

    if (backend == SND_PAULA)
        ord = order[smp_side(s)];
    else if (backend == SND_AHI)
        ord = ahi_order;
    else
        return -1;
    for (k = 0; k < n_voices; k++) {
        int ch = ord[k];
        if (voice_usable(ch) && (voices[ch].owner == NULL))
            return ch;
    }
    /* none free: the quietest, the own side first */
    for (k = 0; k < n_voices; k++) {
        int ch = ord[k];
        if (!voice_usable(ch) || (voices[ch].owner == OWNER_MUSIC))
            continue;
        if (voices[ch].vol < best_vol) {
            best_vol = voices[ch].vol;
            best = ch;
        }
    }
    if ((best >= 0) && (best_vol <= vol)) {
        snd_cnt.stolen++;
        voice_stop(best);
        return best;
    }
    return -1;
}

/** Paula: sends the sample's data to its channel, keeping 2 writes queued. */
static void paula_feed(struct SmpState *st)
{
    struct Voice *c = &voices[(int)st->ch];

    paula_reap(c);
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
            paula_write(c, st->beg, st->len, cycles);
            st->sent_whole = 1;
            break;
        }
        n = st->left;
        if (n > CHUNK_MAX)
            n = CHUNK_MAX;
        paula_write(c, st->data, n, 1);
        st->data += n;
        st->left -= n;
    }
}

static void smp_to_voice(SNDSAMPLE *s, struct SmpState *st, int ch)
{
    struct Voice *c = &voices[ch];

    c->owner = s;
    st->ch = ch;
    if (backend == SND_PAULA) {
        c->per = st->per_cache;
        c->vol = smp_volume(s);
        st->sent_whole = 0;
        paula_feed(st);
    } else {
        c->rate = st->rate;
        c->vol = smp_volume(s);
        c->pan = smp_ahi_pan(s);
        ahi_SetFreq(ch, c->rate, ahi_ctrl, AHISF_IMM);
        ahi_SetVol(ch, (Fixed)c->vol << 10, c->pan, ahi_ctrl, AHISF_IMM);
        ahi_SetSound(ch, AHI_SND_HEAP, st->beg - ahi_heap, st->len, ahi_ctrl, AHISF_IMM);
        if (st->loops == 0) /* played once: then nothing */
            ahi_SetSound(ch, AHI_NOSOUND, 0, 0, ahi_ctrl, AHISF_NONE);
    }
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
    st->playable = (s->format == DIG_F_MONO_8);
    bps = 1;
    if (s->format & DIG_F_16BITS_MASK)
        bps *= 2;
    if (s->format & DIG_F_STEREO_MASK)
        bps *= 2;
    if (s->format != DIG_F_MONO_8)
        snd_cnt.not_8bit++;
    if (backend == SND_PAULA) {
        /* Paula: even address and length */
        if ((ULONG)data & 1) {
            data++;
            len = (len > 0) ? len - 1 : 0;
        }
        len &= ~1UL;
        if (st->playable && !(TypeOfMem((APTR)data) & MEMF_CHIP)) {
            st->playable = false;
            snd_cnt.not_reach++;
        }
    } else if (backend == SND_AHI) {
        if (st->playable && ((ahi_heap == NULL) || (data < ahi_heap) ||
          (data + len > ahi_heap + ahi_heap_len))) {
            st->playable = false;
            snd_cnt.not_reach++;
        }
    }
    if (len < 2)
        st->playable = false;
    snd_cnt.started++;
    st->beg = st->data = data;
    st->len = st->left = len;
    st->loops = (s->loop_count == 0) ? -1 : s->loop_count - 1;
    if (st->rate != s->playback_rate || st->per_cache == 0) {
        st->rate = (s->playback_rate > 100) ? s->playback_rate : 22050;
        st->per_cache = rate_to_period(st->rate);
    }
    dur = len / bps;
    dur = (dur * 10) / (st->rate / 100);
    if (st->loops > 0)
        dur *= st->loops + 1;
    st->end_ms = snd_ms() + dur;
    st->active = 1;
    st->ch = -1;
    if (st->playable) {
        ch = pick_voice(s, smp_volume(s));
        if (ch >= 0) {
            snd_cnt.on_chan++;
            smp_to_voice(s, st, ch);
        } else
            snd_cnt.no_chan++;
    }
}

static void smp_end(struct SmpState *st)
{
    if (st->ch >= 0)
        voice_stop(st->ch);
    st->active = 0;
}

void OPENAL_stop_sample(SNDSAMPLE *s)
{
    struct SmpState *st = smp_st(s);
    if (st != NULL)
        smp_end(st);
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
            smp_end(st);
            continue;
        }
        if (s->pos[s->current_buffer] != st->pos_mark) {
            /* started again */
            smp_end(st);
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
        st->rate = (s->playback_rate > 100) ? s->playback_rate : 22050;
        st->per_cache = rate_to_period(st->rate);
    }
    if ((st->ch >= 0) && (backend == SND_PAULA)) {
        struct Voice *c = &voices[(int)st->ch];
        voice_set(st->ch, st->rate, st->per_cache, smp_volume(s), 0);
        paula_feed(st);
        if (st->len <= CHUNK_MAX)
            finished = (c->nbusy == 0) && st->sent_whole;
        else
            finished = (c->nbusy == 0) && (st->left == 0) && (st->loops == 0);
        if (finished) {
            c->owner = NULL;
            st->ch = -1;
        }
    } else {
        if (st->ch >= 0)
            voice_set(st->ch, st->rate, 0, smp_volume(s), smp_ahi_pan(s));
        if (st->loops < 0) {
            finished = false;
            if ((st->ch < 0) && st->playable) {
                /* a looped sound takes a voice when one gets free */
                int ch;
                for (ch = 0; ch < n_voices; ch++) {
                    if (voice_usable(ch) && (voices[ch].owner == NULL)) {
                        st->data = st->beg;
                        st->left = st->len;
                        smp_to_voice(s, st, ch);
                        break;
                    }
                }
            }
        } else {
            finished = ((LONG)(snd_ms() - st->end_ms) >= 0);
            if (finished && (st->ch >= 0))
                voice_stop(st->ch);
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
/* Music: tracks of 8-bit signed stereo PCM, "SWMU" files.
 * Paula: NREQ slots of a block (left, then right), each slot one write
 * on each channel.  AHI: a ring of 2 halves of 2 blocks for each side,
 * one AHI sound per side; while one half plays the other is filled. */

#define MUSIC_HALF_BLOCKS 2
#define MUSIC_BLOCK_MAX 16384

struct MusicPriv {
    TbFileHandle fh;
    ULONG rate;
    ULONG block;                /**< bytes per channel in a block */
    ULONG nblocks;
    ULONG next_block;
    UBYTE *buf;                 /**< Paula: NREQ slots of 2 * block, Chip RAM */
    UBYTE *ahi_buf;             /**< AHI: a block as read from the file */
    UBYTE *ring[2];             /**< AHI: left and right rings */
    ULONG half;                 /**< AHI: bytes in a half */
    ULONG starts_base;          /**< AHI: ahi_music_starts of the first half */
    ULONG halves_done;          /**< AHI: halves known to have started */
    UWORD per;
    bool ahi_loaded;
};

static struct MusicPriv music = { INVALID_FILE };

static void music_release(void)
{
    if (!music_owns)
        return;
    voice_stop(0);
    voice_stop(1);
    music_owns = false;
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

static void music_free_bufs(void)
{
    if (music.buf != NULL)
        FreeVec(music.buf);
    music.buf = NULL;
}

static void music_close(void)
{
    music_release();
    if ((music.fh != INVALID_FILE) && (music.fh != 0))
        LbFileClose(music.fh);
    music.fh = INVALID_FILE;
    music_free_bufs();
}

static u32 rd_be32(const UBYTE *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

/** Reads the next block (left, then right) to dst; at the end the track
 * starts again. */
static bool music_read_block(UBYTE *dst)
{
    long n = 2 * music.block;

    if (music.next_block >= music.nblocks) {
        music.next_block = 0;
        LbFileSeek(music.fh, 16, Lb_FILE_SEEK_BEGINNING);
    }
    if (LbFileRead(music.fh, dst, n) != n) {
        music.next_block = 0;
        LbFileSeek(music.fh, 16, Lb_FILE_SEEK_BEGINNING);
        if (LbFileRead(music.fh, dst, n) != n)
            return false;
    }
    music.next_block++;
    snd_cnt.music_blocks++;
    return true;
}

static bool music_alloc_bufs(void)
{
    if (backend == SND_PAULA) {
        music.buf = AllocVec(NREQ * 2 * music.block, MEMF_CHIP | MEMF_PUBLIC);
        return (music.buf != NULL);
    }
    if (backend == SND_AHI) {
        /* the rings and their AHI sounds are made with the AHI audio */
        if (!music.ahi_loaded || (music.block > MUSIC_BLOCK_MAX))
            return false;
        music.half = MUSIC_HALF_BLOCKS * music.block;
        return true;
    }
    return false;
}

/** AHI: the music rings, loaded as AHI sounds once, before playing. */
static bool music_ahi_setup(void)
{
    struct AHISampleInfo si;
    ULONG ring = 2 * MUSIC_HALF_BLOCKS * MUSIC_BLOCK_MAX;
    int i;

    music.ahi_buf = AllocVec(2 * MUSIC_BLOCK_MAX, MEMF_ANY);
    for (i = 0; i < 2; i++)
        music.ring[i] = AllocVec(ring, MEMF_ANY | MEMF_CLEAR);
    if ((music.ahi_buf == NULL) || (music.ring[0] == NULL) || (music.ring[1] == NULL))
        return false;
    for (i = 0; i < 2; i++) {
        si.ahisi_Type = AHIST_M8S;
        si.ahisi_Address = music.ring[i];
        si.ahisi_Length = ring;
        if (ahi_LoadSound(AHI_SND_MUSIC_L + i, AHIST_SAMPLE, &si, ahi_ctrl) != AHIE_OK)
            return false;
    }
    music.ahi_loaded = true;
    return true;
}

static void music_ahi_free(void)
{
    int i;

    if (music.ahi_loaded && (ahi_ctrl != NULL)) {
        ahi_UnloadSound(AHI_SND_MUSIC_L, ahi_ctrl);
        ahi_UnloadSound(AHI_SND_MUSIC_R, ahi_ctrl);
    }
    music.ahi_loaded = false;
    if (music.ahi_buf != NULL)
        FreeVec(music.ahi_buf);
    music.ahi_buf = NULL;
    for (i = 0; i < 2; i++) {
        if (music.ring[i] != NULL)
            FreeVec(music.ring[i]);
        music.ring[i] = NULL;
    }
}

/** AHI: fills half h of the rings from the file. */
static bool music_ahi_fill(int h)
{
    ULONG k;

    for (k = 0; k < MUSIC_HALF_BLOCKS; k++) {
        ULONG offs = h * music.half + k * music.block;
        if (!music_read_block(music.ahi_buf))
            return false;
        CopyMem(music.ahi_buf, music.ring[0] + offs, music.block);
        CopyMem(music.ahi_buf + music.block, music.ring[1] + offs, music.block);
    }
    return true;
}

static void music_ahi_queue(int h, ULONG flags)
{
    ahi_SetSound(0, AHI_SND_MUSIC_L, h * music.half, music.half, ahi_ctrl, flags);
    ahi_SetSound(1, AHI_SND_MUSIC_R, h * music.half, music.half, ahi_ctrl, flags);
}

static bool music_take(OggVorbisStream *stream)
{
    int ch;

    if (music_owns)
        return true;
    if (backend == SND_NONE)
        return false;
    for (ch = 0; ch < 2; ch++) {
        if (voices[ch].owner != NULL)
            voice_stop(ch);
        voices[ch].owner = OWNER_MUSIC;
    }
    music_owns = true;
    if (backend == SND_AHI) {
        UWORD vol = music_volume(stream);
        ULONG s0;

        if (!music_ahi_fill(0) || !music_ahi_fill(1))
            return false;
        voices[0].rate = voices[1].rate = music.rate;
        voices[0].vol = voices[1].vol = vol;
        voices[0].pan = 0;
        voices[1].pan = 0x10000;
        ahi_SetFreq(0, music.rate, ahi_ctrl, AHISF_IMM);
        ahi_SetFreq(1, music.rate, ahi_ctrl, AHISF_IMM);
        ahi_SetVol(0, (Fixed)vol << 10, 0, ahi_ctrl, AHISF_IMM);
        ahi_SetVol(1, (Fixed)vol << 10, 0x10000, ahi_ctrl, AHISF_IMM);
        s0 = ahi_music_starts;
        music_ahi_queue(0, AHISF_IMM);
        music_ahi_queue(1, AHISF_NONE);
        /* Is the sound function called for the immediate start too?  Wait
         * for one mixing pass to see; the halves are counted from it. */
        Delay(4);
        music.starts_base = (ahi_music_starts != s0) ? s0 + 1 : s0;
        music.halves_done = 0;
    }
    return true;
}

bool ogg_vorbis_stream_init(OggVorbisStream *stream)
{
    char buf[16];

    memset(stream, 0, sizeof(*stream));
    stream->gain = 1.0f;
    music.fh = INVALID_FILE;
    /* ENV:SyndWarsMusic=OFF: no music, all voices for the effects */
    if ((GetVar((STRPTR)"SyndWarsMusic", (STRPTR)buf, sizeof(buf), 0) >= 0) &&
      (strcasecmp(buf, "OFF") == 0))
        return false;
    return (backend != SND_NONE);
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
    if (backend == SND_NONE)
        return false;
    music.fh = LbFileOpen(fname, Lb_FILE_MODE_READ_ONLY);
    if (music.fh == INVALID_FILE)
        return false;
    if ((LbFileRead(music.fh, head, 16) != 16) || (memcmp(head, "SWMU", 4) != 0))
        goto err;
    music.rate = rd_be32(head + 4);
    music.block = rd_be32(head + 8);
    music.nblocks = rd_be32(head + 12);
    if ((music.block < 256) || (music.block > CHUNK_MAX) || (music.block & 1) ||
      (music.nblocks == 0) || (music.rate < 1000))
        goto err;
    if (!music_alloc_bufs())
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
        voice_set(0, music.rate, music.per, vol, 0);
        voice_set(1, music.rate, music.per, vol, 0x10000);
    }
}

float ogg_vorbis_stream_get_gain(OggVorbisStream *stream)
{
    return stream->gain;
}

static bool music_update_paula(OggVorbisStream *stream, UWORD vol)
{
    struct Voice *cl = &voices[0], *cr = &voices[1];

    paula_reap(cl);
    paula_reap(cr);
    cl->per = cr->per = music.per;
    cl->vol = cr->vol = vol;
    while ((cl->nbusy < NREQ) && (cr->nbusy < NREQ)) {
        UBYTE *slot = music.buf + ((cl->head + cl->nbusy) % NREQ) * 2 * music.block;
        if (!music_read_block(slot)) {
            stream->playing = false;
            music_release();
            return false;
        }
        paula_write(cl, slot, music.block, 1);
        paula_write(cr, slot + music.block, music.block, 1);
    }
    return true;
}

static bool music_update_ahi(OggVorbisStream *stream)
{
    ULONG started = ahi_music_starts - music.starts_base;

    /* a new half started: the other one is free, fill it and queue it */
    while (music.halves_done < started) {
        int h;
        music.halves_done++;
        h = (music.halves_done + 1) & 1;
        if (!music_ahi_fill(h)) {
            stream->playing = false;
            music_release();
            return false;
        }
        music_ahi_queue(h, AHISF_NONE);
    }
    return true;
}

bool ogg_vorbis_stream_update(OggVorbisStream *stream)
{
    UWORD vol;

    if ((stream->file_name == NULL) || !stream->playing)
        return true;
    if ((backend == SND_PAULA) && (music.buf == NULL))
        return true;
    vol = music_volume(stream);
    if (vol == 0) {
        music_release();
        return true;
    }
    if (!music_take(stream)) {
        stream->playing = false;
        music_release();
        return false;
    }
    if (backend == SND_PAULA)
        return music_update_paula(stream, vol);
    if (backend == SND_AHI)
        return music_update_ahi(stream);
    return true;
}
