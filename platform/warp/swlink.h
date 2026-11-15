/* swlink.h - the link between Syndicate Wars on the Warp ARM (swcore.wxt,
 * platform/warp) and its 68k host (tools/warp/swhost.c).
 *
 * Records over the two byte rings of the extension link (WarpExtEnv
 * send/recv on the ARM, cswarp.library extXchg on the 68k):
 *   uint16 type, uint16 len (payload bytes), payload, padded to 4 bytes.
 * All numbers are LITTLE-ENDIAN (the ARM's order); the 68k swaps.
 *
 * Bulk data does not go through the rings:
 *   frames:  the ARM registers its screen buffer as SWL_BUF_SCREEN and sends
 *            FRAME; the 68k extFetch'es it straight into the screen bitmap and
 *            answers FRAME_DONE (the ARM waits for it before drawing on).
 *   reads:   the ARM sends FREAD; the 68k reads the file and extFeed's the
 *            bytes (padded to 16) into SWL_BUF_XFER, then answers RESULT.
 *   sounds: the 68k does all of it (tools/warp/swhost_snd.c with
 *            platform/amiga/snd_engine.c, as the 68k game): it loads the
 *            sound bank into its memory (SND_BANK) and sends the ARM only
 *            the head of each sample (SND_HEADERS, through SWL_BUF_XFER);
 *            the ARM says which sample to play (SND_PLAY) and keeps the time
 *            of each sound itself (platform/warp/snd_proxy.c).
 * Requests (FOPEN..MKDIR, SND_OPEN, MUS_OPEN) are answered by one RESULT
 * each, in order. */
#ifndef SWLINK_H
#define SWLINK_H

#define SWL_VERSION      1

#define SWL_BUF_XFER     1        /* file read transfer buffer */
#define SWL_BUF_SCREEN   2        /* the game's screen buffer (WScreen) */
#define SWL_XFER_BYTES   (256 * 1024)
#define SWL_REC_MAX      3072     /* largest record payload */

/* The ARM's render area: SWL_BUF_XFER, then WScreen (at most 2560 x 1441),
 * a second heap pool (wlibc.c), the sound heap at the end (snd_proxy.c) */
#define WARP_RENDER_SCREEN_MAX   (2560 * 1441)
#define WARP_SOUND_HEAP_RESERVE  (1536 * 1024)

/* ARM -> 68k */
#define SWL_LOG          1        /* text */
#define SWL_HELLO        2        /* u32 version, u32 cpuHz, u32 window, u32 render */
#define SWL_SCREEN       3        /* u16 width, u16 height: open / change the screen */
#define SWL_PAL          4        /* 768 bytes, 6-bit RGB */
#define SWL_FRAME        5        /* u16 width, u16 height, u32 pitch, u32 buffer,
                                     s32 game speed % in the corner (-1: none) */
#define SWL_STATS        6        /* text: profiler lines for the 68k's console */
#define SWL_FOPEN        10       /* u32 flags (O_*), name -> RESULT handle, size */
#define SWL_FREAD        11       /* u32 handle, u32 offset, u32 len -> RESULT bytes */
#define SWL_FWRITE       12       /* u32 handle, u32 offset, data -> RESULT bytes */
#define SWL_FCLOSE       13       /* u32 handle -> RESULT 0 */
#define SWL_FSTAT        14       /* name -> RESULT size (-1 none), isdir */
#define SWL_FREMOVE      15       /* name -> RESULT 0 / -1 */
#define SWL_FRENAME      16       /* old NUL new -> RESULT 0 / -1 */
#define SWL_MKDIR        17       /* name -> RESULT 0 / -1 */
#define SWL_EXIT         20       /* i32 code */
#define SWL_TRACE        21       /* text; RESULT after the host has logged it (trace mode) */
/* sound (slots and units of platform/amiga/snd_engine.h) */
#define SWL_SND_OPEN     30       /* -> RESULT backend opened (SE_*), heap max */
#define SWL_SND_CLOSE    31
#define SWL_SND_PLAY     32       /* u32 slot, u32 sample (~0: silent), u32 offset in the sample,
                                     u32 len, i32 rate, u16 vol, u16 pan, i32 passes, u32 pass_ms */
#define SWL_SND_SET      33       /* u32 slot, i32 rate, u16 vol, u16 pan */
#define SWL_SND_STOP     34       /* u32 slot */
#define SWL_SND_RELEASE  35       /* u32 slot */
#define SWL_MUS_OPEN     36       /* name -> RESULT 1 / 0 */
#define SWL_MUS_CLOSE    37
#define SWL_MUS_PLAY     38       /* u16 vol */
#define SWL_MUS_PAUSE    39
#define SWL_MUS_REWIND   40
#define SWL_SND_BANK     41       /* u32 bank (tpno), name -> RESULT samples in the bank, 0 none */
#define SWL_SND_HEADERS  42       /* -> the first SWL_SND_HDR_BYTES of each sample in
                                     SWL_BUF_XFER, RESULT samples */
#define SWL_SND_HDR_BYTES 64
#define SWL_SMK_PLAY     43       /* u16 flags (SMK_*), u16 volume 0..64, name: the 68k
                                     plays the Smacker video -> RESULT 0 / -1 */

/* 68k -> ARM */
#define SWL_RESULT       100      /* i32 value, i32 extra */
#define SWL_KEY          101      /* u16 raw key code (bit 7 = up), u16 qualifier */
#define SWL_MOUSE        102      /* i16 x, i16 y, u16 event (SWL_MS_*) */
#define SWL_FRAME_DONE   103      /* u32 fetch time in us (board) */
#define SWL_QUIT         104
#define SWL_ACTIVE       105      /* u32 window active */

#define SWL_MS_MOVE      0
#define SWL_MS_LDOWN     1
#define SWL_MS_LUP       2
#define SWL_MS_RDOWN     3
#define SWL_MS_RUP       4
#define SWL_MS_MDOWN     5
#define SWL_MS_MUP       6

/* Amiga input qualifiers passed on in SWL_KEY */
#define SWL_QUAL_LSHIFT  0x0001
#define SWL_QUAL_RSHIFT  0x0002
#define SWL_QUAL_CONTROL 0x0008
#define SWL_QUAL_LALT    0x0010
#define SWL_QUAL_RALT    0x0020

#endif
