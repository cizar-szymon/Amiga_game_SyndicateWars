/* snd_engine.h - the Amiga sound engine: Paula (audio.device) or AHI.
 *
 * Plays effects in slots (one per sound of the game) and a music track
 * streamed from disk.  Used by the game on the 68k (snd_bfs.c, through
 * bfsoundlib) and by swhost, the 68k host of the ARM game (commands from
 * the ARM).  No bfsoundlib types here; compiles with gcc and vbcc.
 *
 * The user of the engine provides se_ms(), a millisecond clock. */
#ifndef SND_ENGINE_H
#define SND_ENGINE_H

#if defined(SW_WARP)
/* the ARM proxy (platform/warp/snd_proxy.c): no AmigaOS headers there */
#include <stdint.h>
#ifndef SND_ENGINE_AMIGA_TYPES
#define SND_ENGINE_AMIGA_TYPES
typedef uint8_t UBYTE;
typedef int8_t BYTE;
typedef uint16_t UWORD;
typedef int16_t WORD;
typedef uint32_t ULONG;
typedef int32_t LONG;
typedef int16_t BOOL;
#endif
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#else
#include <exec/types.h>
#endif

#define SE_SLOTS 64

enum { SE_NONE = 0, SE_PAULA = 1, SE_AHI = 2 };

/** Millisecond clock (provided by the user). */
ULONG se_ms(void);

/** Opens the backend wanted (SE_AHI falls back to Paula); gives the one
 * opened, SE_NONE when none. */
int se_open(int want);
void se_close(void);
int se_backend(void);
/** SE_* from ENV:SyndWarsSound (PAULA default, AHI, NONE). */
int se_backend_from_env(void);
/** SOUND= / MUSIC= (Tool Types, command line) over the ENV: variables;
 * the strings are kept, not copied. */
void se_set_options(const char *sound, const char *music);
BOOL se_music_on(void);
/** TRUE when an ENV: value is the upper case word, in any case. */
BOOL se_env_is(const char *val, const char *word);

/** The sound heap: Chip RAM for Paula (at most 800 KB), Fast RAM for AHI
 * (1.5 MB, loaded as one AHI sound).  Effects play from it. */
ULONG se_heap_max(void);
void *se_heap_alloc(ULONG size);
void se_heap_free(void *ptr);

/** Starts an effect in a slot: 8-bit signed mono data (NULL: silent, it
 * just takes its time), rate in Hz, volume 0..64, pan 0..127 as the game
 * (0 and 64 middle, above 64 left), passes: 0 forever, else the number of
 * passes; pass_ms the length of one pass. */
void se_play(int slot, const void *data, ULONG len, LONG rate, UWORD vol,
    int pan, LONG passes, ULONG pass_ms);
void se_set(int slot, LONG rate, UWORD vol, int pan);
/** Stops at once. */
void se_stop(int slot);
/** A sound played forever ends after the pass which plays now. */
void se_release(int slot);
/** Services the slot; TRUE when its sound has ended (or nothing plays). */
BOOL se_done(int slot);

/** Music: a "SWMU" track (tools/amiga/make_music.py) on voices 0 and 1. */
BOOL se_music_open(const char *path);
void se_music_close(void);
/** Plays (volume 0..64; 0 frees the voices for the effects). */
void se_music_play(UWORD vol);
void se_music_pause(void);
void se_music_rewind(void);
/** Reads the next blocks as the voices need them; call often. */
void se_music_update(void);

/** With ENV:SyndWarsSoundLog, writes the counters to the file every 5 s. */
void se_log(const char *path);

#endif
