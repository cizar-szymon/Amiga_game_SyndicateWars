/* AmigaOS paths for game data and user files (counterpart of osunix.c). */
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>

/** Game data lives next to the program. */
bool sys_get_data_path(char *buffer, size_t size)
{
    snprintf(buffer, size, "PROGDIR:");
    return true;
}

/** Saves and settings also next to the program (qdata/savegame). */
bool sys_get_user_path(char *buffer, size_t size)
{
    snprintf(buffer, size, "PROGDIR:");
    return true;
}
