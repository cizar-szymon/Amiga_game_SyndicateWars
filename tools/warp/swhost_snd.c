/* swhost_snd.c - the sound of the ARM game, done by its 68k host (vbcc).
 *
 * The 68k does all the sound work, as it does in the 68k game; the ARM
 * (platform/warp/snd_proxy.c) only says what to play:
 * - SND_BANK: the sound bank file (sound/sound_LANG.dat, the 8-bit Amiga
 *   one) is loaded whole into the 68k's memory; SND_HEADERS sends the ARM
 *   the head of each sample (all that the game looks at).
 * - SND_PLAY: a sample of the bank by its number, in an engine slot
 *   (platform/amiga/snd_engine.c: Paula or AHI from ENV:SyndWarsSound).
 *   AHI plays straight from the bank in Fast RAM (the bank is the engine's
 *   heap).  Paula needs Chip RAM: the engine's heap there is a cache of the
 *   samples played lately, a sample is copied in once and played from
 *   there until its place is needed (then what plays from it stops).
 * - the music is read from disk here (SWL_MUS_*).
 * A timer wakes the host every 20 ms to keep the voices and the music fed. */
#include <exec/types.h>
#include <exec/memory.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <libraries/cswarp.h>
#include <proto/cswarp.h>
#include <string.h>

#include "swlink.h"
#include "snd_engine.h"
#include "swhost_snd.h"

extern struct CSWarpBase *csWarpBase;

#define SERVICE_MS 20
#define NO_CACHE 0xFFFFFFFFUL

struct Device *TimerBase;
static struct MsgPort *tport;
static struct timerequest *treq;
static BOOL timer_busy;
static ULONG eclock_kHz = 709;

static BOOL opened;
static int backend;

/* the bank: the whole file; for AHI it is the engine's heap */
static UBYTE *bank;
static ULONG bank_size;
static BOOL bank_is_heap;
static int n_smp;
static ULONG *smp_pos, *smp_len;

/* Paula: the engine's heap in Chip RAM, a ring of copied samples */
static UBYTE *chip;
static ULONG chip_size, ring_pos;
static ULONG *cache_off;            /* per sample: offset in chip, or NO_CACHE */

static WORD slot_smp[SE_SLOTS];     /* sample playing in the slot, -1 none */
static UBYTE slot_on[SE_SLOTS];

/* the heads for SND_HEADERS, 16-aligned for extFeed */
static UBYTE *hdr_raw;
static CSWarpExtReq hreq;

static ULONG last_service, last_log;
static ULONG n_play, n_silent, n_copied, n_cached, copy_bytes;
static ULONG play_ticks, play_ticks_max;

static ULONG rd32(const UBYTE *p) { return p[0] | (p[1] << 8) | ((ULONG)p[2] << 16) | ((ULONG)p[3] << 24); }
static UWORD rd16(const UBYTE *p) { return (UWORD)(p[0] | (p[1] << 8)); }

/** Milliseconds from the E clock: (hi:lo) / (freq / 1000), low 32 bits. */
ULONG se_ms(void)
{
    struct EClockVal ev;
    ULONG d, rem, x, q1, y, q2;

    if (TimerBase == NULL)
        return 0;
    ReadEClock(&ev);
    d = eclock_kHz;
    rem = ev.ev_hi % d;
    x = (rem << 16) | (ev.ev_lo >> 16);
    q1 = x / d;
    y = ((x % d) << 16) | (ev.ev_lo & 0xFFFF);
    q2 = y / d;
    return (q1 << 16) + q2;
}

static ULONG eclock_lo(void)
{
    struct EClockVal ev;
    if (TimerBase == NULL)
        return 0;
    ReadEClock(&ev);
    return ev.ev_lo;
}

static void timer_open(void)
{
    struct EClockVal ev;

    tport = CreateMsgPort();
    if (tport == NULL)
        return;
    treq = (struct timerequest *)CreateIORequest(tport, sizeof(struct timerequest));
    if (treq == NULL)
        return;
    if (OpenDevice((STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)treq, 0) != 0) {
        DeleteIORequest((struct IORequest *)treq);
        treq = NULL;
        return;
    }
    TimerBase = treq->tr_node.io_Device;
    eclock_kHz = ReadEClock(&ev) / 1000;
    if (eclock_kHz == 0)
        eclock_kHz = 709;
}

static void timer_start(void)
{
    if ((treq == NULL) || timer_busy)
        return;
    treq->tr_node.io_Command = TR_ADDREQUEST;
    treq->tr_time.tv_secs = 0;
    treq->tr_time.tv_micro = SERVICE_MS * 1000;
    SendIO((struct IORequest *)treq);
    timer_busy = TRUE;
}

static void timer_close(void)
{
    if (treq != NULL) {
        if (timer_busy) {
            AbortIO((struct IORequest *)treq);
            WaitIO((struct IORequest *)treq);
            timer_busy = FALSE;
        }
        CloseDevice((struct IORequest *)treq);
        DeleteIORequest((struct IORequest *)treq);
        treq = NULL;
    }
    TimerBase = NULL;
    if (tport != NULL)
        DeleteMsgPort(tport);
    tport = NULL;
}

ULONG hsnd_wait_mask(void)
{
    if (!opened || (tport == NULL))
        return 0;
    timer_start();
    return 1UL << tport->mp_SigBit;
}

static BOOL music_on_env(void)
{
    return se_music_on();
}

/******************************************************************************/
/* the bank */

static void stop_all(void)
{
    int i;

    for (i = 0; i < SE_SLOTS; i++) {
        if (slot_on[i])
            se_stop(i);
        slot_on[i] = 0;
        slot_smp[i] = -1;
    }
}

static void bank_free(void)
{
    stop_all();
    if (bank != NULL) {
        if (bank_is_heap)
            se_heap_free(bank);
        else
            FreeVec(bank);
    }
    bank = NULL;
    bank_size = 0;
    bank_is_heap = FALSE;
    if (smp_pos != NULL) FreeVec(smp_pos);
    if (smp_len != NULL) FreeVec(smp_len);
    if (cache_off != NULL) FreeVec(cache_off);
    smp_pos = smp_len = cache_off = NULL;
    n_smp = 0;
    ring_pos = 0;
}

/** SND_BANK: loads the bank file and finds the samples of bank tpno.
 * Gives their number, 0 when it fails. */
static LONG bank_load(ULONG tpno, const char *name)
{
    BPTR fh;
    LONG size;
    ULONG pos, tab, dat, tsz;
    int i;

    bank_free();
    fh = Open((STRPTR)name, MODE_OLDFILE);
    if (fh == 0)
        return 0;
    Seek(fh, 0, OFFSET_END);
    size = Seek(fh, 0, OFFSET_BEGINNING);
    if (size < 64) {
        Close(fh);
        return 0;
    }
    if (backend == SE_AHI) {
        /* AHI plays from the bank: it is the engine's heap (one AHI sound) */
        bank = se_heap_alloc((ULONG)size);
        bank_is_heap = (bank != NULL);
    } else {
        bank = AllocVec((ULONG)size, MEMF_ANY);
    }
    if ((bank == NULL) || (Read(fh, bank, size) != size)) {
        Close(fh);
        bank_free();
        return 0;
    }
    Close(fh);
    bank_size = (ULONG)size;
    /* the heads of the banks, at the position in the last 4 bytes */
    pos = rd32(bank + bank_size - 4);
    if ((tpno >= 9) || (pos + 18 + 16 * 9 > bank_size)) {
        bank_free();
        return 0;
    }
    tab = rd32(bank + pos + 18 + 16 * tpno);
    dat = rd32(bank + pos + 18 + 16 * tpno + 4);
    tsz = rd32(bank + pos + 18 + 16 * tpno + 8);
    if ((tab == 0) || (tab == 0xFFFFFFFFUL) || (tab + tsz > bank_size) || (tsz < 32)) {
        bank_free();
        return 0;
    }
    n_smp = (int)(tsz / 32);
    smp_pos = AllocVec(n_smp * sizeof(ULONG), MEMF_ANY | MEMF_CLEAR);
    smp_len = AllocVec(n_smp * sizeof(ULONG), MEMF_ANY | MEMF_CLEAR);
    cache_off = AllocVec(n_smp * sizeof(ULONG), MEMF_ANY);
    if ((smp_pos == NULL) || (smp_len == NULL) || (cache_off == NULL)) {
        bank_free();
        return 0;
    }
    for (i = 0; i < n_smp; i++) {
        const UBYTE *e = bank + tab + 32 * i;
        ULONG p = dat + rd32(e + 18), l = rd32(e + 26);
        if ((l > 0) && (p + l <= bank_size)) {
            smp_pos[i] = p;
            smp_len[i] = l;
        }
        cache_off[i] = NO_CACHE;
    }
    return n_smp;
}

/** SND_HEADERS: the first SWL_SND_HDR_BYTES of each sample to SWL_BUF_XFER. */
static LONG bank_headers(void)
{
    ULONG bytes = ((ULONG)n_smp * SWL_SND_HDR_BYTES + 15) & ~15UL;
    UBYTE *buf;
    LONG rc;
    int i;

    if ((n_smp == 0) || (bytes > SWL_XFER_BYTES))
        return 0;
    if (hdr_raw != NULL)
        FreeVec(hdr_raw);
    hdr_raw = AllocVec(bytes + 16, MEMF_ANY | MEMF_CLEAR);
    if (hdr_raw == NULL)
        return 0;
    buf = (UBYTE *)(((ULONG)hdr_raw + 15) & ~15UL);
    for (i = 0; i < n_smp; i++) {
        ULONG k = smp_len[i] < SWL_SND_HDR_BYTES ? smp_len[i] : SWL_SND_HDR_BYTES;
        if (k > 0)
            CopyMem(bank + smp_pos[i], buf + i * SWL_SND_HDR_BYTES, k);
    }
    memset(&hreq, 0, sizeof(hreq));
    hreq.version = CSWARP_EXTREQ_VERSION;
    rc = extFeed(SWL_BUF_XFER, 0, buf, bytes, &hreq);
    return (rc == CSWEXT_OK) ? n_smp : 0;
}

/******************************************************************************/
/* Paula: the cache of samples in Chip RAM */

/** A place in the ring for a sample; what was there is forgotten, and what
 * plays from it stops. */
static UBYTE *chip_alloc(int id)
{
    ULONG need = (smp_len[id] + 15) & ~15UL;
    ULONG beg;
    int i;

    if ((chip == NULL) || (need > chip_size))
        return NULL;
    if (ring_pos + need > chip_size)
        ring_pos = 0;
    beg = ring_pos;
    for (i = 0; i < n_smp; i++) {
        ULONG o = cache_off[i];
        if ((o != NO_CACHE) && (o < beg + need) && (o + smp_len[i] > beg)) {
            int s;
            cache_off[i] = NO_CACHE;
            for (s = 0; s < SE_SLOTS; s++) {
                if (slot_on[s] && (slot_smp[s] == i)) {
                    se_stop(s);
                    slot_on[s] = 0;
                }
            }
        }
    }
    ring_pos += need;
    cache_off[id] = beg;
    return chip + beg;
}

/** Where sample id is to be played from: the bank (AHI) or its copy in
 * Chip RAM (Paula), NULL when it cannot be played. */
static const UBYTE *sample_data(int id)
{
    UBYTE *c;

    if (backend == SE_AHI)
        return bank + smp_pos[id];
    if (cache_off[id] != NO_CACHE) {
        n_cached++;
        return chip + cache_off[id];
    }
    c = chip_alloc(id);
    if (c == NULL)
        return NULL;
    CopyMem(bank + smp_pos[id], c, smp_len[id]);
    n_copied++;
    copy_bytes += smp_len[id];
    return c;
}

static void snd_play_do(const UBYTE *p)
{
    ULONG slot = rd32(p), id = rd32(p + 4), offs = rd32(p + 8), len = rd32(p + 12);
    LONG rate = (LONG)rd32(p + 16);
    UWORD vol = rd16(p + 20);
    int pan = rd16(p + 22);
    LONG passes = (LONG)rd32(p + 24);
    ULONG pass_ms = rd32(p + 28);
    const UBYTE *src = NULL;

    if (slot >= SE_SLOTS)
        return;
    n_play++;
    se_stop((int)slot);
    slot_on[slot] = 0;
    slot_smp[slot] = -1;
    if ((id < (ULONG)n_smp) && (offs + len <= smp_len[id]) && (len >= 2)) {
        const UBYTE *d = sample_data((int)id);
        if (d != NULL) {
            src = d + offs;
            slot_on[slot] = 1;
            slot_smp[slot] = (WORD)id;
        }
    }
    if (src == NULL)
        n_silent++;
    se_play((int)slot, src, len, rate, vol, pan, passes, pass_ms);
}

static void snd_play(const UBYTE *p)
{
    ULONG t0 = eclock_lo(), t;
    snd_play_do(p);
    t = eclock_lo() - t0;
    play_ticks += t;
    if (t > play_ticks_max)
        play_ticks_max = t;
}

/******************************************************************************/

static void snd_close(void)
{
    if (!opened)
        return;
    bank_free();
    if (chip != NULL)
        se_heap_free(chip);
    chip = NULL;
    se_close();
    opened = FALSE;
}

/** SND_OPEN: the engine with the backend of ENV:; for Paula its heap in
 * Chip RAM as the cache of samples. */
static LONG snd_open(LONG *extra)
{
    int i;

    *extra = 0;
    if (opened)
        snd_close();
    if (TimerBase == NULL)
        timer_open();
    backend = se_open(se_backend_from_env());
    if (backend == SE_NONE)
        return SE_NONE;
    if (backend == SE_PAULA) {
        chip_size = se_heap_max();
        while ((chip = se_heap_alloc(chip_size)) == NULL) {
            if (chip_size < 200000)
                break;
            chip_size -= 50000;
        }
        if (chip == NULL) {
            se_close();
            return SE_NONE;
        }
    }
    for (i = 0; i < SE_SLOTS; i++) {
        slot_on[i] = 0;
        slot_smp[i] = -1;
    }
    opened = TRUE;
    *extra = music_on_env();
    return backend;
}

int hsnd_handle(UWORD type, const UBYTE *p, UWORD len, LONG *res, LONG *extra)
{
    char name[256];
    ULONG n;

    *res = 0;
    *extra = 0;
    switch (type) {
    case SWL_SND_OPEN:
        *res = snd_open(extra);
        return 2;
    case SWL_SND_CLOSE:
        snd_close();
        return 1;
    case SWL_SND_BANK:
        if (!opened || (len < 5))
            return 2;
        n = len - 4;
        if (n >= sizeof(name))
            n = sizeof(name) - 1;
        memcpy(name, p + 4, n);
        name[n] = '\0';
        *res = bank_load(rd32(p), name);
        return 2;
    case SWL_SND_HEADERS:
        if (opened)
            *res = bank_headers();
        return 2;
    case SWL_SND_PLAY:
        if (opened && (len >= 32))
            snd_play(p);
        return 1;
    case SWL_SND_SET:
        if (opened && (len >= 12))
            se_set((int)rd32(p), (LONG)rd32(p + 4), rd16(p + 8), rd16(p + 10));
        return 1;
    case SWL_SND_STOP:
        if (opened && (len >= 4) && (rd32(p) < SE_SLOTS)) {
            se_stop((int)rd32(p));
            slot_on[rd32(p)] = 0;
        }
        return 1;
    case SWL_SND_RELEASE:
        if (opened && (len >= 4))
            se_release((int)rd32(p));
        return 1;
    case SWL_MUS_OPEN:
        n = len;
        if (n >= sizeof(name))
            n = sizeof(name) - 1;
        memcpy(name, p, n);
        name[n] = '\0';
        *res = (opened && se_music_open(name)) ? 1 : 0;
        return 2;
    case SWL_MUS_CLOSE:
        if (opened) se_music_close();
        return 1;
    case SWL_MUS_PLAY:
        if (opened && (len >= 2)) se_music_play(rd16(p));
        return 1;
    case SWL_MUS_PAUSE:
        if (opened) se_music_pause();
        return 1;
    case SWL_MUS_REWIND:
        if (opened) se_music_rewind();
        return 1;
    }
    return 0;
}

void hsnd_service(BOOL timer_signalled)
{
    ULONG now;
    int i;

    if (timer_signalled && timer_busy && (treq != NULL) && CheckIO((struct IORequest *)treq)) {
        WaitIO((struct IORequest *)treq);
        timer_busy = FALSE;
    }
    if (!opened)
        return;
    now = se_ms();
    if ((LONG)(now - last_service) < SERVICE_MS / 2)
        return;
    last_service = now;
    for (i = 0; i < SE_SLOTS; i++) {
        if (slot_on[i] && se_done(i))
            slot_on[i] = 0;
    }
    se_music_update();
    se_log("RAM:SyndWars-sound.txt");
    if ((LONG)(now - last_log) >= 5000) {
        char buf[16];
        last_log = now;
        if (GetVar((STRPTR)"SyndWarsSoundLog", (STRPTR)buf, sizeof(buf), 0) >= 0) {
            BPTR f = Open((STRPTR)"RAM:SyndWars-host.txt", MODE_NEWFILE);
            if (f) {
                FPrintf(f, (STRPTR)"play %lu silent %lu copied %lu cached %lu kbytes %lu bank %lu samples %ld chip %lu/%lu play_ms %lu max_us %lu\n",
                  n_play, n_silent, n_copied, n_cached, copy_bytes / 1024, bank_size, (LONG)n_smp,
                  ring_pos, chip_size, play_ticks / eclock_kHz, (play_ticks_max * 1000) / eclock_kHz);
                Close(f);
            }
        }
    }
}

void hsnd_close(void)
{
    snd_close();
    timer_close();
    if (hdr_raw != NULL)
        FreeVec(hdr_raw);
    hdr_raw = NULL;
}
