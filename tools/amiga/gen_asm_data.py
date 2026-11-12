#!/usr/bin/env python3
"""Generate C definitions of data symbols which are defined in the x86 .sx
files, for the build without asm (Amiga).

For each requested symbol:
  * the C type is taken from its `extern` declaration in the sources,
  * the initial value from the asm data directives (little endian image),
    decoded per element of the C type (scalar types and arrays of them),
  * zero-filled data needs no initializer.
Symbols which cannot be converted automatically (structs with non-zero data,
pointers in data) are listed for manual handling.

Usage: python tools/amiga/gen_asm_data.py UNDEFINED.txt OUT.c
"""
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
import asm_inventory as inv   # noqa: E402

SCALARS = {
    "ubyte": ("B", 1), "sbyte": ("b", 1), "char": ("b", 1), "unsigned char": ("B", 1),
    "TbBool": ("B", 1), "TbPixel": ("B", 1),
    "ushort": ("H", 2), "short": ("h", 2), "u16": ("H", 2), "s16": ("h", 2),
    "ThingIdx": ("h", 2), "uint16_t": ("H", 2), "int16_t": ("h", 2),
    "ulong": ("I", 4), "long": ("i", 4), "u32": ("I", 4), "s32": ("i", 4),
    "uint": ("I", 4), "int": ("i", 4), "unsigned int": ("I", 4), "TbResult": ("i", 4),
    "uint32_t": ("I", 4), "int32_t": ("i", 4), "TbClockMSec": ("I", 4),
    "TbFileHandle": ("I", 4),
}


def asm_data_image(names):
    """Return {name: (bytes or None, reason)} from the .sx data sections."""
    want = set(names)
    res = {}
    for f in inv.ASM_FILES:
        text = open(os.path.join(inv.ROOT, f), encoding="latin-1").read()
        text = re.sub(r"/\*.*?\*/", lambda mc: "\n" * mc.group(0).count("\n"), text, flags=re.S)
        lines = text.split("\n")
        section = "text"
        cur = None
        for line in lines:
            line = re.sub(r"/\*.*?\*/", "", line)
            line = line.split("#")[0] if not ('"' in line) else line
            s = line.strip()
            m = re.match(r"\.(text|data|bss|section\s+\.?(\w+))", s)
            if m:
                section = m.group(2) or m.group(1)
                cur = None
                continue
            m = inv.RE_GLOBAL.match(line) or inv.RE_LABEL.match(line)
            if m:
                name = m.group(1)
                cur = name if (name in want and section != "text") else None
                if cur is not None:
                    res[cur] = [bytearray(), None]
                continue
            if cur is None or not s:
                continue
            img = res[cur]
            m = re.match(r"\.(byte|short|word|long|int|fill|space|zero|skip|ascii|string|asciz|align)\b\s*(.*)", s)
            if not m:
                img[1] = f"unknown line: {s}"
                continue
            d, args = m.group(1), m.group(2)
            try:
                if d == "byte":
                    for a in args.split(","):
                        img[0] += struct.pack("<B", int(a.strip(), 0) & 0xFF)
                elif d in ("short", "word"):
                    for a in args.split(","):
                        img[0] += struct.pack("<H", int(a.strip(), 0) & 0xFFFF)
                elif d in ("long", "int"):
                    for a in args.split(","):
                        img[0] += struct.pack("<I", int(a.strip(), 0) & 0xFFFFFFFF)
                elif d in ("fill", "space", "zero", "skip"):
                    p = [x.strip() for x in args.split(",")]
                    n = int(p[0], 0)
                    size = int(p[1], 0) if len(p) > 1 else 1
                    val = int(p[2], 0) if len(p) > 2 else 0
                    if d in ("space", "zero", "skip"):
                        size, val = 1, (int(p[1], 0) if len(p) > 1 else 0)
                    img[0] += (val & ((1 << (8 * size)) - 1)).to_bytes(size, "little") * n
                elif d in ("ascii", "string", "asciz"):
                    sval = re.findall(r'"((?:[^"\\]|\\.)*)"', args)
                    for sv in sval:
                        b = bytes(sv, "latin-1").decode("unicode_escape").encode("latin-1")
                        img[0] += b
                        if d != "ascii":
                            img[0] += b"\0"
                elif d == "align":
                    pass   # alignment between symbols, not part of the data
            except ValueError:
                img[1] = f"symbolic value: {s}"
    return res


def find_decls(names):
    want = set(names)
    decls = {}
    dirs = ["src", "bflibrary", "bfsoundlib", "bfsmacklib", "swrendersoft", "bfariadne"]
    for d in dirs:
        for dp, _, fns in os.walk(os.path.join(inv.ROOT, d)):
            for fn in fns:
                if not fn.endswith((".c", ".h")):
                    continue
                text = open(os.path.join(dp, fn), encoding="latin-1").read()
                for m in re.finditer(r"^[ \t]*extern[ \t]+([^;(){}\"\n]+);", text, re.M):
                    items = m.group(1).split(",")
                    mt = re.match(r"\s*((?:const\s+)?(?:struct\s+|unsigned\s+)?\w+)\s*(.*)$",
                                  items[0], re.S)
                    if not mt:
                        continue
                    base, first = mt.group(1), mt.group(2)
                    for it in [first] + items[1:]:
                        mi = re.match(r"\s*(\**)\s*(\w+)\s*((?:\[[^\]]*\])*)\s*$", it)
                        if not mi:
                            continue
                        stars, name, dims = mi.group(1), mi.group(2), mi.group(3)
                        typ = (base + " " + stars).strip()
                        if name in want and name not in decls:
                            decls[name] = (typ, dims,
                                           os.path.relpath(os.path.join(dp, fn), inv.ROOT))
    return decls


def eval_dims(dims, consts):
    out = []
    for d in re.findall(r"\[([^\]]*)\]", dims):
        expr = d.strip()
        for k, v in consts.items():
            expr = re.sub(r"\b%s\b" % k, str(v), expr)
        try:
            out.append(int(eval(expr, {}, {})) if expr else None)
        except Exception:
            out.append(("?", d))
    return out


def collect_consts():
    consts = {}
    for d in ["src", "bflibrary/include", "swrendersoft/include", "bfariadne/include"]:
        for dp, _, fns in os.walk(os.path.join(inv.ROOT, d)):
            for fn in fns:
                if fn.endswith(".h"):
                    for m in re.finditer(r"^#define\s+(\w+)\s+\(?\s*(\d+|0x[0-9a-fA-F]+)\s*\)?\s*$",
                                         open(os.path.join(dp, fn), encoding="latin-1").read(), re.M):
                        consts[m.group(1)] = int(m.group(2), 0)
    return consts


def main():
    names = [l.strip() for l in open(sys.argv[1]) if l.strip()]
    data = asm_data_image(names)
    names = [n for n in names if n in data]
    decls = find_decls(names)
    consts = collect_consts()
    out = ["/* Generated by tools/amiga/gen_asm_data.py - data symbols of the x86",
           " * asm files (src/*.sx), for the build without asm. Do not edit. */",
           '#include "asm_data_types.h"', ""]
    manual = []
    for n in sorted(names):
        img, err = data[n]
        if n not in decls:
            manual.append(f"{n}: no extern declaration ({len(img)} bytes)")
            continue
        typ, dims, where = decls[n]
        zero = (err is None) and not any(img)
        decl = f"{typ} {n}{dims}"
        if zero and where.endswith(".c") and typ.startswith("struct"):
            # type private to a .c file: same size, zeroed
            # short elements: word alignment (the maximum of the Amiga hunk format)
            out.append(f"short {n}[{(len(img) + 1) // 2}]; /* {decl}, {where} */")
            continue
        if zero and dims == "[]":
            # "T n[];" without an initializer is a 1-element array: give the
            # size of the asm data (low_trans_grey_bright_limit, 256 bytes,
            # used to overwrite the 255 bytes of variables after it)
            base = typ.replace("const ", "").strip()
            if base not in SCALARS:
                manual.append(f"{n}: {decl} ({where}): unsized array of non-scalar type")
                continue
            count = len(img) // SCALARS[base][1]
            out.append(f"{typ} {n}[{count}]; /* {where}, {len(img)} bytes */")
            continue
        if zero:
            out.append(f"{decl}; /* {where}, {len(img)} bytes */")
            continue
        base = typ.replace("const ", "").strip()
        if err is not None or base not in SCALARS:
            manual.append(f"{n}: {decl} ({where}): {err or 'non-scalar type with data'}")
            continue
        fmt, size = SCALARS[base]
        dimv = eval_dims(dims, consts)
        count = len(img) // size
        vals = [struct.unpack_from("<" + fmt, img, i * size)[0] for i in range(count)]
        if not dimv:
            out.append(f"{decl} = {vals[0]};")
        elif len(dimv) == 1:
            out.append(f"{decl} = {{{', '.join(str(v) for v in vals)}}};")
        else:
            # brace elision: a flat list initializes a multi-dim array
            out.append(f"{decl} = {{{', '.join(str(v) for v in vals)}}};")
    with open(sys.argv[2], "w") as f:
        f.write("\n".join(out) + "\n")
    print(f"{len(names)} asm data symbols, {len(manual)} need manual handling:")
    for m in manual:
        print("  " + m)


if __name__ == "__main__":
    main()
