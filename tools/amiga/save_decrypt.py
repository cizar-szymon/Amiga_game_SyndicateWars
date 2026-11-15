#!/usr/bin/env python3
"""Decrypts a Syndicate Wars save (format 9+, little endian as on the PC).

usage: python tools/amiga/save_decrypt.py FILE.sav [OUT.bin] [--salt N]
       python tools/amiga/save_decrypt.py --diff A.sav B.sav
Writes the plain buffer (without the 25-byte description and the header),
checks the hash; --diff lists differing bytes of two saves with the field
they fall in.  The non-mortal saves use the fixed salt; a mortal save
(synwarsm.sav) needs --salt from keys.dat / the settings file.
"""
import struct
import sys

M32 = 0xFFFFFFFF


def lag_pos(pos, lag, n):
    def sb(v):
        v &= 0xFF
        return v - 0x100 if v >= 0x80 else v
    p = sb(pos - lag)
    if p < 0:
        p = sb(p + n)
    return p


def make_tables(salt):
    seed = salt & M32
    tbls = []
    for n in (56, 58, 59):
        t = []
        for _ in range(n):
            seed = (seed * 0xBB40E62D) & M32
            t.append((seed >> 8) & 0xFFFF)
        tbls.append(t)
    return tbls


class Crypto:
    def __init__(self, salt):
        self.t = make_tables(salt)
        self.ts = [0, 0, 0]        # save_crypto_tables_state
        self.ds = [0, 0, 0]        # save_crypto_data_state (carries)
        self.params = [(56, 55, 24, 0), (58, 57, 7, 1), (59, 58, 19, 0)]
        for k in range(3):
            self.step(k, 1)

    def step(self, k, advance):
        n, l1, l2, lagsrc = self.params[k]
        tbl = self.t[k]
        pos = self.ts[k]
        if not advance:
            return tbl[pos]
        lag_state = self.ts[lagsrc]
        self.ds[k] = 0
        a = lag_pos(lag_state, l1, n)
        b = lag_pos(lag_state, l2, n)
        tbl[pos] = (tbl[a] + tbl[b]) & M32
        val = tbl[pos]
        pos += 1
        if pos > n - 1:
            pos = 0
        self.ts[k] = pos
        if val < tbl[a]:
            self.ds[k] = 1
        return val

    def key(self):
        ds = self.ds
        sel = (ds[1] << 8) | (ds[0] << 16) | ds[2]
        t1, t2, t3 = (lambda a: self.step(0, a)), (lambda a: self.step(1, a)), (lambda a: self.step(2, a))
        if sel in (0x00001, 0x10100):
            k = t1(1); k ^= t2(1); k ^= t3(0)
        elif sel in (0x00100, 0x10001):
            k = t1(1); k ^= t2(0); k ^= t3(1)
        elif sel in (0x00000, 0x10101):
            k = t1(1); k ^= t2(1); k ^= t3(1)
        elif sel in (0x00101, 0x10000):
            k = t1(0); k ^= t2(1); k ^= t3(1)
        else:
            k = 0
        return k & M32


def decrypt(data, salt=0x5E280A1D):
    desc = data[:25]
    gblen, fmtver = struct.unpack_from("<II", data, 25)
    buf = bytearray(data[33:33 + gblen])
    verify = data[33 + gblen:37 + gblen]
    if fmtver >= 9:
        c = Crypto(salt)
        for i in range(0, 4 * (gblen // 4), 4):
            w = struct.unpack_from("<I", buf, i)[0] ^ c.key()
            struct.pack_into("<I", buf, i, w)
    h = bytearray(4)
    for i in range(0, 4 * (gblen // 4), 4):
        for j in range(4):
            h[j] ^= buf[i + j]
    return desc, fmtver, bytes(buf), bytes(h) == verify


def fields(buf):
    """(start, name) of the parts of a format 12 buffer."""
    out, o = [], 0
    for name, size in (("Credits", 4), ("AgentInfo", 389), ("ResearchInfo", 1372),
                       ("PlayerInfo", 426), ("SynTime x3", 15), ("counts", 12)):
        out.append((o, name))
        o += size
    ne, nb, nm = buf[o - 12], buf[o - 10], buf[o - 2]
    for name, size in (("MissionStatus", 40), ("emails", 5 * (ne + nb + nm)),
                       ("ingame.MissionStatus", 2), ("mission_open", 100),
                       ("mission_state", 100), ("next_mission", 2)):
        out.append((o, name))
        o += size
    nmis = buf[o - 2] | (buf[o - 1] << 8)
    out.append((o, "missions SpecialTrigger+Complete"))
    o += max(nmis - 1, 0) * 4
    out.append((o, "login_name"))
    o += 16
    out.append((o, "cities Info, AutoResearch, padding"))
    return out


def field_of(fl, i):
    name, start = "?", 0
    for s, n in fl:
        if s <= i:
            name, start = n, s
    return f"{name}+{i - start}"


def main():
    a = sys.argv[1:]
    salt = 0x5E280A1D
    if "--salt" in a:
        k = a.index("--salt")
        salt = int(a[k + 1], 0)
        del a[k:k + 2]
    if a and a[0] == "--diff":
        r = [decrypt(open(f, "rb").read(), salt) for f in a[1:3]]
        for f, (_, fv, _, ok) in zip(a[1:3], r):
            print(f"{f}: format {fv}, hash {'ok' if ok else 'BAD'}")
        b1, b2 = r[0][2], r[1][2]
        if len(b1) != len(b2):
            print(f"lengths differ: {len(b1)} {len(b2)}")
        fl = fields(b1)
        n = 0
        for i in range(min(len(b1), len(b2))):
            if b1[i] != b2[i]:
                n += 1
                print(f"{i:5} {field_of(fl, i):40} {b1[i]:02x} {b2[i]:02x}")
        print(f"{n} bytes differ")
        return
    desc, fv, buf, ok = decrypt(open(a[0], "rb").read(), salt)
    print(f"desc {desc.split(b'\\0')[0]!r} format {fv} length {len(buf)} hash {'ok' if ok else 'BAD'}")
    if len(a) > 1:
        open(a[1], "wb").write(buf)


if __name__ == "__main__":
    main()
