/******************************************************************************/
// Bullfrog Engine Emulation Library - AmigaOS backend
/******************************************************************************/
/** @file stime.c
 *     Date, time and timer functions for AmigaOS (timer.device).
 * @par Comment:
 *     LbTimerClock() uses ReadEClock() and only 32-bit arithmetic
 *     (no 64-bit MULS.L/DIVS.L, which the 68060 emulates in traps).
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include <time.h>
#include <exec/types.h>
#include <exec/io.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include "bftime.h"
#include "bfwindows.h"

struct Device *TimerBase;
static struct MsgPort *timer_port;
static struct timerequest *timer_req;
static ULONG eclock_freq;
static ULONG eclock_last_lo;
static ULONG eclock_rem;     /**< ticks of the current second, < eclock_freq */
static ULONG clock_sec;      /**< whole seconds since start */

static void amiga_timer_close(void)
{
    if (timer_req != NULL) {
        if (TimerBase != NULL)
            CloseDevice((struct IORequest *)timer_req);
        DeleteIORequest((struct IORequest *)timer_req);
        timer_req = NULL;
    }
    if (timer_port != NULL) {
        DeleteMsgPort(timer_port);
        timer_port = NULL;
    }
    TimerBase = NULL;
}

static int amiga_timer_open(void)
{
    struct EClockVal ev;

    if (TimerBase != NULL)
        return 1;
    timer_port = CreateMsgPort();
    if (timer_port == NULL)
        return 0;
    timer_req = (struct timerequest *)CreateIORequest(timer_port, sizeof(struct timerequest));
    if (timer_req == NULL) {
        amiga_timer_close();
        return 0;
    }
    if (OpenDevice((STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)timer_req, 0) != 0) {
        TimerBase = NULL;
        amiga_timer_close();
        return 0;
    }
    TimerBase = timer_req->tr_node.io_Device;
    eclock_freq = ReadEClock(&ev);
    eclock_last_lo = ev.ev_lo;
    eclock_rem = 0;
    clock_sec = 0;
    atexit(amiga_timer_close);
    return 1;
}

TbClockMSec LbTimerClock(void)
{
    struct EClockVal ev;
    ULONG delta;

    if (!amiga_timer_open())
        return 0;
    ReadEClock(&ev);
    // Only the low part is used: calls come much more often than once
    // per 2^32 ticks (over an hour).
    delta = ev.ev_lo - eclock_last_lo;
    eclock_last_lo = ev.ev_lo;
    eclock_rem += delta;
    if (eclock_rem >= eclock_freq) {
        ULONG sec = eclock_rem / eclock_freq;
        clock_sec += sec;
        eclock_rem -= sec * eclock_freq;
    }
    // eclock_rem * 1000 < 2^32 for E-clock frequencies below 4 MHz
    return clock_sec * 1000 + (eclock_rem * 1000) / eclock_freq;
}

static void LbI_SleepShort(TbClockMSec delay)
{
    if ((delay == 0) || !amiga_timer_open())
        return;
    timer_req->tr_node.io_Command = TR_ADDREQUEST;
    timer_req->tr_time.tv_secs = delay / 1000;
    timer_req->tr_time.tv_micro = (delay % 1000) * 1000;
    DoIO((struct IORequest *)timer_req);
}

TbBool LbSleepUntil(TbClockMSec endtime)
{
    TbClockMSec currclk;

    currclk = LbTimerClock();
    while ((currclk + LB_LARGE_DELAY_TIME) < endtime)
    {
        LbDoMultitasking();
        currclk = LbTimerClock();
    }
    if (currclk < endtime)
        LbI_SleepShort(endtime - currclk);
    return true;
}

TbResult LbDateTimeDecode(const time_t *datetime, struct TbDate *curr_date,
  struct TbTime *curr_time)
{
    struct tm *ltime;

    ltime = localtime(datetime);
    if (curr_date != NULL)
    {
        curr_date->Day = ltime->tm_mday;
        curr_date->Month = ltime->tm_mon + 1;
        curr_date->Year = 1900 + ltime->tm_year;
        curr_date->DayOfWeek = ltime->tm_wday;
    }
    if (curr_time != NULL)
    {
        curr_time->Hour = ltime->tm_hour;
        curr_time->Minute = ltime->tm_min;
        curr_time->Second = ltime->tm_sec;
        curr_time->HSecond = 0;
    }
    return Lb_SUCCESS;
}

TbResult LbTime(struct TbTime *curr_time)
{
    time_t dtime;
    time(&dtime);
    return LbDateTimeDecode(&dtime, NULL, curr_time);
}

TbTimeSec LbTimeSec(void)
{
    time_t dtime;
    time(&dtime);
    return dtime;
}

TbResult LbDate(struct TbDate *curr_date)
{
    time_t dtime;
    time(&dtime);
    return LbDateTimeDecode(&dtime, curr_date, NULL);
}

TbResult LbDateTime(struct TbDate *curr_date, struct TbTime *curr_time)
{
    time_t dtime;
    time(&dtime);
    return LbDateTimeDecode(&dtime, curr_date, curr_time);
}
/******************************************************************************/
