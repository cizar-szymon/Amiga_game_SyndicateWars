/* Amiga port: frame time profiler.
 * AMIGA_PROF_BEGIN(n) / AMIGA_PROF_END(n) (asm_coverage.h) add the E-clock
 * ticks between them to slot n. Every 5 s the average per presented frame
 * (in 0.1 ms) of each used slot goes to error.log. */
#include <exec/types.h>
#include <devices/timer.h>
#include <proto/timer.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/cia.h>
#include <resources/cia.h>
#include <hardware/cia.h>
#include <exec/interrupts.h>
#include <stdlib.h>
#include "bftypes.h"
#include "bflog.h"
#include "bftime.h"

extern struct Device *TimerBase;

#define PROF_SLOTS 64

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
    [33] = "gpoly_setup", [34] = "gpoly_raster", [35] = "th_lights", [36] = "th_things",
    [37] = "th_sthings", [38] = "th_people_intel",
    [39] = "th_vehicle", [40] = "th_person", [41] = "th_building",
    [42] = "p_danger", [43] = "p_weapon", [44] = "p_wander", [45] = "p_state",
    [46] = "person_move", [47] = "p_goto_point",
    [48] = "pm_other_people", [49] = "pm_colide_tile",
    [50] = "dt_building_vis", [51] = "dt_thing", [52] = "dt_sthing", [53] = "w_cond_cmd", [54] = "pm_ground_h", [55] = "p_lighting", [56] = "pm_top", [57] = "scn_coords", [58] = "scn_sthings",
};

static ULONG prof_start[PROF_SLOTS];
static ULONG prof_sum[PROF_SLOTS];
static ULONG prof_frames;
static TbClockMSec prof_report;
static ULONG prof_freq;
unsigned long amiga_prof_pixels, amiga_prof_spans, amiga_prof_gpoly_tris;
unsigned long amiga_prof_cnt[8];
unsigned long amiga_sim_cnt[8];

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

/* Profiling level from ENV:SyndWarsProf, read once at start:
 * unset - only the frame count and average frame time every 5 s (the ~30
 *   slots read the E-clock twice each, about 1 ms per frame);
 * "1" (or any value but "2") - slots 0..31;
 * "2" - also fine slots >= 32, draw item types and render modes (hundreds
 *   of E-clock reads per frame, several ms). */
int amiga_prof_level;

/* Level 3: sampling. A CIA-B timer interrupt (PROF_SAMPLE_HZ) adds one
 * sample to every slot open at that moment; the slot markers only keep a
 * stack of open slots, with no clock reads, so they cost next to nothing
 * and do not bend the shares as the E-clock reads of level 2 do. */
#define PROF_SAMPLE_HZ 4000
#define PROF_STACK 16
extern struct CIA ciab;
struct Library *CiaBBase;
static struct Interrupt prof_int;
static int prof_int_bit = -1;
static UWORD prof_latch;
static volatile UBYTE prof_stack[PROF_STACK];
static volatile int prof_depth;
static volatile ULONG prof_samples[PROF_SLOTS];

static void prof_sample_irq(void)
{
    int i, n;

    n = prof_depth;
    if (n > PROF_STACK)
        n = PROF_STACK;
    for (i = 0; i < n; i++)
        prof_samples[prof_stack[i]]++;
}

static void prof_sampling_stop(void)
{
    if (prof_int_bit < 0)
        return;
    AbleICR(CiaBBase, 1 << prof_int_bit);
    if (prof_int_bit == CIAICRB_TA)
        ciab.ciacra &= ~CIACRAF_START;
    else
        ciab.ciacrb &= ~CIACRBF_START;
    RemICRVector(CiaBBase, prof_int_bit, &prof_int);
    prof_int_bit = -1;
}

static int prof_sampling_start(void)
{
    struct EClockVal ev;
    ULONG freq;

    CiaBBase = (struct Library *)OpenResource((CONST_STRPTR)CIABNAME);
    if (CiaBBase == NULL)
        return 0;
    if (TimerBase == NULL)
        LbTimerClock();
    freq = (TimerBase != NULL) ? ReadEClock(&ev) : 709379;
    prof_freq = freq;
    prof_latch = (UWORD)(freq / PROF_SAMPLE_HZ);
    prof_int.is_Node.ln_Type = NT_INTERRUPT;
    prof_int.is_Node.ln_Name = (char *)"SyndWars prof";
    prof_int.is_Code = (void (*)(void))prof_sample_irq;
    prof_int.is_Data = NULL;
    if (AddICRVector(CiaBBase, CIAICRB_TB, &prof_int) == NULL) {
        prof_int_bit = CIAICRB_TB;
        ciab.ciacrb &= CIACRBF_ALARM;
        ciab.ciatblo = prof_latch & 0xFF;
        ciab.ciatbhi = prof_latch >> 8;
        ciab.ciacrb |= CIACRBF_LOAD | CIACRBF_START;
    } else if (AddICRVector(CiaBBase, CIAICRB_TA, &prof_int) == NULL) {
        prof_int_bit = CIAICRB_TA;
        ciab.ciacra &= ~(CIACRAF_RUNMODE | CIACRAF_INMODE | CIACRAF_START);
        ciab.ciatalo = prof_latch & 0xFF;
        ciab.ciatahi = prof_latch >> 8;
        ciab.ciacra |= CIACRAF_LOAD | CIACRAF_START;
    } else {
        return 0;
    }
    AbleICR(CiaBBase, (1 << prof_int_bit) | CIAICRF_SETCLR);
    atexit(prof_sampling_stop);
    return 1;
}

void amiga_prof_init(void)
{
    char buf[4];

    if (GetVar((STRPTR)"SyndWarsProf", (STRPTR)buf, sizeof(buf), 0) >= 0)
        amiga_prof_level = (buf[0] == '2') ? 2 : (buf[0] == '3') ? 3 : 1;
    else
        amiga_prof_level = 0;
    if ((amiga_prof_level == 3) && !prof_sampling_start()) {
        LbWarnLog("prof: no free CIA-B timer, level 1 instead\n");
        amiga_prof_level = 1;
    }
    LbWarnLog("prof: level %d\n", amiga_prof_level);
}

static int prof_fine_on(void)
{
    return (amiga_prof_level == 2);
}

void amiga_prof_begin(int slot)
{
    if (amiga_prof_level == 3) {
        if (prof_depth < PROF_STACK)
            prof_stack[prof_depth] = slot;
        prof_depth++;
        return;
    }
    if (amiga_prof_level < ((slot >= 32) ? 2 : 1))
        return;
    prof_start[slot] = prof_now();
}

void amiga_prof_end(int slot)
{
    if (amiga_prof_level == 3) {
        int i;

        /* back to the depth of this slot, also when an inner end was missed */
        for (i = ((prof_depth < PROF_STACK) ? prof_depth : PROF_STACK) - 1; i >= 0; i--) {
            if (prof_stack[i] == slot) {
                prof_depth = i;
                return;
            }
        }
        return;
    }
    if (amiga_prof_level < ((slot >= 32) ? 2 : 1))
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

    if (amiga_prof_level == 0)
        return; // no clock read, no log lines during normal play
    prof_frames++;
    now = LbTimerClock();
    if (prof_report == 0)
        prof_report = now;
    if (now - prof_report < 5000)
        return;
    {
        /* game turns in the same time: presents are also made while waiting
         * for the next turn (screen_idle_update), so frames >= turns */
        extern ulong gameturn;
        static ulong turn_last;

        LbWarnLog("prof: %lu frames, %lu game turns in %lu ms\n", prof_frames,
          gameturn - turn_last, (ulong)(now - prof_report));
        turn_last = gameturn;
    }
    if (amiga_prof_level == 0) {
        ULONG per10 = (ULONG)(now - prof_report) * 10 / prof_frames;

        LbWarnLog("prof:   frame (avg)        %4lu.%lu ms\n", per10 / 10, per10 % 10);
        prof_frames = 0;
        prof_report = now;
        return;
    }
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
    LbWarnLog("prof:   sim person %lu move %lu other %lu nodes %lu wander %lu cond %lu weapon %lu %lu\n",
      amiga_sim_cnt[0] / prof_frames, amiga_sim_cnt[1] / prof_frames,
      amiga_sim_cnt[2] / prof_frames, amiga_sim_cnt[3] / prof_frames,
      amiga_sim_cnt[4] / prof_frames, amiga_sim_cnt[5] / prof_frames,
      amiga_sim_cnt[6] / prof_frames, amiga_sim_cnt[7] / prof_frames);
    for (i = 0; i < 8; i++) amiga_sim_cnt[i] = 0;
    amiga_prof_pixels = 0;
    amiga_prof_spans = 0;
    if (amiga_prof_level == 3) {
        Disable();
        for (i = 0; i < PROF_SLOTS; i++) {
            prof_sum[i] = prof_samples[i] * prof_latch;
            prof_samples[i] = 0;
        }
        Enable();
    }
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

/** Integer value of an ENV: variable (first characters as a decimal number),
 * dflt when it is not set. */
int amiga_getenv_int(const char *name, int dflt)
{
    char buf[12];
    int v, i;

    if (GetVar((STRPTR)name, (STRPTR)buf, sizeof(buf), 0) < 0)
        return dflt;
    v = 0;
    for (i = 0; (buf[i] >= '0') && (buf[i] <= '9'); i++)
        v = v * 10 + (buf[i] - '0');
    return (i > 0) ? v : dflt;
}
