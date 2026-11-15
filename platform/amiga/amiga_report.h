/* Bug reports to the game server (platform/amiga/amiga_report.c). */
#ifndef AMIGA_REPORT_H
#define AMIGA_REPORT_H

/* Starts sending a report in a process of its own: the fields and up to 6
   files (missing ones are left out). 0 if one is still being sent. */
int report_send(const char *host, int port, const char *version,
                const char *kind, const char *email, const char *text,
                const char *machine, const char *const *files, int nfiles);
/* 0 while it goes on; 1 when done: text is the report's number (ok) or
   why it failed. */
int report_result(char *text, int size, int *ok);
/* The TCP/IP stack running (its bsdsocket.library in memory). */
int report_network(void);
/* Waits for a report being sent to stop; at the end of the program. */
void report_quit(void);

#endif
