/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file sound.c
 *     Sound related routines.
 * @par Purpose:
 *     Implements routines which help adjust functions from bfsoundlib to
 *     the games needs.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     22 Apr 2023 - 02 Nov 2025
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/

#include "sound.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bfwindows.h"
#include "bffile.h"
#include "bfmemory.h"
#include "bfmemut.h"
#include "bfmusic.h"
#include "bfsound.h"
#include "bfaudio.h"
#include "bfscd.h"
#include "ailss.h"
#include "aila.h"
#include "sb16.h"
#include "awe32.h"
#include "streamfx.h"
#include "sndtimer.h"
#include "snderr.h"
#include "ssampply.h"
#include "ssamplst.h"

#include "display.h"
#include "game_data.h"
#include "game_options.h"
#include "game_speed.h"
#include "game.h"
#include "thing.h"
#include "util.h"
#include "swlog.h"
#include "fncheck.h"
#include "engincam.h"
#include "engintrns.h"
#include "bfsvaribl.h"

/******************************************************************************/

#pragma pack(1)

struct HeapMgrHeader { // sizeof=0x24
    void *field_0;
    void *field_4;
    int field_8;
    int samples_count;
    int field_10;
    int field_14;
    int field_18;
    int field_1C;
    int field_20;
};

#pragma pack()

extern long sound_heap_size;
extern struct SampleTable *sound_heap_memory;
extern TbFileHandle sound_file; // = INVALID_FILE;
extern struct HeapMgrHeader *hmhead;
extern long samples_in_bank;
extern struct SampleTable *sample_table;

short startscr_samplevol;
short startscr_midivol;
short startscr_cdvolume;

/******************************************************************************/

void set_default_sfx_settings(void)
{
    startscr_samplevol = STARTSCR_VOLUME_MAX;
    startscr_midivol = STARTSCR_VOLUME_MAX;
    startscr_cdvolume = STARTSCR_VOLUME_MAX * 71 / 100;
}

void sfx_apply_samplevol(void)
{
    if (startscr_samplevol < 0)
        startscr_samplevol = 0;
    if (startscr_samplevol > STARTSCR_VOLUME_MAX)
        startscr_samplevol = STARTSCR_VOLUME_MAX;
    SetSoundMasterVolume(127 * startscr_samplevol / STARTSCR_VOLUME_MAX);
}

void sfx_apply_midivol(void)
{
    if (startscr_midivol < 0)
        startscr_midivol = 0;
    if (startscr_midivol > STARTSCR_VOLUME_MAX)
        startscr_midivol = STARTSCR_VOLUME_MAX;
    SetMusicMasterVolume(127 * startscr_midivol / STARTSCR_VOLUME_MAX);
}

void sfx_apply_cdvolume(void)
{
    if (startscr_cdvolume < 0)
        startscr_cdvolume = 0;
    if (startscr_cdvolume > STARTSCR_VOLUME_MAX)
        startscr_cdvolume = STARTSCR_VOLUME_MAX;
    SetCDVolume(70 * (127 * startscr_cdvolume / STARTSCR_VOLUME_MAX) / 100);
}


#ifdef SW_FNCHK
static struct SampleInfo *play_sample_using_heap_asm(ulong bank_id, short smptbl_id,
  ulong volume, ulong pan, ulong pitch, sbyte loop_count, ubyte stype)
{
    struct SampleInfo *ret;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[3];

    stkargs[0] = (int)(intptr_t)pitch;
    stkargs[1] = (int)(intptr_t)loop_count;
    stkargs[2] = (int)(intptr_t)stype;

    asm volatile (
      "push 8(%5)\n"
      "push 4(%5)\n"
      "push 0(%5)\n"
      "call ASM_play_sample_using_heap\n"
        : "=a" (ret), "+d" (smptbl_id), "+b" (volume), "+c" (pan)
        : "0" (bank_id), "S" (stkargs)
        : "cc", "memory");
    return ret;
}

static void stop_sample_using_heap_asm(long source_id, ulong sample_number)
{
    asm volatile (
      "call ASM_stop_sample_using_heap\n"
        : "+a" (source_id), "+d" (sample_number) : : "cc", "memory");
}
#endif

#pragma pack(1)

/** Item of the sample heap: one sample loaded into the heap memory. */
struct HeapMgrItem { // sizeof=0x1C
    ubyte *data;
    s32 size;
    ushort flags; /**< 1 in use, 2 and 4 locked (playing) */
    short smptbl_id;
    struct HeapMgrItem *prev; /**< by address */
    struct HeapMgrItem *next;
    struct HeapMgrItem *older; /**< by last use */
    struct HeapMgrItem *newer;
};

struct HeapMgrHead { // the HeapMgrHeader, with the items after it
    ubyte *data_start;
    ubyte *data_end;
    s32 data_size;
    s32 max_items;
    s32 n_items;
    s32 used_size;
    struct HeapMgrItem *first;
    struct HeapMgrItem *oldest;
    struct HeapMgrItem *newest;
    struct HeapMgrItem items[1];
};

#pragma pack()

/** Takes a free item for a sample of `size` bytes and places it in the
 * first gap between the loaded samples which is big enough; NULL when
 * there is no room.  Remade from heapmgr_add_item. */
static struct HeapMgrItem *heapmgr_add_item(struct HeapMgrHead *p_head, s32 size)
{
    struct HeapMgrItem *p_item, *p_cur;

    if (size > p_head->data_size - p_head->used_size)
        return NULL;
    if (p_head->n_items >= p_head->max_items)
        return NULL;
    p_item = &p_head->items[0];
    while (p_item->flags != 0)
        p_item++;
    if (p_head->first == NULL)
    {
        p_head->first = p_item;
        p_item->data = p_head->data_start;
    }
    else
    {
        p_cur = p_head->first;
        while (1)
        {
            ubyte *gap_end;

            if (p_cur->next != NULL)
                gap_end = p_cur->next->data;
            else
                gap_end = p_head->data_end;
            if (gap_end - (p_cur->data + p_cur->size) >= size)
                break;
            if (p_cur->next == NULL)
                return NULL;
            p_cur = p_cur->next;
        }
        p_item->prev = p_cur;
        p_item->data = p_cur->data + p_cur->size;
        p_item->next = p_cur->next;
        if (p_cur->next != NULL)
            p_cur->next->prev = p_item;
        p_cur->next = p_item;
    }
    p_item->flags = 1;
    p_item->size = size;
    p_head->used_size += size;
    p_head->n_items++;
    if (p_head->newest != NULL) {
        p_head->newest->newer = p_item;
        p_item->older = p_head->newest;
    } else {
        p_head->oldest = p_item;
    }
    p_head->newest = p_item;
    return p_item;
}

/** Moves the item to the newest end of the use list.
 * Remade from heapmgr_make_newest. */
static void heapmgr_make_newest(struct HeapMgrHead *p_head, struct HeapMgrItem *p_item)
{
    struct HeapMgrItem *p_newer, *p_older;

    p_newer = p_item->newer;
    if (p_newer == NULL)
        return;
    p_older = p_item->older;
    p_newer->older = p_older;
    if (p_older != NULL)
        p_older->newer = p_item->newer;
    else
        p_head->oldest = p_item->newer;
    p_head->newest->newer = p_item;
    p_item->newer = NULL;
    p_item->older = p_head->newest;
    p_head->newest = p_item;
}

/** Frees an item; the next sample is moved down to close the gap unless
 * it is locked.  Returns the freed item's sample index.
 * Remade from heapmgr_free_handle. */
static int heapmgr_free_handle(struct HeapMgrHead *p_head, struct HeapMgrItem *p_item)
{
    struct HeapMgrItem *p_next;
    int smptbl_id;

    if (p_item->prev != NULL)
        p_item->prev->next = p_item->next;
    else
        p_head->first = p_item->next;
    p_next = p_item->next;
    if (p_next != NULL)
    {
        p_next->prev = p_item->prev;
        if ((p_next->flags & 0x04) == 0)
        {
            if (p_item->prev != NULL) {
                memmove(p_item->prev->data + p_item->prev->size, p_next->data, p_next->size);
                p_next->data = p_item->prev->data + p_item->prev->size;
            } else {
                memmove(p_head->data_start, p_next->data, p_next->size);
                p_next->data = p_head->data_start;
            }
        }
    }
    if (p_item->older != NULL)
        p_item->older->newer = p_item->newer;
    else
        p_head->oldest = p_item->newer;
    if (p_item->newer != NULL)
        p_item->newer->older = p_item->older;
    else
        p_head->newest = p_item->older;
    p_head->n_items--;
    p_head->used_size -= p_item->size;
    smptbl_id = (ushort)p_item->smptbl_id;
    memset(p_item, 0, sizeof(struct HeapMgrItem));
    return smptbl_id;
}

/** Frees the least recently used sample which is not locked; returns its
 * sample index, -1 if none.  Remade from heapmgr_free_oldest. */
static int heapmgr_free_oldest(struct HeapMgrHead *p_head)
{
    struct HeapMgrItem *p_item;

    p_item = p_head->oldest;
    if (p_item == NULL)
        return -1;
    while ((p_item->flags & 0x02) != 0)
    {
        p_item = p_item->newer;
        if (p_item == NULL)
            return -1;
    }
    return heapmgr_free_handle(p_head, p_item);
}

/** Closes the first gap between samples which the next sample (if not
 * locked) can be moved into; one move per call.
 * Remade from heapmgr_defrag. */
static void heapmgr_defrag(struct HeapMgrHead *p_head)
{
    struct HeapMgrItem *p_cur, *p_next;

    if ((p_head == NULL) || (p_head->n_items == 0) || (p_head->first == NULL))
        return;
    for (p_cur = p_head->first; (p_next = p_cur->next) != NULL; p_cur = p_next)
    {
        if (p_cur->data + p_cur->size >= p_next->data)
            continue;
        if ((p_next->flags & 0x04) != 0)
            continue;
        memmove(p_cur->data + p_cur->size, p_next->data, p_next->size);
        p_next->data = p_cur->data + p_cur->size;
        return;
    }
}

/** Plays a sample of the sound bank: it is loaded into the heap if it is
 * not there (freeing old samples as needed), then played from there; the
 * heap item stays locked while it plays.
 * Remade from play_sample_using_heap. */
static struct SampleInfo *play_sample_using_heap_c(ulong bank_id, short smptbl_id,
  ulong volume, ulong pan, ulong pitch, sbyte loop_count, ubyte stype)
{
    struct HeapMgrHead *p_head;
    struct SampleTable *p_smpent;
    struct HeapMgrItem *p_item;
    struct SampleInfo *p_sinfo;

    if ((ingame.Flags & GamF_Unkn00020000) == 0)
        return NULL;
    if (sound_file == INVALID_FILE)
        return NULL;
    if ((smptbl_id <= 0) || (smptbl_id >= samples_in_bank))
        return NULL;
    p_head = (struct HeapMgrHead *)hmhead;
    p_smpent = &sample_table[smptbl_id];
    if (p_smpent->hmhandle == NULL)
    {
        s32 size;
        int tries;

        size = p_smpent->field_4;
        tries = 0;
        while (1)
        {
            int freed;

            p_item = heapmgr_add_item(p_head, size);
            if (p_item != NULL)
                break;
            freed = heapmgr_free_oldest(p_head);
            if (freed < 0)
                break;
            sample_table[freed].hmhandle = NULL;
            tries++;
            if (tries >= 50)
                break;
        }
        if (p_item != NULL)
        {
            p_item->smptbl_id = smptbl_id;
            LbFileSeek(sound_file, p_smpent->field_0, Lb_FILE_SEEK_BEGINNING);
            LbFileRead(sound_file, p_item->data, size);
        }
        p_smpent->hmhandle = (struct HeapMgrHandle *)p_item;
    }
    p_item = (struct HeapMgrItem *)p_smpent->hmhandle;
    if (p_item == NULL)
        return NULL;
    heapmgr_make_newest(p_head, p_item);
    p_sinfo = PlaySampleFromAddress(bank_id, smptbl_id, volume, pan, (ushort)pitch,
      loop_count, stype, p_item->data);
    if (p_sinfo != NULL) {
        p_sinfo->UserFlag |= 0x01;
        ((struct HeapMgrItem *)p_smpent->hmhandle)->flags |= 0x06;
    }
    return p_sinfo;
}

/** Stops a sample; when no other playing instance of it is left, its heap
 * item is unlocked.  As the original, the last sample info structure is
 * not looked at.  Remade from stop_sample_using_heap. */
static void stop_sample_using_heap_c(long source_id, ulong sample_number)
{
    struct SampleInfo *p_sinfo, *p_last;
    short smptbl_id;

    if ((ingame.Flags & GamF_Unkn00020000) == 0)
        return;
    smptbl_id = (short)sample_number;
    StopSample(source_id, smptbl_id);
    p_last = GetLastSampleInfoStructure();
    for (p_sinfo = GetFirstSampleInfoStructure(); p_sinfo < p_last; p_sinfo++)
    {
        if (p_sinfo->SampleNumber != smptbl_id)
            continue;
        if (p_sinfo->SampleHandle == NULL)
            continue;
        if ((p_sinfo->UserFlag & 0x01) == 0)
            continue;
        if (IsSamplePlaying(0, 0, p_sinfo->SampleHandle))
            return;
    }
    {
        struct HeapMgrItem *p_item;

        p_item = (struct HeapMgrItem *)sample_table[smptbl_id].hmhandle;
        if (p_item != NULL)
            p_item->flags &= ~0x06;
    }
}

struct SampleInfo *play_sample_using_heap(ulong bank_id, short smptbl_id,
  ulong volume, ulong pan, ulong pitch, sbyte loop_count, ubyte stype)
{
    struct SampleInfo *ret;
    FNCHK_RET("play_sample_using_heap", ret,
      play_sample_using_heap_asm(bank_id, smptbl_id, volume, pan, pitch, loop_count, stype),
      play_sample_using_heap_c(bank_id, smptbl_id, volume, pan, pitch, loop_count, stype));
    return ret;
}

void stop_sample_using_heap(long source_id, ulong sample_number)
{
    FNCHK_VOID("stop_sample_using_heap", stop_sample_using_heap_asm(source_id, sample_number),
      stop_sample_using_heap_c(source_id, sample_number));
}

int play_dist_speech(struct Thing *p_thing, ushort speech_no, ushort vol,
  ushort pan, int pitch, int loop, ubyte stype)
{
    if ((p_thing <= &things[0]) || (p_thing >= &things[THINGS_LIMIT])) {
        LOGERR("Speech %hu playback requested on invalid thing", speech_no);
        return -1;
    }
    play_dist_sample(p_thing, 129 + speech_no,  vol, pan, pitch, loop, stype);
    return 0;
}

void play_disk_sample(short id, ushort sample, short vol,
  short pan, int pitch, int loop, int stype)
{
    play_sample_using_heap(9999, 129 + sample, vol, pan, pitch, loop, stype);
}

#ifdef SW_FNCHK
static void play_dist_sample_asm(struct Thing *p_thing, ushort smptbl_id, ushort vol,
  ushort pan, int pitch, int loop, ubyte stype)
{
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[3];

    stkargs[0] = (int)(intptr_t)pitch;
    stkargs[1] = (int)(intptr_t)loop;
    stkargs[2] = (int)(intptr_t)stype;

    asm volatile (
      "push 8(%4)\n"
      "push 4(%4)\n"
      "push 0(%4)\n"
      "call ASM_play_dist_sample\n"
        : "+a" (p_thing), "+d" (smptbl_id), "+b" (vol), "+c" (pan) : "S" (stkargs)
        : "cc", "memory");
}
#endif

#ifdef SW_FNCHK
static void play_dist_ssample_asm(struct SimpleThing *p_sthing, ushort smptbl_id, ushort vol,
  ushort pan, int pitch, int loop, ubyte stype)
{
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[3];

    stkargs[0] = (int)(intptr_t)pitch;
    stkargs[1] = (int)(intptr_t)loop;
    stkargs[2] = (int)(intptr_t)stype;

    asm volatile (
      "push 8(%4)\n"
      "push 4(%4)\n"
      "push 0(%4)\n"
      "call ASM_play_dist_ssample\n"
        : "+a" (p_sthing), "+d" (smptbl_id), "+b" (vol), "+c" (pan) : "S" (stkargs)
        : "cc", "memory");
}
#endif

#ifdef SW_FNCHK
static void stop_looped_weapon_sample_asm(struct Thing *p_person, short weapon)
{
    asm volatile ("call ASM_stop_looped_weapon_sample\n"
        : "+a" (p_person), "+d" (weapon) : : "cc", "memory");
}
#endif

/** Stops the looped sound of a weapon (1, 2 or 8) being fired; for weapon
 * 2 it plays its end sound.  Agents use sounds by their player number.
 * Remade from stop_looped_weapon_sample. */
static void stop_looped_weapon_sample_c(struct Thing *p_person, short weapon)
{
    ushort plyr;

    if ((p_person->Flag2 & TgF2_Unkn0200) == 0)
        return;
    if ((p_person->Flag & TngF_PlayerAgent) != 0)
        plyr = p_person->U.UPerson.ComCur & 3;
    else
        plyr = 0;
    switch ((ushort)weapon)
    {
    case 1:
        ReleaseLoopedSample(p_person->ThingOffset, 0x50 + plyr);
        break;
    case 2:
        ReleaseLoopedSample(p_person->ThingOffset, 0x54 + plyr);
        play_dist_sample(p_person, 0x58 + (p_person->U.UPerson.ComCur & 3), 0x7F, 0x40, 100, 0, 1);
        break;
    case 8:
        ReleaseLoopedSample(p_person->ThingOffset, 0x0E);
        break;
    default:
        return;
    }
    p_person->Flag2 &= ~TgF2_Unkn0200;
}

void stop_looped_weapon_sample(struct Thing *p_person, short weapon)
{
    FNCHK_VOID("stop_looped_weapon_sample", stop_looped_weapon_sample_asm(p_person, weapon),
      stop_looped_weapon_sample_c(p_person, weapon));
}

#ifdef SW_FNCHK
static void fill_ail_sample_ids_asm(void)
{
    asm volatile ("call ASM_fill_ail_sample_ids\n"
        :  :  : "eax", "cc", "memory");
}
#endif

#ifdef SW_FNCHK
static void monitor_all_samples_asm(void)
{
    asm volatile ("call ASM_monitor_all_samples\n"
        :  :  : "eax", "cc", "memory");
}
#endif

#ifdef SW_FNCHK
static void snd_unkn1_volume_all_samples_asm(void)
{
    asm volatile ("call ASM_snd_unkn1_volume_all_samples\n"
        :  :  : "eax", "cc", "memory");
}
#endif

void wait_for_sound_sample_finish(ushort smpl_id)
{
    while (IsSamplePlaying(0, smpl_id, NULL)) {
        swap_wscreen();
        game_update();
    }
}

struct HeapMgrHeader *heapmgr_init(void *p_buf, int buf_size, int n_samples)
{
#if 0
    struct HeapMgrHeader *ret;
    asm volatile ("call ASM_heapmgr_init\n"
        : "=r" (ret) : "a" (p_buf), "d" (buf_size), "b" (n_samples));
    return ret;
#endif
    struct HeapMgrHeader *p_hmhead;
    uint offs_after_samples;
    int offs3;

    p_hmhead = (struct HeapMgrHeader *)p_buf;
    offs_after_samples = 0x1C * n_samples + sizeof(struct HeapMgrHeader);
    offs3 = buf_size - offs_after_samples;
    if (offs3 <= 0)
        return NULL;

    p_hmhead->field_10 = 0;
    p_hmhead->field_14 = 0;
    p_hmhead->field_18 = 0;
    p_hmhead->field_1C = 0;
    p_hmhead->field_20 = 0;
    p_hmhead->field_8 = offs3;
    p_hmhead->samples_count = n_samples;
    p_hmhead->field_0 = (ubyte *)p_hmhead + offs_after_samples;
    p_hmhead->field_4 = (ubyte *)p_hmhead + buf_size;
    return p_hmhead;
}

int setup_heap_manager(struct SampleTable *smptable, size_t smptb_len,
  const char *fname, ushort sndtype)
{
    TbFileHandle fh;
    ubyte tpno;
    long totlen;
    long pos;
    struct SampleTable *smptb;
    size_t tab_smptb_len;
    struct BfSoundBankHead *sbh;
    int i;
    long smptb_len_diff;
    ubyte *p_smptb_end;
    struct BfSoundBankHead sbharr[9];
    struct BfSfxInfo sfxi;
    ubyte unkhead1[18];

    LbMemorySet(smptable, 0, smptb_len);
    fh = LbFileOpen(fname, Lb_FILE_MODE_READ_ONLY);
    sound_file = fh;
    if (fh == INVALID_FILE) {
        reset_heaps();
        return 0;
    }
    tpno = GetSoundTpNo(sndtype);
    if (tpno >= 255) {
        reset_heaps();
        return 0;
    }

    totlen = LbFileLengthHandle(fh);
    LbFileSeek(fh, totlen - 4, Lb_FILE_SEEK_BEGINNING);
    LbFileRead(fh, &pos, 4);
    LbFileSeek(fh, pos, Lb_FILE_SEEK_BEGINNING);
    LbFileRead(fh, &unkhead1, 18);
    LbFileRead(fh, &sbharr, sizeof(struct BfSoundBankHead) * 9);
    sbh = &sbharr[tpno];
    if ((sbh->TabPos == 0) || (sbh->TabSize == 0)) {
        reset_heaps();
        return 0;
    }
    samples_in_bank = sbh->TabSize / sizeof(struct BfSfxInfo);
    tab_smptb_len = sizeof(struct SampleTable) * samples_in_bank;
    if (tab_smptb_len >= smptb_len) {
        reset_heaps();
        return 0;
    }
    LbFileSeek(fh, sbh->TabPos, Lb_FILE_SEEK_BEGINNING);
    smptb = smptable;
    for (i = 0; i < samples_in_bank; i++)
    {
        LbFileRead(fh, &sfxi, sizeof(struct BfSfxInfo));
        smptb->field_0 = sbh->DatPos + (long)sfxi.DataBeg;
        smptb->hmhandle = 0;
        smptb->field_4 = (long)sfxi.DataEnd;
        smptb++;
    }
    if (samples_in_bank <= 0) {
        reset_heaps();
        return 0;
    }
    sample_table = smptable;
    smptb_len_diff = smptb_len - tab_smptb_len;
    p_smptb_end = (ubyte *)smptable + tab_smptb_len;
    if (smptb_len_diff <= 0)
        return 0;
    hmhead = heapmgr_init(p_smptb_end, smptb_len_diff, samples_in_bank);
    if (hmhead == NULL) {
        reset_heaps();
        return 0;
    }
    reset_heaps();
    return 1;
}

void reset_heaps(void)
{
}

TbResult sound_banks_fname(char *fname, const char *dir, const char *lang)
{
    sprintf(fname, "%s/sound_%s.dat", dir, lang);
    if (LbFileExists(fname)) {
        return Lb_OK;
    }
    // If no language-specific version found, use the english as default
    sprintf(fname, "%s/sound_%s.dat", dir, "eng");
    if (LbFileExists(fname)) {
        return Lb_OK;
    }

    // As last effort, try the old file name from original game
    sprintf(fname, "%s/sound.dat", dir);
    if (LbFileExists(fname)) {
        return Lb_OK;
    }

    return Lb_FAIL;
}

void setup_heaps(short setup_cmd, const char *lang)
{
    char locstr[DISKPATH_SIZE];
    PathInfo *pinfo;
    long sz;
    unsigned int n;

    if ((ingame.Flags & GamF_Unkn00020000) == 0)
        return;
    pinfo = &game_dirs[DirPlace_Sound];

    switch (setup_cmd)
    {
    case SHSC_CreditsSnd:
        sprintf(locstr, "%s/syncreds.dat", pinfo->directory);
        sz = sound_heap_size;
        setup_heap_manager(sound_heap_memory, sz, locstr, 1622);
        break;
    case 99:
        break;
    case SHSC_GameSndAutoQ:
        sz = 1500000;
        while ((sound_heap_memory == NULL) && (sz > 100))
        {
            sound_heap_memory = LbMemoryAlloc(sz);
            if (sound_heap_memory == NULL)
                sz -= 30000;
        }
        if ((sound_heap_memory == NULL) || sz < 120000) {
            ingame.Flags &= ~GamF_Unkn00020000;
            break;
        }
        if (sz <= 500000)
            n = 822;
        else
            n = 1622;
        if (sound_banks_fname(locstr, pinfo->directory, lang) == Lb_FAIL) {
            LOGERR("Matching sound banks file not found.");
            ingame.Flags &= ~GamF_Unkn00020000;
            break;
        }
        if (!setup_heap_manager(sound_heap_memory, sz, locstr, n))
            ingame.Flags &= ~GamF_Unkn00020000;
        sound_heap_size = sz;
        break;
    case SHSC_GameSndBestQ:
        sz = 1500000;
        if (sound_banks_fname(locstr, pinfo->directory, lang) == Lb_FAIL) {
            LOGERR("Matching sound banks file not found.");
            ingame.Flags &= ~GamF_Unkn00020000;
            break;
        }
        if (!setup_heap_manager(sound_heap_memory, sz, locstr, 1622))
            ingame.Flags &= ~GamF_Unkn00020000;
        sound_heap_size = sz;
        break;
    case SHSC_ResetGameSnd:
    default:
        sz = 1500000;
        reset_heaps();
        LbMemoryFree(sound_heap_memory);
        sound_heap_size = sz;
        break;
    }
}

#ifdef SW_FNCHK
static void process_sound_heap_asm(void)
{
    asm volatile ("call ASM_process_sound_heap\n"
        :  :  : "eax", "cc", "memory");
}
#endif

/** Every frame: the heap items of samples which are still playing stay
 * locked, others are unlocked, and one gap in the heap is closed.
 * Remade from process_sound_heap. */
static void process_sound_heap_c(void)
{
    struct SampleInfo *p_sinfo, *p_last;
    int i;

    if ((ingame.Flags & GamF_Unkn00020000) == 0)
        return;
    for (i = 0; i < samples_in_bank; i++)
    {
        struct HeapMgrItem *p_item;

        p_item = (struct HeapMgrItem *)sample_table[i].hmhandle;
        if (p_item != NULL)
            p_item->flags &= ~0x06;
    }
    p_last = GetLastSampleInfoStructure();
    for (p_sinfo = GetFirstSampleInfoStructure(); p_sinfo <= p_last; p_sinfo++)
    {
        struct HeapMgrItem *p_item;

        if (p_sinfo->SampleHandle == NULL)
            continue;
        if ((p_sinfo->UserFlag & 0x01) == 0)
            continue;
        if (!IsSamplePlaying(0, 0, p_sinfo->SampleHandle)) {
            p_sinfo->UserFlag &= ~0x01;
            continue;
        }
        p_item = (struct HeapMgrItem *)sample_table[p_sinfo->SampleNumber].hmhandle;
        if (p_item != NULL)
            p_item->flags |= 0x06;
    }
    heapmgr_defrag((struct HeapMgrHead *)hmhead);
}

void process_sound_heap(void)
{
    FNCHK_VOID("process_sound_heap", process_sound_heap_asm(), process_sound_heap_c());
}

extern void *ail_sample_ids[20];
/** How many instances of each sample are playing (counted each frame). */
extern ubyte data_1dded8[400];

/** Stereo pan for a sound at camera relative X `rx` (after rotation). */
static ushort sound_pan_for(s32 rx)
{
    s32 pan;

    rx >>= 3;
    if (rx > 0x39C)
        pan = 0x6F;
    else if (rx < -0x39C)
        pan = 0x10;
    else
        pan = (rx >> 4) + 0x4A;
    if (pan < 0x10)
        pan = 0x10;
    if (pan > 0x6F)
        pan = 0x6F;
    return pan;
}

/** Volume of a sound `vol` at camera distance (dx,dz): full up to 0xA00,
 * nothing from 0x1A00. */
static s32 sound_dist_volume(s32 dx, s32 dz, s32 vol)
{
    s32 adx, adz, dist;

    adx = abs(dx);
    adz = abs(dz);
    if (adz >= adx)
        dist = adz + (adx >> 1);
    else
        dist = adx + (adz >> 1);
    if (dist < 0xA00)
        return (vol * 0x7F) >> 7;
    if (dist >= 0x1A00)
        return 0;
    return ((0x7F - ((dist - 0xA00) >> 5)) * vol) >> 7;
}

/** When too many instances of the sample play, stops the quietest one
 * quieter than `vol`; true if one was stopped.
 * Remade from snd_unkn3_find_stop_sample. */
static TbBool snd_unkn3_find_stop_sample(ushort smptbl_id, ushort vol)
{
    struct SampleInfo *p_sinfo, *p_last;
    short best_src, best_vol;

    best_src = 9999;
    best_vol = 9999;
    p_last = GetLastSampleInfoStructure();
    for (p_sinfo = GetFirstSampleInfoStructure(); p_sinfo <= p_last; p_sinfo++)
    {
        if (!IsSamplePlaying(0, 0, p_sinfo->SampleHandle))
            continue;
        if (p_sinfo->SampleNumber != (int)smptbl_id)
            continue;
        if ((s32)vol <= p_sinfo->SampleVolume)
            continue;
        if (best_vol <= p_sinfo->SampleVolume)
            continue;
        best_src = (short)p_sinfo->SourceID;
        best_vol = (short)p_sinfo->SampleVolume;
    }
    if (best_src == 9999)
        return false;
    stop_sample_using_heap(best_src, (short)smptbl_id);
    return true;
}

/** Plays a sample at a thing's place: pan and volume from the distance to
 * the camera; a sound too quiet to hear is not played, and when 3 or more
 * instances of the sample play a quieter one is stopped first. */
static void play_dist_sample_at(long source_id, s32 x, s32 z, ushort smptbl_id, ushort vol,
  int pitch, int loop, ubyte stype)
{
    s32 dx, dy, dz, rx, rz, v;
    ushort pan;

    dx = (x >> 8) - engn_xc;
    dz = (z >> 8) - engn_zc;
    rx = dx;
    rz = dz;
    dy = 0;
    local_to_worldr(&rx, &dy, &rz);
    pan = sound_pan_for(rx);
    v = sound_dist_volume(dx, dz, vol);
    if ((v <= 5) && (vol != 0))
        return;
    if ((data_1dded8[smptbl_id] > 2) && !snd_unkn3_find_stop_sample(smptbl_id, (ushort)v))
        return;
    play_sample_using_heap(source_id, (short)smptbl_id, v, pan, (ushort)pitch, (sbyte)loop, stype);
}

/** Remade from play_dist_sample. */
static void play_dist_sample_c(struct Thing *p_thing, ushort smptbl_id, ushort vol,
  ushort pan, int pitch, int loop, ubyte stype)
{
    if ((p_thing < things) || ((ubyte *)p_thing > (ubyte *)things + 0x29040))
        return;
    play_dist_sample_at((ushort)p_thing->ThingOffset, p_thing->X, p_thing->Z, smptbl_id, vol,
      pitch, loop, stype);
}

/** Remade from play_dist_ssample. */
static void play_dist_ssample_c(struct SimpleThing *p_sthing, ushort smptbl_id, ushort vol,
  ushort pan, int pitch, int loop, ubyte stype)
{
    if (((ubyte *)p_sthing < (ubyte *)sthings - 0x15F90) || (p_sthing > sthings))
        return;
    play_dist_sample_at(p_sthing->ThingOffset, p_sthing->X, p_sthing->Z, smptbl_id, vol,
      pitch, loop, stype);
}

void play_dist_sample(struct Thing *p_thing, ushort smptbl_id, ushort vol,
  ushort pan, int pitch, int loop, ubyte stype)
{
    FNCHK_VOID("play_dist_sample", play_dist_sample_asm(p_thing, smptbl_id, vol, pan, pitch, loop, stype),
      play_dist_sample_c(p_thing, smptbl_id, vol, pan, pitch, loop, stype));
}

void play_dist_ssample(struct SimpleThing *p_sthing, ushort smptbl_id, ushort vol,
  ushort pan, int pitch, int loop, ubyte stype)
{
    FNCHK_VOID("play_dist_ssample", play_dist_ssample_asm(p_sthing, smptbl_id, vol, pan, pitch, loop, stype),
      play_dist_ssample_c(p_sthing, smptbl_id, vol, pan, pitch, loop, stype));
}

/** The volume of a playing sample of a simple thing set from its distance
 * to the camera.  Remade from set_dist_volume. */
void sound_set_dist_volume(struct SimpleThing *p_sthing, ushort smptbl_id, ushort vol)
{
    s32 v;

    v = sound_dist_volume((p_sthing->X >> 8) - engn_xc, (p_sthing->Z >> 8) - engn_zc, vol);
    SetSampleVolume(p_sthing->ThingOffset, (short)smptbl_id, (short)v);
}

/** Remembers the handles of the sample info structures.
 * Remade from fill_ail_sample_ids. */
static void fill_ail_sample_ids_c(void)
{
    struct SampleInfo *p_sinfo, *p_last;
    ushort k;

    if (!GetSoundInstalled())
        return;
    p_last = GetLastSampleInfoStructure();
    k = 0;
    for (p_sinfo = GetFirstSampleInfoStructure(); p_sinfo <= p_last; p_sinfo++, k++)
        ail_sample_ids[k] = (void *)p_sinfo->SampleHandle;
}

/** Danger music, and all playing samples but three made silent.
 * Remade from snd_unkn1_volume_all_samples. */
static void snd_unkn1_volume_all_samples_c(void)
{
    struct SampleInfo *p_sinfo, *p_last;

    if ((ingame.Flags & GamF_Unkn00020000) == 0)
        return;
    update_danger_music(2);
    p_last = GetLastSampleInfoStructure();
    for (p_sinfo = GetFirstSampleInfoStructure(); p_sinfo <= p_last; p_sinfo++)
    {
        short smpl;

        if (!IsSamplePlaying(0, 0, p_sinfo->SampleHandle))
            continue;
        smpl = p_sinfo->SampleNumber;
        if ((smpl == 8) || (smpl == 0x4D) || (smpl == 0x4E))
            continue;
        SetSampleVolume((short)p_sinfo->SourceID, smpl, 0);
    }
}

/** Every frame: counts the playing instances of each sample, and sets the
 * volume and pan of each sample played by a thing from its distance to the
 * camera (and the pitch of a vehicle sound from its speed); quiet or too
 * many sounds are stopped.  Remade from monitor_all_samples. */
static void monitor_all_samples_c(void)
{
    struct SampleInfo *p_sinfo, *p_last;
    struct SimpleThing *p_sthing;
    ushort k;

    if ((ingame.Flags & GamF_Unkn00020000) == 0)
        return;
    memset(data_1dded8, 0, 0x18F);
    p_sthing = NULL;
    p_last = GetLastSampleInfoStructure();
    k = 0;
    for (p_sinfo = GetFirstSampleInfoStructure(); p_sinfo <= p_last; p_sinfo++, k++)
    {
        short src, smpl;
        s32 dx, dy, dz, rx, rz, vol;
        ushort pan;

        if ((void *)p_sinfo->SampleHandle != ail_sample_ids[k])
            continue;
        if (!IsSamplePlaying(0, 0, p_sinfo->SampleHandle))
            continue;
        src = (short)p_sinfo->SourceID;
        // things and simple things have the fields used here at the same place
        if (src > 0)
            p_sthing = (struct SimpleThing *)&things[src];
        else if (src < 0)
            p_sthing = &sthings[src];
        smpl = p_sinfo->SampleNumber;
        if (smpl != 9999)
            data_1dded8[smpl]++;
        if ((src == 0) || (src == 9999))
            continue;
        dx = (p_sthing->X >> 8) - engn_xc;
        dz = (p_sthing->Z >> 8) - engn_zc;
        rx = dx;
        rz = dz;
        dy = 0;
        local_to_worldr(&rx, &dy, &rz);
        pan = sound_pan_for(rx);
        vol = sound_dist_volume(dx, dz, 0x7F);
        if (((p_sthing->Flag & 0x0200) != 0) && ((smpl == 0x34) || (smpl == 7)))
            vol = 0;
        if ((vol < 5) || ((data_1dded8[smpl] > 3) && ((p_sthing->Flag & 0x2000) == 0)))
        {
            if (((smpl == 0x2C) || (smpl == 0x38)) && (data_1dded8[smpl] <= 3))
                continue;
            stop_sample_using_heap(src, smpl);
            continue;
        }
        if (vol > 0x7F)
            vol = 0x7F;
        if (p_sthing->Type == TT_VEHICLE) {
            struct Thing *p_vehicle = (struct Thing *)p_sthing;
            SetSamplePitch(src, smpl, (short)((p_vehicle->Speed >> 6) + 100));
            SetSampleVolume(src, smpl, (short)(vol >> 1));
        } else {
            SetSampleVolume(src, smpl, (short)vol);
        }
        SetSamplePan(src, smpl, pan);
    }
}

void fill_ail_sample_ids(void)
{
    FNCHK_VOID("fill_ail_sample_ids", fill_ail_sample_ids_asm(), fill_ail_sample_ids_c());
}

void monitor_all_samples(void)
{
    FNCHK_VOID("monitor_all_samples", monitor_all_samples_asm(), monitor_all_samples_c());
}

void snd_unkn1_volume_all_samples(void)
{
    FNCHK_VOID("snd_unkn1_volume_all_samples", snd_unkn1_volume_all_samples_asm(),
      snd_unkn1_volume_all_samples_c());
}

/******************************************************************************/
