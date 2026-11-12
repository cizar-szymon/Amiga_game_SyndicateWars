#!/bin/bash
# Builds exthello: ARM half (Keil Arm Compiler 6) -> sealed hello.wxt,
# 68k half (vbcc) -> exthello.  Output in build/warp/exthello (not in git).
# The extension key is only READ by mkwxt.py from its path; never copied.
set -e
cd "$(dirname "$0")"
OUT=../../../build/warp/exthello
mkdir -p $OUT
AC6=/c/Keil_v5/ARM/ARMCLANG/bin
FW_COMMON=${FW_COMMON:-/c/Workprojects/csAmigaWarp/Arm/common}
EXT_KEY=${EXT_KEY:-$FW_COMMON/ext_key_warp.h}
MKWXT=/c/Workprojects/AmigaSoft/YouTube_viewer/warpVideoPlay-main/arm/mkwxt.py
SDK=/c/Workprojects/csWarpAmigaSoftware/cswarp_library/sdk_includes/include
COMMON=/c/Workprojects/csWarpAmigaSoftware/common

"$AC6/armclang" --target=arm-arm-none-eabi -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard \
  -mthumb -fbare-metal-pie -ffunction-sections -std=gnu11 -O2 -fno-unroll-loops \
  -I"$FW_COMMON" -c hello_arm.c -o $OUT/hello_arm.o
"$AC6/armlink" --cpu=Cortex-M7.fp.dp --bare_metal_pie --no_startup --library_type=microlib \
  --entry=ext_main --diag_suppress=6771,6084,6085 --remove $OUT/hello_arm.o -o $OUT/hello.axf
python "$MKWXT" $OUT/hello.axf $OUT/hello.wxt --name exthello --key "$EXT_KEY"

export VBCC=/c/amiga-dev/vbcc
PATH="$VBCC/bin:$PATH" vc +aos68k_win -cpu=68060 -c99 -O1 \
  -I/c/amiga-dev/NDK3.2/Include_H -I"$SDK" -I"$COMMON" -I/c/amiga-dev/NDK3.2/SANA+RoadshowTCP-IP/include \
  -dontwarn=153 -dontwarn=214 -dontwarn=208 -dontwarn=81 \
  exthello.c -o $OUT/exthello -lamiga
ls -l $OUT/hello.wxt $OUT/exthello
