#!/bin/bash
# Amiga (m68k, AmigaOS 3.x, 68060) build of SyndWarsFX, run inside Docker:
#   sh tools/amiga/build_amiga.sh            (from Git Bash on the PC)
# The container (image amixcom-build, bebbo gcc 6.5.0b) runs this same script
# with --in-docker. Output: build/amiga/SyndWars
set -e
IMAGE=amixcom-build:latest

if [ "$1" != "--in-docker" ]; then
  cd "$(dirname "$0")/../.."
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
  -Ibflibrary/src/m68k-amigaos
  -Wno-unused-variable -Wno-unused-but-set-variable"
CFLAGS="$COMMON -std=gnu99"
CXXFLAGS="$COMMON -fno-exceptions -fno-rtti"

# Source sets. Not built: asm (.sx), DOS/Windows/SDL platform code,
# bfsoundlib and bfsmacklib (stubbed), network transports.
SRCS=""
for f in src/*.c; do
  case "$f" in
    src/asm_coverage.c|src/fncheck.c|src/osunix.c|src/oswindws.c) ;;
    *) SRCS="$SRCS $f" ;;
  esac
done
SRCS="$SRCS $(ls swrendersoft/src/*.c bfariadne/src/*.c)"
for f in bflibrary/src/general/*.c bflibrary/src/general/*.cpp; do
  case "$f" in
    *_s.c|*/gpng.c) ;;
    *) SRCS="$SRCS $f" ;;
  esac
done
SRCS="$SRCS $(for f in sdir sdrive sffind sfile slog smemory; do echo bflibrary/src/x86-win-sdl2/$f.c; done)"
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
    echo "FAIL $src"
  fi
}
export -f compile
export CC CXX CFLAGS CXXFLAGS PP OBJ
echo $SRCS | tr ' ' '\n' | grep . | xargs -P "$(nproc)" -I{} bash -c 'compile {}'
echo "objects: $(ls $OBJ/*.o 2>/dev/null | wc -l), failed: $(ls $OBJ/*.err 2>/dev/null | wc -l)"

# Link
if $CXX -m68060 -noixemul -o "$OUT/SyndWars" $OBJ/*.o -lm 2> "$OUT/link.err"; then
  echo "linked: $OUT/SyndWars ($(stat -c %s "$OUT/SyndWars") bytes)"
else
  sed -n "s/.*undefined reference to .\([A-Za-z0-9_]*\)'.*/\1/p" "$OUT/link.err" | sort -u > "$OUT/undefined.txt"
  echo "link failed: $(wc -l < "$OUT/undefined.txt") undefined symbols (build/amiga/undefined.txt)"
fi
