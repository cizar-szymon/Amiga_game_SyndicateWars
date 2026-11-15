/* smk_check - decodes every frame of Smacker files with libsmacker and
 * prints a checksum of each frame (video, palette, audio): to compare a
 * changed decoder (third_party/libsmacker) with the original, frame by frame.
 *
 *   gcc -O2 -o smk_check tools/amiga/smk_check.c third_party/libsmacker/smacker.c
 *   smk_check FILE.smk... > sums.txt        (and time it: -t)
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../../third_party/libsmacker/smacker.h"

static unsigned long adler(const unsigned char *p, unsigned long n, unsigned long a)
{
    unsigned long s1 = a & 0xFFFF, s2 = a >> 16;
    while (n--) {
        s1 = (s1 + *p++) % 65521;
        s2 = (s2 + s1) % 65521;
    }
    return (s2 << 16) | s1;
}

int main(int argc, char **argv)
{
    int i, timing = 0;

    for (i = 1; i < argc; i++) {
        smk s;
        unsigned long frames, w, h, f;
        unsigned char ys, mask, ch[7], bd[7];
        unsigned long rate[7];
        clock_t c0;

        if (strcmp(argv[i], "-t") == 0) {
            timing = 1;
            continue;
        }
        s = smk_open_file(argv[i], SMK_MODE_MEMORY);
        if (s == NULL) {
            printf("%s: cannot open\n", argv[i]);
            continue;
        }
        smk_info_all(s, NULL, &frames, NULL);
        smk_info_video(s, &w, &h, &ys);
        smk_info_audio(s, &mask, ch, bd, rate);
        smk_enable_all(s, SMK_VIDEO_TRACK | mask);
        c0 = clock();
        smk_first(s);
        for (f = 0; f < frames; f++) {
            unsigned long a = adler(smk_get_video(s), w * h, 1);
            a = adler(smk_get_palette(s), 768, a);
            if (mask & 1)
                a = adler(smk_get_audio(s, 0), smk_get_audio_size(s, 0), a);
            if (!timing)
                printf("%s %lu %08lx\n", argv[i], f, a);
            if (smk_next(s) < 0)
                break;
        }
        if (timing)
            printf("%s: %lu frames %lux%lu, %.2f ms a frame\n", argv[i], frames, w, h,
              (double)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC / frames);
        smk_close(s);
    }
    return 0;
}
