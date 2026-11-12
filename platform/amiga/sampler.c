/* Amiga port: statistical profiler.
 *
 * With ENV:SyndWarsSample set, a process with priority above the game wakes
 * every millisecond (timer.device UNIT_MICROHZ) and copies the first
 * SMP_WORDS longwords from the saved context of the game task (tc_SPReg).
 * One of those slots is the interrupted PC; tools/amiga/samples.py finds it
 * and maps the PCs to functions with the symbol table of the binary.
 * The last SMP_MAX samples are kept (ring buffer) and written on exit to
 * RAM:SyndWars-samples.bin. Costs the game nothing when off. */
#include <string.h>
#include <stdlib.h>
#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/memory.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "bftypes.h"
#include "bflog.h"

#define SMP_WORDS 48
#define SMP_MAX 16000

static ULONG *smp_buf;
static volatile ULONG smp_count;
static volatile int smp_stop;
static volatile int smp_done;
static struct Task *smp_game;

int main(int argc, char **argv);
void amiga_prof_frame(void);

static void sampler_entry(void)
{
    struct MsgPort *port;
    struct timerequest *tr;

    port = CreateMsgPort();
    tr = port ? (struct timerequest *)CreateIORequest(port, sizeof(*tr)) : NULL;
    if ((tr != NULL) && (OpenDevice((STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)tr, 0) == 0))
    {
        while (!smp_stop)
        {
            ULONG *d;
            const ULONG *sp;
            int i;

            tr->tr_node.io_Command = TR_ADDREQUEST;
            tr->tr_time.tv_secs = 0;
            tr->tr_time.tv_micro = 1000;
            DoIO((struct IORequest *)tr);

            d = &smp_buf[(smp_count % SMP_MAX) * SMP_WORDS];
            Disable();
            sp = (const ULONG *)smp_game->tc_SPReg;
            for (i = 0; i < SMP_WORDS; i++)
                d[i] = sp[i];
            Enable();
            smp_count++;
        }
        CloseDevice((struct IORequest *)tr);
    }
    if (tr != NULL)
        DeleteIORequest((struct IORequest *)tr);
    if (port != NULL)
        DeleteMsgPort(port);
    Forbid(); // the process ends before anyone runs again
    smp_done = 1;
}

static void amiga_sampler_stop(void)
{
    BPTR fh;
    ULONG hdr[8];
    ULONG n, first;
    int i;

    if (smp_buf == NULL)
        return;
    smp_stop = 1;
    for (i = 0; (i < 100) && !smp_done; i++)
        Delay(2);
    if (!smp_done)
        return; // never free the buffer under a running sampler

    n = smp_count < SMP_MAX ? smp_count : SMP_MAX;
    first = smp_count < SMP_MAX ? 0 : smp_count % SMP_MAX;
    hdr[0] = 0x53574D50; // "SWMP"
    hdr[1] = n;
    hdr[2] = SMP_WORDS;
    hdr[3] = (ULONG)main;
    hdr[4] = (ULONG)amiga_prof_frame;
    hdr[5] = (ULONG)sampler_entry;
    hdr[6] = smp_count;
    hdr[7] = 0;
    fh = Open((STRPTR)"RAM:SyndWars-samples.bin", MODE_NEWFILE);
    if (fh != 0)
    {
        Write(fh, hdr, sizeof(hdr));
        // oldest first
        Write(fh, &smp_buf[first * SMP_WORDS], (n - first) * SMP_WORDS * 4);
        if (first > 0)
            Write(fh, smp_buf, first * SMP_WORDS * 4);
        Close(fh);
    }
    FreeVec(smp_buf);
    smp_buf = NULL;
}

void amiga_sampler_start(void)
{
    char buf[4];

    if (GetVar((STRPTR)"SyndWarsSample", (STRPTR)buf, sizeof(buf), 0) < 0)
        return;
    smp_buf = AllocVec(SMP_MAX * SMP_WORDS * 4, MEMF_ANY);
    if (smp_buf == NULL)
        return;
    smp_game = FindTask(NULL);
    if (CreateNewProcTags(NP_Entry, (ULONG)sampler_entry, NP_Name, (ULONG)"SyndWars sampler",
      NP_Priority, 5, NP_StackSize, 8192, TAG_END) == NULL)
    {
        FreeVec(smp_buf);
        smp_buf = NULL;
        return;
    }
    atexit(amiga_sampler_stop);
    LbWarnLog("sampler: on, main at 0x%08lx\n", (ulong)main);
}
