/******************************************************************************/
// Bullfrog Sound Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file oggvorbis.h
 *     Header file for oggvorbis.c.
 * @par Purpose:
 *     Ogg/Vorbis audio files playback.
 * @par Comment:
 *     None.
 * @author   Gynvael Coldwind
 * @author   Unavowed
 * @date     12 Jan 2010 - 05 Jun 2023
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef OGGVORBIS_H_
#define OGGVORBIS_H_

#include <stdbool.h>
#ifdef SW_AMIGA
#include <stdint.h>
/* No OpenAL or Vorbis on the Amiga: platform/amiga/snd_paula.c plays
 * pre-converted PCM tracks with the same functions. */
typedef uint32_t ALuint;
#else
#include <vorbis/vorbisfile.h>
#include LBS_OPENAL_AL_H
#endif

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/

#define SOUND_MUSIC_BUFFERS   3

typedef struct OggVorbisStream OggVorbisStream;

#ifdef SW_AMIGA
struct OggVorbisStream
{
  ALuint	 source;
  bool		 playing;
  char		*file_name;
  float		 gain;
  void		*priv;
};
#else
struct OggVorbisStream
{
  ALuint	 source;
  OggVorbis_File file;
  vorbis_info	 info;
  bool		 playing;
  char		*file_name;
  ALuint	 buffers[SOUND_MUSIC_BUFFERS];
  size_t	 buffer_count;
};
#endif

bool ogg_vorbis_stream_init (OggVorbisStream *stream);
void ogg_vorbis_stream_free (OggVorbisStream *stream);
void ogg_vorbis_stream_pause (OggVorbisStream *stream);
void ogg_vorbis_stream_play (OggVorbisStream *stream);
void ogg_vorbis_stream_stop (OggVorbisStream *stream);
void ogg_vorbis_stream_clear (OggVorbisStream *stream);
bool ogg_vorbis_stream_open (OggVorbisStream *stream, const char *fname);
bool ogg_vorbis_stream_restart (OggVorbisStream *stream);
void ogg_vorbis_stream_set_gain (OggVorbisStream *stream, float gain);
float ogg_vorbis_stream_get_gain (OggVorbisStream *stream);
bool ogg_vorbis_stream_update (OggVorbisStream *stream);

/******************************************************************************/
#ifdef __cplusplus
};
#endif

#endif // OGGVORBIS_H_
