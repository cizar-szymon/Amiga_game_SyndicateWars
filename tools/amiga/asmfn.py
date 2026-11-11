#!/usr/bin/env python3
"""Print asm functions from the linked .sx files, with the C-side names of the
data symbols they use.

Usage: python tools/amiga/asmfn.py NAME [NAME ...]
NAME: label as in the .sx (ASM_get_new_thing, func_454b0, ...); ASM_ prefix optional.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(__file__))
import asm_inventory as inv   # noqa: E402


def main():
    for want in sys.argv[1:]:
        found = False
        for f in inv.ASM_FILES:
            path = os.path.join(inv.ROOT, f)
            lines = open(path, encoding="latin-1").read().split("\n")
            for i, line in enumerate(lines):
                m = inv.RE_GLOBAL_FUNC.match(line) or inv.RE_LABEL.match(line)
                if not m or m.group(1) not in (want, "ASM_" + want):
                    continue
                found = True
                print(f"===== {f}:{i + 1}")
                j = i
                while j < len(lines):
                    l = lines[j]
                    if j > i and (l.startswith("/*----") and j + 1 < len(lines)
                                  and (inv.RE_GLOBAL_FUNC.match(lines[j + 1])
                                       or inv.RE_LABEL.match(lines[j + 1]))):
                        break
                    if j > i + 1 and (inv.RE_GLOBAL_FUNC.match(l)):
                        break
                    if not l.startswith("/*----"):
                        print(re.sub(r"EXPORT_SYMBOL\((\w+)\)", r"\1", l))
                    j += 1
                break
            if found:
                break
        if not found:
            print(f"===== {want}: not found")


if __name__ == "__main__":
    main()
