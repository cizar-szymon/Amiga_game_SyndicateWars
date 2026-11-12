#!/usr/bin/env python3
"""Byte order layout strings of packed C structs (for lb_le_fix_struct()).

Parses struct definitions from the headers and builds a layout string:
'b' 1-byte field, 'w' 2-byte, 'd' 4-byte, with repeat counts ("12b").
Unions: if all members give the same layout it is used, else the union is
reported and its members are listed separately (the caller must choose).

Usage:
  python tools/amiga/struct_layout.py STRUCT [STRUCT...]      print layouts
  python tools/amiga/struct_layout.py --header OUT.h STRUCT...  write macros
Sizes should be checked against the compiler (tools/amiga/offsets.sh -size).
"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DIRS = ["src", "bflibrary/include", "swrendersoft/include", "bfariadne/include",
        "bfsoundlib/include", "bfsmacklib/include"]

SIZES = {
    "char": 1, "ubyte": 1, "sbyte": 1, "uchar": 1, "TbBool": 1, "TbPixel": 1,
    "u8": 1, "s8": 1, "uint8_t": 1, "int8_t": 1, "bool": 1,
    "short": 2, "ushort": 2, "u16": 2, "s16": 2, "uint16_t": 2, "int16_t": 2,
    "ThingIdx": 2, "TbScreenCoord": 2, "MapCoord": 4, "PlayerIdx": 1,
    "long": 4, "ulong": 4, "int": 4, "uint": 4, "u32": 4, "s32": 4,
    "uint32_t": 4, "int32_t": 4, "TbResult": 4,
    "TrCoord": 4, "TbFileHandle": 4, "float": 4,
}


def load_sources():
    text = ""
    for d in DIRS:
        for dp, _, fns in os.walk(os.path.join(ROOT, d)):
            for fn in fns:
                if fn.endswith((".h", ".c")):
                    t = open(os.path.join(dp, fn), encoding="latin-1").read()
                    text += "\n" + t
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    return text


SRC = None
CONSTS = {}
TYPEDEFS = {}


def find_body(kind, name):
    m = re.search(r"\b%s\s+%s\s*\{" % (kind, re.escape(name)), SRC)
    if not m:
        m = re.search(r"typedef\s+%s\s+\w*\s*\{" % kind, SRC)
        # typedef struct {..} Name;
        for mt in re.finditer(r"typedef\s+%s\s*\w*\s*\{" % kind, SRC):
            body, end = extract_braces(SRC, mt.end() - 1)
            after = SRC[end:end + 80]
            ma = re.match(r"\s*(\w+)\s*;", after)
            if ma and ma.group(1) == name:
                return body
        return None
    body, _ = extract_braces(SRC, m.end() - 1)
    return body


def extract_braces(s, i):
    depth = 0
    j = i
    while j < len(s):
        if s[j] == "{":
            depth += 1
        elif s[j] == "}":
            depth -= 1
            if depth == 0:
                return s[i + 1:j], j + 1
        j += 1
    raise ValueError("unbalanced")


def eval_dim(expr):
    e = expr
    for _ in range(5):
        e2 = re.sub(r"\b([A-Za-z_]\w*)\b", lambda m: str(CONSTS.get(m.group(1), m.group(1))), e)
        if e2 == e:
            break
        e = e2
    return int(eval(e, {}, {}))


def split_fields(body):
    """Top-level declarations of a struct/union body."""
    out, depth, cur = [], 0, ""
    for c in body:
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
        if c == ";" and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += c
    return [x for x in out if x]


class Unresolved(Exception):
    pass


def type_layout(typ):
    typ = typ.replace("const ", "").replace("volatile ", "").strip()
    if typ in SIZES:
        return "b" * 0 + {1: "b", 2: "w", 4: "d"}[SIZES[typ]]
    if typ in TYPEDEFS:
        return type_layout(TYPEDEFS[typ])
    m = re.match(r"(struct|union)\s+(\w+)$", typ)
    if m:
        return compound_layout(m.group(1), m.group(2))
    if re.match(r"enum\s+\w+$", typ):
        return "d"
    raise Unresolved(typ)


def compound_layout(kind, name):
    body = find_body(kind, name)
    if body is None:
        raise Unresolved(f"{kind} {name}")
    return body_layout(kind, body, name)


UNION_CHOICE = set()   # union member names chosen by the caller


def body_layout(kind, body, name):
    parts = []
    names = []
    for decl in split_fields(body):
        # anonymous nested struct/union
        m = re.match(r"(struct|union)\s*(\w*)\s*\{(.*)\}\s*(\w*)\s*((?:\[[^\]]*\])*)\s*$", decl, re.S)
        if m:
            sub = body_layout(m.group(1), m.group(3), m.group(4) or "anon")
            parts.append(repeat(sub, m.group(5)))
            names.append(m.group(4))
            continue
        decl = re.sub(r"\s+", " ", decl)
        mb = re.match(r"(.+?)\s*(\w+)\s*:\s*(\d+)$", decl)
        if mb:
            raise Unresolved(f"bitfield in {name}: {decl}")
        for i, it in enumerate(decl.split(",")):
            if i == 0:
                mt = re.match(r"\s*((?:const\s+)?(?:struct\s+|union\s+|enum\s+|unsigned\s+|signed\s+)?\w+)\s*(.*)$", it)
                base, rest = mt.group(1), mt.group(2)
            else:
                rest = it
            mi = re.match(r"\s*(\**)\s*(\w+)\s*((?:\[[^\]]*\])*)\s*$", rest)
            if not mi:
                raise Unresolved(f"{name}: {decl}")
            stars, fname, dims = mi.groups()
            lay = "d" if stars else type_layout(base)
            parts.append(repeat(lay, dims))
            names.append(fname)
    if kind == "union":
        members = [p for p in parts]
        size = max(layout_size(p) for p in members)
        if len(set(members)) == 1:
            return members[0]
        chosen = [p for p, n in zip(parts, names) if n in UNION_CHOICE]
        if len(chosen) == 1:
            # the chosen member, padded to the size of the union
            return chosen[0] + "b" * (size - layout_size(chosen[0]))
        raise Unresolved(f"union {name}: members " + " | ".join(
            f"{n}={compress(p)}" for p, n in zip(parts, names)))
    return "".join(parts)


def repeat(lay, dims):
    n = 1
    for d in re.findall(r"\[([^\]]*)\]", dims):
        n *= eval_dim(d)
    return lay * n


def layout_size(lay):
    return sum({"b": 1, "w": 2, "d": 4}[c] for c in lay)


def compress(lay):
    out, i = "", 0
    while i < len(lay):
        j = i
        while j < len(lay) and lay[j] == lay[i]:
            j += 1
        n = j - i
        out += (str(n) if n > 1 else "") + lay[i]
        i = j
    return out


def init():
    global SRC
    SRC = load_sources()
    for m in re.finditer(r"^\s*#define\s+(\w+)\s+\(?\s*(\d+|0x[0-9a-fA-F]+)\s*\)?\s*$", SRC, re.M):
        CONSTS[m.group(1)] = int(m.group(2), 0)
    for m in re.finditer(r"typedef\s+((?:unsigned\s+|signed\s+|struct\s+|union\s+)?\w+)\s+(\w+)\s*;", SRC):
        TYPEDEFS.setdefault(m.group(2), m.group(1))


def main():
    init()
    args = sys.argv[1:]
    header = None
    if args and args[0] == "--header":
        header, args = args[1], args[2:]
    out = []
    for name in args:
        kind = "struct"
        if name.startswith("union:"):
            kind, name = "union", name[6:]
        # STRUCT:Member[:Member] chooses union members; macro name STRUCT_Member
        sel = name.split(":")
        name = sel[0]
        UNION_CHOICE.clear()
        UNION_CHOICE.update(sel[1:])
        label = "_".join(sel)
        try:
            lay = compound_layout(kind, name)
            print(f"{label:28} {layout_size(lay):5}  {compress(lay)}")
            out.append((label, layout_size(lay), compress(lay)))
        except Unresolved as e:
            print(f"{name:28}  ?  {e}")
    if header:
        with open(header, "w", newline="\n") as f:
            f.write("/* Generated by tools/amiga/struct_layout.py - byte order layouts of\n"
                    " * data file structures, for lb_le_fix_struct(). Do not edit. */\n")
            f.write("#ifndef SW_DATA_LAYOUTS_H\n#define SW_DATA_LAYOUTS_H\n\n")
            for name, size, lay in out:
                f.write(f"/* sizeof(struct {name}) == {size} */\n")
                f.write(f'#define LAYOUT_{name} "{lay}"\n')
            f.write("\n#endif\n")


if __name__ == "__main__":
    main()
