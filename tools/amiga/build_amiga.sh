#!/bin/bash
# Amiga (m68k, AmigaOS 3.x, 68060) build of SyndWarsFX, run inside Docker:
#   sh tools/amiga/build_amiga.sh            (from Git Bash on the PC)
# The container (image amixcom-build, bebbo gcc 6.5.0b) runs this same script
# with --in-docker. Output: build/amiga/SyndWars
set -e
IMAGE=amixcom-build:latest

if [ "$1" != "--in-docker" ]; then
  cd "$(dirname "$0")/../.."
  # Release number of the port: 0.<commits of the port since upstream>
  # (beta; 1.xx for official releases), with the build date. Written only
  # when it changes, so that a build stays incremental.
  N=$(git rev-list --count upstream/master..HEAD 2>/dev/null || echo 0)
  V="#define SW_PORT_VERSION \"0.$N\"
#define SW_PORT_DATE \"$(date +%d.%m.%Y)\""
  mkdir -p build/amiga
  if [ "$(cat build/amiga/sw_version.h 2>/dev/null)" != "$V" ]; then
    printf '%s\n' "$V" > build/amiga/sw_version.h
  fi
  export MSYS_NO_PATHCONV=1
  exec docker run --rm -v "$(pwd -W 2>/dev/null || pwd):/src" -w /src "$IMAGE" \
    bash tools/amiga/build_amiga.sh --in-docker "$@"
fi
shift
export PATH=/opt/amiga/bin:$PATH
CC=m68k-amigaos-gcc
CXX=m68k-amigaos-g++
OUT=build/amiga
PP=$OUT/pp
OBJ=$OUT/obj
mkdir -p "$PP" "$OBJ"

COMMON="-m68060 -mtune=68060 -O2 -fomit-frame-pointer -noixemul
  -DHAVE_CONFIG_H -DSW_AMIGA -include platform/amiga/config.h
  -Iplatform/amiga -Isrc -Ibflibrary/include -Ibfsoundlib/include
  -Ibfsmacklib/include -Iswrendersoft/include -Ibfariadne/include
  -Ibflibrary/src/m68k-amigaos -Ibuild/amiga
  -Wno-unused-variable -Wno-unused-but-set-variable"
CFLAGS="$COMMON -std=gnu99"
CXXFLAGS="$COMMON -fno-exceptions -fno-rtti"

# Source sets. Not built: asm (.sx), DOS/Windows/SDL platform code,
# bfsmacklib (stubbed), network transports, the OpenAL and Vorbis parts of
# bfsoundlib (platform/amiga/snd_paula.c instead).
SRCS=""
for f in src/*.c; do
  case "$f" in
    src/asm_coverage.c|src/fncheck.c|src/osunix.c|src/oswindws.c) ;;
    *) SRCS="$SRCS $f" ;;
  esac
done
SRCS="$SRCS $(ls swrendersoft/src/*.c bfariadne/src/*.c)"
for f in bfsoundlib/src/*.c; do
  case "$f" in
    */drv_oal.c|*/oggvorbis.c) ;;
    *) SRCS="$SRCS $f" ;;
  esac
done
for f in bflibrary/src/general/*.c bflibrary/src/general/*.cpp; do
  case "$f" in
    *_s.c|*/gpng.c) ;;
    *) SRCS="$SRCS $f" ;;
  esac
done
SRCS="$SRCS $(for f in sdir sdrive sffind sfile slog smemory; do echo bflibrary/src/x86-win-sdl2/$f.c; done)"
SRCS="$SRCS third_party/libsmacker/smacker.c"  # with platform/amiga/smk_core.c
SRCS="$SRCS $(ls platform/amiga/*.c bflibrary/src/m68k-amigaos/*.c bflibrary/src/m68k-amigaos/*.cpp 2>/dev/null || true)"

compile() {
  src="$1"
  pp="$PP/$src"
  obj="$OBJ/$(echo "$src" | tr '/' '_' | sed 's/\.c$/.o/; s/\.cpp$/.o/')"
  mkdir -p "$(dirname "$pp")"
  python3 tools/amiga/m68k_prep.py "$src" "$pp"
  case "$src" in
    *.cpp) cmd="$CXX $CXXFLAGS" ;;
    *) cmd="$CC $CFLAGS" ;;
  esac
  if $cmd -I"$(dirname "$src")" -c "$pp" -o "$obj" 2> "$obj.err"; then
    rm -f "$obj.err"
  else
    rm -f "$obj"
    echo "FAIL $src"
  fi
}
export -f compile
export CC CXX CFLAGS CXXFLAGS PP OBJ
echo $SRCS | tr ' ' '\n' | grep . | xargs -P "$(nproc)" -I{} bash -c 'compile {}'
NFAIL=$(ls $OBJ/*.err 2>/dev/null | wc -l)
echo "objects: $(ls $OBJ/*.o 2>/dev/null | wc -l), failed: $NFAIL"
if [ "$NFAIL" != "0" ]; then
  for e in $OBJ/*.err; do echo "== $e"; grep -m3 "error" "$e"; done
  exit 1
fi

# Link
if $CXX -m68060 -noixemul -o "$OUT/SyndWars" $OBJ/*.o -lm 2> "$OUT/link.err"; then
  echo "linked: $OUT/SyndWars ($(stat -c %s "$OUT/SyndWars") bytes)"
else
  sed -n "s/.*undefined reference to .\([A-Za-z0-9_]*\)'.*/\1/p" "$OUT/link.err" | sort -u > "$OUT/undefined.txt"
  echo "link failed: $(wc -l < "$OUT/undefined.txt") undefined symbols (build/amiga/undefined.txt)"
fi
