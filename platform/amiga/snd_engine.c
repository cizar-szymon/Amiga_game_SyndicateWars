/* snd_engine.c - the Amiga sound engine: Paula (audio.device) or AHI.
 * See snd_engine.h.
 *
 * - Effects: each playing slot gets a voice, a free one or the quietest
 *   one, if that is not louder.  A slot without a voice goes on silently
 *   and ends when its time is up, as if it played (one played forever takes
 *   a voice when one gets free).
 *   Paula: the 4 channels; a channel of the sound's side (0 and 3 left,
 *   1 and 2 right); the data must be in Chip RAM.
 *   AHI: 8 voices mixed by AHI with real panning; the sound heap is one AHI
 *   sound, effects are parts of it.
 * - Music: 8-bit signed stereo PCM tracks streamed from disk to voices 0
 *   (left) and 1 (right); the effects then have the other voices.
 *   Paula: NREQ slots of a block (left, then right), each one write on each
 *   channel.  AHI: a ring of 2 halves of 2 blocks for each side, one AHI
 *   sound per side; while one half plays the other is filled.
 * Panning follows the PC (OpenAL) build of the game.
 */
#include <string.h>
#include <stdlib.h>

#include <exec/exec.h>
#include <exec/memory.h>
#include <devices/audio.h>
#include <devices/ahi.h>
#include <utility/hooks.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/ahi.h>
#ifdef __VBCC__
#include <clib/alib_protos.h>
#endif

#include "snd_engine.h"

#ifndef AHISF_NONE
#define AHISF_NONE 0    /* older devices/ahi.h */
#endif

#define MAX_VOICES 8
#define PAULA_VOICES 4
#define AHI_VOICES 8
#define NREQ 3
/* Longest Paula write: the length register counts 16-bit words */
#define CHUNK_MAX 65536
#define OWNER_NONE (-1)
#define OWNER_MUSIC (-2)

/* AHI sounds */
#define AHI_SND_HEAP 0
#define AHI_SND_MUSIC_L 1
#define AHI_SND_MUSIC_R 2

struct Voice {
    struct IOAudio *req[NREQ];  /**< Paula: writes, issued and done in order */
    struct IOAudio *pv;         /**< Paula: period/volume changes */
    UBYTE head;                 /**< Paula: oldest busy write */
    UBYTE nbusy;
    WORD owner;                 /**< slot playing, OWNER_MUSIC or OWNER_NONE */
    UWORD vol;                  /**< 0..64 */
    UWORD per;                  /**< Paula period */
    LONG rate;                  /**< AHI frequency */
    LONG pan;                   /**< AHI pan, 0 left .. 0x10000 right */
};

struct Slot {
    const UBYTE *beg;           /**< whole sound data */
    ULONG len;
    const UBYTE *data;          /**< Paula: data not yet sent */
    ULONG left;
    LONG loops;                 /**< repeats after this pass, -1 forever */
    ULONG end_ms;               /**< end (when not played forever) */
    ULONG pass_ms;              /**< length of one pass */
    LONG rate;
    UWORD per;
    UWORD vol;
    WORD pan;
    BYTE ch;                    /**< voice, -1 none */
    UBYTE active;
    UBYTE sent_whole;           /**< Paula, short sound: its write is sent */
    UBYTE playable;             /**< data the backend can play */
};

static int backend;
static int n_voices;
static struct Voice voices[MAX_VOICES];
static struct Slot slots[SE_SLOTS];
static BOOL music_owns;         /**< voices 0 and 1 are the music's */

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

/* the sound heap */
static void *heap_mem;
static ULONG heap_size;

/* diagnostics */
static struct {
    ULONG started, on_chan, stolen, no_chan, not_reach, music_blocks, released;
} snd_cnt;
static int snd_log = -1;
static ULONG snd_log_ms;

/******************************************************************************/
/* Compiler specific: the AHI sound function, BeginIO() and the AHI calls */

#ifdef __VBCC__

static ULONG ahi_hook_entry(__reg("a0") struct Hook *hook,
    __reg("a2") struct AHIAudioCtrl *actrl,
    __reg("a1") struct AHISoundMessage *msg)
{
    (void)hook;
    (void)actrl;
    if (msg->ahism_Channel == 0)
        ahi_music_starts++;
    return 0;
}

#define begin_io(r) BeginIO(r)
#define ahi_LoadSound(s, t, i, a) AHI_LoadSound((s), (t), (i), (a))
#define ahi_UnloadSound(s, a) AHI_UnloadSound((s), (a))
#define ahi_SetVol(c, v, p, a, f) AHI_SetVol((c), (v), (p), (a), (f))
#define ahi_SetFreq(c, fr, a, f) AHI_SetFreq((c), (fr), (a), (f))
#define ahi_SetSound(c, s, o, l, a, f) AHI_SetSound((c), (s), (o), (l), (a), (f))

#else /* gcc */

/* The sound function of AHI: counts the sounds started on voice 0.  It may
 * be called from ahi_SetSound() or from AHI's mixing. */
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

/* BeginIO() keeps io_Flags (SendIO() clears ADIOF_PERVOL). */
static void begin_io(struct IORequest *r)
{
    register struct IORequest *a1 __asm("a1") = r;
    register struct Device *a6 __asm("a6") = r->io_Device;
    __asm volatile ("jsr -30(%%a6)"
        : "+r" (a1), "+r" (a6) : : "d0", "d1", "a0", "cc", "memory");
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

#endif

/******************************************************************************/

void se_log(const char *path)
{
    char buf[16];
    BPTR fh;
    ULONG now;

    if (snd_log < 0)
        snd_log = (GetVar((STRPTR)"SyndWarsSoundLog", (STRPTR)buf, sizeof(buf), 0) >= 0);
    if (!snd_log)
        return;
    now = se_ms();
    if ((LONG)(now - snd_log_ms) < 5000)
        return;
    snd_log_ms = now;
    fh = Open((STRPTR)path, MODE_NEWFILE);
    if (fh == 0)
        return;
    FPrintf(fh, (STRPTR)"backend %ld voices %ld clock %lu ms %lu heap %lu\n",
        (LONG)backend, (LONG)n_voices, paula_clock, now, heap_size);
    FPrintf(fh, (STRPTR)"started %lu on_voice %lu stolen %lu no_voice %lu released %lu\n",
        snd_cnt.started, snd_cnt.on_chan, snd_cnt.stolen, snd_cnt.no_chan, snd_cnt.released);
    FPrintf(fh, (STRPTR)"not_reachable %lu music_blocks %lu music_owns %ld ahi_starts %lu\n",
        snd_cnt.not_reach, snd_cnt.music_blocks, (LONG)music_owns, ahi_music_starts);
    Close(fh);
}

/******************************************************************************/
/* The sound heap */

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

static void voice_stop(int ch);

static void ahi_heap_unload(void)
{
    int ch;

    if ((ahi_ctrl == NULL) || (ahi_heap == NULL))
        return;
    for (ch = 0; ch < n_voices; ch++) {
        if (voices[ch].owner >= 0)
            voice_stop(ch);
    }
    ahi_UnloadSound(AHI_SND_HEAP, ahi_ctrl);
    ahi_heap = NULL;
    ahi_heap_len = 0;
}

ULONG se_heap_max(void)
{
    return (backend == SE_AHI) ? 1500000 : 800000;
}

void *se_heap_alloc(ULONG size)
{
    void *p;

    if (backend == SE_AHI) {
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

void se_heap_free(void *ptr)
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
/* Voices */

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
    return (UWORD)per;
}

/** Stops the voice at once; its slot (if an effect) goes on silently. */
static void voice_stop(int ch)
{
    struct Voice *c = &voices[ch];
    int k;

    if (backend == SE_PAULA) {
        for (k = c->nbusy - 1; k >= 0; k--) {
            struct IOAudio *r = c->req[(c->head + k) % NREQ];
            AbortIO((struct IORequest *)r);
            WaitIO((struct IORequest *)r);
        }
        c->nbusy = 0;
        c->head = 0;
    } else if (backend == SE_AHI) {
        ahi_SetSound(ch, AHI_NOSOUND, 0, 0, ahi_ctrl, AHISF_IMM);
    }
    if (c->owner >= 0)
        slots[c->owner].ch = -1;
    c->owner = OWNER_NONE;
}

/** Sets the voice's rate, volume (0..64) and pan (AHI only). */
static void voice_set(int ch, LONG rate, UWORD per, UWORD vol, LONG pan)
{
    struct Voice *c = &voices[ch];

    if (backend == SE_PAULA) {
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
    } else if (backend == SE_AHI) {
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

static BOOL paula_startup(void)
{
    static UBYTE all_chans = 0x0F;
    int ch, k;

    if (SysBase->ex_EClockFrequency != 0)
        paula_clock = SysBase->ex_EClockFrequency * 5;
    snd_port = CreateMsgPort();
    if (snd_port == NULL)
        return FALSE;
    alloc_req = (struct IOAudio *)CreateIORequest(snd_port, sizeof(struct IOAudio));
    if (alloc_req == NULL)
        return FALSE;
    alloc_req->ioa_Request.io_Message.mn_Node.ln_Pri = 127;
    alloc_req->ioa_Request.io_Flags = ADIOF_NOWAIT;
    alloc_req->ioa_Data = &all_chans;
    alloc_req->ioa_Length = 1;
    if (OpenDevice((STRPTR)AUDIONAME, 0, (struct IORequest *)alloc_req, 0) != 0) {
        DeleteIORequest((struct IORequest *)alloc_req);
        alloc_req = NULL;
        return FALSE;
    }
    for (ch = 0; ch < PAULA_VOICES; ch++) {
        struct Voice *c = &voices[ch];
        for (k = 0; k <= NREQ; k++) {
            struct IOAudio *r;
            r = (struct IOAudio *)CreateIORequest(snd_port, sizeof(struct IOAudio));
            if (r == NULL)
                return FALSE;
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
    return TRUE;
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

static BOOL music_ahi_setup(void);
static void music_ahi_free(void);

static BOOL ahi_startup(void)
{
    ahi_port = CreateMsgPort();
    if (ahi_port == NULL)
        return FALSE;
    ahi_io = (struct AHIRequest *)CreateIORequest(ahi_port, sizeof(struct AHIRequest));
    if (ahi_io == NULL)
        return FALSE;
    ahi_io->ahir_Version = 4;
    if (OpenDevice((STRPTR)AHINAME, AHI_NO_UNIT, (struct IORequest *)ahi_io, 0) != 0) {
        DeleteIORequest((struct IORequest *)ahi_io);
        ahi_io = NULL;
        return FALSE;
    }
    AHIBase = (struct Library *)ahi_io->ahir_Std.io_Device;
    ahi_hook.h_Entry = (HOOKFUNC)(APTR)ahi_hook_entry;
    ahi_ctrl = AHI_AllocAudio(
        AHIA_AudioID, AHI_DEFAULT_ID,
        AHIA_MixFreq, AHI_DEFAULT_FREQ,
        AHIA_Channels, AHI_VOICES,
        AHIA_Sounds, 3,
        AHIA_SoundFunc, (ULONG)&ahi_hook,
        TAG_DONE);
    if (ahi_ctrl == NULL)
        return FALSE;
    /* sounds loaded before playing starts */
    if (!music_ahi_setup())
        return FALSE;
    if (AHI_ControlAudio(ahi_ctrl, AHIC_Play, TRUE, TAG_DONE) != AHIE_OK)
        return FALSE;
    n_voices = AHI_VOICES;
    return TRUE;
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

/** Compares an ENV: value with an upper case word, ignoring the case. */
static BOOL env_is(const char *val, const char *word)
{
    for (; *word != '\0'; val++, word++) {
        char c = *val;
        if ((c >= 'a') && (c <= 'z'))
            c -= 'a' - 'A';
        if (c != *word)
            return FALSE;
    }
    return (*val == '\0') || (*val == '\n') || (*val == ' ');
}

int se_backend_from_env(void)
{
    char buf[16];

    if (GetVar((STRPTR)"SyndWarsSound", (STRPTR)buf, sizeof(buf), 0) < 0)
        return SE_PAULA;
    if (env_is(buf, "NONE"))
        return SE_NONE;
    if (env_is(buf, "AHI"))
        return SE_AHI;
    return SE_PAULA;
}

/** At the exit of the program, however it ends (Ctrl-C in the menu did not
 * free the sound): the channels, the AHI audio and the heap go back to the
 * system.  Otherwise Paula stays allocated until a reset. */
static void se_atexit(void)
{
    if (heap_mem != NULL) {
        ahi_heap_unload();
        FreeVec(heap_mem);
        heap_mem = NULL;
        heap_size = 0;
    }
    if (backend != SE_NONE)
        se_close();
}

int se_open(int want)
{
    static BOOL atexit_done = FALSE;
    int ch;

    if (backend != SE_NONE)
        return backend;
    if (!atexit_done) {
        atexit(se_atexit);
        atexit_done = TRUE;
    }
    for (ch = 0; ch < MAX_VOICES; ch++) {
        memset(&voices[ch], 0, sizeof(voices[ch]));
        voices[ch].owner = OWNER_NONE;
    }
    for (ch = 0; ch < SE_SLOTS; ch++) {
        memset(&slots[ch], 0, sizeof(slots[ch]));
        slots[ch].ch = -1;
    }
    if (want == SE_AHI) {
        if (ahi_startup()) {
            backend = SE_AHI;
            ahi_heap_load();
            return backend;
        }
        ahi_shutdown();
        want = SE_PAULA;  /* no AHI: Paula */
    }
    if (want == SE_PAULA) {
        if (paula_startup()) {
            backend = SE_PAULA;
            return backend;
        }
        paula_shutdown();
    }
    n_voices = 0;
    return SE_NONE;
}

void se_close(void)
{
    int ch;

    se_music_close();
    for (ch = 0; ch < n_voices; ch++)
        voice_stop(ch);
    music_owns = FALSE;
    if (backend == SE_PAULA)
        paula_shutdown();
    else if (backend == SE_AHI)
        ahi_shutdown();
    backend = SE_NONE;
    n_voices = 0;
}

BOOL se_env_is(const char *val, const char *word)
{
    return env_is(val, word);
}

int se_backend(void)
{
    return backend;
}

/******************************************************************************/
/* Effects */

/** Side of a pan: 0 left, 1 right, 2 middle (as the PC build). */
static int pan_side(int pan)
{
    if ((pan == 0) || (pan == 64))
        return 2;
    return (pan > 64) ? 0 : 1;
}

/** AHI pan as the OpenAL position of the PC build. */
static LONG pan_ahi(int pan)
{
    LONG p;

    if (pan == 0)
        return 0x8000;
    p = 0x8000 + (63 - pan) * 0x200;
    if (p < 0)
        p = 0;
    if (p > 0x10000)
        p = 0x10000;
    return p;
}

static BOOL voice_usable(int ch)
{
    return !(music_owns && (ch < 2));
}

/** A voice for a sound, or -1. */
static int pick_voice(int pan, UWORD vol)
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

    if (backend == SE_PAULA)
        ord = order[pan_side(pan)];
    else if (backend == SE_AHI)
        ord = ahi_order;
    else
        return -1;
    for (k = 0; k < n_voices; k++) {
        int ch = ord[k];
        if (voice_usable(ch) && (voices[ch].owner == OWNER_NONE))
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

/** Paula: sends the sound's data to its channel, keeping 2 writes queued. */
static void paula_feed(struct Slot *st)
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
            /* whole sound in one write, the repeats done by Paula */
            UWORD cycles = 1;
            if (st->sent_whole)
                break;
            if (st->loops < 0)
                cycles = 0;
            else if (st->loops < 65535)
                cycles = (UWORD)(st->loops + 1);
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

static void slot_to_voice(int slot, int ch)
{
    struct Slot *st = &slots[slot];
    struct Voice *c = &voices[ch];

    c->owner = (WORD)slot;
    st->ch = (BYTE)ch;
    if (backend == SE_PAULA) {
        c->per = st->per;
        c->vol = st->vol;
        st->sent_whole = 0;
        paula_feed(st);
    } else {
        c->rate = st->rate;
        c->vol = st->vol;
        c->pan = pan_ahi(st->pan);
        ahi_SetFreq(ch, c->rate, ahi_ctrl, AHISF_IMM);
        ahi_SetVol(ch, (Fixed)c->vol << 10, c->pan, ahi_ctrl, AHISF_IMM);
        ahi_SetSound(ch, AHI_SND_HEAP, st->beg - ahi_heap, st->len, ahi_ctrl, AHISF_IMM);
        if (st->loops == 0) /* played once: then nothing */
            ahi_SetSound(ch, AHI_NOSOUND, 0, 0, ahi_ctrl, AHISF_NONE);
    }
}

void se_stop(int slot)
{
    struct Slot *st;

    if ((slot < 0) || (slot >= SE_SLOTS))
        return;
    st = &slots[slot];
    if (st->ch >= 0)
        voice_stop(st->ch);
    st->active = 0;
}

void se_play(int slot, const void *data, ULONG len, LONG rate, UWORD vol,
    int pan, LONG passes, ULONG pass_ms)
{
    struct Slot *st;
    const UBYTE *p = (const UBYTE *)data;
    int ch;

    if ((slot < 0) || (slot >= SE_SLOTS))
        return;
    se_stop(slot);
    st = &slots[slot];
    st->playable = (p != NULL);
    if (backend == SE_PAULA) {
        /* Paula: even address and length */
        if ((ULONG)p & 1) {
            p++;
            len = (len > 0) ? len - 1 : 0;
        }
        len &= ~1UL;
        if (st->playable && !(TypeOfMem((APTR)p) & MEMF_CHIP)) {
            st->playable = 0;
            snd_cnt.not_reach++;
        }
    } else if (backend == SE_AHI) {
        if (st->playable && ((ahi_heap == NULL) || (p < ahi_heap) ||
          (p + len > ahi_heap + ahi_heap_len))) {
            st->playable = 0;
            snd_cnt.not_reach++;
        }
    } else {
        st->playable = 0;
    }
    if (len < 2)
        st->playable = 0;
    snd_cnt.started++;
    st->beg = st->data = p;
    st->len = st->left = len;
    st->loops = (passes <= 0) ? -1 : passes - 1;
    st->rate = (rate > 100) ? rate : 22050;
    st->per = rate_to_period(st->rate);
    st->vol = vol;
    st->pan = (WORD)pan;
    st->pass_ms = pass_ms;
    st->end_ms = se_ms() + pass_ms * ((st->loops > 0) ? st->loops + 1 : 1);
    st->active = 1;
    st->ch = -1;
    if (st->playable) {
        ch = pick_voice(pan, vol);
        if (ch >= 0) {
            snd_cnt.on_chan++;
            slot_to_voice(slot, ch);
        } else
            snd_cnt.no_chan++;
    }
}

void se_set(int slot, LONG rate, UWORD vol, int pan)
{
    struct Slot *st;

    if ((slot < 0) || (slot >= SE_SLOTS))
        return;
    st = &slots[slot];
    if (rate < 100)
        rate = 22050;
    if (st->rate != rate) {
        st->rate = rate;
        st->per = rate_to_period(rate);
    }
    st->vol = vol;
    st->pan = (WORD)pan;
    if (st->active && (st->ch >= 0))
        voice_set(st->ch, st->rate, st->per, vol, pan_ahi(pan));
}

void se_release(int slot)
{
    struct Slot *st;

    if ((slot < 0) || (slot >= SE_SLOTS))
        return;
    st = &slots[slot];
    if (!st->active || (st->loops >= 0))
        return;
    snd_cnt.released++;
    st->loops = 0;
    st->left = 0;
    st->end_ms = se_ms() + st->pass_ms;
    if (st->ch < 0)
        return;
    if (backend == SE_PAULA) {
        struct Voice *c = &voices[(int)st->ch];
        paula_reap(c);
        /* a long sound (in chunks) just gets no more chunks */
        if ((c->nbusy > 0) && (st->len <= CHUNK_MAX)) {
            /* the write playing now ends at the end of its cycle */
            c->pv->ioa_Request.io_Command = ADCMD_FINISH;
            c->pv->ioa_Request.io_Flags = ADIOF_SYNCCYCLE;
            DoIO((struct IORequest *)c->pv);
        }
    } else if (backend == SE_AHI) {
        ahi_SetSound(st->ch, AHI_NOSOUND, 0, 0, ahi_ctrl, AHISF_NONE);
    }
}

BOOL se_done(int slot)
{
    struct Slot *st;
    BOOL finished;

    if ((slot < 0) || (slot >= SE_SLOTS))
        return TRUE;
    st = &slots[slot];
    if (!st->active)
        return TRUE;
    if ((st->ch >= 0) && (backend == SE_PAULA)) {
        struct Voice *c = &voices[(int)st->ch];
        paula_feed(st);
        if (st->len <= CHUNK_MAX)
            finished = (c->nbusy == 0) && st->sent_whole;
        else
            finished = (c->nbusy == 0) && (st->left == 0) && (st->loops == 0);
        if (finished) {
            c->owner = OWNER_NONE;
            st->ch = -1;
        }
    } else if (st->loops < 0) {
        finished = FALSE;
        if ((st->ch < 0) && st->playable) {
            /* a sound played forever takes a voice when one gets free */
            int ch;
            for (ch = 0; ch < n_voices; ch++) {
                if (voice_usable(ch) && (voices[ch].owner == OWNER_NONE)) {
                    st->data = st->beg;
                    st->left = st->len;
                    slot_to_voice(slot, ch);
                    break;
                }
            }
        }
    } else {
        finished = ((LONG)(se_ms() - st->end_ms) >= 0);
        if (finished && (st->ch >= 0))
            voice_stop(st->ch);
    }
    if (finished)
        st->active = 0;
    return finished;
}

/******************************************************************************/
/* Music: tracks of 8-bit signed stereo PCM, "SWMU" files */

#define MUSIC_HALF_BLOCKS 2
#define MUSIC_BLOCK_MAX 16384

struct Music {
    BPTR fh;
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
    UWORD vol;
    BOOL playing;
    BOOL ahi_loaded;
};

static struct Music music;

static void music_release(void)
{
    if (!music_owns)
        return;
    voices[0].owner = voices[1].owner = OWNER_NONE;
    voice_stop(0);
    voice_stop(1);
    music_owns = FALSE;
}

static ULONG rd_be32(const UBYTE *p)
{
    return ((ULONG)p[0] << 24) | ((ULONG)p[1] << 16) | ((ULONG)p[2] << 8) | p[3];
}

/** Reads the next block (left, then right) to dst; at the end the track
 * starts again. */
static BOOL music_read_block(UBYTE *dst)
{
    LONG n = 2 * music.block;

    if (music.next_block >= music.nblocks) {
        music.next_block = 0;
        Seek(music.fh, 16, OFFSET_BEGINNING);
    }
    if (Read(music.fh, dst, n) != n) {
        music.next_block = 0;
        Seek(music.fh, 16, OFFSET_BEGINNING);
        if (Read(music.fh, dst, n) != n)
            return FALSE;
    }
    music.next_block++;
    snd_cnt.music_blocks++;
    return TRUE;
}

/** AHI: the music rings, loaded as AHI sounds once, before playing. */
static BOOL music_ahi_setup(void)
{
    struct AHISampleInfo si;
    ULONG ring = 2 * MUSIC_HALF_BLOCKS * MUSIC_BLOCK_MAX;
    int i;

    music.ahi_buf = AllocVec(2 * MUSIC_BLOCK_MAX, MEMF_ANY);
    for (i = 0; i < 2; i++)
        music.ring[i] = AllocVec(ring, MEMF_ANY | MEMF_CLEAR);
    if ((music.ahi_buf == NULL) || (music.ring[0] == NULL) || (music.ring[1] == NULL))
        return FALSE;
    for (i = 0; i < 2; i++) {
        si.ahisi_Type = AHIST_M8S;
        si.ahisi_Address = music.ring[i];
        si.ahisi_Length = ring;
        if (ahi_LoadSound(AHI_SND_MUSIC_L + i, AHIST_SAMPLE, &si, ahi_ctrl) != AHIE_OK)
            return FALSE;
    }
    music.ahi_loaded = TRUE;
    return TRUE;
}

static void music_ahi_free(void)
{
    int i;

    if (music.ahi_loaded && (ahi_ctrl != NULL)) {
        ahi_UnloadSound(AHI_SND_MUSIC_L, ahi_ctrl);
        ahi_UnloadSound(AHI_SND_MUSIC_R, ahi_ctrl);
    }
    music.ahi_loaded = FALSE;
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
static BOOL music_ahi_fill(int h)
{
    ULONG k;

    for (k = 0; k < MUSIC_HALF_BLOCKS; k++) {
        ULONG offs = h * music.half + k * music.block;
        if (!music_read_block(music.ahi_buf))
            return FALSE;
        CopyMem(music.ahi_buf, music.ring[0] + offs, music.block);
        CopyMem(music.ahi_buf + music.block, music.ring[1] + offs, music.block);
    }
    return TRUE;
}

static void music_ahi_queue(int h, ULONG flags)
{
    ahi_SetSound(0, AHI_SND_MUSIC_L, h * music.half, music.half, ahi_ctrl, flags);
    ahi_SetSound(1, AHI_SND_MUSIC_R, h * music.half, music.half, ahi_ctrl, flags);
}

static BOOL music_take(void)
{
    int ch;

    if (music_owns)
        return TRUE;
    if (backend == SE_NONE)
        return FALSE;
    for (ch = 0; ch < 2; ch++) {
        if (voices[ch].owner != OWNER_NONE)
            voice_stop(ch);
        voices[ch].owner = OWNER_MUSIC;
    }
    music_owns = TRUE;
    if (backend == SE_AHI) {
        ULONG s0;

        if (!music_ahi_fill(0) || !music_ahi_fill(1))
            return FALSE;
        voices[0].rate = voices[1].rate = music.rate;
        voices[0].vol = voices[1].vol = music.vol;
        voices[0].pan = 0;
        voices[1].pan = 0x10000;
        ahi_SetFreq(0, music.rate, ahi_ctrl, AHISF_IMM);
        ahi_SetFreq(1, music.rate, ahi_ctrl, AHISF_IMM);
        ahi_SetVol(0, (Fixed)music.vol << 10, 0, ahi_ctrl, AHISF_IMM);
        ahi_SetVol(1, (Fixed)music.vol << 10, 0x10000, ahi_ctrl, AHISF_IMM);
        s0 = ahi_music_starts;
        music_ahi_queue(0, AHISF_IMM);
        music_ahi_queue(1, AHISF_NONE);
        /* Is the sound function called for the immediate start too?  Wait
         * for one mixing pass to see; the halves are counted from it. */
        Delay(4);
        music.starts_base = (ahi_music_starts != s0) ? s0 + 1 : s0;
        music.halves_done = 0;
    }
    return TRUE;
}

void se_music_close(void)
{
    music_release();
    music.playing = FALSE;
    if (music.fh != 0)
        Close(music.fh);
    music.fh = 0;
    if (music.buf != NULL)
        FreeVec(music.buf);
    music.buf = NULL;
}

BOOL se_music_open(const char *path)
{
    UBYTE head[16];

    se_music_close();
    if (backend == SE_NONE)
        return FALSE;
    music.fh = Open((STRPTR)path, MODE_OLDFILE);
    if (music.fh == 0)
        return FALSE;
    if ((Read(music.fh, head, 16) != 16) || (memcmp(head, "SWMU", 4) != 0))
        goto err;
    music.rate = rd_be32(head + 4);
    music.block = rd_be32(head + 8);
    music.nblocks = rd_be32(head + 12);
    if ((music.block < 256) || (music.block > MUSIC_BLOCK_MAX) || (music.block & 1) ||
      (music.nblocks == 0) || (music.rate < 1000))
        goto err;
    if (backend == SE_PAULA) {
        music.buf = AllocVec(NREQ * 2 * music.block, MEMF_CHIP | MEMF_PUBLIC);
        if (music.buf == NULL)
            goto err;
    } else {
        if (!music.ahi_loaded)
            goto err;
        music.half = MUSIC_HALF_BLOCKS * music.block;
    }
    music.next_block = 0;
    music.per = rate_to_period(music.rate);
    return TRUE;
err:
    se_music_close();
    return FALSE;
}

void se_music_rewind(void)
{
    if (music.fh == 0)
        return;
    music.next_block = 0;
    Seek(music.fh, 16, OFFSET_BEGINNING);
}

void se_music_pause(void)
{
    music.playing = FALSE;
    music_release();
}

void se_music_play(UWORD vol)
{
    if (vol > 64)
        vol = 64;
    music.vol = vol;
    music.playing = TRUE;
    if (music_owns) {
        voice_set(0, music.rate, music.per, vol, 0);
        voice_set(1, music.rate, music.per, vol, 0x10000);
    }
}

static BOOL music_update_paula(void)
{
    struct Voice *cl = &voices[0], *cr = &voices[1];

    paula_reap(cl);
    paula_reap(cr);
    cl->per = cr->per = music.per;
    cl->vol = cr->vol = music.vol;
    while ((cl->nbusy < NREQ) && (cr->nbusy < NREQ)) {
        UBYTE *slot = music.buf + ((cl->head + cl->nbusy) % NREQ) * 2 * music.block;
        if (!music_read_block(slot))
            return FALSE;
        paula_write(cl, slot, music.block, 1);
        paula_write(cr, slot + music.block, music.block, 1);
    }
    return TRUE;
}

static BOOL music_update_ahi(void)
{
    ULONG started = ahi_music_starts - music.starts_base;

    /* a new half started: the other one is free, fill it and queue it */
    while (music.halves_done < started) {
        int h;
        music.halves_done++;
        h = (music.halves_done + 1) & 1;
        if (!music_ahi_fill(h))
            return FALSE;
        music_ahi_queue(h, AHISF_NONE);
    }
    return TRUE;
}

void se_music_update(void)
{
    BOOL ok = TRUE;

    if ((music.fh == 0) || !music.playing)
        return;
    if (music.vol == 0) {
        music_release();
        return;
    }
    if (!music_take())
        ok = FALSE;
    else if (backend == SE_PAULA)
        ok = music_update_paula();
    else if (backend == SE_AHI)
        ok = music_update_ahi();
    if (!ok) {
        music.playing = FALSE;
        music_release();
    }
}
