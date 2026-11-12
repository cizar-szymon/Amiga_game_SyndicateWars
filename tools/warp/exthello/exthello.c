/* exthello - 68k half (vbcc): loads hello.wxt on the Warp ARM through
 * cswarp.library, prints what the extension reports, echoes two lines,
 * fetches its 64 KB buffer with extFetch and checks it, then ends it.
 * usage: exthello [container]   (default PROGDIR:hello.wxt) */
#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <libraries/cswarp.h>
#include <proto/cswarp.h>
#include <string.h>
#include <stdio.h>

struct CSWarpBase *csWarpBase;

static CSWarpExtReq req;
static char rx[CSWEXT_XCHG_MAX + 1];

static void zreq(void)
{
    memset(&req, 0, sizeof(req));
    req.version = CSWARP_EXTREQ_VERSION;
}

/* receive until the text ends with endMark or ms runs out; prints it */
static int recv_until(const char *endMark, int ticks)
{
    static char acc[4096];
    ULONG len = 0;
    LONG rc;

    acc[0] = 0;
    while (ticks-- > 0) {
        zreq();
        req.recv = rx;
        req.recvMax = CSWEXT_XCHG_MAX;
        rc = extXchg(&req);
        if (rc != CSWEXT_OK) {
            printf("extXchg -> %ld (state %lu)\n", rc, req.state);
            return 0;
        }
        if (req.received) {
            if (len + req.received < sizeof(acc) - 1) {
                memcpy(acc + len, rx, req.received);
                len += req.received;
                acc[len] = 0;
            }
            if (strstr(acc, endMark)) {
                printf("%s", acc);
                return 1;
            }
            continue;
        }
        if (req.state != EXTST_RUNNING) {
            printf("%s[extension not running: state %lu, exit %ld]\n", acc, req.state, req.exitCode);
            return 0;
        }
        Delay(1);
    }
    printf("%s[timeout]\n", acc);
    return 0;
}

static int send_line(const char *s)
{
    LONG rc;
    zreq();
    req.send = s;
    req.sendLen = strlen(s);
    rc = extXchg(&req);
    if (rc != CSWEXT_OK || req.sent != req.sendLen) {
        printf("send -> %ld, sent %lu of %lu\n", rc, req.sent, req.sendLen);
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "PROGDIR:hello.wxt";
    BPTR f;
    LONG size, rc;
    UBYTE *wxt = NULL, *raw = NULL, *dst;
    int ret = 20;

    csWarpBase = (struct CSWarpBase *)OpenLibrary("cswarp.library", CSWARP_EXT_MIN_VER);
    if (!csWarpBase) { printf("no cswarp.library %d\n", CSWARP_EXT_MIN_VER); return 20; }
    {
        struct Library *l = (struct Library *)csWarpBase;
        printf("cswarp.library %d.%d\n", l->lib_Version, l->lib_Revision);
        if (l->lib_Version == CSWARP_EXT_MIN_VER && l->lib_Revision < CSWARP_EXT_MIN_REV) {
            printf("needs %d.%d\n", CSWARP_EXT_MIN_VER, CSWARP_EXT_MIN_REV);
            goto out;
        }
    }

    if (!(f = Open((STRPTR)path, MODE_OLDFILE))) { printf("cannot open %s\n", path); goto out; }
    Seek(f, 0, OFFSET_END);
    size = Seek(f, 0, OFFSET_BEGINNING);
    wxt = AllocVec(size, MEMF_ANY);
    if (!wxt || Read(f, wxt, size) != size) { Close(f); printf("cannot read %s\n", path); goto out; }
    Close(f);
    printf("container %s, %ld bytes\n", path, size);

    zreq();
    rc = extLoad(wxt, size, "hello from the 68k", 64, &req);
    printf("extLoad -> %ld: state %lu, exit %ld, window %lu KB, render %lu KB\n", rc, req.state,
      req.exitCode, req.windowBytes / 1024, req.renderBytes / 1024);
    if (rc != CSWEXT_OK)
        goto out;

    if (!recv_until("\n.\n", 500))
        goto stop;

    printf("--- echo\n");
    if (send_line("hello arm, this is the 68060\n")) recv_until("\n", 100);
    if (send_line("second line 12345\n")) recv_until("\n", 100);

    printf("--- extFetch\n");
    raw = AllocVec(65536 + 16, MEMF_ANY);
    if (raw) {
        ULONG i, bad = 0;
        dst = (UBYTE *)(((ULONG)raw + 15) & ~15UL);
        memset(dst, 0, 65536);
        zreq();
        rc = extFetch(0, 0, dst, 1024, 64, 1024, &req);
        for (i = 0; i < 65536; i++)
            if (dst[i] != (UBYTE)(i * 7 + (i >> 8))) bad++;
        printf("extFetch 64 KB -> %ld, board %lu us, %lu bad bytes\n", rc, req.boardUs, bad);
    }

    send_line("QUIT\n");
    recv_until("bye\n", 100);
    {
        int i;
        for (i = 0; i < 50; i++) {
            zreq();
            extXchg(&req);
            if (req.state != EXTST_RUNNING) break;
            Delay(2);
        }
        printf("end: state %lu, exit code %ld\n", req.state, req.exitCode);
        if (req.state == EXTST_EXITED && req.exitCode == 42) ret = 0;
    }
stop:
    zreq();
    extStop(1000, &req);
out:
    if (raw) FreeVec(raw);
    if (wxt) FreeVec(wxt);
    CloseLibrary((struct Library *)csWarpBase);
    printf(ret ? "RESULT: FAIL\n" : "RESULT: OK\n");
    return ret;
}
