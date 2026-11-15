#!/usr/bin/env python3
"""Make the Amiga music tracks from the PC ones (music/track_N.ogg).

The Amiga does not decode Vorbis: each track is turned once on the PC into
a file of 8-bit signed stereo PCM, which Paula plays straight from the
buffers it is read into.  The samples are stored by blocks, the left
channel of a block followed by its right channel, so each half goes
directly to one Paula channel.

File (big endian):  "SWMU", u32 rate, u32 block (bytes per channel),
u32 number of blocks, then the blocks; the last block is padded with 0.

From the .ogg tracks of the PC install it needs ffmpeg in PATH; from the
.wav tracks of the CD (44.1 kHz 16-bit stereo, tools/amiga/cdimage.py) at
22050 Hz it needs nothing more (convert_wav: each two samples averaged).
usage: python tools/amiga/make_music.py IN_DIR OUT_DIR [--rate 22050]
"""
import argparse
import array
import os
import struct
import subprocess
import sys
import wave

BLOCK = 8192


def write_swmu(dst, rate, left, right):
    n = (len(left) + BLOCK - 1) // BLOCK
    pad = n * BLOCK - len(left)
    left += b"\0" * pad
    right += b"\0" * pad
    with open(dst, "wb") as f:
        f.write(b"SWMU" + struct.pack(">III", rate, BLOCK, n))
        for i in range(n):
            f.write(left[i * BLOCK:(i + 1) * BLOCK])
            f.write(right[i * BLOCK:(i + 1) * BLOCK])
    print("%s: %d s, %d bytes" % (dst, len(left) // rate, 16 + 2 * n * BLOCK))


def convert(src, dst, rate):
    pcm = subprocess.run(
        ["ffmpeg", "-v", "error", "-i", src, "-ac", "2", "-ar", str(rate),
         "-f", "s8", "-acodec", "pcm_s8", "-"],
        check=True, stdout=subprocess.PIPE).stdout
    write_swmu(dst, rate, pcm[0::2], pcm[1::2])


def convert_wav(src, dst):
    """A CD track (44.1 kHz 16-bit stereo WAV) to 22050 Hz 8-bit, Python only."""
    with wave.open(src, "rb") as w:
        if (w.getframerate(), w.getsampwidth(), w.getnchannels()) != (44100, 2, 2):
            raise SystemExit("%s: not 44.1 kHz 16-bit stereo" % src)
        a = array.array("h")
        a.frombytes(w.readframes(w.getnframes()))
    if sys.byteorder == "big":
        a.byteswap()
    out = []
    for ch in (0, 1):
        c = a[ch::2]
        # two samples of 44.1 kHz to one of 22.05 kHz, 16 to 8 bits
        out.append(bytes(((x + y) >> 9) & 0xFF for x, y in zip(c[0::2], c[1::2])))
    write_swmu(dst, 22050, out[0], out[1])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("inp")
    ap.add_argument("out")
    ap.add_argument("--rate", type=int, default=22050)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    for name in sorted(os.listdir(a.inp)):
        if not name.startswith("track_"):
            continue
        if name.endswith(".ogg"):
            convert(os.path.join(a.inp, name),
                    os.path.join(a.out, name[:-4] + ".smu"), a.rate)
        elif name.endswith(".wav") and a.rate == 22050:
            convert_wav(os.path.join(a.inp, name),
                        os.path.join(a.out, name[:-4] + ".smu"))


if __name__ == "__main__":
    main()
