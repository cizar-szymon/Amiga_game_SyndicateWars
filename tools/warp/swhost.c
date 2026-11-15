/* swhost - 68k host of Syndicate Wars running on the Warp ARM (vbcc).
 *
 * Loads swcore.wxt (the whole game, built by tools/warp/build_arm.sh) into
 * the ARM through cswarp.library and serves it (platform/warp/swlink.h):
 * opens the 8-bit RTG screen, fetches every frame from the ARM straight into
 * the screen bitmap (extFetch), sets the palette, does the file operations,
 * passes the keyboard and the mouse on, plays the sound (swhost_snd.c,
 * with the engine of the 68k game: Paula or AHI, ENV:SyndWarsSound and
 * ENV:SyndWarsMusic as there).  The 68060 does nothing else.
 *
 * usage (from the game directory):
 *   swhost [WXT=file] [game arguments...]       e.g. swhost -q -m 0,1
 * ENV:SyndWarsProf (1 / 2) is passed on as the profiler level.
 * Ctrl-C ends the game. */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfx.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <utility/tagitem.h>

struct Library *CyberGfxBase;

/* CyberGraphX: the three calls used, for vbcc (no headers for it here) */
#define CYBRBIDTG_Depth         (TAG_USER + 0x50000)
#define CYBRBIDTG_NominalWidth  (TAG_USER + 0x50001)
#define CYBRBIDTG_NominalHeight (TAG_USER + 0x50002)
#define LBMI_BYTESPERROW        0x84001006
#define LBMI_BASEADDRESS        0x84001007
ULONG __BestCModeIDTagList(__reg("a6") struct Library *, __reg("a0") struct TagItem *) = "	jsr	-60(a6)";
APTR __LockBitMapTagList(__reg("a6") struct Library *, __reg("a0") APTR, __reg("a1") struct TagItem *) = "	jsr	-168(a6)";
void __UnLockBitMap(__reg("a6") struct Library *, __reg("a0") APTR) = "	jsr	-174(a6)";
static ULONG BestCModeIDTags(ULONG tag, ...) { return __BestCModeIDTagList(CyberGfxBase, (struct TagItem *)&tag); }
static APTR LockBitMapTags(APTR bm, ULONG tag, ...) { return __LockBitMapTagList(CyberGfxBase, bm, (struct TagItem *)&tag); }
#define UnLockBitMap(h) __UnLockBitMap(CyberGfxBase, (h))
#include <libraries/cswarp.h>
#include <proto/cswarp.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "swlink.h"
#include "swhost_snd.h"

struct CSWarpBase *csWarpBase;

static CSWarpExtReq req;     /* the link exchanges (main loop) */
static CSWarpExtReq xreq;    /* fetches and feeds: their own, so that the main
                                loop's view of the extension state stays */
static struct Screen *scr;
static struct Window *win;
static void do_smk_play(const UBYTE *p, UWORD len);
static void do_report(const UBYTE *p, UWORD len);
static void do_report_poll(void);
static void do_report_snap(void);
static UWORD *blank_pointer;
static int scr_w, scr_h;

/* outgoing records */
static UBYTE out_buf[65536];
static ULONG out_len;
/* incoming bytes */
static UBYTE rx_buf[16384];
static ULONG rx_len;

/* file read buffer, 16-aligned for extFeed */
static UBYTE *io_raw, *io_buf;

#define MAX_FH 32
static BPTR fh[MAX_FH];

static int trace_ticks;
static int ext_running, ext_exit_code = -1000, quit_sent;

/* stats */
static ULONG frames, frames_total, fetch_us_sum, fetch_us_max;
static struct DateStamp stat_start;
static ULONG fps_last;

/* the log: stdout and RAM:swarm-log.txt, closed after every line so that
   it can be read while the game runs (and survives up to a crash) */
#include <stdarg.h>
static void hlog(const char *fmt, ...)
{
    static char line[4096];
    va_list ap;
    BPTR f;
    LONG n;
    va_start(ap, fmt);
    n = vsprintf(line, fmt, ap);
    va_end(ap);
    fputs(line, stdout);
    f = Open((STRPTR)"RAM:swarm-log.txt", MODE_READWRITE);
    if (f) {
        Seek(f, 0, OFFSET_END);
        Write(f, line, n);
        Close(f);
    }
}

static ULONG rd32(const UBYTE *p) { return p[0] | (p[1] << 8) | ((ULONG)p[2] << 16) | ((ULONG)p[3] << 24); }
static UWORD rd16(const UBYTE *p) { return (UWORD)(p[0] | (p[1] << 8)); }
static void wr32(UBYTE *p, ULONG v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static void wr16(UBYTE *p, UWORD v) { p[0] = v; p[1] = v >> 8; }

static void zreq(void)
{
    memset(&req, 0, sizeof(req));
    req.version = CSWARP_EXTREQ_VERSION;
}

static void zxreq(void)
{
    memset(&xreq, 0, sizeof(xreq));
    xreq.version = CSWARP_EXTREQ_VERSION;
}

static void queue(UWORD type, const void *payload, UWORD len)
{
    ULONG padded = (len + 3) & ~3UL;
    if (out_len + 4 + padded > sizeof(out_buf))
        return;     /* full: dropped (only input events can fill it) */
    wr16(out_buf + out_len, type);
    wr16(out_buf + out_len + 2, len);
    if (len) memcpy(out_buf + out_len + 4, payload, len);
    memset(out_buf + out_len + 4 + len, 0, padded - len);
    out_len += 4 + padded;
}

static void result(LONG val, LONG extra)
{
    UBYTE p[8];
    wr32(p, (ULONG)val);
    wr32(p + 4, (ULONG)extra);
    queue(SWL_RESULT, p, 8);
}

/******************************************************************************/
/* screen */

static void close_display(void)
{
    if (win) {
        ClearPointer(win);
        CloseWindow(win);
        win = NULL;
    }
    if (scr) {
        CloseScreen(scr);
        scr = NULL;
    }
}

static int open_display(int w, int h)
{
    ULONG modeid;

    if (scr && scr_w == w && scr_h == h)
        return 1;
    close_display();
    modeid = BestCModeIDTags(CYBRBIDTG_NominalWidth, w, CYBRBIDTG_NominalHeight, h,
      CYBRBIDTG_Depth, 8, TAG_DONE);
    if (modeid == INVALID_ID) {
        hlog("swhost: no 8-bit mode %dx%d\n", w, h);
        return 0;
    }
    scr = OpenScreenTags(NULL, SA_DisplayID, modeid, SA_Width, w, SA_Height, h, SA_Depth, 8,
      SA_Quiet, TRUE, SA_ShowTitle, FALSE, SA_Type, CUSTOMSCREEN,
      SA_Title, (ULONG)"Syndicate Wars ARM", TAG_DONE);
    if (!scr)
        return 0;
    win = OpenWindowTags(NULL, WA_CustomScreen, (ULONG)scr, WA_Left, 0, WA_Top, 0,
      WA_Width, w, WA_Height, h, WA_Backdrop, TRUE, WA_Borderless, TRUE, WA_Activate, TRUE,
      WA_RMBTrap, TRUE, WA_ReportMouse, TRUE, WA_NoCareRefresh, TRUE, WA_SimpleRefresh, TRUE,
      WA_Title, (ULONG)"Syndicate Wars ARM",
      WA_IDCMP, IDCMP_RAWKEY | IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_ACTIVEWINDOW
        | IDCMP_INACTIVEWINDOW, TAG_DONE);
    if (!win) {
        close_display();
        return 0;
    }
    if (blank_pointer)
        SetPointer(win, blank_pointer, 1, 16, 0, 0);
    scr_w = w;
    scr_h = h;
    return 1;
}

/* the last palette set (6 bits a colour): for the screen of a bug report */
static UBYTE cur_pal[768];

static void set_palette(const UBYTE *pal)
{
    static ULONG table[1 + 256 * 3 + 1];
    ULONG i;
    memcpy(cur_pal, pal, 768);
    if (!scr)
        return;
    table[0] = (256UL << 16) | 0;
    for (i = 0; i < 256 * 3; i++) {
        ULONG v6 = pal[i] & 0x3F;
        ULONG v8 = (v6 << 2) | (v6 >> 4);
        table[1 + i] = v8 * 0x01010101UL;
    }
    table[1 + 256 * 3] = 0;
    LoadRGB32(&scr->ViewPort, table);
}

/* game speed in the corner (SHOW SPEED), as the 68k build shows it */
static const UBYTE fps_font[11][5] = {
    {7,5,5,5,7}, {2,6,2,2,7}, {7,1,7,4,7}, {7,1,7,1,7}, {5,5,7,1,1},
    {7,4,7,1,7}, {7,4,7,5,7}, {7,1,1,1,1}, {7,5,7,5,7}, {7,5,7,1,7},
    {5,1,2,4,5}, /* % */
};

static void meter_draw(UBYTE *base, ULONG bpr, ULONG w, ULONG h, LONG value)
{
    char txt[8];
    int i, n, y, x;
    n = sprintf(txt, "%ld%%", value);
    /* bottom right corner */
    base += (h - 8) * bpr + (w - (n * 4 + 3));
    for (y = 0; y < 7; y++)
        for (x = 0; x < n * 4 + 1; x++)
            base[(1 + y) * bpr + 1 + x] = 0;
    for (i = 0; i < n; i++) {
        const UBYTE *g = fps_font[(txt[i] == '%') ? 10 : (txt[i] - '0')];
        for (y = 0; y < 5; y++) {
            UBYTE *row = base + (2 + y) * bpr + 2 + i * 4;
            for (x = 0; x < 3; x++)
                if (g[y] & (4 >> x))
                    row[x] = 255;
        }
    }
}

static ULONG ms_since(struct DateStamp *a)
{
    struct DateStamp now;
    DateStamp(&now);
    return (ULONG)((now.ds_Days - a->ds_Days) * 86400000L + (now.ds_Minute - a->ds_Minute) * 60000L
      + (now.ds_Tick - a->ds_Tick) * 20L);
}

static void do_frame(const UBYTE *p, UWORD len)
{
    UWORD w = rd16(p), h = rd16(p + 2);
    ULONG pitch = rd32(p + 4);
    ULONG buf = len >= 12 ? rd32(p + 8) : SWL_BUF_SCREEN;
    LONG meter = len >= 16 ? (LONG)rd32(p + 12) : -1;
    APTR lock;
    UBYTE *base = NULL;
    ULONG bpr = 0, us = 0;
    LONG rc = -1;
    UBYTE r[4];

    if (scr && w <= scr_w && h <= scr_h) {
        lock = LockBitMapTags(scr->RastPort.BitMap, LBMI_BASEADDRESS, (ULONG)&base,
          LBMI_BYTESPERROW, (ULONG)&bpr, TAG_DONE);
        if (lock) {
            zxreq();
            if (pitch == w) {
                rc = extFetch(buf, 0, base, w, h, bpr, &xreq);
                us = xreq.boardUs;
            } else {
                ULONG y;
                for (y = 0; y < h; y++) {
                    zxreq();
                    rc = extFetch(buf, y * pitch, base + y * bpr, w, 1, bpr, &xreq);
                    us += xreq.boardUs;
                }
            }
            if (meter >= 0 && meter <= 999 && w >= 32 && h >= 8)
                meter_draw(base, bpr, w, h, meter);
            UnLockBitMap(lock);
        }
    }
    if (rc != CSWEXT_OK) {
        static int warned;
        if (warned++ < 3)
            hlog("swhost: extFetch %ux%u (pitch %lu, bpr %lu) -> %ld\n", w, h, pitch, bpr, rc);
    }
    frames++;
    frames_total++;
    fetch_us_sum += us;
    if (us > fetch_us_max) fetch_us_max = us;
    {
        ULONG ms = ms_since(&stat_start);
        if (ms >= 1000 && frames_total > 0) {
            static ULONG win_frames, win_ms;
            fps_last = (frames * 1000 + ms / 2) / ms;
            win_frames += frames;
            win_ms += ms;
            if (win_ms >= 5000) {
                hlog("swhost: %lu frames in %lu ms (%lu.%lu fps), fetch avg %lu us max %lu us\n",
                  win_frames, win_ms, win_frames * 1000 / win_ms, (win_frames * 10000 / win_ms) % 10,
                  fetch_us_sum / win_frames, fetch_us_max);
                win_frames = win_ms = 0;
                fetch_us_sum = fetch_us_max = 0;
            }
            frames = 0;
            DateStamp(&stat_start);
        }
    }
    wr32(r, us);
    queue(SWL_FRAME_DONE, r, 4);
}

/******************************************************************************/
/* files */

static void do_fopen(const UBYTE *p, UWORD len)
{
    ULONG flags = rd32(p);
    const char *name = (const char *)p + 4;
    LONG mode, size, i;
    BPTR f;

    for (i = 0; i < MAX_FH; i++)
        if (!fh[i]) break;
    if (i >= MAX_FH) { result(-1, 0); return; }
    if (flags & 0x0200) mode = MODE_NEWFILE;                /* O_TRUNC */
    else if (flags & 0x0100) mode = MODE_READWRITE;         /* O_CREAT */
    else mode = MODE_OLDFILE;
    f = Open((STRPTR)name, mode);
    if (!f) { result(-1, 0); return; }
    Seek(f, 0, OFFSET_END);
    size = Seek(f, 0, OFFSET_BEGINNING);
    fh[i] = f;
    result(i, size);
    (void)len;
}

static void do_fread(const UBYTE *p)
{
    ULONG h = rd32(p), off = rd32(p + 4), len = rd32(p + 8);
    LONG n;

    if (h >= MAX_FH || !fh[h]) { result(-1, 0); return; }
    if (len > SWL_XFER_BYTES) len = SWL_XFER_BYTES;
    Seek(fh[h], off, OFFSET_BEGINNING);
    n = Read(fh[h], io_buf, len);
    if (n > 0) {
        /* extFeed moves multiples of 16, at least 32 bytes */
        ULONG padded = n < 32 ? 32 : (n + 15) & ~15UL;
        LONG rc;
        memset(io_buf + n, 0, padded - n);
        zxreq();
        rc = extFeed(SWL_BUF_XFER, 0, io_buf, padded, &xreq);
        if (rc != CSWEXT_OK) {
            hlog("swhost: extFeed %lu -> %ld\n", padded, rc);
            n = -1;
        }
    }
    result(n, 0);
}

static void do_fwrite(const UBYTE *p, UWORD len)
{
    ULONG h = rd32(p), off = rd32(p + 4);
    LONG n;
    if (h >= MAX_FH || !fh[h] || len < 8) { result(-1, 0); return; }
    Seek(fh[h], off, OFFSET_BEGINNING);
    n = Write(fh[h], (APTR)(p + 8), len - 8);
    result(n, 0);
}

static void do_fclose(const UBYTE *p)
{
    ULONG h = rd32(p);
    if (h < MAX_FH && fh[h]) {
        Close(fh[h]);
        fh[h] = 0;
    }
    result(0, 0);
}

static void do_fstat(const char *name)
{
    BPTR lock = Lock((STRPTR)name, SHARED_LOCK);
    struct FileInfoBlock *fib;
    if (!lock) { result(-1, 0); return; }
    fib = AllocDosObject(DOS_FIB, NULL);
    if (fib && Examine(lock, fib))
        result(fib->fib_DirEntryType > 0 ? 0 : fib->fib_Size, fib->fib_DirEntryType > 0);
    else
        result(-1, 0);
    if (fib) FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
}

/******************************************************************************/
/* records from the ARM */

static void handle(UWORD type, const UBYTE *p, UWORD len)
{
    switch (type) {
    case SWL_LOG: {
        static char t[SWL_REC_MAX + 1];
        memcpy(t, p, len);
        t[len] = 0;
        hlog("%s", t);
        break;
    }
    case SWL_TRACE: {
        static char t[SWL_REC_MAX + 1];
        memcpy(t, p, len);
        t[len] = 0;
        hlog("T %s", t);
        Delay(trace_ticks);
        result(0, 0);
        break;
    }
    case SWL_HELLO:
        hlog("swhost: ARM link v%lu, %lu MHz, heap %lu KB, render %lu KB\n", rd32(p),
          rd32(p + 4) / 1000000, rd32(p + 8) / 1024, rd32(p + 12) / 1024);
        break;
    case SWL_SCREEN:
        hlog("swhost: screen %ux%u\n", rd16(p), rd16(p + 2));
        if (!open_display(rd16(p), rd16(p + 2)))
            hlog("swhost: cannot open the screen\n");
        DateStamp(&stat_start);
        break;
    case SWL_PAL:
        set_palette(p);
        break;
    case SWL_FRAME:
        do_frame(p, len);
        break;
    case SWL_SMK_PLAY:
        do_smk_play(p, len);
        break;
    case SWL_REPORT:
        do_report(p, len);
        break;
    case SWL_REPORT_POLL:
        do_report_poll();
        break;
    case SWL_REPORT_SNAP:
        do_report_snap();
        break;
    case SWL_FOPEN: do_fopen(p, len); break;
    case SWL_FREAD: do_fread(p); break;
    case SWL_FWRITE: do_fwrite(p, len); break;
    case SWL_FCLOSE: do_fclose(p); break;
    case SWL_FSTAT: do_fstat((const char *)p); break;
    case SWL_FREMOVE: result(DeleteFile((STRPTR)p) ? 0 : -1, 0); break;
    case SWL_FRENAME: result(Rename((STRPTR)p, (STRPTR)(p + strlen((const char *)p) + 1)) ? 0 : -1, 0); break;
    case SWL_MKDIR: {
        BPTR l = CreateDir((STRPTR)p);
        if (l) UnLock(l);
        result(l ? 0 : -1, 0);
        break;
    }
    case SWL_EXIT:
        ext_exit_code = (LONG)rd32(p);
        hlog("swhost: game exit %ld\n", ext_exit_code);
        break;
    default: {
        LONG res, extra;
        int h = hsnd_handle(type, p, len, &res, &extra);
        if (h == 2)
            result(res, extra);
        else if (h == 0)
            hlog("swhost: unknown record %u (%u bytes)\n", type, len);
        break;
    }
    }
}

/******************************************************************************/
/* Smacker videos of the ARM game, played here by the 68060 (SWL_SMK_PLAY):
 * the player of the 68k game (platform/amiga/smk_core.c) on this screen */

#include "smk_core.h"
#include "snd_engine.h"

static UWORD smk_flags;

static void hsmk_show(const unsigned char *pix, unsigned long w, unsigned long h,
  unsigned char yscale, const unsigned char *pal6, void *ctx)
{
    APTR lock;
    UBYTE *base = NULL;
    ULONG bpr = 0;
    long out_h = (yscale != 0) ? (long)h * 2 : (long)h;
    long cw, ch, x, y, sy;

    (void)ctx;
    if (!scr)
        return;
    lock = LockBitMapTags(scr->RastPort.BitMap, LBMI_BASEADDRESS, (ULONG)&base,
      LBMI_BYTESPERROW, (ULONG)&bpr, TAG_DONE);
    if (lock) {
        cw = ((long)w < (long)scr_w) ? (long)w : (long)scr_w;
        ch = (out_h < (long)scr_h) ? out_h : (long)scr_h;
        x = ((long)scr_w - cw) / 2;
        y = ((long)scr_h - ch) / 2;
        for (sy = 0; sy < ch; sy++) {
            UBYTE *d = base + (y + sy) * bpr + x;
            long src_line = (yscale != 0) ? (sy >> 1) : sy;
            if ((yscale == 1) && (sy & 1))
                memset(d, 0, cw);
            else
                memcpy(d, pix + src_line * w, cw);
        }
        UnLockBitMap(lock);
    }
    set_palette(pal6);
}

static void hsmk_clear(void *ctx)
{
    APTR lock;
    UBYTE *base = NULL;
    ULONG bpr = 0, y;

    (void)ctx;
    if (!scr)
        return;
    lock = LockBitMapTags(scr->RastPort.BitMap, LBMI_BASEADDRESS, (ULONG)&base,
      LBMI_BYTESPERROW, (ULONG)&bpr, TAG_DONE);
    if (lock) {
        for (y = 0; y < scr_h; y++)
            memset(base + y * bpr, 0, scr_w);
        UnLockBitMap(lock);
    }
}

static unsigned long hsmk_clock(void)
{
    return se_ms();
}

/** The window's input during a video: Esc, Return, Space or the left
 * button end it (unless SMK_NoStopOnUserInput); not passed to the game. */
static int hsmk_idle(void *ctx)
{
    struct IntuiMessage *msg;
    int stop = 0;

    (void)ctx;
    if (SetSignal(0, 0) & SIGBREAKF_CTRL_C)
        stop = 1;
    if (win) {
        while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort)) != NULL) {
            ULONG cls = msg->Class;
            UWORD code = msg->Code;
            ReplyMsg((struct Message *)msg);
            if ((cls == IDCMP_RAWKEY) && ((code == 0x45) || (code == 0x44) || (code == 0x40)))
                stop = 1;
            if ((cls == IDCMP_MOUSEBUTTONS) && (code == SELECTDOWN))
                stop = 1;
        }
    }
    if (smk_flags & 0x0002)     /* SMK_NoStopOnUserInput */
        stop = (SetSignal(0, 0) & SIGBREAKF_CTRL_C) != 0;
    return stop;
}

static void hsmk_log(const char *text)
{
    hlog("%s", text);
}

static void do_smk_play(const UBYTE *p, UWORD len)
{
    struct SmkOut out;
    char name[128];
    static char t[64];
    int r;

    smk_flags = rd16(p);
    if (len <= 4) {
        result(-1, 0);
        return;
    }
    strncpy(name, (const char *)p + 4, sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;
    /* test: ENV:SyndWarsSmkTest=file plays that file instead */
    if (GetVar((STRPTR)"SyndWarsSmkTest", (STRPTR)t, sizeof(t), 0) > 0)
        strcpy(name, t);
    out.show = hsmk_show;
    out.clear = hsmk_clear;
    out.clock_ms = hsmk_clock;
    out.idle = hsmk_idle;
    out.log = hsmk_log;
    out.ctx = NULL;
    out.sound = (se_backend() != SE_NONE);
    out.volume = rd16(p + 2);
    hlog("swhost: video %s\n", name);
    r = smk_core_play(name, &out);
    result(r, 0);
}

/******************************************************************************/
/* Bug reports of the ARM game (SWL_REPORT): sent by the 68k's process of
 * platform/amiga/amiga_report.c, with the files of this host */

#include "amiga_report.h"

#define REPORT_HOST "83.12.152.135"
#define REPORT_PORT 6810

static int report_used;

static void wr_be32(UBYTE *p, ULONG v)
{
    p[0] = (UBYTE)(v >> 24); p[1] = (UBYTE)(v >> 16); p[2] = (UBYTE)(v >> 8); p[3] = (UBYTE)v;
}

/** The screen as it is now to RAM:SyndWars-screen.iff (IFF PBM, as the 68k
 * game saves it): SWL_REPORT_SNAP, when the box opens, before it is drawn.
 * The ARM game writes no files for a report (its file writes through this
 * host hung there). */
static void do_report_snap(void)
{
    APTR lock;
    UBYTE *base = NULL;
    ULONG bpr = 0, w = scr_w, h = scr_h, row = (scr_w + 1) & ~1UL, y, i;
    UBYTE hd[48 + 8 + 768 + 8], *line;
    BPTR f;

    if (!scr) {
        result(-1, 0);
        return;
    }
    line = AllocVec(row, MEMF_ANY | MEMF_CLEAR);
    f = Open((STRPTR)"RAM:SyndWars-screen.iff", MODE_NEWFILE);
    if (!line || !f) {
        if (line) FreeVec(line);
        if (f) Close(f);
        result(-1, 0);
        return;
    }
    memcpy(hd, "FORM", 4);
    wr_be32(hd + 4, 4 + 8 + 20 + 8 + 768 + 8 + row * h);
    memcpy(hd + 8, "PBM BMHD", 8);
    wr_be32(hd + 16, 20);
    memset(hd + 20, 0, 20);
    hd[20] = (UBYTE)(w >> 8); hd[21] = (UBYTE)w;
    hd[22] = (UBYTE)(h >> 8); hd[23] = (UBYTE)h;
    hd[28] = 8;                                /* planes */
    hd[34] = 1; hd[35] = 1;                    /* aspect */
    hd[36] = (UBYTE)(w >> 8); hd[37] = (UBYTE)w;
    hd[38] = (UBYTE)(h >> 8); hd[39] = (UBYTE)h;
    memcpy(hd + 40, "CMAP", 4);
    wr_be32(hd + 44, 768);
    for (i = 0; i < 768; i++)
        hd[48 + i] = (UBYTE)((cur_pal[i] & 0x3F) << 2);
    memcpy(hd + 48 + 768, "BODY", 4);
    wr_be32(hd + 48 + 768 + 4, row * h);
    Write(f, hd, 48 + 768 + 8);
    lock = LockBitMapTags(scr->RastPort.BitMap, LBMI_BASEADDRESS, (ULONG)&base,
      LBMI_BYTESPERROW, (ULONG)&bpr, TAG_DONE);
    for (y = 0; y < h; y++) {
        if (lock)
            CopyMem(base + y * bpr, line, w);
        Write(f, line, row);
    }
    if (lock)
        UnLockBitMap(lock);
    Close(f);
    FreeVec(line);
    result(0, 0);
}

static void write_text_file(const char *name, const char *text)
{
    BPTR f = Open((STRPTR)name, MODE_NEWFILE);
    if (f) {
        Write(f, (APTR)text, strlen(text));
        Close(f);
    }
}

static void do_report(const UBYTE *p, UWORD len)
{
    static char rec[SWL_REC_MAX + 1];
    static char dir[256], flog[300], fini[300], mach[200];
    const char *f[5], *s[5];
    char host[64];
    int port = REPORT_PORT, i, n;
    ULONG pos = 0;

    if (len > SWL_REC_MAX)
        len = SWL_REC_MAX;
    memcpy(rec, p, len);
    rec[len] = 0;
    for (i = 0; i < 5; i++) {
        s[i] = rec + pos;
        while (pos < len && rec[pos]) pos++;
        if (pos < len) pos++;
    }
    if (!NameFromLock(GetProgramDir(), (STRPTR)dir, sizeof(dir)))
        strcpy(dir, "PROGDIR:");
    /* the state of the game and the e-mail for next time, from the ARM */
    write_text_file("RAM:SyndWars-state.txt", s[4]);
    {
        static char fe[300], line[100];
        strcpy(fe, dir);
        AddPart((STRPTR)fe, (STRPTR)"qdata/savegame/report_email.txt", sizeof(fe));
        sprintf(line, "%.90s\n", s[1]);
        write_text_file(fe, line);
    }
    if (!report_network()) {
        result(0, SWL_REP_NONET);
        return;
    }
    strcpy(flog, dir);
    AddPart((STRPTR)flog, (STRPTR)"error.log", sizeof(flog));
    strcpy(fini, dir);
    AddPart((STRPTR)fini, (STRPTR)"qdata/savegame/default.ini", sizeof(fini));
    n = 0;
    f[n++] = "RAM:SyndWars-state.txt";
    f[n++] = "RAM:SyndWars-screen.iff";
    f[n++] = flog;
    f[n++] = fini;
    f[n++] = "RAM:swarm-log.txt";
    sprintf(mach, "%s; 68k host: %s", s[3],
      (SysBase->AttnFlags & 0x80) ? "68060" : "68040 or older");
    strcpy(host, REPORT_HOST);
    if (GetVar((STRPTR)"SyndWarsReportServer", (STRPTR)host, sizeof(host), 0) > 0) {
        char *c = strchr(host, ':');
        if (c) {
            *c = 0;
            port = atoi(c + 1);
        }
    }
    hlog("swhost: bug report to %s:%d\n", host, port);
    if (!report_send(host, port, s[0], "Help", s[1], s[2], mach, f, n)) {
        result(0, SWL_REP_BUSY);
        return;
    }
    report_used = 1;
    result(1, 0);
}

static void do_report_poll(void)
{
    char text[64];
    int ok;

    if (!report_result(text, sizeof(text), &ok)) {
        result(SWL_REP_GOING, 0);
        return;
    }
    hlog("swhost: bug report %s: %s\n", ok ? "sent" : "not sent", text);
    if (ok)
        result(SWL_REP_SENT, atoi(text));
    else
        result(SWL_REP_FAILED, strstr(text, "did not answer") ? SWL_REP_NOANSWER :
          SWL_REP_REFUSED);
}

static void parse_rx(void)
{
    ULONG pos = 0;
    while (rx_len - pos >= 4) {
        UWORD type = rd16(rx_buf + pos), len = rd16(rx_buf + pos + 2);
        ULONG total = 4 + ((len + 3) & ~3UL);
        if (rx_len - pos < total)
            break;
        handle(type, rx_buf + pos + 4, len);
        pos += total;
    }
    if (pos) {
        memmove(rx_buf, rx_buf + pos, rx_len - pos);
        rx_len -= pos;
    }
}

/******************************************************************************/
/* input */

static void read_input(void)
{
    struct IntuiMessage *msg;
    UBYTE p[6];

    if (!win)
        return;
    while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort)) != NULL) {
        ULONG cls = msg->Class;
        UWORD code = msg->Code, qual = msg->Qualifier;
        WORD mx = msg->MouseX, my = msg->MouseY;
        ReplyMsg((struct Message *)msg);
        switch (cls) {
        case IDCMP_RAWKEY:
            wr16(p, code & 0xFF);
            wr16(p + 2, qual);
            queue(SWL_KEY, p, 4);
            break;
        case IDCMP_MOUSEMOVE:
        case IDCMP_MOUSEBUTTONS: {
            UWORD ev = SWL_MS_MOVE;
            if (cls == IDCMP_MOUSEBUTTONS) {
                switch (code) {
                case SELECTDOWN: ev = SWL_MS_LDOWN; break;
                case SELECTUP: ev = SWL_MS_LUP; break;
                case MENUDOWN: ev = SWL_MS_RDOWN; break;
                case MENUUP: ev = SWL_MS_RUP; break;
                case MIDDLEDOWN: ev = SWL_MS_MDOWN; break;
                case MIDDLEUP: ev = SWL_MS_MUP; break;
                default: continue;
                }
            }
            wr16(p, (UWORD)mx);
            wr16(p + 2, (UWORD)my);
            wr16(p + 4, ev);
            queue(SWL_MOUSE, p, 6);
            break;
        }
        case IDCMP_ACTIVEWINDOW:
        case IDCMP_INACTIVEWINDOW:
            wr16(p, cls == IDCMP_ACTIVEWINDOW);
            wr16(p + 2, 0);
            queue(SWL_ACTIVE, p, 4);
            break;
        }
    }
}

/******************************************************************************/

int main(int argc, char **argv)
{
    const char *wxt_path = "PROGDIR:swcore.wxt";
    const char *snd_opt_sound = NULL, *snd_opt_music = NULL;
    static char args[CSWEXT_ARGS_MAX];
    char prof[4] = "0";
    BPTR f;
    LONG size, rc;
    UBYTE *wxt = NULL;
    BYTE door_sig = -1;
    int i, ret = 20;
    struct DateStamp quit_at;

    setvbuf(stdout, NULL, _IONBF, 0);
    DeleteFile((STRPTR)"RAM:swarm-log.txt");
    csWarpBase = (struct CSWarpBase *)OpenLibrary("cswarp.library", CSWARP_EXT_MIN_VER);
    CyberGfxBase = OpenLibrary("cybergraphics.library", 40);
    if (!csWarpBase || !CyberGfxBase) {
        hlog("swhost: needs cswarp.library %d.%d and cybergraphics.library\n",
          CSWARP_EXT_MIN_VER, CSWARP_EXT_MIN_REV);
        goto out;
    }
    if (((struct Library *)csWarpBase)->lib_Version == CSWARP_EXT_MIN_VER
      && ((struct Library *)csWarpBase)->lib_Revision < CSWARP_EXT_MIN_REV) {
        hlog("swhost: cswarp.library too old\n");
        goto out;
    }

    /* command line for the ARM: "prof=N time=T | game args" */
    if (GetVar((STRPTR)"SyndWarsProf", (STRPTR)prof, sizeof(prof), 0) >= 0)
        prof[0] = (prof[0] == '2') ? '2' : '1';
    {
        struct DateStamp ds;
        ULONG t;
        DateStamp(&ds);
        /* AmigaOS epoch 1978-01-01 -> Unix */
        t = 252460800UL + ds.ds_Days * 86400UL + ds.ds_Minute * 60UL + ds.ds_Tick / 50;
        sprintf(args, "prof=%c time=%lu |", prof[0], t);
    }
    /* far civilians option (see far_civilians_level() in src/thing.c):
     * the ARM core has no ENV:, so it gets the value on its command line */
    {
        char civ[4];
        if ((GetVar((STRPTR)"SyndWarsCivLOD", (STRPTR)civ, sizeof(civ), 0) >= 0)
          && (civ[0] >= '0') && (civ[0] <= '9'))
            sprintf(args + strlen(args) - 1, " civlod=%c |", civ[0]);
        if ((GetVar((STRPTR)"SyndWarsRadar2", (STRPTR)civ, sizeof(civ), 0) >= 0)
          && (civ[0] >= '0') && (civ[0] <= '9'))
            sprintf(args + strlen(args) - 1, " radar2=%c |", civ[0]);
        if ((GetVar((STRPTR)"SyndWarsAutoDetail", (STRPTR)civ, sizeof(civ), 0) >= 0)
          && (civ[0] >= '0') && (civ[0] <= '9'))
            sprintf(args + strlen(args) - 1, " autodet=%c |", civ[0]);
        if ((GetVar((STRPTR)"SyndWarsShowSpeed", (STRPTR)civ, sizeof(civ), 0) >= 0)
          && (civ[0] >= '0') && (civ[0] <= '9'))
            sprintf(args + strlen(args) - 1, " showspeed=%c |", civ[0]);
    }
    for (i = 1; i < argc; i++) {
        if (strncmp(argv[i], "STAGE=", 6) == 0) {
            sprintf(args + strlen(args) - 1, " stage=%c |", argv[i][6]);
            continue;
        }
        if (strncmp(argv[i], "TRACE=", 6) == 0) {
            trace_ticks = atoi(argv[i] + 6) / 20;
            sprintf(args + strlen(args) - 1, " trace=1 |");
            continue;
        }
        if (strncmp(argv[i], "WXT=", 4) == 0) {
            wxt_path = argv[i] + 4;
            continue;
        }
        /* the Tool Types SOUND= / MUSIC= of SyndWars's icon */
        if (strncmp(argv[i], "SOUND=", 6) == 0) {
            snd_opt_sound = argv[i] + 6;
            continue;
        }
        if (strncmp(argv[i], "MUSIC=", 6) == 0) {
            snd_opt_music = argv[i] + 6;
            continue;
        }
        if (strlen(args) + strlen(argv[i]) + 2 < sizeof(args)) {
            strcat(args, " ");
            strcat(args, argv[i]);
        }
    }

    se_set_options(snd_opt_sound, snd_opt_music);
    hlog("swhost: sound backend %d (1 Paula, 2 AHI), music %s%c", se_backend_from_env(),
      se_music_on() ? "on" : "off", 10);
    if (!(f = Open((STRPTR)wxt_path, MODE_OLDFILE))) {
        hlog("swhost: cannot open %s\n", wxt_path);
        goto out;
    }
    Seek(f, 0, OFFSET_END);
    size = Seek(f, 0, OFFSET_BEGINNING);
    wxt = AllocVec(size, MEMF_ANY);
    if (!wxt || Read(f, wxt, size) != size) {
        Close(f);
        hlog("swhost: cannot read %s\n", wxt_path);
        goto out;
    }
    Close(f);

    io_raw = AllocVec(SWL_XFER_BYTES + 32, MEMF_ANY);
    blank_pointer = AllocVec(6 * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
    if (!io_raw) {
        hlog("swhost: out of memory\n");
        goto out;
    }
    io_buf = (UBYTE *)(((ULONG)io_raw + 15) & ~15UL);

    door_sig = AllocSignal(-1);
    if (door_sig < 0 || !unitSigRegister(FindTask(NULL), CSWARP_UNIT_EXT, door_sig)) {
        hlog("swhost: no doorbell signal\n");
        goto out;
    }

    hlog("swhost: extLoad...\n");
    Delay(100);
    zreq();
    rc = extLoad(wxt, size, args, 512, &req);
    if (rc == CSWEXT_ERR_BUSY) {
        /* the game of a host that did not end it (stopped too early, a
         * crash) still runs on the ARM: ended, and loaded again */
        hlog("swhost: an extension still runs on the ARM, stopping it\n");
        zreq();
        extStop(2000, &req);
        Delay(25);
        zreq();
        rc = extLoad(wxt, size, args, 512, &req);
    }
    hlog("swhost: %s (%ld bytes) -> extLoad %ld, state %lu, exit %ld; args \"%s\"\n", wxt_path, size,
      rc, req.state, req.exitCode, args);
    if (rc != CSWEXT_OK)
        goto out;
    FreeVec(wxt);
    wxt = NULL;
    ext_running = 1;
    DateStamp(&stat_start);

    for (;;) {
        ULONG sigs;
        ULONG sendLen = out_len < CSWEXT_XCHG_MAX ? out_len : CSWEXT_XCHG_MAX;

        zreq();
        req.send = out_buf;
        req.sendLen = sendLen;
        req.recv = rx_buf + rx_len;
        req.recvMax = sizeof(rx_buf) - rx_len < CSWEXT_XCHG_MAX ? sizeof(rx_buf) - rx_len : CSWEXT_XCHG_MAX;
        rc = extXchg(&req);
        if (rc != CSWEXT_OK) {
            hlog("swhost: extXchg -> %ld\n", rc);
            break;
        }
        if (req.sent) {
            memmove(out_buf, out_buf + req.sent, out_len - req.sent);
            out_len -= req.sent;
        }
        rx_len += req.received;
        parse_rx();
        read_input();
        hsnd_service(FALSE);

        if (req.state != EXTST_RUNNING && req.outPending == 0 && req.received == 0) {
            hlog("swhost: extension state %lu, exit code %ld\n", req.state, req.exitCode);
            ret = 0;
            break;
        }
        if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
            if (!quit_sent) {
                hlog("swhost: Ctrl-C, asking the game to quit\n");
                queue(SWL_QUIT, NULL, 0);
                quit_sent = 1;
                DateStamp(&quit_at);
            }
        }
        if (quit_sent && ms_since(&quit_at) > 5000) {
            hlog("swhost: game did not quit, stopping the extension\n");
            break;
        }
        if (req.received == 0 && req.outPending == 0 && out_len == 0) {
            ULONG mask = (1UL << door_sig) | SIGBREAKF_CTRL_C;
            ULONG tmask = hsnd_wait_mask();
            if (win) mask |= 1UL << win->UserPort->mp_SigBit;
            mask |= tmask;
            if (quit_sent) {
                Delay(1);
                continue;
            }
            sigs = Wait(mask);
            if (sigs & SIGBREAKF_CTRL_C)
                SetSignal(SIGBREAKF_CTRL_C, SIGBREAKF_CTRL_C);
            hsnd_service((sigs & tmask) != 0);
        }
    }

out:
    if (ext_running) {
        zreq();
        extStop(2000, &req);
        hlog("swhost: stopped, state %lu, exit %ld\n", req.state, req.exitCode);
    }
    hsnd_close();
    if (report_used)
        report_quit();      /* a report on its way: waited for */
    if (door_sig >= 0) {
        if (csWarpBase) unitSigUnregister(FindTask(NULL), CSWARP_UNIT_EXT);
        FreeSignal(door_sig);
    }
    for (i = 0; i < MAX_FH; i++)
        if (fh[i]) Close(fh[i]);
    close_display();
    if (blank_pointer) FreeVec(blank_pointer);
    if (io_raw) FreeVec(io_raw);
    if (wxt) FreeVec(wxt);
    if (CyberGfxBase) CloseLibrary(CyberGfxBase);
    if (csWarpBase) CloseLibrary((struct Library *)csWarpBase);
    return ret;
}
