#!/usr/bin/env python3
"""Make the Amiga music tracks from the PC ones (music/track_N.ogg).

The Amiga does not decode Vorbis: each track is turned once on the PC into
a file of 8-bit signed stereo PCM, which Paula plays straight from the
buffers it is read into.  The samples are stored by blocks, the left
channel of a block followed by its right channel, so each half goes
directly to one Paula channel.

File (big endian):  "SWMU", u32 rate, u32 block (bytes per channel),
u32 number of blocks, then the blocks; the last block is padded with 0.

Needs ffmpeg in PATH.
usage: python tools/amiga/make_music.py IN_DIR OUT_DIR [--rate 22050]
"""
import argparse
import os
import struct
import subprocess

BLOCK = 8192


def convert(src, dst, rate):
    pcm = subprocess.run(
        ["ffmpeg", "-v", "error", "-i", src, "-ac", "2", "-ar", str(rate),
         "-f", "s8", "-acodec", "pcm_s8", "-"],
        check=True, stdout=subprocess.PIPE).stdout
    left = pcm[0::2]
    right = pcm[1::2]
    n = (len(left) + BLOCK - 1) // BLOCK
    pad = n * BLOCK - len(left)
    left += b"\0" * pad
    right += b"\0" * pad
    with open(dst, "wb") as f:
        f.write(b"SWMU" + struct.pack(">III", rate, BLOCK, n))
        for i in range(n):
            f.write(left[i * BLOCK:(i + 1) * BLOCK])
            f.write(right[i * BLOCK:(i + 1) * BLOCK])
    print("%s: %d s, %d bytes" % (dst, len(pcm) // 2 // rate, 16 + 2 * n * BLOCK))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("inp")
    ap.add_argument("out")
    ap.add_argument("--rate", type=int, default=22050)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    for name in sorted(os.listdir(a.inp)):
        if name.startswith("track_") and name.endswith(".ogg"):
            convert(os.path.join(a.inp, name),
                    os.path.join(a.out, name[:-4] + ".smu"), a.rate)


if __name__ == "__main__":
    main()
