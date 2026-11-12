/* wlink.h - Warp ARM build: the record link to the 68k host (swlink.h). */
#ifndef WLINK_H
#define WLINK_H

#include <stdint.h>
#include "WarpExtEnv.h"
#include "swlink.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned type;      /* SWL_KEY, SWL_MOUSE, SWL_ACTIVE */
    int a, b, c;        /* KEY: code, qual; MOUSE: x, y, event; ACTIVE: flag */
} WlEvent;

extern const WarpExtEnv *wl_env;
extern int wl_quit;
/** "trace=1": every log line waits until the host has written it */
extern int wl_trace_mode;

void wl_init(const WarpExtEnv *env);
void wl_send(unsigned type, const void *payload, unsigned len);
void wl_send2(unsigned type, const void *a, unsigned alen, const void *b, unsigned blen);
/** Receives and dispatches what the 68k has sent; never blocks. */
void wl_poll(void);
/** Sends a request (a then b as payload) and waits for its RESULT. */
int wl_request(unsigned type, const void *a, unsigned alen, const void *b, unsigned blen,
  int32_t *extra);
void wl_frame_begin(void);
/** Waits for FRAME_DONE; returns the board's fetch time in us. */
uint32_t wl_frame_wait(void);
int wl_next_event(WlEvent *e);
void wl_log(const char *text);

#ifdef __cplusplus
}
#endif

#endif
