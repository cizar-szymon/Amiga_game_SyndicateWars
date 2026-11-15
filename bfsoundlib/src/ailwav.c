/******************************************************************************/
// Bullfrog Sound Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file ailss.c
 *     OpenAL based reimplementation of MSS API.
 * @par Purpose:
 *     SS functions from MSS API.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     12 Jun 2022 - 05 Sep 2022
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include <assert.h>

#include "ailss.h"
#include "aildebug.h"
/******************************************************************************/

/** RIFF header, used, among others, in .WAV files */
typedef struct {
    char RIFF_string[4];
    uint32_t chunk_size;
    char ID_string[4];
    uint8_t data[1];
} RIFF;

/** .WAV PCM file format chunk */
typedef struct {
    char FMT_string[4];
    uint32_t  chunk_size;
    int16_t format_tag;
    int16_t channels;
    int32_t sample_rate;
    int32_t average_data_rate;
    int16_t alignment;
    int16_t bits_per_sample;
    int16_t extra;
    int16_t samples_per_block;
} WAVE_FMT;

enum WAVE_FORMAT_TAGS {
    WAVE_FORMAT_PCM = 0x0001,
    WAVE_FORMAT_IEEE_FLOAT = 0x0003,
    WAVE_FORMAT_ALAW = 0x0006,
    WAVE_FORMAT_MULAW = 0x0007,
    WAVE_FORMAT_EXTENSIBLE = 0xFFFE,
};

/** .WAV file data chunk */
typedef struct {
    char DATA_string[4];
    uint32_t chunk_size;
    uint8_t data[1];
} WAVE_DATA;

/** Terminate playback of .WAV file.
 *
 * Invoke application callback function, if any, and release the sample
 * allocated to play this file.
 * Used also as End-of-sample callback handler for .WAV file playback.
 */
void AIL_WAV_terminate(SNDSAMPLE *s)
{
    AILSAMPLECB cb;

    cb = (AILSAMPLECB)s->system_data[SmpSD_EOD_CALLBACK];
    if (cb != NULL)
        cb(s);

    if (s->system_data[SmpSD_RELEASE] > 0)
        AIL_release_sample_handle(s);

    s->system_data[SmpSD_RELEASE] = -1;
}

/** Little endian reads from the image: the WAV headers are little endian,
 * and the chunks need not be aligned. */
static uint32_t wav_rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t wav_rd16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

/** Finds the chunk with the given name after the RIFF header. */
static const uint8_t *wav_find_chunk(const uint8_t *image, const char *name)
{
    const uint8_t *c;

    c = image + 12; // after "RIFF", size, "WAVE"
    while (strncasecmp((const char *)c, name, 4) != 0)
    {
        uint32_t size = wav_rd32(c + 4);
        c += size + 8 + (size & 1);
    }
    return c;
}

void AIL_process_WAV_image(const uint8_t *image, SNDSAMPLE *s)
{
    const uint8_t *f, *d;
    uint16_t channels, bits;

    // Find mandatory <fmt-ck>; fields: tag +8, channels +10, rate +12, bits +22
    f = wav_find_chunk(image, "fmt ");
    assert(wav_rd16(f + 8) == WAVE_FORMAT_PCM);
    channels = wav_rd16(f + 10);
    bits = wav_rd16(f + 22);

    // Configure sample type and rate based on FMT chunk
    // 8-bit PCM is always unsigned, 16-bit is always signed
    if ((channels == 1) && (bits == 8))
    {
        AIL_set_sample_type(s, DIG_F_MONO_8, 0);
    }
    else if ((channels == 2) && (bits == 8))
    {
        AIL_set_sample_type(s, DIG_F_STEREO_8, 0);
    }
    else if ((channels == 1) && (bits == 16))
    {
        AIL_set_sample_type(s, DIG_F_MONO_16, DIG_PCM_SIGN);
    }
    else if ((channels == 2) && (bits == 16))
    {
        AIL_set_sample_type(s, DIG_F_STEREO_16, DIG_PCM_SIGN);
    }

    AIL_set_sample_playback_rate(s, wav_rd32(f + 12));

    // Find mandatory <data-ck>
    d = wav_find_chunk(image, "data");
    AIL_set_sample_address(s, (void *)(d + 8), wav_rd32(d + 4));
}

/******************************************************************************/
