/* Sound options of the 68k game from ENV: (the ARM game gets them on its
 * command line, platform/warp/snd_proxy.c). */
#include <exec/types.h>
#include <proto/dos.h>
#include "snd_engine.h"

int amiga_sound_backend_wanted(void)
{
    return se_backend_from_env();
}

/** ENV:SyndWarsMusic=OFF: no music, all voices for the effects. */
int amiga_music_wanted(void)
{
    char buf[16];

    if ((GetVar((STRPTR)"SyndWarsMusic", (STRPTR)buf, sizeof(buf), 0) >= 0) &&
      se_env_is(buf, "OFF"))
        return 0;
    return 1;
}
