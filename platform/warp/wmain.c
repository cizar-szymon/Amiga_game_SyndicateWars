/* wmain.c - Warp ARM build: the extension's entry point.
 *
 * Sets up the heap (the arena), the file transfer buffer (render area,
 * SWL_BUF_XFER), the clock, runs the C++ constructors and calls the game's
 * main() with the command line from the 68k host.  The host's line is
 *   "prof=<0|1|2> time=<seconds since 1970> | <game arguments>"
 * exit() comes back here through w_exit_jump. */
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "WarpExtEnv.h"
#include "wlibc.h"
#include "wlink.h"

void __arm_relocate_pie_(void) { }   /* the firmware relocates */
void __arm_preinit_(void) { }        /* wanted by the AC 6.13 linker */

void w_heap_init(void *base, uint32_t size);
void w_heap_add(void *base, uint32_t size);
void w_files_init(uint8_t *xfer_buf);
void w_time_init(void);
extern uint32_t w_heap_used, w_heap_peak;
extern time_t w_start_time;

jmp_buf w_exit_jump;
int w_exit_code;
int w_prof_level;
/* release of the port (main menu, bug reports) */
#include "sw_version.h"
const char amiga_port_version[] = SW_PORT_VERSION;
/* ENV:SyndWarsCivLOD of the 68k side, -1 when not set (amiga_getenv_int) */
int w_civlod = -1;
int w_radar2 = -1;
int w_autodet = -1;
int w_showspeed = -1;
int w_bangtest = -1;
static int w_stage = 99;  /* diagnostics: "stage=N" returns after step N */
uint32_t w_cpu_hz;

int main(int argc, char **argv);

/* C++ constructors (bflibrary mouse handler and the like).  armlink writes
   .init_array entries as offsets relative to the entry itself (the C library
   start-up adds them up), so the loader leaves them alone and so do we. */
typedef void (*InitFn)(void);
extern const int32_t init_array_base[] __asm("SHT$$INIT_ARRAY$$Base");
extern const int32_t init_array_limit[] __asm("SHT$$INIT_ARRAY$$Limit");

static char cmdline[1024];
static char *argv_buf[32];

static int split_args(char *s, char **argv, int max)
{
    int argc = 0;
    argv[argc++] = (char *)"SyndWars";
    while (*s && argc < max - 1) {
        while (*s == ' ') s++;
        if (!*s) break;
        argv[argc++] = s;
        while (*s && *s != ' ') s++;
        if (*s) *s++ = 0;
    }
    argv[argc] = NULL;
    return argc;
}

int ext_main(const WarpExtEnv *env)
{
    uint8_t *xfer;
    char *game_args, *p;
    int argc;
    const int32_t *fn;
    char msg[160];

    env->log("swcore: entry\n");
    if (env->args && strstr(env->args, "stage=0"))
        return 100;
    wl_init(env);
    w_cpu_hz = env->cpuHz;
    w_time_init();

    /* render area: the file transfer buffer at its start */
    xfer = (uint8_t *)env->renderArena;
    env->bufRegister(SWL_BUF_XFER, xfer, SWL_XFER_BYTES);
    w_files_init(xfer);
    w_heap_init(env->arena, env->arenaSize);
    /* the render area holds the file buffer and WScreen (at most 2560x1441)
     * at its start and the sound heap (snd_proxy.c) at its end; what is
     * between is a second heap pool, used when the arena is full */
    {
        uint32_t lo = SWL_XFER_BYTES + WARP_RENDER_SCREEN_MAX;
        uint32_t hi = env->renderArenaSize - WARP_SOUND_HEAP_RESERVE;
        if (hi > lo + 65536)
            w_heap_add((uint8_t *)env->renderArena + lo, hi - lo);
    }

    strncpy(cmdline, env->args ? env->args : "", sizeof(cmdline) - 1);
    game_args = strchr(cmdline, '|');
    if (game_args) *game_args++ = 0;
    else game_args = cmdline + strlen(cmdline);
    if ((p = strstr(cmdline, "prof=")) != NULL) w_prof_level = p[5] - '0';
    if (strstr(cmdline, "trace=1") != NULL) wl_trace_mode = 1;
    if ((p = strstr(cmdline, "stage=")) != NULL) w_stage = p[6] - '0';
    if ((p = strstr(cmdline, "time=")) != NULL) w_start_time = (time_t)strtoul(p + 5, NULL, 10);
    if ((p = strstr(cmdline, "civlod=")) != NULL) w_civlod = p[7] - '0';
    if ((p = strstr(cmdline, "radar2=")) != NULL) w_radar2 = p[7] - '0';
    if ((p = strstr(cmdline, "autodet=")) != NULL) w_autodet = p[8] - '0';
    if ((p = strstr(cmdline, "showspeed=")) != NULL) w_showspeed = p[10] - '0';
    if ((p = strstr(cmdline, "bangtest=")) != NULL) w_bangtest = atoi(p + 9);
    argc = split_args(game_args, argv_buf, 32);

    {
        uint32_t hello[4] = { SWL_VERSION, env->cpuHz, env->arenaSize, env->renderArenaSize };
        wl_send(SWL_HELLO, hello, sizeof(hello));
    }
    snprintf(msg, sizeof(msg), "swcore: ARM %lu MHz, heap %lu KB, render %lu KB, prof %d, args \"%s\"\n",
      (unsigned long)(env->cpuHz / 1000000u), (unsigned long)(env->arenaSize / 1024u),
      (unsigned long)(env->renderArenaSize / 1024u), w_prof_level, game_args);
    wl_log(msg);
    if (w_stage <= 1)
        return 101;

    snprintf(msg, sizeof(msg), "swcore: %d constructors\n",(int)(init_array_limit - init_array_base));
    wl_log(msg);
    for (fn = init_array_base; fn < init_array_limit; fn++)
        ((InitFn)((uintptr_t)fn + (uintptr_t)*fn))();

    if (w_stage <= 2)
        return 102;
    if (setjmp(w_exit_jump) == 0)
        w_exit_code = main(argc, argv_buf);

    snprintf(msg, sizeof(msg), "swcore: exit %d, heap peak %lu KB\n", w_exit_code,
      (unsigned long)(w_heap_peak / 1024u));
    wl_log(msg);
    {
        int32_t code = w_exit_code;
        wl_send(SWL_EXIT, &code, 4);
    }
    return w_exit_code;
}
