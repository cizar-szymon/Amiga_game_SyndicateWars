#!/bin/bash
# Builds swhost, the 68k host of the ARM game (vbcc) -> build/warp/swhost
# libsmacker (the videos) is compiled with -O0: vbcc's -O1 and -O2 made wrong
# code of it (the first frame of each video "ran out of bits"); gcc is right.
set -e
cd "$(dirname "$0")/../.."
SDK=/c/Workprojects/csWarpAmigaSoftware/cswarp_library/sdk_includes/include
COMMON=/c/Workprojects/csWarpAmigaSoftware/common
export VBCC=/c/amiga-dev/vbcc
export PATH="$VBCC/bin:$PATH"
INC="-I/c/amiga-dev/NDK3.2/Include_H -I$SDK -I$COMMON -I/c/amiga-dev/NDK3.2/SANA+RoadshowTCP-IP/include
  -I/c/amiga-dev/ahidev/AHI/Developer/include/C -Iplatform/amiga -Itools/warp -Iplatform/warp"
W="-dontwarn=153 -dontwarn=214 -dontwarn=208 -dontwarn=81"
O=build/warp/obj_host
mkdir -p $O
for f in tools/warp/swhost.c tools/warp/swhost_snd.c platform/amiga/snd_engine.c \
    platform/amiga/smk_core.c; do
  vc +aos68k_win -cpu=68060 -c99 -O1 $INC $W -c $f -o $O/$(basename $f .c).o
done
vc +aos68k_win -cpu=68060 -c99 -O0 $INC $W -c third_party/libsmacker/smacker.c -o $O/smacker.o
vc +aos68k_win -cpu=68060 $O/swhost.o $O/swhost_snd.o $O/snd_engine.o $O/smk_core.o \
  $O/smacker.o -o build/warp/swhost -lauto -lamiga
ls -l build/warp/swhost
