#!/usr/bin/env python3
"""ASFILTER for an asm coverage build of SyndWarsFX (x86 only, diagnostic).

Reads preprocessed GNU as source on stdin and writes it to stdout with a
"hit" marker after every function label in .text:

    name:
        movl $1, .Lcov_name+4        # MOV does not change flags or registers

and one entry per label in section .data$covb: {const char *name; u32 hit; u32 mask;}.
src/asm_coverage.c finds the entries between its markers in .data$cova / .data$covc
(the PE linker sorts grouped sections by the text after '$').

A label gets a marker only if the next statement is an instruction (jump tables
like vtable_* also live in .text). `jump_*` labels are branch targets inside a
function and are not marked.

Use: make ASFILTER="| python3 <repo>/tools/amiga/asm_cov_filter.py"
"""
import re
import sys

RE_LEAD_LABEL = re.compile(r"^\s*([A-Za-z_.][\w.]*):\s*")
RE_SECTION = re.compile(r"^\s*\.(text|data|bss|section\s+([^,\s]+))")


def statements(line):
    if '"' in line or line.lstrip().startswith("#"):
        return [line]
    return line.split(";")


def main():
    src = sys.stdin.read().split("\n")
    out = []
    entries = []
    seen = set()
    section = "text"
    pending = []          # labels waiting for the next statement

    def flush(is_instr):
        for name in pending:
            if is_instr and section == "text" and name not in seen:
                seen.add(name)
                out.append(f"\tmovl $1, .Lcov_{name}+4")
                entries.append(name)
        pending.clear()

    for line in src:
        stmts = statements(line)
        for st in stmts:
            s = st.strip()
            if not s or s.startswith("#"):
                if not s.startswith("#"):
                    continue
                out.append(st)
                continue
            m = RE_SECTION.match(s)
            if m:
                flush(False)
                section = "text" if m.group(1) == "text" else (m.group(2) or m.group(1))
                if section.startswith(".text"):
                    section = "text"
                out.append(st)
                continue
            # one or more labels at the start ("_ASM_x: ASM_x:" from GLOBAL_FUNC)
            labels = []
            while True:
                m = RE_LEAD_LABEL.match(s)
                if not m:
                    break
                labels.append(m.group(1))
                s = s[m.end():]
            if labels:
                out.append(" ".join(l + ":" for l in labels))
                names = [l for l in labels if not l.startswith(("jump_", "L", "."))]
                plain = [l for l in names if not l.startswith("_")]
                if names:
                    pending.append((plain or names)[0])
                if not s:
                    continue
                st = s
            if s.startswith("."):
                # .global etc. do not end the label run; data directives do
                if re.match(r"\.(global|globl|type|size|p2align|align|balign)\b", s):
                    out.append(st)
                    continue
                flush(False)
                out.append(st)
                continue
            flush(True)
            out.append(st)

    out.append("\t.section .data$covb,\"w\"\n\t.p2align 4")
    for name in entries:
        out.append(f".Lcov_{name}:\n\t.long .Lcovs_{name}\n\t.long 0\n\t.long 0\n\t.long 0")
    out.append("\t.section .rdata$covs,\"dr\"")
    for name in entries:
        out.append(f".Lcovs_{name}:\n\t.asciz \"{name}\"")
    out.append("\t.text")
    sys.stdout.write("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
