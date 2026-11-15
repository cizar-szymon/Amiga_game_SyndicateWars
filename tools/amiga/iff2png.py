#!/usr/bin/env python3
"""IFF PBM (chunky 8-bit, as LbIffSave writes the screen of a bug report)
to PNG.

usage: python tools/amiga/iff2png.py SCREEN.iff [OUT.png]
"""
import struct
import sys

from PIL import Image


def chunks(data):
    pos = 12
    while pos + 8 <= len(data):
        name = data[pos:pos + 4]
        size = struct.unpack('>I', data[pos + 4:pos + 8])[0]
        yield name, data[pos + 8:pos + 8 + size]
        pos += 8 + size + (size & 1)


def unpack_byterun1(src, size):
    out = bytearray()
    i = 0
    while len(out) < size and i < len(src):
        n = src[i]
        i += 1
        if n < 128:
            out += src[i:i + n + 1]
            i += n + 1
        elif n > 128:
            out += bytes([src[i]]) * (257 - n)
            i += 1
    return bytes(out[:size])


def main():
    data = open(sys.argv[1], 'rb').read()
    if data[:4] != b'FORM' or data[8:12] != b'PBM ':
        raise SystemExit('not an IFF PBM')
    w = h = comp = 0
    pal = body = None
    for name, c in chunks(data):
        if name == b'BMHD':
            w, h = struct.unpack('>HH', c[:4])
            comp = c[10]
        elif name == b'CMAP':
            pal = c
        elif name == b'BODY':
            body = c
    row = w + (w & 1)
    pix = unpack_byterun1(body, row * h) if comp == 1 else body[:row * h]
    pix = b''.join(pix[y * row:y * row + w] for y in range(h))
    im = Image.frombytes('P', (w, h), pix)
    if pal:
        im.putpalette(list(pal) + [0] * (768 - len(pal)))
    out = sys.argv[2] if len(sys.argv) > 2 else sys.argv[1].rsplit('.', 1)[0] + '.png'
    im.save(out)
    print(f'{out}: {w}x{h}')


if __name__ == '__main__':
    main()
