#!/usr/bin/env python3
"""Make the Amiga sound bank from the PC one.

The GOG sound banks (sound/sound_LANG.dat) hold 16-bit 22 kHz WAV samples
(banks 1622 and 822 both point to them). Paula plays signed 8-bit samples,
so every sample is converted once here to an 8-bit WAV whose data bytes are
SIGNED (unlike normal 8-bit WAV); the Amiga sound driver knows that.

The file layout is kept: sample data, the table of 32-byte entries
(DataBeg is an offset into the data, DataEnd the length), 18 unknown bytes,
9 bank heads (TabPos, DatPos, TabSize, DatSize, -1 for a missing bank) and
the position of the 18 bytes in the last 4 bytes; all little endian.
Banks 1622 and 822 of the output point to the converted samples.

usage: python tools/amiga/make_sound_bank.py IN.dat OUT.dat [--rate 22050]
"""
import argparse
import struct


def parse_wav(b):
    assert b[:4] == b"RIFF" and b[8:12] == b"WAVE", "not a WAV"
    p = 12
    fmt = data = None
    while p + 8 <= len(b):
        cid = b[p:p + 4]
        size = struct.unpack("<I", b[p + 4:p + 8])[0]
        body = b[p + 8:p + 8 + size]
        if cid == b"fmt ":
            fmt = struct.unpack("<HHIIHH", body[:16])
        elif cid == b"data":
            data = body
        p += 8 + size + (size & 1)
    assert fmt is not None and data is not None
    return fmt, data


def to_s8(fmt, data, rate_out):
    tag, ch, rate, _, _, bits = fmt
    assert tag == 1, "not PCM"
    if bits == 16:
        n = len(data) // 2
        vals = struct.unpack("<%dh" % n, data[:2 * n])
    elif bits == 8:
        vals = [(v - 128) << 8 for v in data]
    else:
        raise ValueError("bits %d" % bits)
    if ch == 2:  # mix to mono
        vals = [(vals[i] + vals[i + 1]) // 2 for i in range(0, len(vals) - 1, 2)]
    if rate_out and rate_out != rate:
        step = rate / rate_out
        out_n = int(len(vals) / step)
        vals = [vals[int(i * step)] for i in range(out_n)]
        rate = rate_out
    out = bytearray(len(vals))
    for i, v in enumerate(vals):
        v = (v + 128) >> 8
        if v > 127:
            v = 127
        out[i] = v & 0xFF
    return rate, bytes(out)


def make_wav(rate, s8):
    fmt = struct.pack("<HHIIHH", 1, 1, rate, rate, 1, 8)
    body = b"WAVE" + b"fmt " + struct.pack("<I", 16) + fmt + b"data" + struct.pack("<I", len(s8)) + s8
    if len(s8) & 1:
        body += b"\0"
    return b"RIFF" + struct.pack("<I", len(body)) + body


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("inp")
    ap.add_argument("out")
    ap.add_argument("--rate", type=int, default=0, help="resample (0: keep)")
    a = ap.parse_args()
    d = open(a.inp, "rb").read()
    pos = struct.unpack("<i", d[-4:])[0]
    unk = d[pos:pos + 18]
    heads = [list(struct.unpack("<4i", d[pos + 18 + 16 * i:pos + 34 + 16 * i])) for i in range(9)]
    src = 2  # bank 1622
    tab, dat, tsz, dsz = heads[src]
    n = tsz // 32
    data = bytearray()
    table = bytearray()
    for k in range(n):
        e = bytearray(d[tab + 32 * k:tab + 32 * k + 32])
        beg = struct.unpack("<i", e[18:22])[0]
        size = struct.unpack("<i", e[26:30])[0]
        if size > 0:
            fmt, pcm = parse_wav(d[dat + beg:dat + beg + size])
            rate, s8 = to_s8(fmt, pcm, a.rate)
            w = make_wav(rate, s8)
        else:
            w = b""
        e[18:22] = struct.pack("<i", len(data))
        e[26:30] = struct.pack("<i", len(w))
        data += w
        while len(data) & 3:
            data += b"\0"
        table += e
    out = bytearray()
    new_dat = 0
    out += data
    new_tab = len(out)
    out += table
    new_pos = len(out)
    out += unk
    for i in range(9):
        if i in (2, 6):
            out += struct.pack("<4i", new_tab, new_dat, len(table), len(data))
        else:
            out += struct.pack("<4i", -1, -1, -1, -1)
    out += struct.pack("<i", new_pos)
    open(a.out, "wb").write(out)
    print("%s: %d samples, %d -> %d bytes" % (a.out, n, len(d), len(out)))


if __name__ == "__main__":
    main()
