/* Sound options of the 68k game from ENV: (the ARM game gets them on its
 * command line, platform/warp/snd_proxy.c). */
#include <exec/types.h>
#include <proto/dos.h>
#include "snd_engine.h"

int amiga_sound_backend_wanted(void)
{
    return se_backend_from_env();
}

/** MUSIC=OFF / ENV:SyndWarsMusic=OFF: no music, all voices for the
 * effects. */
int amiga_music_wanted(void)
{
    return se_music_on() ? 1 : 0;
}
