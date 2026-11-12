#!/bin/bash
# Reference i386 build of SyndWarsFX on Windows (MSYS2, mingw32 environment).
# Run from Git Bash or PowerShell:
#   C:/msys64/usr/bin/bash -lc "<repo>/tools/amiga/build_x86.sh [release|debug|cov]"
# Output: build/x86-<kind>/src/syndwarsfx.exe
#   release - reference build
#   debug   - -g -O0, verbose error.log
#   check   - remade C functions run next to their asm originals, results
#             compared (src/fncheck.c), fncheck.log in the game directory
#   prof    - frame profiler: time per game turn of input, drawing, world
#             simulation (process_things) etc., to error.log every 5 s
#   cov     - asm coverage: every asm function marks itself as executed,
#             asm_coverage.txt written on exit (tools/amiga/asm_cov_filter.py)
set -e
cd "$(dirname "$0")/../.."
REPO="$(pwd)"
export PATH="/mingw32/bin:$PATH"
export PKG_CONFIG_PATH="/mingw32/lib/pkgconfig"
kind=${1:-release}
FLAGS="-m32 -fno-PIC -fno-PIE"
MAKEVARS=()
case "$kind" in
  debug)
    FLAGS="$FLAGS -g -O0 -Wall"
    export CPPFLAGS="-DDEBUG -D__DEBUG" ;;
  check)
    export CPPFLAGS="-DSW_FNCHK" ;;
  prof)
    # same optimisation as the reference (none): with -O1 or -O2 the game
    # ends at mission start (C and inline x86 asm do not mix there yet);
    # PROF_OPT adds flags for experiments
    FLAGS="$FLAGS $PROF_OPT"
    export CPPFLAGS="-DSW_PCPROF" ;;
  cov)
    export CPPFLAGS="-DSW_ASM_COVERAGE"
    MAKEVARS=(CCAS="bash \"$REPO/tools/amiga/ccas_cov.sh\"") ;;
esac
[ -x configure ] || autoreconf -ivf --include=/mingw32/share/aclocal/
mkdir -p "build/x86-$kind" && cd "build/x86-$kind"
if [ ! -f Makefile ]; then
  CFLAGS="$FLAGS" CXXFLAGS="$FLAGS" LDFLAGS="$FLAGS" \
    ../../configure --prefix=/mingw32 --disable-lb-wscreen-control
fi
make -j"$(nproc)" "${MAKEVARS[@]}"
ls -la src/syndwarsfx.exe
