#!/usr/bin/env python3
"""Inventory of x86 assembly still linked into SyndWarsFX.

Static analysis, no build needed:
  * splits every linked .sx file into functions (GLOBAL_FUNC / top-level labels,
    `jump_*` labels stay inside the current function) and data objects,
  * collects symbol references of each function / data object,
  * finds asm symbols referenced from live C code (`#if 0` blocks and comments removed),
  * walks the reference graph from those roots.

Output: summary per file and a CSV / Markdown table of reachable functions.

Usage: python tools/amiga/asm_inventory.py [--md docs/amiga/asm-inventory-table.md] [--csv out.csv]
"""
import argparse
import csv
import os
import re
import sys
from collections import defaultdict

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

# .sx files linked into the game (src/Makefile.am)
ASM_FILES = [
    "src/swars.sx", "src/thing_fire_s.sx", "src/radica3.sx", "src/bat_s.sx",
    "src/netdos.sx", "src/bflib_vidraw_s.sx", "src/radica2.sx", "src/radica1.sx",
    "src/netipx1.sx", "src/sound_smp_s.sx", "src/sound_heap_s.sx",
    "src/thing_expld_s.sx", "src/netser1.sx",
    "bfsoundlib/src/ailssa.sx", "bfsmacklib/src/bfsmacklib_s.sx",
]
C_DIRS = ["src", "bflibrary", "bfsoundlib", "bfsmacklib", "swrendersoft", "bfariadne"]
C_EXT = (".c", ".h", ".cpp", ".hpp")

RE_GLOBAL_FUNC = re.compile(r"^\s*GLOBAL_FUNC\s*\(\s*(\w+)\s*\)")
RE_GLOBAL = re.compile(r"^\s*GLOBAL\s*\(\s*(\w+)\s*\)")
RE_LABEL = re.compile(r"^([A-Za-z_][\w.]*):")
RE_SECTION = re.compile(r"^\s*\.(text|data|bss|section\s+\.(\w+))")
RE_SYM = re.compile(r"[A-Za-z_][\w]*")
RE_INSTR = re.compile(r"^\s+([a-z][a-z0-9]*)\b")
RE_EXPORT = re.compile(r"EXPORT_SYMBOL\s*\(\s*(\w+)\s*\)")
DIRECTIVES = {"byte", "short", "long", "word", "ascii", "string", "fill", "space",
              "zero", "align", "int", "quad", "global", "skip"}
REGS = set("eax ebx ecx edx esi edi ebp esp ax bx cx dx si di bp sp al ah bl bh cl ch dl dh "
           "cs ds es fs gs ss st".split())


def strip_c(text):
    """Remove comments and inactive #if 0 / #else-of-#if-1 blocks."""
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    out, stack = [], []          # stack entries: (parent_active, this_active, is_const)
    active = True
    for line in text.split("\n"):
        s = line.strip()
        m = re.match(r"#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)", s)
        if m:
            kw, rest = m.group(1), m.group(2).strip()
            if kw in ("if", "ifdef", "ifndef"):
                const = None
                if kw == "if" and re.fullmatch(r"0+", rest):
                    const = False
                elif kw == "if" and re.fullmatch(r"[1-9]\d*", rest):
                    const = True
                this = active and (const if const is not None else True)
                stack.append((active, this, const is not None))
                active = this
            elif kw in ("else", "elif") and stack:
                parent, this, is_const = stack[-1]
                if is_const:
                    this = parent and not this
                else:
                    this = parent
                stack[-1] = (parent, this, is_const)
                active = this
            elif kw == "endif" and stack:
                active = stack.pop()[0]
            continue
        if active:
            out.append(line)
    return "\n".join(out)


class Obj:
    def __init__(self, name, file, kind, line):
        self.name, self.file, self.kind, self.line = name, file, kind, line
        self.lines = 0
        self.instr = 0
        self.refs = set()
        self.aliases = {name}


def parse_asm(path):
    objs = []
    cur = None
    section = "text"
    rel = os.path.relpath(path, ROOT).replace("\\", "/")
    with open(path, encoding="latin-1") as f:
        text = f.read()
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    for no, line in enumerate(text.split("\n"), 1):
        ms = RE_SECTION.match(line)
        if ms:
            section = ms.group(2) or ms.group(1)
            continue
        name = None
        m = RE_GLOBAL_FUNC.match(line) or RE_GLOBAL.match(line)
        if m:
            name = m.group(1)
        else:
            m = RE_LABEL.match(line)
            if m and not m.group(1).startswith(("jump_", ".")):
                name = m.group(1)
        if name:
            kind = "func" if section == "text" else "data"
            # consecutive labels without content are aliases of one object
            if cur is not None and cur.lines == 0 and cur.kind == kind:
                cur.aliases.add(name)
            else:
                cur = Obj(name, rel, kind, no)
                objs.append(cur)
            rest = line.split(":", 1)[1] if ":" in line and not RE_GLOBAL_FUNC.match(line) and not RE_GLOBAL.match(line) else ""
            if not rest.strip():
                continue
            line = rest
        if cur is None or line.strip().startswith("#") or not line.strip():
            continue
        cur.lines += 1
        mi = RE_INSTR.match(line)
        if mi and mi.group(1) not in DIRECTIVES and not line.strip().startswith("."):
            cur.instr += 1
        body = RE_EXPORT.sub(r"\1", line)
        body = re.sub(r"^\s*[a-z][a-z0-9]*\s", " ", body, count=1)   # mnemonic / directive
        for sym in RE_SYM.findall(body):
            if sym not in REGS and not sym.startswith("jump_"):
                cur.refs.add(sym)
    return objs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--md")
    ap.add_argument("--csv")
    ap.add_argument("--dead", help="write list of unreachable functions")
    a = ap.parse_args()

    objs = []
    for f in ASM_FILES:
        objs += parse_asm(os.path.join(ROOT, f))
    by_name = {}
    for o in objs:
        for n in o.aliases:
            by_name[n] = o

    # live C text
    c_syms = defaultdict(set)    # symbol -> set of C files
    for d in C_DIRS:
        for dp, _, fns in os.walk(os.path.join(ROOT, d)):
            for fn in fns:
                if not fn.endswith(C_EXT):
                    continue
                p = os.path.join(dp, fn)
                with open(p, encoding="latin-1") as f:
                    t = strip_c(f.read())
                rel = os.path.relpath(p, ROOT).replace("\\", "/")
                for sym in set(RE_SYM.findall(t)):
                    if sym in by_name:
                        c_syms[sym].add(rel)

    roots = {by_name[s] for s in c_syms}
    reach, stack = set(), list(roots)
    while stack:
        o = stack.pop()
        if id(o) in reach:
            continue
        reach.add(id(o))
        for r in o.refs:
            t = by_name.get(r)
            if t is not None and id(t) not in reach:
                stack.append(t)

    funcs = [o for o in objs if o.kind == "func"]
    datas = [o for o in objs if o.kind == "data"]
    per_file = defaultdict(lambda: [0, 0, 0, 0, 0, 0])
    for o in funcs:
        pf = per_file[o.file]
        pf[0] += 1; pf[1] += o.instr
        if id(o) in reach:
            pf[2] += 1; pf[3] += o.instr
    for o in datas:
        pf = per_file[o.file]
        pf[4] += 1
        if id(o) in reach:
            pf[5] += 1

    print(f"{'file':34} {'funcs':>6} {'instr':>7} | {'live f':>6} {'live in':>7} | {'data':>5} {'live d':>6}")
    tot = [0] * 6
    for f in ASM_FILES:
        pf = per_file[f]
        tot = [x + y for x, y in zip(tot, pf)]
        print(f"{f:34} {pf[0]:6} {pf[1]:7} | {pf[2]:6} {pf[3]:7} | {pf[4]:5} {pf[5]:6}")
    print(f"{'TOTAL':34} {tot[0]:6} {tot[1]:7} | {tot[2]:6} {tot[3]:7} | {tot[4]:5} {tot[5]:6}")

    # incoming references (callers) for live functions
    callers = defaultdict(set)
    for o in objs:
        if id(o) not in reach:
            continue
        for r in o.refs:
            t = by_name.get(r)
            if t is not None and t is not o:
                callers[id(t)].add(o.name)

    def c_users(o):
        files = set()
        for n in o.aliases:
            files |= c_syms.get(n, set())
        return files

    live = sorted((o for o in funcs if id(o) in reach), key=lambda o: -o.instr)
    rows = []
    for o in live:
        calls_asm = sorted(n for n in o.refs if n in by_name and by_name[n].kind == "func"
                           and by_name[n] is not o)
        calls_c = sorted(n for n in o.refs if n.startswith("ac_"))
        rows.append({
            "name": o.name, "file": o.file, "line": o.line, "instr": o.instr,
            "c_users": " ".join(sorted(c_users(o))),
            "asm_callers": len(callers[id(o)]),
            "calls_asm": len(calls_asm), "calls_c": len(calls_c),
        })
    if a.csv:
        with open(a.csv, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
            w.writeheader()
            w.writerows(rows)
    if a.md:
        with open(a.md, "w", encoding="utf-8") as f:
            f.write("| funkcja | plik:linia | instr. | wołana z C | wołających asm | woła asm | woła C (ac_) |\n")
            f.write("|---|---|---:|---|---:|---:|---:|\n")
            for r in rows:
                f.write(f"| `{r['name']}` | {r['file']}:{r['line']} | {r['instr']} | {r['c_users']} "
                        f"| {r['asm_callers']} | {r['calls_asm']} | {r['calls_c']} |\n")
    if a.dead:
        with open(a.dead, "w") as f:
            for o in funcs:
                if id(o) not in reach:
                    f.write(f"{o.file}:{o.line} {o.name} {o.instr}\n")


if __name__ == "__main__":
    main()
