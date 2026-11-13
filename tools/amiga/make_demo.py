#!/usr/bin/env python3
"""Private test package of the Amiga 68060 build, with the game data.

Builds DEMO/SyndWars/ (the game directory as it goes to the Amiga) and
DEMO/SyndWars-68060-demo.zip of it (Unix permissions: program 755, the rest
644, so UnZip on the Amiga keeps the E bit). The data set is the one of
pack_data.py: no sound, music or videos; player saves are left out.
DEMO/ holds game data - it is ignored by git and must never be committed.

Usage: python tools/amiga/make_demo.py [--gamedir "Syndicate Wars/syndwarsfx"]
"""
import argparse
import os
import shutil
import sys
import zipfile

sys.path.insert(0, os.path.dirname(__file__))
from pack_data import DIRS, SKIP_EXT  # noqa: E402

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gamedir", default=os.path.join(ROOT, "Syndicate Wars", "syndwarsfx"))
    a = ap.parse_args()

    out = os.path.join(ROOT, "DEMO")
    gdir = os.path.join(out, "SyndWars")
    if os.path.isdir(gdir):
        shutil.rmtree(gdir)
    os.makedirs(gdir)
    rel = os.path.join(ROOT, "platform", "amiga", "release")
    shutil.copy2(os.path.join(ROOT, "build", "amiga", "SyndWars"), gdir)
    for fn in ("SyndWars.info", "config.ini", "ReadMe.txt"):
        shutil.copy2(os.path.join(rel, fn), gdir)

    for d in DIRS:
        for dp, _, fns in os.walk(os.path.join(a.gamedir, d)):
            r = os.path.relpath(dp, a.gamedir)
            os.makedirs(os.path.join(gdir, r), exist_ok=True)
            if r.replace("\\", "/").lower().startswith("qdata/savegame"):
                continue
            for fn in fns:
                if not fn.lower().endswith(SKIP_EXT):
                    shutil.copy2(os.path.join(dp, fn), os.path.join(gdir, r, fn))

    zpath = os.path.join(out, "SyndWars-68060-demo.zip")
    nfiles = 0
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for dp, dns, fns in os.walk(gdir):
            dns.sort()
            r = os.path.relpath(dp, out).replace("\\", "/")
            zi = zipfile.ZipInfo(r + "/")
            zi.create_system = 3
            zi.external_attr = (0o40755 << 16) | 0x10
            zf.writestr(zi, b"")
            for fn in sorted(fns):
                p = os.path.join(dp, fn)
                zi = zipfile.ZipInfo.from_file(p, r + "/" + fn)
                zi.create_system = 3
                zi.external_attr = (0o100755 if fn == "SyndWars" else 0o100644) << 16
                zi.compress_type = zipfile.ZIP_DEFLATED
                with open(p, "rb") as f:
                    zf.writestr(zi, f.read())
                nfiles += 1
    print(f"{zpath}: {nfiles} files, {os.path.getsize(zpath) / 1048576:.1f} MB")


if __name__ == "__main__":
    main()
