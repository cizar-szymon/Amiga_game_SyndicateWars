/* wlink.c - Warp ARM build: the record link to the 68k host (swlink.h). */
#include <stdint.h>
#include <string.h>
#include "WarpExtEnv.h"
#include "swlink.h"
#include "wlink.h"

const WarpExtEnv *wl_env;

static uint8_t in_buf[8192];
static uint32_t in_len;

static volatile int result_ready;
static int32_t result_val, result_extra;
/* frames sent and fetched by the 68k (FRAME / FRAME_DONE, in order) */
static uint32_t frames_sent, frames_done;
static uint32_t frame_fetch_us;
int wl_quit;

/* input events for swindows.c */
#define EVQ_SIZE 256
static WlEvent evq[EVQ_SIZE];
static unsigned evq_head, evq_tail;

void wl_init(const WarpExtEnv *env)
{
    wl_env = env;
    in_len = 0;
    evq_head = evq_tail = 0;
    result_ready = 0;
    frames_sent = frames_done = 0;
    wl_quit = 0;
}

static void send_all(const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    while (len) {
        uint32_t n = wl_env->send(p, len);
        p += n;
        len -= n;
        if (n == 0 && wl_env->stopRequested()) {
            wl_quit = 1;
            return;
        }
    }
}

void wl_send(unsigned type, const void *payload, unsigned len)
{
    static uint8_t rec[4 + SWL_REC_MAX + 4];
    unsigned padded;

    if (len > SWL_REC_MAX)
        len = SWL_REC_MAX;
    rec[0] = (uint8_t)type; rec[1] = (uint8_t)(type >> 8);
    rec[2] = (uint8_t)len;  rec[3] = (uint8_t)(len >> 8);
    memcpy(rec + 4, payload, len);
    padded = (len + 3) & ~3u;
    memset(rec + 4 + len, 0, padded - len);
    send_all(rec, 4 + padded);
}

void wl_send2(unsigned type, const void *a, unsigned alen, const void *b, unsigned blen)
{
    static uint8_t tmp[SWL_REC_MAX];
    if (alen > SWL_REC_MAX) alen = SWL_REC_MAX;
    if (alen + blen > SWL_REC_MAX) blen = SWL_REC_MAX - alen;
    memcpy(tmp, a, alen);
    memcpy(tmp + alen, b, blen);
    wl_send(type, tmp, alen + blen);
}

static int32_t rd32(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static int rd16s(const uint8_t *p)
{
    return (int16_t)(p[0] | (p[1] << 8));
}

static void dispatch(unsigned type, const uint8_t *p, unsigned len)
{
    WlEvent *e;

    switch (type) {
    case SWL_RESULT:
        result_val = len >= 4 ? rd32(p) : -1;
        result_extra = len >= 8 ? rd32(p + 4) : 0;
        result_ready = 1;
        break;
    case SWL_FRAME_DONE:
        frame_fetch_us = len >= 4 ? (uint32_t)rd32(p) : 0;
        frames_done++;
        break;
    case SWL_QUIT:
        wl_quit = 1;
        break;
    case SWL_KEY:
    case SWL_MOUSE:
    case SWL_ACTIVE:
        if (((evq_head + 1) % EVQ_SIZE) == evq_tail)
            break;  /* full: drop */
        e = &evq[evq_head];
        e->type = type;
        e->a = len >= 2 ? rd16s(p) : 0;
        e->b = len >= 4 ? rd16s(p + 2) : 0;
        e->c = len >= 6 ? rd16s(p + 4) : 0;
        evq_head = (evq_head + 1) % EVQ_SIZE;
        break;
    default:
        break;
    }
}

void wl_poll(void)
{
    uint32_t pos;

    if (wl_env->stopRequested())
        wl_quit = 1;
    for (;;) {
        uint32_t n = wl_env->recv(in_buf + in_len, sizeof(in_buf) - in_len);
        if (n == 0)
            break;
        in_len += n;
        if (in_len == sizeof(in_buf))
            break;
    }
    pos = 0;
    while (in_len - pos >= 4) {
        unsigned type = in_buf[pos] | (in_buf[pos + 1] << 8);
        unsigned len = in_buf[pos + 2] | (in_buf[pos + 3] << 8);
        unsigned total = 4 + ((len + 3) & ~3u);
        if (in_len - pos < total)
            break;
        dispatch(type, in_buf + pos + 4, len);
        pos += total;
    }
    if (pos) {
        memmove(in_buf, in_buf + pos, in_len - pos);
        in_len -= pos;
    }
}

int wl_request(unsigned type, const void *a, unsigned alen, const void *b, unsigned blen,
  int32_t *extra)
{
    result_ready = 0;
    wl_send2(type, a, alen, b, blen);
    while (!result_ready) {
        if (wl_quit && wl_env->stopRequested())
            return -1;
        wl_env->waitIn(50);
        wl_poll();
    }
    if (extra)
        *extra = result_extra;
    return result_val;
}

void wl_frame_send(uint32_t buf, unsigned w, unsigned h, uint32_t pitch)
{
    struct { uint16_t w, h; uint32_t pitch, buf; } fr;

    fr.w = (uint16_t)w;
    fr.h = (uint16_t)h;
    fr.pitch = pitch;
    fr.buf = buf;
    frames_sent++;
    wl_send(SWL_FRAME, &fr, sizeof(fr));
}

uint32_t wl_frames_wait(unsigned max_pending)
{
    while (frames_sent - frames_done > max_pending) {
        if (wl_env->stopRequested()) {
            wl_quit = 1;
            break;
        }
        wl_env->waitIn(50);
        wl_poll();
    }
    return frame_fetch_us;
}

int wl_next_event(WlEvent *e)
{
    if (evq_tail == evq_head)
        return 0;
    *e = evq[evq_tail];
    evq_tail = (evq_tail + 1) % EVQ_SIZE;
    return 1;
}

int wl_trace_mode;

void wl_log(const char *text)
{
    if (wl_trace_mode)
        wl_request(SWL_TRACE, text, (unsigned)strlen(text), NULL, 0, NULL);
    else
        wl_send(SWL_LOG, text, (unsigned)strlen(text));
}
