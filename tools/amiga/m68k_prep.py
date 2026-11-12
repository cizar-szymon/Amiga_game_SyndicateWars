#!/usr/bin/env python3
"""Prepare C sources for the m68k (Amiga) build.

The remaining x86 inline asm in the C sources (`asm volatile ("call ASM_x" ...)`,
wrappers around functions not remade yet) cannot be compiled for m68k. This
tool copies a source file and replaces every such statement by a call to
asm_stub_hit("ASM_x"), which logs the first call of each missing function
(platform/amiga/asm_stubs.c). Output operands are set to 0.

The upstream sources stay unchanged; the build runs this over a copy.

Usage: python tools/amiga/m68k_prep.py IN.c OUT.c
       python tools/amiga/m68k_prep.py --report DIR...   (count statements)
"""
import os
import re
import sys

RE_ASM = re.compile(r"\b(?:asm|__asm__)\s+(?:volatile|__volatile__)?\s*\(")


def find_close(text, pos):
    """pos is just after '('; return index of the matching ')'."""
    depth = 1
    i = pos
    in_str = False
    while i < len(text):
        c = text[i]
        if in_str:
            if c == "\\":
                i += 2
                continue
            if c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise ValueError("unbalanced asm statement")


def split_top(s, sep):
    """Split by `sep` outside strings and parentheses."""
    parts, depth, in_str, cur = [], 0, False, ""
    i = 0
    while i < len(s):
        c = s[i]
        if in_str:
            cur += c
            if c == "\\" and i + 1 < len(s):
                cur += s[i + 1]
                i += 2
                continue
            if c == '"':
                in_str = False
        elif c == '"':
            in_str = True
            cur += c
        elif c == "(":
            depth += 1
            cur += c
        elif c == ")":
            depth -= 1
            cur += c
        elif c == sep and depth == 0:
            parts.append(cur)
            cur = ""
        else:
            cur += c
        i += 1
    parts.append(cur)
    return parts


def transform(text):
    out = []
    pos = 0
    count = 0
    while True:
        m = RE_ASM.search(text, pos)
        if not m:
            out.append(text[pos:])
            break
        # skip matches in comments: crude check for // on the same line
        line_start = text.rfind("\n", 0, m.start()) + 1
        if "//" in text[line_start:m.start()]:
            out.append(text[pos:m.end()])
            pos = m.end()
            continue
        close = find_close(text, m.end())
        end = close + 1
        while end < len(text) and text[end] in " \t":
            end += 1
        if end < len(text) and text[end] == ";":
            end += 1
        body = text[m.end():close]
        parts = split_top(body, ":")
        code = " ".join(re.findall(r'"((?:[^"\\]|\\.)*)"', parts[0]))
        code = code.replace("\\n", " ").replace("\\t", " ")
        call = re.search(r"\b(?:call|jmp)\s+(\w+)", code)
        name = call.group(1) if call else "inline:" + code.strip().split("\\n")[0][:40]
        assigns = []
        if len(parts) > 1:
            for op in split_top(parts[1], ","):
                mo = re.match(r'\s*"[^"]*"\s*\((.*)\)\s*$', op, re.S)
                if mo:
                    assigns.append(f"{mo.group(1).strip()} = 0;")
        repl = f'asm_stub_hit("{name}"); ' + " ".join(assigns)
        out.append(text[pos:m.start()])
        out.append(repl.strip())
        pos = end
        count += 1
    return "".join(out), count


def main():
    if sys.argv[1] == "--report":
        total = 0
        for d in sys.argv[2:]:
            for fn in sorted(os.listdir(d)):
                if fn.endswith(".c"):
                    t = open(os.path.join(d, fn), encoding="latin-1").read()
                    _, n = transform(t)
                    if n:
                        print(f"{n:5} {d}/{fn}")
                        total += n
        print(f"{total:5} total")
        return
    src, dst = sys.argv[1], sys.argv[2]
    text = open(src, encoding="latin-1").read()
    text, n = transform(text)
    if n:
        text = '#include "asm_stubs.h"\n' + text
    os.makedirs(os.path.dirname(dst) or ".", exist_ok=True)
    with open(dst, "w", encoding="latin-1", newline="") as f:
        f.write(text)


if __name__ == "__main__":
    main()
