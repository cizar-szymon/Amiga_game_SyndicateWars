#!/bin/bash
# Builds swhost, the 68k host of the ARM game (vbcc) -> build/warp/swhost
set -e
cd "$(dirname "$0")/../.."
SDK=/c/Workprojects/csWarpAmigaSoftware/cswarp_library/sdk_includes/include
COMMON=/c/Workprojects/csWarpAmigaSoftware/common
export VBCC=/c/amiga-dev/vbcc
mkdir -p build/warp
PATH="$VBCC/bin:$PATH" vc +aos68k_win -cpu=68060 -c99 -O1 \
  -I/c/amiga-dev/NDK3.2/Include_H -I"$SDK" -I"$COMMON" -I/c/amiga-dev/NDK3.2/SANA+RoadshowTCP-IP/include \
  -I/c/amiga-dev/ahidev/AHI/Developer/include/C -Iplatform/amiga -Itools/warp \
  -Iplatform/warp -dontwarn=153 -dontwarn=214 -dontwarn=208 -dontwarn=81 \
  tools/warp/swhost.c tools/warp/swhost_snd.c platform/amiga/snd_engine.c \
  -o build/warp/swhost -lauto -lamiga
ls -l build/warp/swhost
