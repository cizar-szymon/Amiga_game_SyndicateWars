/* The Smacker video player of the Amiga port (smk_core.c), shared by the 68k
 * game (smk_play.c, bflibrary's screen) and the Warp host (tools/warp/
 * swhost_smk.c, its RTG screen): the game on the ARM has its videos played
 * by the 68060. Plain C (gcc and vbcc), no game or bflibrary headers. */
#ifndef SMK_CORE_H
#define SMK_CORE_H

struct SmkOut {
    /** Puts the frame (w x h, 8-bit) into the middle of the screen, sets the
     * palette (768 bytes, 6 bits a colour) and shows it. yscale: 0 none,
     * 1 interlaced, 2 doubled (libsmacker's SMK_FLAG_Y_*). */
    void (*show)(const unsigned char *pix, unsigned long w, unsigned long h,
      unsigned char yscale, const unsigned char *pal6, void *ctx);
    /** Clears the screen before the video. */
    void (*clear)(void *ctx);
    /** A millisecond clock. */
    unsigned long (*clock_ms)(void);
    /** Keeps the program going while waiting (events, input); nonzero when
     * the player asked to end the video. */
    int (*idle)(void *ctx);
    /** A line for the log (with its newline). */
    void (*log)(const char *text);
    void *ctx;
    /** The video's sound through the sound engine (snd_engine.c) at this
     * volume (0..64); 0: none. */
    int sound;
    unsigned short volume;
};

/** Plays the file; 0 when played (to its end or ended by the player), -1
 * when it cannot be opened. */
int smk_core_play(const char *fname, struct SmkOut *o);

#endif
