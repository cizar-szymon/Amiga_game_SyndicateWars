#!/usr/bin/env python3
"""Extract a Syndicate Wars CD image (CUE + raw BIN, e.g. GOG game.ins/game.gog).

  * data track (MODE1/2352): files of the ISO9660 file system -> OUT/files/
  * audio tracks: 44.1 kHz 16-bit stereo WAV -> OUT/track_N.wav  (N = CD track - 1,
    matching the music file names of SyndWarsFX: track_1..3)

Usage: python tools/amiga/cdimage.py game.ins OUT [--no-audio] [--list]
"""
import argparse
import os
import re
import struct
import sys
import wave

RAW = 2352


def msf(s):
    m, sec, f = (int(x) for x in s.split(":"))
    return (m * 60 + sec) * 75 + f


def parse_cue(path):
    tracks, binfile = [], None
    for line in open(path, encoding="latin-1"):
        t = line.split()
        if not t:
            continue
        if t[0] == "FILE":
            binfile = re.search(r'"(.+)"', line).group(1)
        elif t[0] == "TRACK":
            tracks.append({"no": int(t[1]), "mode": t[2], "start": None})
        elif t[0] == "INDEX" and t[1] == "01":
            tracks[-1]["start"] = msf(t[2])
    binpath = os.path.join(os.path.dirname(path), binfile)
    total = os.path.getsize(binpath) // RAW
    for i, tr in enumerate(tracks):
        tr["end"] = tracks[i + 1]["start"] if i + 1 < len(tracks) else total
    return binpath, tracks


class Iso:
    def __init__(self, f, base):
        self.f, self.base = f, base

    def sector(self, lba, count=1):
        out = bytearray()
        for i in range(count):
            self.f.seek((self.base + lba + i) * RAW + 16)   # MODE1: 16-byte header
            out += self.f.read(2048)
        return bytes(out)

    def read_file(self, lba, size):
        n = (size + 2047) // 2048
        return self.sector(lba, n)[:size]

    def walk(self, lba, size, path=""):
        data = self.sector(lba, (size + 2047) // 2048)
        pos = 0
        while pos < size:
            ln = data[pos]
            if ln == 0:                      # rest of this sector is padding
                pos = (pos // 2048 + 1) * 2048
                continue
            rec = data[pos:pos + ln]
            ext = struct.unpack_from("<I", rec, 2)[0]
            dsize = struct.unpack_from("<I", rec, 10)[0]
            flags = rec[25]
            nlen = rec[32]
            name = rec[33:33 + nlen]
            pos += ln
            if name in (b"\x00", b"\x01"):
                continue
            name = name.decode("latin-1").split(";")[0].rstrip(".")
            full = f"{path}/{name}" if path else name
            if flags & 2:
                yield from self.walk(ext, dsize, full)
            else:
                yield full, ext, dsize


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cue")
    ap.add_argument("out")
    ap.add_argument("--no-audio", action="store_true")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()

    binpath, tracks = parse_cue(a.cue)
    with open(binpath, "rb") as f:
        data = [t for t in tracks if t["mode"].startswith("MODE1")][0]
        iso = Iso(f, data["start"])
        pvd = iso.sector(16)
        if pvd[1:6] != b"CD001":
            sys.exit("no ISO9660 primary volume descriptor")
        print("volume:", pvd[40:72].decode("latin-1").strip())
        root = pvd[156:156 + 34]
        rlba, rsize = struct.unpack_from("<I", root, 2)[0], struct.unpack_from("<I", root, 10)[0]
        nfiles = nbytes = 0
        for name, lba, size in iso.walk(rlba, rsize):
            nfiles += 1
            nbytes += size
            if a.list:
                print(f"{size:10} {name}")
                continue
            dst = os.path.join(a.out, "files", *name.split("/"))
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            with open(dst, "wb") as o:
                o.write(iso.read_file(lba, size))
        print(f"{nfiles} files, {nbytes} bytes")

        if a.no_audio or a.list:
            return
        for tr in tracks:
            if tr["mode"] != "AUDIO":
                continue
            dst = os.path.join(a.out, f"track_{tr['no'] - 1}.wav")
            with wave.open(dst, "wb") as w:
                w.setnchannels(2)
                w.setsampwidth(2)
                w.setframerate(44100)
                f.seek(tr["start"] * RAW)
                left = tr["end"] - tr["start"]
                while left > 0:
                    n = min(left, 1000)
                    w.writeframes(f.read(n * RAW))    # CD audio is little-endian 16-bit
                    left -= n
            secs = (tr["end"] - tr["start"]) / 75
            print(f"{dst}: {secs:.1f} s")


if __name__ == "__main__":
    main()
