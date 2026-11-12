#!/usr/bin/env python3
"""Add static functions to the symbol list of the Amiga binary.

The hunk executable keeps only global symbols, so a profile sample inside a
static function is counted for the global function placed before it.
This script takes the static text symbols from the object files and moves
them to their final addresses, using a global symbol of the same object file
as the anchor (each object's code section is linked as one block).

usage: python tools/amiga/nm_statics.py NM.txt OBJNM.txt > NMFULL.txt
  NM.txt    = m68k-amigaos-nm -n build/amiga/SyndWars
  OBJNM.txt = m68k-amigaos-nm -A build/amiga/obj/*.o
"""
import sys
from collections import defaultdict


def main():
    final = {}
    lines = []
    for line in open(sys.argv[1], encoding="latin-1"):
        parts = line.split()
        if len(parts) == 3:
            lines.append((int(parts[0], 16), parts[1], parts[2]))
            if parts[1] == "T":
                final[parts[2]] = int(parts[0], 16)
    objs = defaultdict(list)
    for line in open(sys.argv[2], encoding="latin-1"):
        head, _, rest = line.partition(":")
        parts = rest.split()
        if len(parts) == 3 and parts[1] in "tT":
            objs[head].append((int(parts[0], 16), parts[1], parts[2]))
    added = 0
    for obj, syms in objs.items():
        base = None
        for off, typ, name in syms:
            if typ == "T" and name in final:
                base = final[name] - off
                break
        if base is None:
            continue
        for off, typ, name in syms:
            if typ == "t" and not name.startswith("___func__"):
                lines.append((base + off, "t", name))
                added += 1
    lines.sort()
    for addr, typ, name in lines:
        print("%08x %s %s" % (addr, typ, name))
    print("nm_statics: %d static symbols added" % added, file=sys.stderr)


if __name__ == "__main__":
    main()
