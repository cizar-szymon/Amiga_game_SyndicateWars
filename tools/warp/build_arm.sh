#!/bin/bash
# Warp ARM build of SyndWarsFX: the same game sources as the Amiga build
# (tools/amiga/build_amiga.sh), compiled with Keil Arm Compiler 6 for the
# Cortex-M7 of the Warp as a bare-metal PIE extension, platform layer in
# platform/warp and bflibrary/src/arm-warp.  The 68k loader is tools/warp/swhost.
#   sh tools/warp/build_arm.sh      -> build/warp/swcore.wxt (sealed)
# The extension key is only read by mkwxt.py from its path, never copied.
set -e
cd "$(dirname "$0")/../.."
# release number of the port, as tools/amiga/build_amiga.sh writes it
N=$(git rev-list --count upstream/master..HEAD 2>/dev/null || echo 0)
V="#define SW_PORT_VERSION \"0.$N\"
#define SW_PORT_DATE \"$(date +%d.%m.%Y)\""
mkdir -p build/amiga
if [ "$(cat build/amiga/sw_version.h 2>/dev/null)" != "$V" ]; then
  printf '%s
' "$V" > build/amiga/sw_version.h
fi
AC6=${AC6:-/c/Keil_v5/ARM/ARMCLANG/bin}
FW_COMMON=${FW_COMMON:-/c/Workprojects/csAmigaWarp/Arm/common}
EXT_KEY=${EXT_KEY:-$FW_COMMON/ext_key_warp.h}
MKWXT=${MKWXT:-/c/Workprojects/AmigaSoft/YouTube_viewer/warpVideoPlay-main/arm/mkwxt.py}
OUT=build/warp
PP=$OUT/pp
OBJ=$OUT/obj
mkdir -p "$PP" "$OBJ"

COMMON="--target=arm-arm-none-eabi -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb
  -fbare-metal-pie -ffunction-sections -fdata-sections ${OPT:--O3} ${UNROLL:--fno-unroll-loops}
  -fno-short-enums -fsigned-char
  -DHAVE_CONFIG_H -DSW_AMIGA -DSW_WARP -include platform/warp/config.h
  -Iplatform/warp -Iplatform/warp/include -Iplatform/amiga -I$FW_COMMON
  -Isrc -Ibflibrary/include -Ibfsoundlib/include
  -Ibfsmacklib/include -Iswrendersoft/include -Ibfariadne/include
  -Ibflibrary/src/arm-warp -Ibuild/amiga
  -Wno-everything"
CFLAGS="$COMMON -std=gnu99"
CXXFLAGS="$COMMON -xc++ -std=gnu++11 -fno-exceptions -fno-rtti"

# Same source set as the Amiga build; the Amiga platform files that use
# AmigaOS are replaced by platform/warp and bflibrary/src/arm-warp.
SRCS=""
for f in src/*.c; do
  case "$f" in
    src/asm_coverage.c|src/fncheck.c|src/osunix.c|src/oswindws.c) ;;
    *) SRCS="$SRCS $f" ;;
  esac
done
SRCS="$SRCS $(ls swrendersoft/src/*.c bfariadne/src/*.c)"
# bfsoundlib without OpenAL and Vorbis: snd_bfs.c and the proxy
# platform/warp/snd_proxy.c (the 68k host plays the sounds)
for f in bfsoundlib/src/*.c; do
  case "$f" in
    */drv_oal.c|*/oggvorbis.c) ;;
    *) SRCS="$SRCS $f" ;;
  esac
done
SRCS="$SRCS platform/amiga/snd_bfs.c"
for f in bflibrary/src/general/*.c bflibrary/src/general/*.cpp; do
  case "$f" in
    *_s.c|*/gpng.c) ;;
    *) SRCS="$SRCS $f" ;;
  esac
done
SRCS="$SRCS $(for f in sdir sdrive sffind sfile slog smemory; do echo bflibrary/src/x86-win-sdl2/$f.c; done)"
SRCS="$SRCS platform/amiga/asm_data.c platform/amiga/asm_data_manual.c platform/amiga/asm_stubs.c
  platform/amiga/sound_stubs.c platform/amiga/smk_play.c third_party/libsmacker/smacker.c
  platform/amiga/osamiga.c platform/amiga/phase_trace.c
  bflibrary/src/m68k-amigaos/smouse.cpp bflibrary/src/m68k-amigaos/ssemphr.cpp"
SRCS="$SRCS $(ls platform/warp/*.c bflibrary/src/arm-warp/*.c)"

compile() {
  src="$1"
  pp="$PP/$src"
  obj="$OBJ/$(echo "$src" | tr '/' '_' | sed 's/\.c$/.o/; s/\.cpp$/.o/')"
  mkdir -p "$(dirname "$pp")"
  python tools/amiga/m68k_prep.py "$src" "$pp"
  case "$src" in
    *.cpp) flags="$CXXFLAGS" ;;
    *) flags="$CFLAGS" ;;
  esac
  if "$AC6/armclang" $flags -I"$(dirname "$src")" -c "$pp" -o "$obj" 2> "$obj.err"; then
    rm -f "$obj.err"
  else
    rm -f "$obj"
    echo "FAIL $src"
  fi
}
export -f compile
export AC6 CFLAGS CXXFLAGS PP OBJ
if [ "$1" != "--link-only" ]; then
  rm -f $OBJ/*.o $OBJ/*.err
  echo $SRCS | tr ' ' '\n' | grep . | xargs -P 8 -I{} bash -c 'compile {}'
fi
NFAIL=$(ls $OBJ/*.err 2>/dev/null | wc -l)
echo "objects: $(ls $OBJ/*.o 2>/dev/null | wc -l), failed: $NFAIL"
if [ "$NFAIL" != "0" ]; then
  for e in $OBJ/*.err; do echo "== $e"; grep -m3 "error" "$e"; done
  exit 1
fi

"$AC6/armlink" --cpu=Cortex-M7.fp.dp --bare_metal_pie --no_startup --library_type=microlib \
  --entry=ext_main --diag_suppress=6771,6084,6085,6314 --remove --map --list=$OUT/swcore.map \
  --info=sizes,totals $OBJ/*.o -o $OUT/swcore.axf > $OUT/link.log 2>&1 || {
  grep -E "Error|error" $OUT/link.log | sort | uniq | head -80; exit 1; }
grep -E "Grand Totals|ROM Totals" -A0 $OUT/link.log | head -3
python "$MKWXT" $OUT/swcore.axf $OUT/swcore.wxt --name swcore --key "$EXT_KEY"
