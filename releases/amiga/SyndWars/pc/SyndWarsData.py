#!/usr/bin/env python3
"""SyndWarsData - the game data of Syndicate Wars for the Amiga port, made on
a PC (Windows, Linux, macOS) from the GOG version of the game.

Needs only Python 3 (python.org) and the internet (it downloads the three
data packages of SyndWarsFX from GitHub); ffmpeg, when it is installed, makes
the music a little better.  The texts of the game (language/) and conf/ come
with the Amiga package.

  python SyndWarsData.py GOG_FOLDER OUT_FOLDER [--lang eng] [--packages DIR]

GOG_FOLDER  the folder where GOG installed Syndicate Wars (it has the CD image
            SWARS/game.ins and game.gog), or a folder with the files of the
            original CD (the one with the GAME folder)
OUT_FOLDER  a new folder: copy what is in it into the game's drawer on the
            Amiga (next to the program SyndWars)
--lang      eng (default), fre, ger, ita, pol, spa or swe
--packages  a folder with the three zips of SyndWarsFX already downloaded
            (syndwarsfx-levels / -gfx / -sfx, the versions below): no download

What it does (as util/install of SyndWarsFX, then the Amiga's own formats):
 1. reads the CD image: the files, and the 3 music tracks (CD audio)
 2. copies the game's files into OUT_FOLDER (lower case names)
 3. adds the levels, graphics and sounds of the SyndWarsFX packages
 4. sound banks to 8-bit samples for Paula (make_sound_bank.py)
 5. the music to the Amiga's tracks, 22 kHz 8-bit (make_music.py)
 6. config.ini for the Amiga with the language
"""
import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_music  # noqa: E402

# the SyndWarsFX data packages (as util/install of SyndWarsFX)
PACKAGES = [
    ("swfans/syndwarsfx-levels", "syndwarsfx-levels-1_0_1_38", "1.0.1.38",
     "d6448c212093e1ddb653d09d55bcdf64"),
    ("swfans/syndwarsfx-gfx", "syndwarsfx-gfx-1_0_4_312", "1.0.4.312",
     "c4ff17e5ecd577b0148627765d8aa63e"),
    ("swfans/syndwarsfx-sfx", "syndwarsfx-sfx-1_0_1_30", "1.0.1.30",
     "802295bb0bea825a3772d1d120229d93"),
]
LANGS = ["eng", "fre", "ger", "ita", "pol", "spa", "swe"]
LANG_NAMES = {"eng": "English", "fre": "French", "ger": "German", "ita": "Italian",
              "pol": "Polish", "spa": "Spanish", "swe": "Swedish"}


def cd_files():
    """(file on the CD under GAME/, folder in the game) - util/install's list."""
    out = []
    for fn in ["bgtables.dat", "tables.dat", "startgho.dat", "synghost.tab",
               "cities.dat", "mapinsid.dat", "mech.dat", "people.txt", "textwalk.dat"]:
        out.append(("data/" + fn, "data"))
    for n in ["00", "01", "02", "03", "04", "05"]:
        out += [("data/mapout%s.dat" % n, "data"), ("data/tex%s.dat" % n, "data")]
    for n in range(6):
        for ext in ("dat", "tab", "inf"):
            out.append(("data/mspr-%d.%s" % (n, ext), "data"))
    for n in ("s", "z"):
        out += [("data/outro-%s.smk" % n, "data"), ("data/outro-%s.raw" % n, "data")]
    out += [("data/pal.pal", "data"), ("data/s-proj.pal", "data")]
    for n in range(1, 19):
        out.append(("data/equip/net%02d.fli" % n, "data/equip"))
    for fn in ["posdefs.idx", "posdefs.wad", "primveh.obj", "flyby.fli", "pal.pal"]:
        out.append(("qdata/" + fn, "qdata"))
    for n in (10, 11, 12, 13):
        out.append(("qdata/demo-%d.fli" % n, "qdata"))
    for n in range(4):
        out.append(("qdata/pal%d.dat" % n, "qdata"))
    for fn in ["syn_fail", "syn_go", "syn_mc", "zel-go", "zel-mc"]:
        out.append(("qdata/%s.smk" % fn, "qdata"))
    for n in ["000", "001", "002", "003", "004", "005", "006", "007", "008", "009",
              "010", "011", "020", "021", "022", "025", "026", "027", "028", "030",
              "032", "035", "036", "040", "041", "044", "045", "046", "047", "050", "079"]:
        out.append(("maps/map%s.scn" % n, "maps"))
    out += [("sound/music.dat", "sound"), ("sound/sample.ad", "sound")]
    return out


def find_ci(base, rel):
    """The path of rel under base, whatever the case of its letters."""
    path = base
    for part in rel.split("/"):
        if not os.path.isdir(path):
            return None
        names = {n.lower(): n for n in os.listdir(path)}
        if part.lower() not in names:
            return None
        path = os.path.join(path, names[part.lower()])
    return path


def step(text):
    print("\n== " + text, flush=True)


def get_package(proj, name, ver, md5, pkg_dir, tmp):
    for d in (pkg_dir, tmp):
        if d:
            p = os.path.join(d, name + ".zip")
            if os.path.isfile(p) and hashlib.md5(open(p, "rb").read()).hexdigest() == md5:
                return p
    url = "https://github.com/%s/releases/download/%s/%s.zip" % (proj, ver, name)
    p = os.path.join(tmp, name + ".zip")
    print("downloading", url, flush=True)
    urllib.request.urlretrieve(url, p)
    if hashlib.md5(open(p, "rb").read()).hexdigest() != md5:
        sys.exit("%s: not the expected file (MD5)" % p)
    return p


def move_matching(src_dir, dst_dir, lower=False, pattern=None):
    """The files of src_dir (not its folders) into dst_dir."""
    if not src_dir or not os.path.isdir(src_dir):
        return 0
    os.makedirs(dst_dir, exist_ok=True)
    n = 0
    for f in os.listdir(src_dir):
        p = os.path.join(src_dir, f)
        if not os.path.isfile(p):
            continue
        if pattern and not pattern(f.lower()):
            continue
        d = os.path.join(dst_dir, f.lower() if lower else f)
        if not os.path.exists(d):
            shutil.move(p, d)
            n += 1
    return n


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("gog")
    ap.add_argument("out")
    ap.add_argument("--lang", default="eng", choices=LANGS)
    ap.add_argument("--packages")
    a = ap.parse_args()

    if os.path.exists(a.out) and os.listdir(a.out):
        sys.exit("%s is not empty: give a new folder" % a.out)
    os.makedirs(a.out, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix="swdata-")
    try:
        # 1. the CD
        cue = find_ci(a.gog, "SWARS/game.ins") or find_ci(a.gog, "game.ins")
        if cue:
            step("1. the CD image " + cue)
            subprocess.run([sys.executable, os.path.join(HERE, "cdimage.py"), cue,
                            os.path.join(tmp, "cd")], check=True)
            cd = os.path.join(tmp, "cd", "files")
            tracks = os.path.join(tmp, "cd")
        else:
            step("1. the CD files in " + a.gog)
            cd = a.gog
            tracks = None
        game = find_ci(cd, "game")
        if game is None:
            sys.exit("no GAME folder on the CD in %s" % cd)

        # 2. the files of the game
        step("2. the game's files")
        for rel, dst in cd_files():
            src = find_ci(game, rel)
            if src is None:
                sys.exit("missing on the CD: GAME/" + rel)
            os.makedirs(os.path.join(a.out, dst), exist_ok=True)
            shutil.copyfile(src, os.path.join(a.out, dst, os.path.basename(rel)))
        for d in ("levels", "intro", "qdata/equip", "qdata/savegame") + tuple(
                "language/" + l for l in LANGS):
            os.makedirs(os.path.join(a.out, d), exist_ok=True)
        for rel, dst in (("language/%s/syn_ele.smk" % a.lang, "data"),
                         ("language/%s/chu_ele.smk" % a.lang, "data"),
                         ("language/%s/intro.smk" % a.lang, "intro")):
            src = find_ci(game, rel)
            if src is None:
                sys.exit("missing on the CD: GAME/" + rel)
            shutil.copyfile(src, os.path.join(a.out, dst, os.path.basename(rel)))

        # 3. the SyndWarsFX packages
        step("3. the SyndWarsFX data packages")
        zips = [get_package(*p, a.packages, tmp) for p in PACKAGES]
        for z in zips:
            ex = os.path.join(tmp, "pkg")
            shutil.rmtree(ex, ignore_errors=True)
            with zipfile.ZipFile(z) as zf:
                zf.extractall(ex)
            root = find_ci(ex, "syndwarsfx") or ex
            move_matching(find_ci(root, "LEVELS"), os.path.join(a.out, "levels"), True,
                          lambda f: f.startswith("c"))
            move_matching(find_ci(root, "MAPS"), os.path.join(a.out, "maps"), True,
                          lambda f: f.startswith("map"))
            for d in ("data", "data/equip", "qdata", "qdata/equip"):
                move_matching(find_ci(root, d), os.path.join(a.out, d))
            for l in LANGS:
                move_matching(find_ci(root, "language/" + l), os.path.join(a.out, "language", l))
            move_matching(find_ci(root, "sound"), os.path.join(a.out, "sound"),
                          pattern=lambda f: f.endswith(".dat"))
            print(os.path.basename(z), "unpacked")

        # 4. the sound banks for Paula: the language's and the credits'
        step("4. the sound banks")
        snd = os.path.join(a.out, "sound")
        for f in sorted(os.listdir(snd)):
            fl = f.lower()
            if fl.startswith("sound_") and fl.endswith(".dat") and fl != "sound_%s.dat" % a.lang:
                os.remove(os.path.join(snd, f))      # other languages: not needed
                continue
            if (fl.startswith("sound_") and fl.endswith(".dat")) or fl == "syncreds.dat":
                tmpf = os.path.join(tmp, f)
                shutil.move(os.path.join(snd, f), tmpf)
                subprocess.run([sys.executable, os.path.join(HERE, "make_sound_bank.py"),
                                tmpf, os.path.join(snd, fl)], check=True)

        # 5. the music
        if tracks:
            step("5. the music (a few minutes)")
            mus = os.path.join(a.out, "music")
            os.makedirs(mus, exist_ok=True)
            for n in (1, 2, 3):
                src = os.path.join(tracks, "track_%d.wav" % n)
                if not os.path.isfile(src):
                    continue
                dst = os.path.join(mus, "track_%d.smu" % n)
                # ffmpeg, when there is one, filters better; else Python alone
                if shutil.which("ffmpeg"):
                    make_music.convert(src, dst, 22050)
                else:
                    make_music.convert_wav(src, dst)
        else:
            step("5. no CD image: no music (the game plays without it)")

        # 6. config.ini
        with open(os.path.join(a.out, "config.ini"), "w", newline="\n") as f:
            f.write('CD="PROGDIR:"\nInstallDrive="PROGDIR:"\nLanguage="%s"\n'
                    'Intro="Max"\nAnims="Max"\nSound="Max"\nLevels="Max"\nData="Max"\n'
                    'Maps="Max"\nDOS="Max"\n' % LANG_NAMES[a.lang])
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    total = sum(os.path.getsize(os.path.join(r, f)) for r, _, fs in os.walk(a.out) for f in fs)
    print("\ndone: %s, %d MB. Copy all of it into the game's drawer on the Amiga"
          " (next to the program SyndWars)." % (a.out, total // 1000000))


if __name__ == "__main__":
    main()
