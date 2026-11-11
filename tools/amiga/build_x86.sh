#!/bin/bash
# Reference i386 build of SyndWarsFX on Windows (MSYS2, mingw32 environment).
# Run from Git Bash or PowerShell:  C:/msys64/usr/bin/bash -lc "<repo>/tools/amiga/build_x86.sh [debug]"
# Output: build/x86-release/src/syndwarsfx.exe (or build/x86-debug/...)
set -e
cd "$(dirname "$0")/../.."
export PATH="/mingw32/bin:$PATH"
export PKG_CONFIG_PATH="/mingw32/lib/pkgconfig"
kind=${1:-release}
if [ "$kind" = debug ]; then
  FLAGS="-m32 -fno-PIC -fno-PIE -g -O0 -Wall"
  export CPPFLAGS="-DDEBUG -D__DEBUG"
else
  FLAGS="-m32 -fno-PIC -fno-PIE"
fi
[ -x configure ] || autoreconf -ivf --include=/mingw32/share/aclocal/
mkdir -p "build/x86-$kind" && cd "build/x86-$kind"
if [ ! -f Makefile ]; then
  CFLAGS="$FLAGS" CXXFLAGS="$FLAGS" LDFLAGS="$FLAGS" \
    ../../configure --prefix=/mingw32 --disable-lb-wscreen-control
fi
make -j"$(nproc)"
ls -la src/syndwarsfx.exe
