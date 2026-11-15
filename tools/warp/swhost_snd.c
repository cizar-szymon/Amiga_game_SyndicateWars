/* swhost_snd.c - the sound of the ARM game, played by its 68k host (vbcc).
 *
 * The SWL_SND_* / SWL_MUS_* records of the ARM (platform/warp/snd_proxy.c)
 * drive the Amiga sound engine (platform/amiga/snd_engine.c, Paula or AHI
 * from ENV:SyndWarsSound, as the 68k game).  For SND_PLAY the sound's
 * bytes are fetched from the ARM's sound heap (SWL_BUF_SOUND) into a ring
 * in the engine's heap (Chip RAM for Paula, Fast RAM for AHI); a sound
 * which is overwritten there is stopped.  The music is read from disk here.
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

#define FETCH_MAX (256 * 1024)
#define SERVICE_MS 20

struct Device *TimerBase;
static struct MsgPort *tport;
static struct timerequest *treq;
static BOOL timer_busy;
static ULONG eclock_kHz = 709;

static UBYTE *heap;             /* the engine's heap, a ring of sounds */
static ULONG heap_size, ring_pos;
static UBYTE *fetch_raw, *fetch_buf;
static CSWarpExtReq freq_;
static ULONG slot_beg[SE_SLOTS], slot_len[SE_SLOTS];
static UBYTE slot_on[SE_SLOTS];
static BOOL opened;
static ULONG last_service;
static ULONG fetch_fail, n_play, n_silent, n_toobig, n_fetched;
static ULONG last_log;
static LONG last_rc;
static ULONG last_aoff, last_rows, ok_max_rows, fail_min_rows = 0xFFFFFFFFUL;

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
    char buf[16];

    if ((GetVar((STRPTR)"SyndWarsMusic", (STRPTR)buf, sizeof(buf), 0) >= 0) &&
      se_env_is(buf, "OFF"))
        return FALSE;
    return TRUE;
}

static void snd_close(void)
{
    if (!opened)
        return;
    se_close();
    if (heap != NULL)
        se_heap_free(heap);
    heap = NULL;
    opened = FALSE;
}

/** SND_OPEN: the engine with the backend of ENV:, its heap as the ring. */
static LONG snd_open(LONG *extra)
{
    int be;

    *extra = 0;
    if (opened)
        snd_close();
    if (fetch_raw == NULL) {
        fetch_raw = AllocVec(FETCH_MAX + 16, MEMF_ANY);
        if (fetch_raw == NULL)
            return SE_NONE;
        fetch_buf = (UBYTE *)(((ULONG)fetch_raw + 15) & ~15UL);
    }
    if (TimerBase == NULL)
        timer_open();
    be = se_open(se_backend_from_env());
    if (be == SE_NONE)
        return SE_NONE;
    heap_size = se_heap_max();
    while ((heap = se_heap_alloc(heap_size)) == NULL) {
        if (heap_size < 200000)
            break;
        heap_size -= 50000;
    }
    if (heap == NULL) {
        se_close();
        return SE_NONE;
    }
    ring_pos = 0;
    memset(slot_on, 0, sizeof(slot_on));
    opened = TRUE;
    *extra = music_on_env();
    return be;
}

/** A place in the ring for len bytes (16-aligned); the sounds there stop. */
static UBYTE *ring_alloc(ULONG len)
{
    ULONG need = (len + 15) & ~15UL;
    int i;

    if (need > heap_size)
        return NULL;
    if (ring_pos + need > heap_size)
        ring_pos = 0;
    for (i = 0; i < SE_SLOTS; i++) {
        if (slot_on[i] && (slot_beg[i] < ring_pos + need) &&
          (slot_beg[i] + slot_len[i] > ring_pos)) {
            se_stop(i);
            slot_on[i] = 0;
        }
    }
    ring_pos += need;
    return heap + ring_pos - need;
}

static void snd_play(const UBYTE *p)
{
    ULONG slot = rd32(p), offs = rd32(p + 4), len = rd32(p + 8);
    LONG rate = (LONG)rd32(p + 12);
    UWORD vol = rd16(p + 16);
    int pan = rd16(p + 18);
    LONG passes = (LONG)rd32(p + 20);
    ULONG pass_ms = rd32(p + 24);
    UBYTE *dst = NULL;

    if (slot >= SE_SLOTS)
        return;
    n_play++;
    if (offs == 0xFFFFFFFFUL)
        n_silent++;
    se_stop((int)slot);
    slot_on[slot] = 0;
    if ((offs != 0xFFFFFFFFUL) && (len >= 2)) {
        ULONG aoff = offs & ~15UL;
        ULONG rows = ((offs + len) - aoff + 15) & ~15UL;
        /* extFetch moves 32 bytes or more (LINK_QSDMA_MIN_LEN): a short one
         * starts 16 bytes earlier rather than reading past the heap */
        if ((rows < 32) && (aoff >= 16)) {
            aoff -= 16;
            rows = 32;
        }
        if (rows <= FETCH_MAX)
            dst = ring_alloc(len);
        else
            n_toobig++;
        if (dst != NULL) {
            LONG rc;
            memset(&freq_, 0, sizeof(freq_));
            freq_.version = CSWARP_EXTREQ_VERSION;
            rc = extFetch(SWL_BUF_SOUND, aoff, fetch_buf, rows, 1, rows, &freq_);
            if (rc == CSWEXT_OK) {
                CopyMem(fetch_buf + (offs - aoff), dst, len);
                slot_beg[slot] = (ULONG)(dst - heap);
                slot_len[slot] = len;
                slot_on[slot] = 1;
                n_fetched++;
                if (rows > ok_max_rows) ok_max_rows = rows;
            } else {
                fetch_fail++;
                last_rc = rc;
                last_aoff = aoff;
                last_rows = rows;
                if (rows < fail_min_rows) fail_min_rows = rows;
                dst = NULL;
            }
        }
    }
    se_play((int)slot, dst, len, rate, vol, pan, passes, pass_ms);
}

int hsnd_handle(UWORD type, const UBYTE *p, UWORD len, LONG *res, LONG *extra)
{
    char name[256];

    *res = 0;
    *extra = 0;
    switch (type) {
    case SWL_SND_OPEN:
        *res = snd_open(extra);
        return 2;
    case SWL_SND_CLOSE:
        snd_close();
        return 1;
    case SWL_SND_PLAY:
        if (opened && (len >= 28))
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
        if (len >= sizeof(name))
            len = sizeof(name) - 1;
        memcpy(name, p, len);
        name[len] = '\0';
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
                FPrintf(f, (STRPTR)"play %lu silent %lu fetched %lu fetch_fail %lu too_big %lu ring %lu/%lu rc %ld aoff %lu rows %lu okmax %lu failmin %lu\n",
                  n_play, n_silent, n_fetched, fetch_fail, n_toobig, ring_pos, heap_size,
                  last_rc, last_aoff, last_rows, ok_max_rows, fail_min_rows);
                Close(f);
            }
        }
    }
}

void hsnd_close(void)
{
    snd_close();
    timer_close();
    if (fetch_raw != NULL)
        FreeVec(fetch_raw);
    fetch_raw = NULL;
}
