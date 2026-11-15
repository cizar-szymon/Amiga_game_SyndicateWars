#!/usr/bin/env python3
"""A release ZIP for the Amiga (tools/amiga/make_release.sh); from the
Settlers port.

  python tools/amiga/make_zip.py OUT.zip BASE NAME... [--add DIR ARCDIR]...

NAME: files or directories under BASE, stored with their path from BASE.
--add DIR ARCDIR: DIR with its subdirectories stored under ARCDIR - the game
data of the full release (from the GOG CD and the SyndWarsFX packages).
Directories get their own entries.
"""
import argparse
import os
import zipfile

EXECUTABLES = {"SyndWars", "swhost"}	# the programs: must get the Amiga's E bit


WRITTEN = set()		# names in the archive: each one once


def put(z, path, arcname):
    """A file with Unix permissions (made-by Unix): Amiga UnZip turns them into the
    protection bits - without them the program unpacks without E (cannot run)."""
    if arcname in WRITTEN:
        return
    WRITTEN.add(arcname)
    info = zipfile.ZipInfo.from_file(path, arcname)
    info.create_system = 3
    mode = 0o755 if os.path.basename(arcname) in EXECUTABLES else 0o644
    info.external_attr = (0o100000 | mode) << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    with open(path, "rb") as f:
        z.writestr(info, f.read())


def put_dir(z, arcname):
    name = arcname.rstrip("/") + "/"
    if name in WRITTEN:
        return
    WRITTEN.add(name)
    info = zipfile.ZipInfo(name)
    info.create_system = 3
    info.external_attr = (0o040755 << 16) | 0x10
    z.writestr(info, "")


def add_tree(z, base, name):
    path = os.path.join(base, name)
    if os.path.isfile(path):
        put(z, path, name.replace(os.sep, "/"))
        return
    for root, dirs, files in os.walk(path):
        dirs.sort()
        rel = os.path.relpath(root, base).replace(os.sep, "/")
        put_dir(z, rel)
        for f in sorted(files):
            put(z, os.path.join(root, f), rel + "/" + f)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("base")
    ap.add_argument("names", nargs="+")
    ap.add_argument("--add", nargs=2, action="append", default=[], metavar=("DIR", "ARCDIR"))
    a = ap.parse_args()

    with zipfile.ZipFile(a.out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for n in a.names:
            add_tree(z, a.base, n)
        for d, arcdir in a.add:
            for root, dirs, files in os.walk(d):
                dirs.sort()
                rel = os.path.relpath(root, d).replace(os.sep, "/")
                arc = arcdir if rel == "." else arcdir + "/" + rel
                put_dir(z, arc)
                for f in sorted(files):
                    put(z, os.path.join(root, f), arc + "/" + f)
    print(a.out, os.path.getsize(a.out), "bytes")


if __name__ == "__main__":
    main()
