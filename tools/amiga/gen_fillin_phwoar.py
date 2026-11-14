"""Symbolic run of ASM_fillin_phwoar_type_info for each phwoar type -> C.

usage: python tools/amiga/asmfn.py ASM_fillin_phwoar_type_info > fill.s
       python tools/amiga/gen_fillin_phwoar.py fill.s > switch.c
The function is a binary tree of type comparisons with straight blocks
setting the phwoar fields; each of the 256 types is run through it and
the types with the same statements are grouped into one case. Used for
fillin_phwoar_type_info_c() in src/bmbang.c.
"""
import os
import re
import sys
from collections import OrderedDict

lines = [l.rstrip('\n') for l in open(sys.argv[1])]
code = []
labels = {}
for l in lines:
    s = l.strip()
    if not s or s.startswith('=====') or s.startswith('GLOBAL_FUNC'):
        continue
    if s.endswith(':'):
        labels[s[:-1]] = len(code)
        continue
    code.append(re.sub(r"[ \t]+", " ", s))

FIELDS = {0x0c: 'vx', 0x0d: 'vy', 0x0e: 'vz', 0x0f: 'type', 0x10: 'rabbit', 0x11: 'gestation',
          0x12: 'die', 0x13: 'vf', 0x14: 'f', 0x1a: 'fc', 0x1b: 'shit'}


def num(t):
    return int(t, 16) if t.startswith('0x') else int(t)


def run(tval):
    pc = 0
    ah = tval
    al = 'p_phwoar->type'
    ax = None
    flags = None
    out = []
    nrand = 0
    steps = 0
    while True:
        steps += 1
        if steps > 1000:
            raise RuntimeError('loop')
        ins = code[pc]
        pc += 1
        op, _, args = ins.partition(' ')
        args = args.strip()
        if op == 'push' or op == 'sub' and args == '$0x20,%esp':
            continue
        if op == 'mov' and args == '%eax,%edx':
            continue
        if op == 'mov' and args == '0xf(%eax),%ah':
            continue
        if op == 'mov' and args == '%ah,%al':
            al = 'p_phwoar->type'
            continue
        if op == 'cmp':
            m = re.match(r'\$(0x[0-9a-f]+|\d+),%ah', args)
            flags = num(m.group(1))
            continue
        if op in ('jb', 'jbe', 'je', 'jmp', 'ja', 'jae', 'jne'):
            tgt = args
            take = {'jb': ah < (flags if flags is not None else 0),
                    'jbe': ah <= (flags or 0),
                    'je': ah == flags, 'jne': ah != flags,
                    'ja': ah > (flags or 0), 'jae': ah >= (flags or 0),
                    'jmp': True}[op]
            if take:
                pc = labels[tgt]
            continue
        if op == 'mov' and args == 'nstart_ani,%eax':
            continue
        m = re.match(r'mov (0x[0-9a-f]+)\(%eax\),%ax$', ins)
        if m:
            ax = 'nstart_ani[%d]' % (num(m.group(1)) // 2)
            continue
        m = re.match(r'mov %ax,(0x[0-9a-f]+)\(%edx\)$', ins)
        if m:
            out.append('p_phwoar->%s = %s;' % (FIELDS[num(m.group(1))], ax))
            continue
        if op == 'call' and args == 'ac_LbRandomAnyShort':
            nrand += 1
            out.append('rnd = LbRandomAnyShort();')
            al = 'rnd'
            continue
        if op == 'call' and args == 'ac_sprintf':
            out.append('/* bad type */')
            continue
        m = re.match(r'(and|add|sub) \$(0x[0-9a-f]+|\d+),%al$', ins)
        if m:
            o = {'and': '&', 'add': '+', 'sub': '-'}[m.group(1)]
            al = '(%s %s 0x%x)' % (al, o, num(m.group(2)))
            continue
        if ins == 'inc    %al' or (op == 'inc' and args == '%al'):
            al = '(%s + 1)' % al
            continue
        m = re.match(r'movb \$(0x[0-9a-f]+|\d+),(0x[0-9a-f]+)\(%edx\)$', ins)
        if m:
            out.append('p_phwoar->%s = 0x%x;' % (FIELDS[num(m.group(2))], num(m.group(1))))
            continue
        m = re.match(r'mov %al,(0x[0-9a-f]+)\(%edx\)$', ins)
        if m:
            out.append('p_phwoar->%s = (ubyte)%s;' % (FIELDS[num(m.group(1))], al))
            continue
        if op in ('xor', 'lea', 'add') and ('%esp' in args or args == '%eax,%eax'):
            continue
        if op == 'mov' and args == '0xf(%edx),%al':
            continue
        if op == 'pop':
            continue
        if op == 'ret':
            return out
        raise RuntimeError('unknown: %s (type %d)' % (ins, tval))


groups = OrderedDict()
for t in range(256):
    body = tuple(run(t))
    groups.setdefault(body, []).append(t)

print('/* generated from ASM_fillin_phwoar_type_info by a symbolic run */')
print('    switch (p_phwoar->type)')
print('    {')
for body, types in groups.items():
    if body == ('/* bad type */',):
        continue
    for t in types:
        print('    case 0x%02X:' % t)
    for st in body:
        print('        ' + st)
    print('        break;')
print('    default:')
print('        LOGERR("Bad phwoar type %d", (int)p_phwoar->type);')
print('        break;')
print('    }')
bad = [t for b, ts in groups.items() if b == ('/* bad type */',) for t in ts]
sys.stderr.write('%d groups, bad types %d\n' % (len(groups), len(bad)))
