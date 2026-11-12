/******************************************************************************/
// Bullfrog Engine Emulation Library - Warp ARM backend
/******************************************************************************/
/** @file stime.c
 *     Date, time and timer functions for the Warp ARM (core cycle counter).
 * @par Comment:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include <time.h>
#include <stdint.h>
#include "bftime.h"
#include "bfwindows.h"
#include <stdlib.h>
#include "privbflog.h"
#include "wlibc.h"
#include "wlink.h"

TbClockMSec LbTimerClock(void)
{
    return w_millis();
}

/** Time spent waiting for the next game turn, in core cycles / 64 (read
 * and reset by the profiler report: the work per turn is the rest). */
uint32_t w_idle_units;

TbBool LbSleepUntil(TbClockMSec endtime)
{
    TbClockMSec currclk;
    uint32_t c0 = w_cycles();

    currclk = LbTimerClock();
    if (endtime > currclk + 1000)
        LOGWARN("long sleep: now %lu until %lu", (unsigned long)currclk, (unsigned long)endtime);
    // as on the Amiga: 20 ms steps with the event handling (and the idle
    // presents) between them, the rest waited out
    while ((currclk + LB_LARGE_DELAY_TIME) < endtime)
    {
        LbDoMultitasking();
        currclk = LbTimerClock();
    }
    while (currclk < endtime)
    {
        wl_poll();
        currclk = LbTimerClock();
    }
    w_idle_units += (w_cycles() - c0) >> 6;
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
