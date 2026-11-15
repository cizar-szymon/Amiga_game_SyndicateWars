/* snd_proxy.c - Warp ARM build: the sound engine (platform/amiga/snd_engine.h)
 * as a proxy.  The 68k host (tools/warp/swhost_snd.c) does all the sound
 * work: it loads the sound bank into its own memory and plays it; the ARM
 * only says which sample to play (SWL_SND_* / SWL_MUS_* records).  The
 * game's sound heap on the ARM holds no sound data, only the head of each
 * sample (from SND_HEADERS), which is all bfsoundlib looks at.  The ARM
 * keeps the time of each sound itself.
 * The backend and the music option come from the host's ENV:. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "snd_engine.h"
#include "swlink.h"
#include "wlink.h"
#include "bffile.h"

uint8_t *w_xfer_buf(void);
/** The sample of the sound heap holding p (src/sound.c), -1 none. */
int sound_heap_locate(const void *p, ULONG *p_offs);

struct PSlot {
    LONG loops;                 /**< repeats after this pass, -1 forever */
    ULONG end_ms;
    ULONG pass_ms;
    UBYTE active;
};

static struct PSlot pslots[SE_SLOTS];
/* what the host has, so that only changes go over the link */
static LONG last_rate[SE_SLOTS];
static UWORD last_vol[SE_SLOTS];
static int last_pan[SE_SLOTS];
static int backend;
static int music_on;
static uint8_t *heap;
static ULONG heap_size;
/* heads of the samples of the current bank, SWL_SND_HDR_BYTES each */
static uint8_t *hdrs;
static int n_hdrs;

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static void put16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}

static void send_slot(unsigned type, int slot)
{
    uint8_t rec[4];
    put32(rec, (uint32_t)slot);
    wl_send(type, rec, 4);
}

int se_open(int want)
{
    int32_t extra = 0;

    (void)want;
    if (backend != SE_NONE)
        return backend;
    memset(pslots, 0, sizeof(pslots));
    backend = wl_request(SWL_SND_OPEN, NULL, 0, NULL, 0, &extra);
    if (backend < 0)
        backend = SE_NONE;
    music_on = (extra != 0);
    return backend;
}

void se_close(void)
{
    if (backend == SE_NONE)
        return;
    wl_send(SWL_SND_CLOSE, NULL, 0);
    backend = SE_NONE;
}

int se_backend(void)
{
    return backend;
}

int se_backend_from_env(void)
{
    return SE_PAULA;    /* the host decides */
}

BOOL se_env_is(const char *val, const char *word)
{
    (void)val; (void)word;
    return FALSE;
}

/** Options of snd_bfs.c: the host chooses the backend; the music flag
 * comes back with SND_OPEN. */
int amiga_sound_backend_wanted(void)
{
    return SE_PAULA;
}

int amiga_music_wanted(void)
{
    return music_on;
}

/******************************************************************************/

ULONG se_heap_max(void)
{
    return 1500000;
}

/* The sound heap lives at the end of the render area (the file transfer
 * buffer and WScreen, at most 2560x1441, are at its start): the game's own
 * heap on the board has no room for it. */

void *se_heap_alloc(ULONG size)
{
    ULONG reg = (size + 15) & ~15UL;
    uint8_t *end;

    if (heap != NULL)
        return NULL;
    if ((reg + 16 > WARP_SOUND_HEAP_RESERVE) ||
      (SWL_XFER_BYTES + WARP_RENDER_SCREEN_MAX + WARP_SOUND_HEAP_RESERVE > wl_env->renderArenaSize))
        return NULL;
    end = (uint8_t *)wl_env->renderArena + wl_env->renderArenaSize;
    heap = (uint8_t *)(((uintptr_t)(end - reg)) & ~(uintptr_t)15);
    heap_size = size;
    return heap;
}

void se_heap_free(void *ptr)
{
    if ((ptr == NULL) || (ptr != heap))
        return;
    heap = NULL;
    heap_size = 0;
}

/******************************************************************************/

/** SND_BANK and SND_HEADERS: the host loads the bank, the ARM keeps the
 * heads of its samples.  Gives the number of samples, 0 when none. */
int amiga_sound_bank_open(const char *fname, int tpno)
{
    char real_fname[FILENAME_MAX];
    const char *path = fname;
    uint8_t a[4];
    int n, k;

    n_hdrs = 0;
    if (backend == SE_NONE)
        return 0;
#if LB_FILENAME_TRANSFORM
    if (lbFileNameTransform != NULL) {
        lbFileNameTransform(real_fname, fname);
        path = real_fname;
    }
#else
    (void)real_fname;
#endif
    put32(a, (uint32_t)tpno);
    n = wl_request(SWL_SND_BANK, a, 4, path, (unsigned)strlen(path) + 1, NULL);
    if ((n <= 0) || ((uint32_t)n * SWL_SND_HDR_BYTES > SWL_XFER_BYTES))
        return 0;
    k = wl_request(SWL_SND_HEADERS, NULL, 0, NULL, 0, NULL);
    if (k != n)
        return 0;
    free(hdrs);
    hdrs = (uint8_t *)malloc((size_t)n * SWL_SND_HDR_BYTES);
    if (hdrs == NULL)
        return 0;
    wl_env->cacheInvalidate(w_xfer_buf(), ((uint32_t)n * SWL_SND_HDR_BYTES + 31) & ~31u);
    memcpy(hdrs, w_xfer_buf(), (size_t)n * SWL_SND_HDR_BYTES);
    n_hdrs = n;
    return n;
}

/** Puts the head of a sample where the game would load the whole of it. */
int amiga_sound_sample_header(int id, void *dst, long size)
{
    long k = (size < SWL_SND_HDR_BYTES) ? size : SWL_SND_HDR_BYTES;

    if ((id < 0) || (id >= n_hdrs) || (k <= 0))
        return 0;
    memcpy(dst, hdrs + (size_t)id * SWL_SND_HDR_BYTES, (size_t)k);
    return 1;
}

void se_play(int slot, const void *data, ULONG len, LONG rate, UWORD vol,
    int pan, LONG passes, ULONG pass_ms)
{
    struct PSlot *ps;
    uint8_t rec[32];
    uint32_t id = 0xFFFFFFFFu;
    ULONG offs = 0;

    if ((slot < 0) || (slot >= SE_SLOTS))
        return;
    ps = &pslots[slot];
    ps->loops = (passes <= 0) ? -1 : passes - 1;
    ps->pass_ms = pass_ms;
    ps->end_ms = se_ms() + pass_ms * ((ps->loops > 0) ? ps->loops + 1 : 1);
    ps->active = 1;
    last_rate[slot] = rate;
    last_vol[slot] = vol;
    last_pan[slot] = pan;
    if (backend == SE_NONE)
        return;
    if (data != NULL) {
        int k = sound_heap_locate(data, &offs);
        if (k >= 0)
            id = (uint32_t)k;
    }
    put32(rec, (uint32_t)slot);
    put32(rec + 4, id);
    put32(rec + 8, offs);
    put32(rec + 12, len);
    put32(rec + 16, (uint32_t)rate);
    put16(rec + 20, vol);
    put16(rec + 22, (uint32_t)pan);
    put32(rec + 24, (uint32_t)passes);
    put32(rec + 28, pass_ms);
    wl_send(SWL_SND_PLAY, rec, sizeof(rec));
}

void se_set(int slot, LONG rate, UWORD vol, int pan)
{
    uint8_t rec[12];

    if ((slot < 0) || (slot >= SE_SLOTS) || (backend == SE_NONE) || !pslots[slot].active)
        return;
    if ((last_rate[slot] == rate) && (last_vol[slot] == vol) && (last_pan[slot] == pan))
        return;
    last_rate[slot] = rate;
    last_vol[slot] = vol;
    last_pan[slot] = pan;
    put32(rec, (uint32_t)slot);
    put32(rec + 4, (uint32_t)rate);
    put16(rec + 8, vol);
    put16(rec + 10, (uint32_t)pan);
    wl_send(SWL_SND_SET, rec, sizeof(rec));
}

void se_stop(int slot)
{
    if ((slot < 0) || (slot >= SE_SLOTS))
        return;
    if (pslots[slot].active && (backend != SE_NONE))
        send_slot(SWL_SND_STOP, slot);
    pslots[slot].active = 0;
}

void se_release(int slot)
{
    struct PSlot *ps;

    if ((slot < 0) || (slot >= SE_SLOTS))
        return;
    ps = &pslots[slot];
    if (!ps->active || (ps->loops >= 0))
        return;
    ps->loops = 0;
    ps->end_ms = se_ms() + ps->pass_ms;
    if (backend != SE_NONE)
        send_slot(SWL_SND_RELEASE, slot);
}

BOOL se_done(int slot)
{
    struct PSlot *ps;

    if ((slot < 0) || (slot >= SE_SLOTS))
        return TRUE;
    ps = &pslots[slot];
    if (!ps->active)
        return TRUE;
    if (ps->loops < 0)
        return FALSE;
    if ((LONG)(se_ms() - ps->end_ms) < 0)
        return FALSE;
    ps->active = 0;
    return TRUE;
}

/******************************************************************************/

BOOL se_music_open(const char *path)
{
    if (backend == SE_NONE)
        return FALSE;
    return wl_request(SWL_MUS_OPEN, path, (unsigned)strlen(path) + 1, NULL, 0, NULL) == 1;
}

void se_music_close(void)
{
    if (backend != SE_NONE)
        wl_send(SWL_MUS_CLOSE, NULL, 0);
}

void se_music_play(UWORD vol)
{
    uint8_t rec[4];

    if (backend == SE_NONE)
        return;
    put32(rec, vol);
    wl_send(SWL_MUS_PLAY, rec, 4);
}

void se_music_pause(void)
{
    if (backend != SE_NONE)
        wl_send(SWL_MUS_PAUSE, NULL, 0);
}

void se_music_rewind(void)
{
    if (backend != SE_NONE)
        wl_send(SWL_MUS_REWIND, NULL, 0);
}

void se_music_update(void)
{
    /* the host streams the music itself */
}

void se_log(const char *path)
{
    (void)path;
}
