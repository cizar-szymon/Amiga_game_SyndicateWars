#!/usr/bin/env python3
"""Pack the game data needed on the Amiga into ZIP files of moderate size
(transfer through AmiAgent, then `UnZip` on the Amiga).

Leaves out what the Amiga build does not use yet: sound, music, intro and
Smacker/FLI videos, other languages.

Usage: python tools/amiga/pack_data.py GAMEDIR OUTDIR [--max-mb 12]
Writes OUTDIR/swdata-NN.zip; paths inside are relative to the game dir.
"""
import argparse
import os
import zipfile

SKIP_EXT = (".smk", ".fli", ".raw", ".ogg", ".exe", ".dll", ".log", ".txt")
DIRS = ["conf", "data", "qdata", "levels", "maps", "language/eng"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("gamedir")
    ap.add_argument("outdir")
    ap.add_argument("--max-mb", type=float, default=12)
    a = ap.parse_args()

    files = []
    for d in DIRS:
        for dp, _, fns in os.walk(os.path.join(a.gamedir, d)):
            for fn in sorted(fns):
                if fn.lower().endswith(SKIP_EXT):
                    continue
                p = os.path.join(dp, fn)
                files.append((os.path.relpath(p, a.gamedir).replace("\\", "/"), p))
    os.makedirs(a.outdir, exist_ok=True)
    limit = a.max_mb * 1024 * 1024
    n, size, zf = 0, 0, None
    for rel, p in files:
        if zf is None or size > limit:
            if zf:
                zf.close()
            n += 1
            zf = zipfile.ZipFile(os.path.join(a.outdir, f"swdata-{n:02d}.zip"), "w",
                                 zipfile.ZIP_DEFLATED, compresslevel=9)
            size = 0
        zi = zipfile.ZipInfo.from_file(p, rel)
        zi.external_attr = 0o644 << 16
        zi.compress_type = zipfile.ZIP_DEFLATED
        with open(p, "rb") as f:
            zf.writestr(zi, f.read())
        size += os.path.getsize(p)
    if zf:
        zf.close()
    total = sum(os.path.getsize(os.path.join(a.outdir, f)) for f in os.listdir(a.outdir))
    print(f"{len(files)} files in {n} zips, {total / 1048576:.1f} MB")


if __name__ == "__main__":
    main()
