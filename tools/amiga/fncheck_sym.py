#!/usr/bin/env python3
"""Add symbol names to .data/.bss addresses in fncheck.log.

The program is relocated at run time; the log header gives the run-time start
of .data, nm gives the link-time one, the difference is applied to every address.

Usage: python tools/amiga/fncheck_sym.py EXE fncheck.log
"""
import bisect
import re
import subprocess
import sys


def main():
    exe, log = sys.argv[1], sys.argv[2]
    nm = subprocess.run(["nm", "-n", exe], capture_output=True, text=True, check=True).stdout
    syms = []
    data_start = None
    for line in nm.splitlines():
        t = line.split()
        if len(t) != 3:
            continue
        addr, kind, name = int(t[0], 16), t[1], t[2]
        if name == "__data_start__":
            data_start = addr
        if kind in "dDbBcC":
            syms.append((addr, name.lstrip("_")))
    syms.sort()
    keys = [a for a, _ in syms]
    delta = 0
    for line in open(log):
        m = re.match(r"fncheck: data ([0-9a-fA-F]+)", line)
        if m:
            delta = int(m.group(1), 16) - data_start
        m = re.match(r"(\s+)([0-9a-fA-F]{8}) len (\d+) (data|bss)( .*)", line.rstrip("\n"))
        if m:
            a = int(m.group(2), 16) - delta
            i = bisect.bisect_right(keys, a) - 1
            name = f"{syms[i][1]}+{a - syms[i][0]}" if i >= 0 else "?"
            print(f"{m.group(1)}{m.group(2)} len {m.group(3)} {m.group(4)} {name}{m.group(5)}")
        else:
            print(line, end="")


if __name__ == "__main__":
    main()
