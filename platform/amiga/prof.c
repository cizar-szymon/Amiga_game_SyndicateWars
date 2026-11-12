/* Amiga port: frame time profiler.
 * AMIGA_PROF_BEGIN(n) / AMIGA_PROF_END(n) (asm_coverage.h) add the E-clock
 * ticks between them to slot n. Every 5 s the average per presented frame
 * (in 0.1 ms) of each used slot goes to error.log. */
#include <exec/types.h>
#include <devices/timer.h>
#include <proto/timer.h>
#include <proto/dos.h>
#include "bftypes.h"
#include "bflog.h"
#include "bftime.h"

extern struct Device *TimerBase;

#define PROF_SLOTS 48

static const char *const prof_names[PROF_SLOTS] = {
    [0] = "frame",
    [1] = "input", [2] = "draw", [3] = "things", [4] = "other", [5] = "swap",
    [10] = "view_inputs", [11] = "scanner_zoom+cam", [12] = "prepare_drawlist",
    [13] = "mouse_on_map", [14] = "engine_unk3",
    [20] = "camera_setup", [21] = "draw_things", [22] = "floor",
    [23] = "explode", [24] = "draw_screen", [25] = "hud",
    [26] = "hud_goto+lock", [27] = "hud_agents+target", [28] = "hud_panel", [29] = "scanner",
    [6] = "scan_blips", [7] = "scan_arcs", [8] = "scan_dots",
    [15] = "scanner_turn", [16] = "scanner_map", [17] = "scanner_signals",
    [18] = "weapons_panel", [19] = "chat+pickable",
    [30] = "present_copy", [31] = "clear",
    [33] = "gpoly_setup", [34] = "gpoly_raster",
};

static ULONG prof_start[PROF_SLOTS];
static ULONG prof_sum[PROF_SLOTS];
static ULONG prof_frames;
static TbClockMSec prof_report;
static ULONG prof_freq;
unsigned long amiga_prof_pixels, amiga_prof_spans, amiga_prof_gpoly_tris;
unsigned long amiga_prof_cnt[8];

static ULONG prof_now(void)
{
    struct EClockVal ev;

    if (TimerBase == NULL) {
        LbTimerClock(); // opens timer.device
        if (TimerBase == NULL)
            return 0;
    }
    prof_freq = ReadEClock(&ev);
    return ev.ev_lo;
}

/* Fine profiling (slots >= 32, draw item types, render modes) reads the
 * E-clock hundreds of times per frame, which costs several ms itself.
 * It is on only when ENV:SyndWarsProf is set to 2. */
static int prof_fine = -1;

static int prof_fine_on(void)
{
    if (prof_fine < 0) {
        char buf[4];
        prof_fine = (GetVar((STRPTR)"SyndWarsProf", (STRPTR)buf, sizeof(buf), 0) >= 0)
          && (buf[0] == '2');
        LbWarnLog("prof: fine profiling %s\n", prof_fine ? "ON" : "off");
    }
    return prof_fine;
}

void amiga_prof_begin(int slot)
{
    if ((slot >= 32) && !prof_fine_on())
        return;
    prof_start[slot] = prof_now();
}

void amiga_prof_end(int slot)
{
    if ((slot >= 32) && !prof_fine_on())
        return;
    prof_sum[slot] += prof_now() - prof_start[slot];
}

void amiga_prof_item_report(ULONG frames);
void amiga_prof_mode_report(ULONG frames);

/** Called once per presented frame. */
void amiga_prof_frame(void)
{
    TbClockMSec now;
    int i;

    prof_frames++;
    now = LbTimerClock();
    if (prof_report == 0)
        prof_report = now;
    if (now - prof_report < 5000)
        return;
    LbWarnLog("prof: %lu frames in %lu ms\n", prof_frames, (ulong)(now - prof_report));
    LbWarnLog("prof:   gpoly %lu pixels, %lu spans, %lu tris per frame\n",
      amiga_prof_pixels / prof_frames, amiga_prof_spans / prof_frames,
      amiga_prof_gpoly_tris / prof_frames);
    amiga_prof_gpoly_tris = 0;
    LbWarnLog("prof:   cnt sb %lu snb %lu nb %lu nnb %lu walk %lu ytop %lu %lu %lu\n",
      amiga_prof_cnt[0] / prof_frames, amiga_prof_cnt[1] / prof_frames,
      amiga_prof_cnt[2] / prof_frames, amiga_prof_cnt[3] / prof_frames,
      amiga_prof_cnt[4] / prof_frames, amiga_prof_cnt[5] / prof_frames,
      amiga_prof_cnt[6] / prof_frames, amiga_prof_cnt[7] / prof_frames);
    for (i = 0; i < 8; i++) amiga_prof_cnt[i] = 0;
    amiga_prof_pixels = 0;
    amiga_prof_spans = 0;
    for (i = 0; i < PROF_SLOTS; i++)
    {
        ULONG per10;

        if (prof_sum[i] == 0)
            continue;
        // average per frame in 0.1 ms
        per10 = (prof_sum[i] / prof_frames) * 10000 / prof_freq;
        LbWarnLog("prof:   %-18s %4lu.%lu ms\n", prof_names[i] ? prof_names[i] : "?",
          per10 / 10, per10 % 10);
        prof_sum[i] = 0;
    }
    amiga_prof_item_report(prof_frames);
    amiga_prof_mode_report(prof_frames);
    prof_frames = 0;
    prof_report = now;
}

/* Per draw item type (DrawItem.Type) time and count */
#define PROF_ITEM_TYPES 40
static ULONG item_sum[PROF_ITEM_TYPES];
static ULONG item_cnt[PROF_ITEM_TYPES];
static ULONG item_start;

void amiga_prof_item_begin(void)
{
    if (!prof_fine_on())
        return;
    item_start = prof_now();
}

void amiga_prof_item_end(int type)
{
    if (!prof_fine_on())
        return;
    if ((type < 0) || (type >= PROF_ITEM_TYPES))
        type = 0;
    item_sum[type] += prof_now() - item_start;
    item_cnt[type]++;
}

void amiga_prof_item_report(ULONG frames)
{
    int i;

    for (i = 0; i < PROF_ITEM_TYPES; i++)
    {
        ULONG per10;

        if (item_cnt[i] == 0)
            continue;
        per10 = (item_sum[i] / frames) * 10000 / prof_freq;
        LbWarnLog("prof:   item type %2d: %4lu.%lu ms, %lu items/frame\n", i,
          per10 / 10, per10 % 10, item_cnt[i] / frames);
        item_sum[i] = 0;
        item_cnt[i] = 0;
    }
}

/* Per render mode (vec_mode) time of trig() and count of triangles */
#define PROF_MODES 32
static ULONG mode_sum[PROF_MODES];
static ULONG mode_cnt[PROF_MODES];
static ULONG mode_start;

void amiga_prof_mode_begin(void)
{
    if (!prof_fine_on())
        return;
    mode_start = prof_now();
}

void amiga_prof_mode_end(int mode)
{
    if (!prof_fine_on())
        return;
    if ((mode < 0) || (mode >= PROF_MODES))
        mode = 0;
    mode_sum[mode] += prof_now() - mode_start;
    mode_cnt[mode]++;
}

void amiga_prof_mode_report(ULONG frames)
{
    int i;

    for (i = 0; i < PROF_MODES; i++)
    {
        ULONG per10;

        if (mode_cnt[i] == 0)
            continue;
        per10 = (mode_sum[i] / frames) * 10000 / prof_freq;
        LbWarnLog("prof:   trig mode %2d: %4lu.%lu ms, %lu tris/frame\n", i,
          per10 / 10, per10 % 10, mode_cnt[i] / frames);
        mode_sum[i] = 0;
        mode_cnt[i] = 0;
    }
}

/** Render check mode: ENV:SyndWarsCheck exists. */
int amiga_check_enabled(void)
{
    char buf[4];
    int on = (GetVar((STRPTR)"SyndWarsCheck", (STRPTR)buf, sizeof(buf), 0) >= 0);
    LbWarnLog("render check mode %s\n", on ? "ON" : "off");
    return on;
}
