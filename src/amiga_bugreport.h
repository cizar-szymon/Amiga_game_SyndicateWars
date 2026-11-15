/******************************************************************************/
// Syndicate Wars Fan Expansion, the Amiga port.
/******************************************************************************/
/** @file amiga_bugreport.h
 *     Header file for amiga_bugreport.c: bug reports (Help key).
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 3 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef AMIGA_BUGREPORT_H
#define AMIGA_BUGREPORT_H

#ifdef __cplusplus
extern "C" {
#endif

/** Writes the files of a report: the screen (before the box is drawn) and
 * a short state of the game. */
void bug_report_capture(void);
/** The e-mail address given last time, or "". */
void bug_report_load_email(char *buf, int size);
/** Starts sending; 0 and why when it cannot. */
int bug_report_start(const char *email, const char *text, char *why, int size);
/** 0 while it is sent; 1 when done: msg is the report's number (ok) or
 * why it failed. */
int bug_report_poll(char *msg, int size, int *ok);
/** At the end of the game: waits for a report being sent. */
void bug_report_quit(void);

#ifdef __cplusplus
}
#endif
#endif
