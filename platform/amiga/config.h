/* config.h for the AmigaOS m68k build (no autotools). */
#ifndef SW_AMIGA_CONFIG_H
#define SW_AMIGA_CONFIG_H

#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define STDC_HEADERS 1

#define GAME_DATA_PATH "PROGDIR:"

#define PACKAGE "syndwarsfx"
#define PACKAGE_BUGREPORT ""
#define PACKAGE_NAME "Syndicate Wars Fan Expansion"
#define PACKAGE_STRING "Syndicate Wars Fan Expansion 0.3.8.4599"
#define PACKAGE_TARNAME "syndwarsfx"
#define PACKAGE_URL ""
#define PACKAGE_VERSION "0.3.8.4599"
#define VERSION "0.3.8.4599"
#define VER_BUILD 4599
#define VER_MAJOR 0
#define VER_MINOR 3
#define VER_RELEASE 8

#define SW_AMIGA 1

/* Music: pre-converted PCM tracks (tools/amiga/make_music.py). */
#define MUSIC_TRACK_EXT "smu"

/* No text/binary distinction on AmigaDOS. */
#ifndef O_BINARY
#define O_BINARY 0
#endif

#endif
