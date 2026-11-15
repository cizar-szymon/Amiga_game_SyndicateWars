/******************************************************************************/
// Syndicate Wars Fan Expansion, the Amiga port.
/******************************************************************************/
/** @file amiga_bugreport.c
 *     Bug reports from the game (Help key, the box in fepause.c): the
 *     screen, a short state of the game and the logs, sent to the game
 *     server of the Amiga ports (on the Pi400) with the player's e-mail
 *     address and words.
 * @par Purpose:
 *     Files of a report and its sending; the 68k build sends it by a
 *     process of its own (platform/amiga/amiga_report.c).
 * @author   The Amiga port
 * @date     15 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 3 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "amiga_bugreport.h"

#if defined(SW_AMIGA) && !defined(SW_WARP)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "bfscreen.h"
#include "bfpalette.h"
#include "bfiff.h"
#include "campaign.h"
#include "game.h"
#include "game_options.h"
#include "game_speed.h"
#include "swlog.h"
#include "../platform/amiga/amiga_report.h"

/* The game server of the Amiga ports (Settlers server/, on the Pi400);
 * ENV:SyndWarsReportServer="host[:port]" sets another. */
#define REPORT_HOST "83.12.152.135"
#define REPORT_PORT 6810
#define REPORT_VERSION "SyndWars-0.01"

#define REPORT_SCREEN "RAM:SyndWars-screen.iff"
#define REPORT_STATE  "RAM:SyndWars-state.txt"
#define REPORT_EMAIL  "qdata/savegame/report_email.txt"

extern const char amiga_version_tag[];
extern long lbSpeedMeter;

static char prog_dir[256];
static char file_log[300], file_ini[300];

static void find_prog_dir(void)
{
    if (prog_dir[0] != '\0')
        return;
    if (!NameFromLock(GetProgramDir(), (STRPTR)prog_dir, sizeof(prog_dir)))
        strcpy(prog_dir, "PROGDIR:");
    strcpy(file_log, prog_dir);
    AddPart((STRPTR)file_log, (STRPTR)"error.log", sizeof(file_log));
    strcpy(file_ini, prog_dir);
    AddPart((STRPTR)file_ini, (STRPTR)"qdata/savegame/default.ini", sizeof(file_ini));
}

static const char *cpu_name(void)
{
    UWORD f = SysBase->AttnFlags;
    if (f & 0x80) return "68060";          /* AFF_68060 */
    if (f & AFF_68040) return "68040";
    if (f & AFF_68030) return "68030";
    if (f & AFF_68020) return "68020";
    return "68000";
}

static void machine_text(char *buf, int size)
{
    snprintf(buf, size, "%s, chip %lu KB, fast %lu KB free, 68k build",
      cpu_name(), (ulong)(AvailMem(MEMF_CHIP) / 1024),
      (ulong)(AvailMem(MEMF_FAST) / 1024));
}

void bug_report_capture(void)
{
    ubyte pal[768];
    char mach[120];
    FILE *f;

    find_prog_dir();
    LbPaletteGet(pal);
    if (lbDisplay.WScreen != NULL)
        LbIffSave(REPORT_SCREEN, lbDisplay.WScreen, lbDisplay.GraphicsScreenWidth,
          lbDisplay.GraphicsScreenHeight, pal, true);

    machine_text(mach, sizeof(mach));
    f = fopen(REPORT_STATE, "w");
    if (f == NULL)
        return;
    fprintf(f, "version %s (%s), built %s %s\n", REPORT_VERSION,
      amiga_version_tag + 6, __DATE__, __TIME__);
    fprintf(f, "machine %s\n", mach);
    fprintf(f, "screen %dx%d, display mode %d, screen type %d\n",
      (int)lbDisplay.GraphicsScreenWidth, (int)lbDisplay.GraphicsScreenHeight,
      (int)ingame.DisplayMode, (int)screentype);
    fprintf(f, "mission %d \"%s\", status %d, game turn %lu, credits %ld\n",
      (int)ingame.CurrentMission, mission_name, (int)ingame.MissionStatus,
      (ulong)gameturn, (long)ingame.Credits);
    fprintf(f, "options: detail %d, far pedestrians %d, radar half rate %d, "
      "auto detail %d, show speed %d (now %ld%%)\n", (int)ingame.DetailLevel,
      (int)game_far_pedestrians, (int)game_scanner_half_rate,
      (int)game_auto_detail, (int)game_show_speed, lbSpeedMeter);
    fclose(f);
}

void bug_report_load_email(char *buf, int size)
{
    FILE *f;

    buf[0] = '\0';
    f = fopen(REPORT_EMAIL, "r");
    if (f == NULL)
        return;
    if (fgets(buf, size, f) == NULL)
        buf[0] = '\0';
    fclose(f);
    buf[strcspn(buf, "\r\n")] = '\0';
}

static void save_email(const char *email)
{
    FILE *f = fopen(REPORT_EMAIL, "w");
    if (f == NULL)
        return;
    fprintf(f, "%s\n", email);
    fclose(f);
}

int bug_report_start(const char *email, const char *text, char *why, int size)
{
    const char *files[4];
    char mach[120], host[64];
    int port = REPORT_PORT;

    static int quit_set;

    find_prog_dir();
    save_email(email);
    // a report still on its way when the game ends: waited for
    if (!quit_set) {
        atexit(bug_report_quit);
        quit_set = 1;
    }
    if (!report_network()) {
        snprintf(why, size, "NO TCP/IP STACK RUNNING");
        return 0;
    }
    strcpy(host, REPORT_HOST);
    if (GetVar((STRPTR)"SyndWarsReportServer", (STRPTR)host, sizeof(host), 0) > 0) {
        char *c = strchr(host, ':');
        if (c != NULL) {
            *c = '\0';
            port = atoi(c + 1);
        }
    }
    files[0] = REPORT_STATE;
    files[1] = REPORT_SCREEN;
    files[2] = file_log;
    files[3] = file_ini;
    machine_text(mach, sizeof(mach));
    LOGSYNC("bug report to %s:%d", host, port);
    if (!report_send(host, port, REPORT_VERSION, "Help", email, text, mach,
      files, 4)) {
        snprintf(why, size, "A REPORT IS STILL BEING SENT");
        return 0;
    }
    return 1;
}

int bug_report_poll(char *msg, int size, int *ok)
{
    return report_result(msg, size, ok);
}

void bug_report_quit(void)
{
    report_quit();
}

#endif
/******************************************************************************/
