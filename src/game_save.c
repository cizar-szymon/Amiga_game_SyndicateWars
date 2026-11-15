/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file game_save.c
 *     Loading and saving the game and user profile.
 * @par Purpose:
 *     Functions for storing and loading game state data and settings.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     10 Feb 2024 - 02 May 2024
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "game_save.h"

#include "bffile.h"
#include "bfstrut.h"
#include "bfendian.h"
#include <assert.h>

#include "bfjoyst.h"

#include "campaign.h"
#include "display.h"
#include "engincam.h"
#include "febrief.h"
#include "femail.h"
#include "femain.h"
#include "game_data.h"
#include "game_options.h"
#include "game.h"
#include "keyboard.h"
#include "misstat.h"
#include "player.h"
#include "research.h"
#include "sound.h"
#include "swlog.h"
#include "fncheck.h"
#include "wrcities.h"

/******************************************************************************/
/* The save files are little endian (the PC's order) on every platform, so
 * that saves and settings go between the PC, the 68k Amiga and the Warp ARM
 * builds.  On a big endian CPU the fields are converted here: scalars by
 * their size, the save buffer by the layouts of its structs. */

/* layouts for lb_le_fix_struct() (tools/amiga/struct_layout.py; MissionStatus
 * with the single player member of its union, as the saves hold) */
#define SAVE_LAYOUT_AgentInfo     "32d32wd193b"
#define SAVE_LAYOUT_ResearchInfo  "640w64b2d4b4d"
#define SAVE_LAYOUT_PlayerInfo    "d2bw2b12wd2bw2b12wd2bw2b12wd2bw2b12w9d12bw2bw18b4d19w148b8w"
#define SAVE_LAYOUT_MissionStatus "4d6wdd4b"
#define SAVE_FMTVER_CURRENT 12

static void le_fix_scalar(void *p, ulong size)
{
    if (size == 2)
        lb_le_fix16_array(p, 1);
    else if (size == 4)
        lb_le_fix32_array(p, 1);
}

/** Reads a scalar (1, 2 or 4 bytes) stored little endian. */
static long file_read_le(TbFileHandle fh, void *p, ulong size)
{
    long n = LbFileRead(fh, p, size);
    le_fix_scalar(p, size);
    return n;
}

/** Writes a scalar (1, 2 or 4 bytes) little endian. */
static long file_write_le(TbFileHandle fh, const void *p, ulong size)
{
    u32 tmp = 0;
    if (size > sizeof(tmp))
        return LbFileWrite(fh, p, size);
    memcpy(&tmp, p, size);
    le_fix_scalar(&tmp, size);
    return LbFileWrite(fh, &tmp, size);
}

/** Reads an array of count values of esize (2 or 4) bytes, little endian. */
static long file_read_le_arr(TbFileHandle fh, void *p, ulong count, ulong esize)
{
    long n = LbFileRead(fh, p, count * esize);
    if (esize == 2)
        lb_le_fix16_array(p, count);
    else if (esize == 4)
        lb_le_fix32_array(p, count);
    return n;
}

/** Writes an array of count values of esize (2 or 4) bytes, little endian. */
static long file_write_le_arr(TbFileHandle fh, const void *p, ulong count, ulong esize)
{
#if LB_BIG_ENDIAN
    ubyte buf[256];
    const ubyte *src = (const ubyte *)p;
    ulong left = count * esize;
    long total = 0;

    while (left > 0) {
        ulong k = (left < sizeof(buf)) ? left : sizeof(buf);
        memcpy(buf, src, k);
        if (esize == 2)
            lb_le_fix16_array(buf, k / 2);
        else if (esize == 4)
            lb_le_fix32_array(buf, k / 4);
        total += LbFileWrite(fh, buf, k);
        src += k;
        left -= k;
    }
    return total;
#else
    return LbFileWrite(fh, p, count * esize);
#endif
}

/** Converts the save buffer (format SAVE_FMTVER_CURRENT) between the CPU's
 * order and little endian, in place (the same both ways); from_le says in
 * which order the counts inside it are now. */
static TbBool save_buf_fix_order(ubyte *buf, u32 len, TbBool from_le)
{
#if LB_BIG_ENDIAN
    u32 o = 0;
    int ne, nb, nm, nmis;

    lb_le_fix32_array(buf + o, 1); // Credits
    o += 4;
    lb_le_fix_struct(buf + o, 1, sizeof(struct AgentInfo), SAVE_LAYOUT_AgentInfo);
    o += sizeof(struct AgentInfo);
    lb_le_fix_struct(buf + o, 1, sizeof(struct ResearchInfo), SAVE_LAYOUT_ResearchInfo);
    o += sizeof(struct ResearchInfo);
    lb_le_fix_struct(buf + o, 1, sizeof(PlayerInfo), SAVE_LAYOUT_PlayerInfo);
    o += sizeof(PlayerInfo);
    o += 3 * sizeof(struct SynTime);
    ne = buf[o + 0];
    nb = buf[o + 2];
    nm = buf[o + 10];
    o += 12;
    lb_le_fix_struct(buf + o, 1, sizeof(struct MissionStatus), SAVE_LAYOUT_MissionStatus);
    o += sizeof(struct MissionStatus);
    o += sizeof(struct EmailItem) * (ne + nb + nm);
    lb_le_fix16_array(buf + o, 1); // ingame.MissionStatus
    o += 2;
    lb_le_fix16_array(buf + o, 2 * MISSION_STATE_SLOTS_COUNT); // mission_open, mission_state
    o += 4 * MISSION_STATE_SLOTS_COUNT;
    if (from_le)
        nmis = buf[o] | (buf[o + 1] << 8);
    else
        nmis = (buf[o] << 8) | buf[o + 1];
    lb_le_fix16_array(buf + o, 1); // next_mission
    o += 2;
    if (nmis > 1)
        o += (nmis - 1) * (sizeof(mission_list[0].SpecialTrigger) + 1);
    // login name, cities info, auto research: bytes
    if (o > len) {
        LOGERR("save buffer shorter (%lu) than its contents (%lu)", (ulong)len, (ulong)o);
        return false;
    }
#else
    (void)buf; (void)len; (void)from_le;
#endif
    return true;
}
/******************************************************************************/

/******************************************************************************/

ubyte *save_game_buffer = NULL;

extern ubyte save_crypto_tables_state[3];
extern ubyte save_crypto_data_state[3];

char login_name[16] = "";
char save_slot_names[SAVE_SLOTS_VISIBLE_COUNT][25] = {0};
char save_active_desc[25];
s32 save_slot_base = 0;
short save_slot = -1;

/******************************************************************************/

/* Key generator tables (in the asm data, one after another; each one
 * includes the long before the next one's label). */
extern u32 save_hashtbl_01[56];
extern u32 save_hashtbl_02[58];
extern u32 save_hashtbl_03[59];
extern u32 save_hashtbl_verify_code;

#ifdef SW_FNCHK
static void save_crypto_make_hashtable_asm(ubyte simple_salt)
{
    asm volatile ("call ASM_save_crypto_make_hashtable\n"
        : "+a" (simple_salt) : : "cc", "memory");
}

static ulong save_crypto_transform1_asm(ubyte a1)
{
    ulong ret;
    asm volatile ("call ASM_save_crypto_transform1\n"
        : "=a" (ret) : "0" (a1) : "cc", "memory");
    return ret;
}

static ulong save_crypto_transform2_asm(ubyte a1)
{
    ulong ret;
    asm volatile ("call ASM_save_crypto_transform2\n"
        : "=a" (ret) : "0" (a1) : "cc", "memory");
    return ret;
}

static ulong save_crypto_transform3_asm(ubyte a1)
{
    ulong ret;
    asm volatile ("call ASM_save_crypto_transform3\n"
        : "=a" (ret) : "0" (a1) : "cc", "memory");
    return ret;
}
#endif

/** Position `pos` moved back by `lag` in a table of `len` entries, done in
 * signed bytes as the original. */
static int save_crypto_lag_pos(ubyte pos, ubyte lag, ubyte len)
{
    sbyte p;

    p = (sbyte)(pos - lag);
    if (p < 0)
        p = (sbyte)(p + len);
    return p;
}

/** One step of an additive lagged generator: with `advance`, the entry at
 * the state position becomes the sum of the two lagged entries (their
 * positions taken from `lag_state`), the state moves on and `*p_carry`
 * tells if the sum wrapped; returns the entry at the state position. */
static u32 save_crypto_step(u32 *tbl, ubyte len, ubyte lag1, ubyte lag2,
  ubyte *p_state, ubyte lag_state, ubyte *p_carry, ubyte advance)
{
    ubyte pos;
    int a, b;
    u32 val;

    pos = *p_state;
    if (!advance)
        return tbl[pos];
    *p_carry = 0;
    a = save_crypto_lag_pos(lag_state, lag1, len);
    b = save_crypto_lag_pos(lag_state, lag2, len);
    tbl[pos] = tbl[a] + tbl[b];
    val = tbl[pos];
    pos++;
    if (pos > len - 1)
        pos = 0;
    *p_state = pos;
    if (val < tbl[a])
        *p_carry = 1;
    return val;
}

/** Remade from save_crypto_transform1. */
static ulong save_crypto_transform1_c(ubyte a1)
{
    return save_crypto_step(save_hashtbl_01, 56, 55, 24, &save_crypto_tables_state[0],
      save_crypto_tables_state[0], &save_crypto_data_state[0], a1);
}

/** Remade from save_crypto_transform2. */
static ulong save_crypto_transform2_c(ubyte a1)
{
    return save_crypto_step(save_hashtbl_02, 58, 57, 7, &save_crypto_tables_state[1],
      save_crypto_tables_state[1], &save_crypto_data_state[1], a1);
}

/** As the original, the lagged positions come from the state of the first
 * generator.  Remade from save_crypto_transform3. */
static ulong save_crypto_transform3_c(ubyte a1)
{
    return save_crypto_step(save_hashtbl_03, 59, 58, 19, &save_crypto_tables_state[2],
      save_crypto_tables_state[0], &save_crypto_data_state[2], a1);
}

/** Fills the three generator tables from the salt (or a fixed one) and
 * makes the first step of each.  Remade from save_crypto_make_hashtable. */
static void save_crypto_make_hashtable_c(ubyte simple_salt)
{
    u32 seed;
    int i;

    if (simple_salt == 0)
        seed = save_mortal_salt;
    else
        seed = 0x5E280A1D;
    for (i = 0; i < 56; i++) {
        seed *= 0xBB40E62D;
        save_hashtbl_01[i] = (seed >> 8) & 0xFFFF;
    }
    for (i = 0; i < 58; i++) {
        seed *= 0xBB40E62D;
        save_hashtbl_02[i] = (seed >> 8) & 0xFFFF;
    }
    for (i = 0; i < 59; i++) {
        seed *= 0xBB40E62D;
        save_hashtbl_03[i] = (seed >> 8) & 0xFFFF;
    }
    save_crypto_tables_state[2] = 0;
    save_crypto_tables_state[1] = 0;
    save_crypto_tables_state[0] = 0;
    save_hashtbl_verify_code = seed;
    save_crypto_transform1_c(1);
    save_crypto_transform2_c(1);
    save_crypto_transform3_c(1);
}

void save_crypto_make_hashtable(ubyte simple_salt)
{
    FNCHK_VOID("save_crypto_make_hashtable", save_crypto_make_hashtable_asm(simple_salt),
      save_crypto_make_hashtable_c(simple_salt));
}

ulong save_crypto_transform1(ubyte a1)
{
    ulong ret;
    FNCHK_RET("save_crypto_transform1", ret, save_crypto_transform1_asm(a1),
      save_crypto_transform1_c(a1));
    return ret;
}

ulong save_crypto_transform2(ubyte a1)
{
    ulong ret;
    FNCHK_RET("save_crypto_transform2", ret, save_crypto_transform2_asm(a1),
      save_crypto_transform2_c(a1));
    return ret;
}

ulong save_crypto_transform3(ubyte a1)
{
    ulong ret;
    FNCHK_RET("save_crypto_transform3", ret, save_crypto_transform3_asm(a1),
      save_crypto_transform3_c(a1));
    return ret;
}

u32 save_buffer_hash(ubyte *save_buf, u32 save_buf_len)
{
    u32 *cpos;
    u32 clen, hash;
    u32 i;

    if (save_buf_len & 3)
        clen = save_buf_len + 4;
    else
        clen = save_buf_len;
    clen >>= 2;
    cpos = (u32 *)save_game_buffer;

    hash = *cpos;
    for (i = 1; i < clen; i++) {
        cpos++;
        hash ^= *cpos;
    }

    return hash;
}

void save_crypto_docrypt(ubyte *save_buf, u32 save_buf_len)
{
    ubyte *gbpos;
    ubyte *gbend;

    gbpos = save_game_buffer;
    gbend = &save_game_buffer[4 * (save_buf_len >> 2)];
    while (gbpos < gbend)
    {
        u32 key, keysel;
        u32 *cryptpos;
        keysel = (save_crypto_data_state[1] << 8) | (save_crypto_data_state[0] << 16) | save_crypto_data_state[2];
        cryptpos = (u32 *)gbpos;
        switch (keysel)
        {
        case 0x00001:
        case 0x10100:
            key = save_crypto_transform1(1);
            key ^= save_crypto_transform2(1);
            key ^= save_crypto_transform3(0);
            break;
        case 0x00100:
        case 0x10001:
            key = save_crypto_transform1(1);
            key ^= save_crypto_transform2(0);
            key ^= save_crypto_transform3(1);
            break;
        case 0x00000:
        case 0x10101:
            key = save_crypto_transform1(1);
            key ^= save_crypto_transform2(1);
            key ^= save_crypto_transform3(1);
            break;
        case 0x00101:
        case 0x10000:
            key = save_crypto_transform1(0);
            key ^= save_crypto_transform2(1);
            key ^= save_crypto_transform3(1);
            break;
        default:
            key = 0;
            break;
        }
        gbpos += 4;
        // the key as the PC applies it, bytes in little endian order
        *cryptpos ^= lb_le32(key);
    }
}

TbBool save_game_decrypt_and_verify(u32 fmtver, TbBool simple_salt, ubyte *save_buf, u32 save_buf_len, u32 decrypt_verify)
{
    if (fmtver >= 9)
    {
        // Decrypt the file
        save_crypto_make_hashtable(simple_salt);
        save_crypto_docrypt(save_buf, save_buf_len);
    }

    // Verify data
    if (save_buffer_hash(save_buf, save_buf_len) != decrypt_verify)
        return false;

    return true;
}

u32 save_game_encrypt(u32 fmtver, TbBool simple_salt, ubyte *save_buf, u32 save_buf_len)
{
    u32 decrypt_verify;

    decrypt_verify = save_buffer_hash(save_buf, save_buf_len);

    save_crypto_make_hashtable(simple_salt);
    save_crypto_docrypt(save_buf, save_buf_len);

    return decrypt_verify;
}

void resave_salt_to_keys(void)
{
    char locstr[DISKPATH_SIZE];
    PathInfo *pinfo;
    TbFileHandle fh;
    int i;

    pinfo = &game_dirs[DirPlace_Savegame];
    snprintf(locstr, sizeof(locstr), "%s/keys.dat", pinfo->directory);
    fh = LbFileOpen(locstr, Lb_FILE_MODE_OLD);
    if (fh == INVALID_FILE)
    {
        return;
    }
    i = sizeof(save_mortal_salt);
    // Add salts at end, storing all salts ever used with this copy
    LbFileSeek(fh, 0, Lb_FILE_SEEK_END);
    file_write_le(fh, &save_mortal_salt, i);
    LbFileClose(fh);
}

TbResult reload_salt_from_keys(void)
{
    char locstr[DISKPATH_SIZE];
    PathInfo *pinfo;
    TbFileHandle fh;
    int i;

    pinfo = &game_dirs[DirPlace_Savegame];
    snprintf(locstr, sizeof(locstr), "%s/keys.dat", pinfo->directory);
    if (!LbFileExists(locstr))
    {
        return Lb_FAIL;
    }
    fh = LbFileOpen(locstr, Lb_FILE_MODE_READ_ONLY);
    if (fh == INVALID_FILE)
    {
        return Lb_FAIL;
    }
    i = sizeof(save_mortal_salt);
    LbFileSeek(fh, -i, Lb_FILE_SEEK_END);
    file_read_le(fh, &save_mortal_salt, i);
    LbFileClose(fh);
    return Lb_SUCCESS;
}

void apply_user_settings(void)
{
    apply_user_gfx_settings();
    apply_user_sfx_settings();
}

void set_default_user_settings(void)
{
    set_default_game_keys();
    set_default_player_control();
    set_default_sfx_settings();
    set_default_gfx_settings();
    set_default_visual_prefernces();
    set_default_audio_tracks();
    apply_user_settings();
}

void read_user_settings(void)
{
    char fname[DISKPATH_SIZE];
    TbFileHandle fh;
    u32 fmtver;
    TbBool read_mortal_salt_backup;
    int i;

    read_mortal_salt_backup = false;
    fh = INVALID_FILE;
    get_user_settings_fname(fname, login_name);
    if (login_name[0] == '\0')
        read_mortal_salt_backup = true;

    if (LbFileExists(fname))
        fh = LbFileOpen(fname, Lb_FILE_MODE_READ_ONLY);

    // Try default settings file instead
    if ((fh == INVALID_FILE) && (strlen(login_name) > 0))
    {
        get_user_settings_fname(fname, "");
        if (LbFileExists(fname))
            fh = LbFileOpen(fname, Lb_FILE_MODE_READ_ONLY);
        read_mortal_salt_backup = true;
    }

    if (fh == INVALID_FILE)
    {
        LOGSYNC_F("Build-in defaults used, as could not open \"%s\" file", fname);
        set_default_user_settings();
        read_mortal_salt_backup = true;
    } else
    {
        ushort locflags;

        assert(sizeof(locflags) == sizeof(ingame.UserFlags));

        if (LbFileLengthHandle(fh) > 126)
            file_read_le(fh, &fmtver, sizeof(u32));
        else
            fmtver = 0;

        if (fmtver == 0) {
            // In original game, the last key (agent 4 select) is not saved at all
            set_default_game_keys();
            ushort jskeys_old[23];
            file_read_le_arr(fh, kbkeys, 23, sizeof(ushort));
            file_read_le_arr(fh, jskeys_old, 23, sizeof(ushort));
            for (i = 0; i < 23; i++)
                jskeys[i] = jskeys_old[i];
        } else if (fmtver == 1) {
            set_default_game_keys();
            ushort jskeys_old[29];
            file_read_le_arr(fh, kbkeys, 29, sizeof(ushort));
            file_read_le_arr(fh, jskeys_old, 29, sizeof(ushort));
            for (i = 0; i < 29; i++)
                jskeys[i] = jskeys_old[i];
        } else if (fmtver == 2) {
            set_default_game_keys();
            ushort jskeys_old[50];
            file_read_le_arr(fh, kbkeys, 50, sizeof(ushort));
            file_read_le_arr(fh, jskeys_old, 50, sizeof(ushort));
            for (i = 0; i < 50; i++)
                jskeys[i] = jskeys_old[i];
        } else {
            assert(sizeof(JoyButtonSet) == 4);
            if (fmtver != 3)
                LOGWARN("Settings may be invalid, as \"%s\" has unrecognized format version %d", fname, (int)fmtver);
            file_read_le_arr(fh, kbkeys, GKey_KEYS_COUNT, sizeof(ushort));
            file_read_le_arr(fh, jskeys, GKey_KEYS_COUNT, sizeof(JoyButtonSet));
        }

        file_read_le(fh, &ctl_joystick_type, sizeof(ctl_joystick_type));
        LbFileRead(fh, &players[local_player_no].DoubleMode,
          sizeof(players[local_player_no].DoubleMode));
        for (i = 0; i != 4; i++)
        {
            PlayerInfo *p_locplayer;
            p_locplayer = &players[local_player_no];
            file_read_le(fh, &p_locplayer->UserInput[i].ControlMode,
              sizeof(p_locplayer->UserInput[i].ControlMode));
        }
        file_read_le(fh, &startscr_samplevol, sizeof(startscr_samplevol));
        file_read_le(fh, &startscr_midivol, sizeof(startscr_midivol));
        file_read_le(fh, &startscr_cdvolume, sizeof(startscr_cdvolume));
        file_read_le(fh, &game_gfx_advanced_lights, sizeof(game_gfx_advanced_lights));
        file_read_le(fh, &game_billboard_movies, sizeof(game_billboard_movies));
        file_read_le(fh, &game_gfx_deep_radar, sizeof(game_gfx_deep_radar));
        file_read_le(fh, &ingame.DetailLevel, sizeof(ingame.DetailLevel));
        file_read_le(fh, &game_high_resolution, sizeof(game_high_resolution));
        file_read_le(fh, &game_projector_speed, sizeof(game_projector_speed));
        file_read_le(fh, &game_perspective, sizeof(game_perspective));
        file_read_le(fh, &ingame.PanelPermutation, sizeof(ingame.PanelPermutation));
        file_read_le(fh, &ingame.TrenchcoatPreference, sizeof(ingame.TrenchcoatPreference));
        file_read_le(fh, &ingame.CDTrack, sizeof(ingame.CDTrack));
        file_read_le(fh, &ingame.DangerTrack, sizeof(ingame.DangerTrack));
        file_read_le(fh, &ingame.UseMultiMedia, sizeof(ingame.UseMultiMedia));
        file_read_le(fh, &locflags, sizeof(locflags));
        locflags &= ~UsrF_Cheats; // Cheats are per-session setting, not per-user
        ingame.UserFlags |= locflags; // Do not replace these, add to them
        file_read_le(fh, &save_mortal_salt, sizeof(save_mortal_salt));
        {   // speed options, added at the end: older files keep the defaults
            ubyte opt;
            if (file_read_le(fh, &opt, sizeof(opt)) == sizeof(opt) && (opt <= 3))
                game_far_pedestrians = opt;
            if (file_read_le(fh, &opt, sizeof(opt)) == sizeof(opt) && (opt <= 1))
                game_scanner_half_rate = opt;
            if (file_read_le(fh, &opt, sizeof(opt)) == sizeof(opt) && (opt <= 1))
                game_auto_detail = opt;
        }
        LbFileClose(fh);

        apply_user_settings();
    }

    if (read_mortal_salt_backup)
    {
        // Read mortal game encryption salt from backup
        reload_salt_from_keys();
    }

    i = -1;
    if (ctl_joystick_type)
        i = JoySetupDevice(&joy, ctl_joystick_type);
    if (i != 1)
        ctl_joystick_type = JTyp_NONE;
}

TbBool save_user_settings(void)
{
    char fname[DISKPATH_SIZE];
    TbFileHandle fh;
    u32 fmtver;
    int i;

    get_user_settings_fname(fname, login_name);
    fmtver = 3;

    fh = LbFileOpen(fname, Lb_FILE_MODE_NEW);
    if (fh == INVALID_FILE)
        return 1;

    file_write_le(fh, &fmtver, sizeof(fmtver));
    file_write_le_arr(fh, kbkeys, GKey_KEYS_COUNT, sizeof(ushort));
    file_write_le_arr(fh, jskeys, GKey_KEYS_COUNT, sizeof(JoyButtonSet));
    file_write_le(fh, &ctl_joystick_type, sizeof(ctl_joystick_type));
    LbFileWrite(fh, &players[local_player_no].DoubleMode,
      sizeof(players[local_player_no].DoubleMode));
    for (i = 0; i < 4; i++)
    {
        PlayerInfo *p_locplayer;
        p_locplayer = &players[local_player_no];
        file_write_le(fh, &p_locplayer->UserInput[i].ControlMode,
          sizeof(p_locplayer->UserInput[i].ControlMode));
    }
    file_write_le(fh, &startscr_samplevol, sizeof(startscr_samplevol));
    file_write_le(fh, &startscr_midivol, sizeof(startscr_midivol));
    file_write_le(fh, &startscr_cdvolume, sizeof(startscr_cdvolume));
    file_write_le(fh, &game_gfx_advanced_lights, sizeof(game_gfx_advanced_lights));
    file_write_le(fh, &game_billboard_movies, sizeof(game_billboard_movies));
    file_write_le(fh, &game_gfx_deep_radar, sizeof(game_gfx_deep_radar));
    file_write_le(fh, &ingame.DetailLevel, sizeof(ingame.DetailLevel));
    file_write_le(fh, &game_high_resolution, sizeof(game_high_resolution));
    file_write_le(fh, &game_projector_speed, sizeof(game_projector_speed));
    file_write_le(fh, &game_perspective, sizeof(game_perspective));
    file_write_le(fh, &ingame.PanelPermutation, sizeof(ingame.PanelPermutation));
    file_write_le(fh, &ingame.TrenchcoatPreference, sizeof(ingame.TrenchcoatPreference));
    file_write_le(fh, &ingame.CDTrack, sizeof(ingame.CDTrack));
    file_write_le(fh, &ingame.DangerTrack, sizeof(ingame.DangerTrack));
    file_write_le(fh, &ingame.UseMultiMedia, sizeof(ingame.UseMultiMedia));
    file_write_le(fh, &ingame.UserFlags, sizeof(ingame.UserFlags));
    file_write_le(fh, &save_mortal_salt, sizeof(save_mortal_salt));
    file_write_le(fh, &game_far_pedestrians, sizeof(game_far_pedestrians));
    file_write_le(fh, &game_scanner_half_rate, sizeof(game_scanner_half_rate));
    file_write_le(fh, &game_auto_detail, sizeof(game_auto_detail));
    LbFileClose(fh);
    return 0;
}

int save_game_write(ubyte slot, char *desc)
{
    char locstr[32];
    PlayerInfo *p_locplyr;
    TbFileHandle fh;
    u32 gblen, fmtver;
    u32 decrypt_verify;
    TbBool simple_salt;
    int i;

    fmtver = SAVE_FMTVER_CURRENT;
    gblen = 0;
    memcpy(&save_game_buffer[gblen], &ingame.Credits, sizeof(ingame.Credits));
    assert(sizeof(ingame.Credits) == 4);
    gblen += sizeof(ingame.Credits);

    memcpy(&save_game_buffer[gblen], &cryo_agents, sizeof(cryo_agents));
    assert(sizeof(cryo_agents) == 389);
    gblen += sizeof(cryo_agents);

    memcpy(&save_game_buffer[gblen], &research, sizeof(research));
    assert(sizeof(research) == 1372);
    gblen += sizeof(research);

    p_locplyr = &players[local_player_no];
    memcpy(&save_game_buffer[gblen], p_locplyr, sizeof(PlayerInfo));
    assert(sizeof(PlayerInfo) == 426);
    gblen += sizeof(PlayerInfo);

    memcpy(&save_game_buffer[gblen], &global_date, sizeof(global_date));
    assert(sizeof(global_date) == 5);
    gblen += sizeof(global_date);

    memcpy(&save_game_buffer[gblen], &research_curr_wep_date, sizeof(research_curr_wep_date));
    assert(sizeof(research_curr_wep_date) == 5);
    gblen += sizeof(research_curr_wep_date);

    memcpy(&save_game_buffer[gblen], &research_curr_mod_date, sizeof(research_curr_mod_date));
    assert(sizeof(research_curr_mod_date) == 5);
    gblen += sizeof(research_curr_mod_date);
    assert(gblen == 2206);

    save_game_buffer[gblen + 0] = next_email;
    save_game_buffer[gblen + 2] = next_brief;
    save_game_buffer[gblen + 4] = old_mission_brief;
    save_game_buffer[gblen + 6] = open_brief;
    save_game_buffer[gblen + 8] = next_ref;
    save_game_buffer[gblen + 10] = new_mail;
    save_game_buffer[gblen + 11] = background_type;
    gblen += 12;

    memcpy(&save_game_buffer[gblen], &mission_status[open_brief], sizeof(mission_status[0]));
    assert(sizeof(mission_status[0]) == 40);
    gblen += sizeof(mission_status[0]);

    memcpy(&save_game_buffer[gblen], email_store, sizeof(struct EmailItem) * next_email);
    assert(sizeof(struct EmailItem) == 5);
    gblen += sizeof(struct EmailItem) * next_email;
    memcpy(&save_game_buffer[gblen], brief_store, sizeof(struct EmailItem) * next_brief);
    gblen += sizeof(struct EmailItem) * next_brief;
    memcpy(&save_game_buffer[gblen], newmail_store, sizeof(struct EmailItem) * new_mail);
    gblen += sizeof(struct EmailItem) * new_mail;

    memcpy(&save_game_buffer[gblen], &ingame.MissionStatus, sizeof(ingame.MissionStatus));
    assert(sizeof(ingame.MissionStatus) == 2);
    gblen += sizeof(ingame.MissionStatus);

    memcpy(&save_game_buffer[gblen], mission_open, sizeof(mission_open));
    assert(sizeof(mission_open) == 2 * 50);
    gblen += sizeof(mission_open);

    memcpy(&save_game_buffer[gblen], mission_state, sizeof(mission_state));
    assert(sizeof(mission_state) == 2 * 50);
    gblen += sizeof(mission_state);

    memcpy(&save_game_buffer[gblen], &next_mission, sizeof(next_mission));
    assert(sizeof(next_mission) == 2);
    gblen += sizeof(next_mission);

    for (i = 1; i < next_mission; i++)
    {
        struct Mission *p_missi;
        p_missi = &mission_list[i];
        memcpy(&save_game_buffer[gblen], p_missi->SpecialTrigger, sizeof(p_missi->SpecialTrigger));
        gblen += sizeof(p_missi->SpecialTrigger);
        save_game_buffer[gblen] = p_missi->Complete;
        gblen += 1;
    }

    memcpy(&save_game_buffer[gblen], login_name, sizeof(login_name));
    gblen += sizeof(login_name);

    for (i = 0; i < num_cities; i++)
    {
        save_game_buffer[gblen] = cities[i].Info;
        gblen += 1;
    }

    save_game_buffer[gblen] = ingame.AutoResearch;
    gblen += 1;

    if (((ingame.Flags & GamF_MortalGame) != 0) || (desc[0] == '\0'))
    {
        struct Campaign *p_campgn;
        p_campgn = &campaigns[background_type];
        sprintf(desc, "%s %02d:%02d:%02d NC", p_campgn->TextName,
          global_date.Day, global_date.Month, global_date.Year);
        LbStringToUpper(desc);
    }

    while ((gblen & 7) != 0)
    {
        save_game_buffer[gblen] = 0;
        gblen += 1;
    }

    if ((ingame.Flags & GamF_MortalGame) != 0)
    {
        save_mortal_salt = time(0);
        resave_salt_to_keys();
    }

    simple_salt = ((ingame.Flags & GamF_MortalGame) == 0);
    // the PC's byte order in the file; the hash and the crypto work on
    // the bytes as they are then
    save_buf_fix_order(save_game_buffer, gblen, false);
    decrypt_verify = save_game_encrypt(fmtver, simple_salt, save_game_buffer, gblen);

    if ((ingame.Flags & GamF_MortalGame) != 0)
        sprintf(locstr, "qdata/savegame/synwarsm.sav");
    else if (slot >= 9)
        sprintf(locstr, "qdata/savegame/swars%03d.sav", slot - 1);
    else
        sprintf(locstr, "qdata/savegame/synwars%d.sav", slot - 1);

    fh = LbFileOpen(locstr, Lb_FILE_MODE_NEW);
    if (fh == INVALID_FILE) {
        return 1;
    }
    LbFileWrite(fh, desc, 25);
    file_write_le(fh, &gblen, sizeof(gblen));
    file_write_le(fh, &fmtver, sizeof(fmtver));
    LbFileWrite(fh, save_game_buffer, gblen);
    // the hash is a xor of the bytes in 4 lanes: stored as it is
    LbFileWrite(fh, &decrypt_verify, sizeof(decrypt_verify));
    LbFileClose(fh);

    save_user_settings();

    return 0;
}

void load_save_slot_names(void)
{
    char locstr[DISKPATH_SIZE];
    int i;

    for (i = 0; i < SAVE_SLOTS_VISIBLE_COUNT + 1; i++)
    {
        TbFileHandle fh;
        int slot;

        // the last slot is for mortal game
        if (i == SAVE_SLOTS_VISIBLE_COUNT)
            slot = 0;
        else
            slot = save_slot_base + i + 1;
        get_saved_game_fname(locstr, slot);

        if (!LbFileExists(locstr)) {
            save_slot_names[i][0] = '\0';
            continue;
        }

        fh = LbFileOpen(locstr, Lb_FILE_MODE_READ_ONLY);
        if (fh == INVALID_FILE) {
            save_slot_names[i][0] = '\0';
            continue;
        }

        if (LbFileRead(fh, save_slot_names[i], 25) != 25)
            save_slot_names[i][0] = '\0';
        LbFileClose(fh);
    }
}

ubyte load_game(int slot, char *desc)
{
    char locstr[DISKPATH_SIZE];
    u32 gblen, fmtver, decrypt_verify;
    TbFileHandle fh;
    TbBool ok;

    get_saved_game_fname(locstr, slot);

    fh = LbFileOpen(locstr, Lb_FILE_MODE_READ_ONLY);
    if (fh == INVALID_FILE)
        return 1;
    LbFileRead(fh, desc, 25);
    file_read_le(fh, &gblen, 4);
    file_read_le(fh, &fmtver, sizeof(u32));
    LbFileRead(fh, save_game_buffer, gblen);
    LbFileRead(fh, &decrypt_verify, 4);
    LbFileClose(fh);

    ok = save_game_decrypt_and_verify(fmtver, slot != 0, save_game_buffer, gblen, decrypt_verify);
    if (!ok) return 2;
#if LB_BIG_ENDIAN
    // older formats have parts of structs: not converted here
    if (fmtver != SAVE_FMTVER_CURRENT) {
        LOGERR("save format %lu cannot be read on this CPU, only %d", (ulong)fmtver,
          SAVE_FMTVER_CURRENT);
        return 2;
    }
#endif
    if (!save_buf_fix_order(save_game_buffer, gblen, true))
        return 2;

    gblen = 0;
    memcpy(&ingame.Credits, &save_game_buffer[gblen], sizeof(ingame.Credits));
    assert(sizeof(ingame.Credits) == 4);
    gblen += sizeof(ingame.Credits);

    if (fmtver >= 5)
    {
        ushort cryo_no;
        memcpy(&cryo_agents, &save_game_buffer[gblen], sizeof(struct AgentInfo));
        gblen += sizeof(struct AgentInfo);
        for (cryo_no = 0; cryo_no < CRYO_PODS_MAX_COUNT; cryo_no++)
        {
            // Remove bad mod flags
            cybmod_fix_all(&cryo_agents.Mods[cryo_no]);
            // Check weapons count, reset bad amounts of consumable weapons
            sanitize_weapon_quantities(&cryo_agents.Weapons[cryo_no],
              &cryo_agents.FourPacks[cryo_no]);
        }
    }
    else
    {
        ushort cryo_no;
        memcpy(&cryo_agents, &save_game_buffer[gblen], offsetof(struct AgentInfo, FourPacks));
        gblen += offsetof(struct AgentInfo, FourPacks);
        memcpy(&cryo_agents.NumAgents, &save_game_buffer[gblen], 1);
        gblen += 1;
        for (cryo_no = 0; cryo_no < CRYO_PODS_MAX_COUNT; cryo_no++)
        {
            // Remove bad mod flags
            cybmod_fix_all(&cryo_agents.Mods[cryo_no]);
            // Check weapons count, reset bad amounts of consumable weapons
            sanitize_weapon_quantities(&cryo_agents.Weapons[cryo_no],
              &cryo_agents.FourPacks[cryo_no]);
        }
    }

    if (fmtver >= 3)
    {
        assert(sizeof(struct ResearchInfo) == 1372);
        memcpy(&research, &save_game_buffer[gblen], sizeof(struct ResearchInfo));
        gblen += sizeof(struct ResearchInfo);
    }
    else
    {
        int i, k;
        // Old version has one byte progress
        for (i = 0; i < 32; i++)
        {
            for (k = 0; k < 10; k++)
            {
                research.WeaponProgress[i][k] = save_game_buffer[gblen];
                gblen++;
            }
        }
        for (i = 0; i < 32; i++)
        {
            for (k = 0; k < 10; k++)
            {
                research.ModProgress[i][k] = save_game_buffer[gblen];
                gblen++;
            }
        }
        i = sizeof(struct ResearchInfo) - offsetof(struct ResearchInfo, WeaponDaysDone);
        assert(i == 92);
        memcpy(research.WeaponDaysDone, &save_game_buffer[gblen], i);
        gblen += 732; // and not 92? we don't have old saves, so cannot verify.
    }

    if (fmtver >= 7)
    {
        PlayerInfo *p_locplayer;
        int i;
        assert(sizeof(PlayerInfo) == 426);
        p_locplayer = &players[local_player_no];
        memcpy(p_locplayer, &save_game_buffer[gblen], sizeof(PlayerInfo));
        gblen += sizeof(PlayerInfo);
        // Remove bad mod flags
        for (i = 0; i < 4; i++)
        {
            cybmod_fix_all(&p_locplayer->Mods[i]);
        }
    }
    else if (fmtver >= 4)
    {
        PlayerInfo *p_locplayer;
        int i;
        p_locplayer = &players[local_player_no];

        i = sizeof(PlayerInfo) - offsetof(PlayerInfo, WepDelays);
        assert(i == 282);
        memcpy(p_locplayer, &save_game_buffer[gblen], i);
        gblen += i;
        // The old struct matches until WepDelays
        i = 48;
        memcpy(p_locplayer->WepDelays, &save_game_buffer[gblen], i);
        gblen += i;
        // Remove bad mod flags
        for (i = 0; i < 4; i++)
        {
            cybmod_fix_all(&p_locplayer->Mods[i]);
        }
    }
    else
    {
        PlayerInfo *p_locplayer;
        int i;

        p_locplayer = &players[local_player_no];

        i = sizeof(PlayerInfo) - offsetof(PlayerInfo, FourPacks);
        assert(i == 262);
        memcpy(p_locplayer, &save_game_buffer[gblen], i);
        gblen += i;
        // The old struct matches until FourPacks
        i = 8;
        memcpy(p_locplayer->FourPacks, &save_game_buffer[gblen], i);
        gblen += i;

        for (i = 0; i < 4; i++)
        {
            // Remove bad mod flags
            cybmod_fix_all(&p_locplayer->Mods[i]);
            // Reset bad amounts of consumable weapons
            if (weapons_has_weapon(p_locplayer->Weapons[i], WEP_NUCLGREN))
                p_locplayer->FourPacks[WFRPK_NUCLGREN][i] = 1;
            if (weapons_has_weapon(p_locplayer->Weapons[i], WEP_ELEMINE))
                p_locplayer->FourPacks[WFRPK_ELEMINE][i] = 1;
            if (weapons_has_weapon(p_locplayer->Weapons[i], WEP_EXPLMINE))
                p_locplayer->FourPacks[WFRPK_EXPLMINE][i] = 1;
            if (weapons_has_weapon(p_locplayer->Weapons[i], WEP_KOGAS))
                p_locplayer->FourPacks[WFRPK_KOGAS][i] = 1;
            if (weapons_has_weapon(p_locplayer->Weapons[i], WEP_CRAZYGAS))
                p_locplayer->FourPacks[WFRPK_CRAZYGAS][i] = 1;
        }

        for (i = 0; i < LOCAL_USERS_MAX_COUNT; i++)
        {
            p_locplayer->UserVX[i] = 0;
            p_locplayer->UserVZ[i] = 0;
        }

        player_agents_clear_weapon_delays(local_player_no);
    }
    players_sync_from_cryo();

    memcpy(&global_date, &save_game_buffer[gblen], sizeof(struct SynTime));
    gblen += sizeof(struct SynTime);
    memcpy(&research_curr_wep_date, &save_game_buffer[gblen], sizeof(struct SynTime));
    gblen += sizeof(struct SynTime);
    memcpy(&research_curr_mod_date, &save_game_buffer[gblen], sizeof(struct SynTime));
    gblen += sizeof(struct SynTime);

    next_email = save_game_buffer[gblen + 0];
    next_brief = save_game_buffer[gblen + 2];
    old_mission_brief = save_game_buffer[gblen + 4];
    open_brief = save_game_buffer[gblen + 6];
    next_ref = save_game_buffer[gblen + 8];
    new_mail = save_game_buffer[gblen + 10];
    background_type = save_game_buffer[gblen + 11];
    gblen += 12;

    load_missions(background_type);

    if (fmtver >= 12)
    {
        int i;
        i = sizeof(struct MissionStatus);
        assert(i == 40);
        memcpy(&mission_status[open_brief], &save_game_buffer[gblen], i);
        gblen += i;
    }
    else if (fmtver >= 10)
    {
        struct MissionStatus *p_mistat;
        int i;
        i = sizeof(struct MissionStatus) - offsetof(struct MissionStatus, Expenditure);
        assert(i == 32);
        p_mistat = &mission_status[open_brief];
        memcpy(p_mistat, &save_game_buffer[gblen], i);
        gblen += i;
        gblen += 2;
        p_mistat->AgentsLost = save_game_buffer[gblen];
        gblen++;
        p_mistat->AgentsGained = save_game_buffer[gblen];
        gblen++;
    }
    else
    {
        // Mission status block did not existed in this version
    }

    memcpy(email_store, &save_game_buffer[gblen], 5 * next_email);
    gblen += 5 * next_email;
    memcpy(brief_store, &save_game_buffer[gblen], 5 * next_brief);
    gblen += 5 * next_brief;
    memcpy(newmail_store, &save_game_buffer[gblen], 5 * new_mail);
    gblen += 5 * new_mail;

    {
        int i;
        memcpy(&ingame.MissionStatus, &save_game_buffer[gblen], sizeof(ingame.MissionStatus));
        gblen += sizeof(ingame.MissionStatus);
        i = sizeof(mission_open);
        assert(i == 100);
        memcpy(mission_open, &save_game_buffer[gblen], i);
        gblen += i;
        i = sizeof(mission_state);
        assert(i == 100);
        memcpy(mission_state, &save_game_buffer[gblen], i);
        gblen += i;
    }

    if (fmtver < 8)
    {
        // Deprecated data
        gblen += 32;
    }

    if (fmtver >= 12)
    {
        memcpy(&next_mission, &save_game_buffer[gblen], sizeof(next_mission));
        gblen += 2;
    } else
    {
        next_mission = save_game_buffer[gblen];
        gblen += 2;
    }


    {
        int i;
        for (i = 1; i < next_mission; i++)
        {
            struct Mission *p_missi;
            p_missi = &mission_list[i];
            memcpy(p_missi->SpecialTrigger, &save_game_buffer[gblen], sizeof(p_missi->SpecialTrigger));
            gblen += sizeof(p_missi->SpecialTrigger);
            if (fmtver > 1)
            {
                p_missi->Complete = save_game_buffer[gblen];
                gblen++;
            }
        }
    }

    if (fmtver >= 6)
    {
        int i;
        i = 16;
        memcpy(login_name, &save_game_buffer[gblen], i);
        gblen += i;
    }
    else
    {
        if (login_name[0] == '\0')
          strcpy(login_name, "ANON");
    }

    if (fmtver >= 11)
    {
        int i;
        for (i = 0; i < num_cities; i++)
        {
            cities[i].Info = save_game_buffer[gblen];
            gblen++;
            recount_city_credit_reward(i);
        }
        ingame.AutoResearch = save_game_buffer[gblen];
        gblen++;
    }

    read_user_settings();
    login_control[0].Money = ingame.Credits;
    ingame.CashAtStart = ingame.Credits;
    ingame.Expenditure = 0;
    return 0;
}

/******************************************************************************/
