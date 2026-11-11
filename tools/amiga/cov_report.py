#!/usr/bin/env python3
"""Report of an asm coverage run (asm_coverage.txt from the `cov` build).

Usage: python tools/amiga/cov_report.py COVERAGE.txt [COVERAGE2.txt ...] [--md OUT.md]
Several files are merged (union of phases).
"""
import argparse
import os
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(__file__))
import asm_inventory as inv   # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cov", nargs="+")
    ap.add_argument("--md")
    a = ap.parse_args()

    objs = []
    for f in inv.ASM_FILES:
        objs += inv.parse_asm(os.path.join(inv.ROOT, f))
    by_name = {}
    for o in objs:
        for n in o.aliases:
            by_name[n] = o

    phases = defaultdict(set)
    frames = []
    for path in a.cov:
        for line in open(path):
            if line.startswith("#"):
                frames.append(line.split()[2])
                continue
            t = line.split()
            if len(t) > 1:
                phases[t[0]] |= set(t[1:])

    # a label marks the function object it belongs to (aliases share an object)
    hit = {}
    for name, ph in phases.items():
        o = by_name.get(name) or by_name.get(name.lstrip("_"))
        if o is None:
            print("unknown label", name, file=sys.stderr)
            continue
        hit.setdefault(id(o), (o, set()))[1].update(ph)

    order = ["init", "input", "draw", "things", "other", "menu"]
    print(f"runs: {len(a.cov)}, frames: {', '.join(frames)}")
    print(f"{'phase':8} {'funcs':>6} {'instr':>7}")
    for ph in order:
        fs = [o for o, p in hit.values() if ph in p]
        print(f"{ph:8} {len(fs):6} {sum(o.instr for o in fs):7}")
    allf = [o for o, p in hit.values()]
    print(f"{'any':8} {len(allf):6} {sum(o.instr for o in allf):7}")

    rows = sorted(hit.values(), key=lambda x: (x[0].file, -x[0].instr))
    lines = ["| funkcja | plik:linia | instr. | fazy |", "|---|---|---:|---|"]
    for o, p in rows:
        lines.append(f"| `{o.name}` | {o.file}:{o.line} | {o.instr} | "
                     f"{' '.join(x for x in order if x in p)} |")
    if a.md:
        with open(a.md, "w", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")
    else:
        print("\n".join(lines))


if __name__ == "__main__":
    main()
