#!/bin/bash
# The release of the Amiga port (after tools/amiga/build_amiga.sh,
# tools/warp/build_arm.sh and tools/warp/build_host.sh); the number from
# build/amiga/sw_version.h (what the program has). Made again each time:
#   releases/amiga/SyndWars/            the program, its icon, swhost, swcore.wxt,
#                                       config.ini, conf/ and the texts of the game
#                                       (language/*/alltext-*, guitext.dat: built from
#                                       the GPL sources of SyndWarsFX)
#   releases/amiga/SyndWars/doc/        all texts (PL/EN, Install, history, licences),
#                                       with MultiView icons; doc.info
#   releases/amiga/SyndWars/pc/         SyndWarsData.py and its modules: the game's
#                                       data from the GOG version, on a PC
#   releases/amiga/SyndWars.info        the game drawer's icon
#   releases/amiga/SyndWars-<v>.zip     the same as an archive - no game data (in git)
#   private/SyndWars-<v>-full.zip       the same + the game's data ("Syndicate
#                                       Wars/amiga_data": runs at once; only on this PC)
# The public tree may hold only the files of PUBLIC below: the script refuses
# when game data gets in. Older archives go to private/old (kept).
#   sh tools/amiga/make_release.sh [--no-full]
set -e
cd "$(dirname "$0")/../.."
V=$(sed -n 's/^#define SW_PORT_VERSION "\(.*\)"/\1/p' build/amiga/sw_version.h)
DATE=$(date +%d.%m.%Y)
D=docs/release/amiga
R=releases/amiga
G="$R/SyndWars"
DATA="Syndicate Wars/amiga_data"

[ -n "$V" ] || { echo "no build/amiga/sw_version.h: build first" >&2; exit 1; }
grep -a -q "\$VER: SyndWars $V " build/amiga/SyndWars || { echo "build/amiga/SyndWars is not $V" >&2; exit 1; }
for f in build/warp/swhost build/warp/swcore.wxt; do
	[ -s "$f" ] || { echo "$f is missing: tools/warp/build_*.sh" >&2; exit 1; }
	[ "$f" -nt platform/amiga/smk_core.c ] || echo "warning: $f older than smk_core.c" >&2
done

# the public tree, made again
rm -rf "$G"
mkdir -p "$G/doc" "$G/pc" "$G/conf"
cp build/amiga/SyndWars build/warp/swhost build/warp/swcore.wxt "$G/"
cp platform/amiga/release/SyndWars.info "$G/SyndWars.info"	# (ARM) (SOUND=AHI) (MUSIC=OFF)
cp platform/amiga/release/config.ini "$G/config.ini"
# conf: the GPL settings of SyndWarsFX (as the game reads them)
cp -r conf/Tone_000 "$G/conf/"
for f in conf/*.ini conf/*.cfg; do cp "$f" "$G/conf/"; done
# the texts of the game, built from lang/*.po of SyndWarsFX (GPL)
for l in eng fre ger ita pol spa swe; do
	mkdir -p "$G/language/$l"
	for f in "$DATA/language/$l"/alltext-*.idx "$DATA/language/$l"/alltext-*.wad "$DATA/language/$l"/guitext.dat; do
		[ -e "$f" ] && cp "$f" "$G/language/$l/"
	done
done
cp tools/amiga/SyndWarsData.py tools/amiga/cdimage.py tools/amiga/make_sound_bank.py \
   tools/amiga/make_music.py "$G/pc/"
for f in SyndWars.txt SyndWars_en.txt SyndWars_dev.txt SyndWars_dev_en.txt \
         SyndWars_release.txt SyndWars_release_en.txt Install.txt; do
	sed -e "s/@VERSION@/$V/g" -e "s/@DATE@/$DATE/g" "$D/$f" > "$G/doc/$f"
done
cp COPYING "$G/doc/COPYING"
cp third_party/libsmacker/COPYING "$G/doc/libsmacker-COPYING"
# the drawers and the texts' icons (opened by MultiView)
cp tools/amiga/icon/warp-drawer.info "$R/SyndWars.info"
python tools/amiga/make_doc_icon.py "$G/doc.info" --frame tools/amiga/icon/drawer.png --crop 0 0 46 40 --transparent --drawer --window 430 330 --pos 100 12 >/dev/null
python tools/amiga/make_doc_icon.py "$G/pc.info" --frame tools/amiga/icon/drawer.png --crop 0 0 46 40 --transparent --drawer --window 300 120 --pos 170 12 >/dev/null
for t in Install.txt:doc_i:72:10 SyndWars.txt:doc_pl:72:80 SyndWars_en.txt:doc_en:282:80 \
         SyndWars_release.txt:doc_pl:72:150 SyndWars_release_en.txt:doc_en:282:150 \
         SyndWars_dev.txt:doc_pl:72:220 SyndWars_dev_en.txt:doc_en:282:220; do
	IFS=: read -r f pic x y <<EOT
$t
EOT
	python tools/amiga/make_doc_icon.py "$G/doc/$f.info" --frame "tools/amiga/icon/$pic.png" --crop 0 0 36 44 --transparent --project SYS:Utilities/MultiView --pos "$x" "$y" >/dev/null
done

# only these files in the public tree - no game data
bad=$(cd "$R" && find SyndWars -type f | while read -r f; do
	case "$f" in
	SyndWars/SyndWars|SyndWars/SyndWars.info|SyndWars/swhost|SyndWars/swcore.wxt) ;;
	SyndWars/config.ini|SyndWars/doc.info|SyndWars/pc.info) ;;
	SyndWars/doc/*|SyndWars/conf/*) ;;
	SyndWars/pc/SyndWarsData.py|SyndWars/pc/cdimage.py|SyndWars/pc/make_sound_bank.py|SyndWars/pc/make_music.py) ;;
	SyndWars/language/*/alltext-*.idx|SyndWars/language/*/alltext-*.wad|SyndWars/language/*/guitext.dat) ;;
	*) echo "$f" ;;
	esac
done)
if [ -n "$bad" ]; then
	echo "refused: not for the public archive:" $bad >&2
	exit 1
fi

# the archives: older public ones out of git into private/old (kept)
mkdir -p private/old
for f in "$R"/SyndWars-*.zip; do
	[ -e "$f" ] && [ "$f" != "$R/SyndWars-$V.zip" ] && { git rm -q --cached "$f" 2>/dev/null || true; mv "$f" private/old/; }
done
python tools/amiga/make_zip.py "$R/SyndWars-$V.zip" "$R" SyndWars SyndWars.info

if [ "$1" != "--no-full" ]; then
	for f in private/SyndWars-*-full.zip; do
		[ -e "$f" ] && [ "$f" != "private/SyndWars-$V-full.zip" ] && mv "$f" private/old/
	done
	# the game's data, as the Amiga has it (no saves, no other languages' sounds)
	FD=build/release_full_data
	rm -rf "$FD"
	python - "$DATA" "$FD" <<'EOF'
import os, shutil, sys
src, dst = sys.argv[1], sys.argv[2]
for d in ("data", "intro", "levels", "maps", "music", "qdata", "sound", "language"):
    def ignore(path, names):
        rel = os.path.relpath(path, src).replace(os.sep, "/")
        out = []
        for n in names:
            low = n.lower()
            if rel == "qdata" and low == "savegame":
                out.append(n)
            elif rel == "sound" and low.startswith("sound_") and low != "sound_eng.dat":
                out.append(n)
            elif rel.startswith("language/") and (low.startswith("alltext-") or low == "guitext.dat"):
                out.append(n)       # in the public tree already
        return out
    shutil.copytree(os.path.join(src, d), os.path.join(dst, d), ignore=ignore)
EOF
	mkdir -p "$FD/qdata/savegame"
	# the public tree and the data in one archive
	python tools/amiga/make_zip.py "private/SyndWars-$V-full.zip" "$R" SyndWars SyndWars.info \
		$(for d in data intro levels maps music qdata sound; do echo "--add $FD/$d SyndWars/$d"; done) \
		$(for l in "$FD"/language/*; do echo "--add $l SyndWars/language/$(basename "$l")"; done)
fi
ls -l "$R/SyndWars-$V.zip" private/SyndWars-$V-full.zip 2>/dev/null
