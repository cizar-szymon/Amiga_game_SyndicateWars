#!/usr/bin/env python3
"""Function profile from RAM:SyndWars-samples.bin (platform/amiga/sampler.c).

usage: python tools/amiga/samples.py SAMPLES.bin NM.txt [top]
  NM.txt = output of `m68k-amigaos-nm -n build/amiga/SyndWars`

Each sample holds the first N longwords of the saved context of the game task.
The slot that holds the interrupted PC is the one whose values almost always
fall inside the program code; it is found here, then the PCs are mapped to
functions. PCs outside the program (exec, graphics, P96 ...) count as <system>.
"""
import bisect
import struct
import sys
from collections import Counter


def load_nm(path):
    syms = []
    for line in open(path, encoding="latin-1"):
        parts = line.split()
        if len(parts) == 3 and parts[1] in "tT":
            syms.append((int(parts[0], 16), parts[2]))
    syms.sort()
    return syms


def main():
    data = open(sys.argv[1], "rb").read()
    syms = load_nm(sys.argv[2])
    top = int(sys.argv[3]) if len(sys.argv) > 3 else 40
    magic, n, words, a_main, a_frame, a_sampler, total, _ = struct.unpack(">8I", data[:32])
    assert magic == 0x53574D50, "bad file"
    by_name = {name: addr for addr, name in syms}
    delta = a_main - by_name["_main"]
    for nm, a in (("_amiga_prof_frame", a_frame), ("_sampler_entry", a_sampler)):
        if nm in by_name and a - by_name[nm] != delta:
            print(f"warning: {nm} gives a different load offset - code not in one hunk?")
    lo = syms[0][0]
    hi = syms[-1][0] + 0x1000
    samples = [struct.unpack(">%dI" % words, data[32 + i * words * 4: 32 + (i + 1) * words * 4])
               for i in range(n)]
    print(f"{n} samples ({total} taken), load offset 0x{delta:08x}")

    def in_code(v):
        return lo <= v - delta < hi

    # The saved FPU registers come first and stay constant; the PC slot is the
    # one with many different values, nearly all inside the program.
    best, best_frac = 0, -1.0
    for s in range(words):
        if len(set(smp[s] for smp in samples)) < 50:
            continue
        frac = sum(1 for smp in samples if in_code(smp[s])) / max(n, 1)
        if frac > best_frac:
            best, best_frac = s, frac
    print(f"PC slot {best} (word offset), {best_frac * 100:.1f}% of PCs in program code")

    addrs = [a for a, _ in syms]
    funcs = Counter()
    for smp in samples:
        v = smp[best]
        if not in_code(v):
            funcs["<system>"] += 1
            continue
        i = bisect.bisect_right(addrs, v - delta) - 1
        funcs[syms[i][1] if i >= 0 else "?"] += 1
    cum = 0
    for name, c in funcs.most_common(top):
        cum += c
        print(f"{c * 100.0 / n:6.2f}% {cum * 100.0 / n:6.2f}%  {name}")


if __name__ == "__main__":
    main()
