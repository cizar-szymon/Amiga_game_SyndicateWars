/* Amiga port bring-up: log the first game loop phase changes (and then
 * every 500th) to error.log, to see where a run gets stuck. Each of the
 * first entries also checks the map `mapwho` heads for invalid indexes. */
#include "bftypes.h"
#include "bflog.h"
#include "bigmap.h"
#include "thing.h"

extern int amiga_prof_level;

static int count_bad_mapwho(int *first)
{
    int i, bad;

    bad = 0;
    *first = -1;
    if (game_my_big_map == NULL)
        return -1;
    for (i = 0; i < MAP_TILE_WIDTH * MAP_TILE_HEIGHT; i++) {
        short c = game_my_big_map[i].Child;
        if ((c >= THINGS_LIMIT) || (c <= -STHINGS_LIMIT)) {
            if (*first < 0)
                *first = i;
            bad++;
        }
    }
    return bad;
}

void amiga_phase_trace(int phase)
{
    static unsigned long n;
    int bad, first;

    n++;
    if (n <= 60) {
        bad = count_bad_mapwho(&first);
        LbWarnLog("phase %d (call %lu), bad mapwho %d first %d\n", phase, n, bad, first);
    } else if (((n % 500) == 0) && amiga_prof_level) {
        // each log line opens and closes error.log - not during normal play
        LbWarnLog("phase %d (call %lu)\n", phase, n);
    }
}
