#!/bin/bash
# One test iteration: build, install, run the game, show its output.
# usage: sh tools/amiga/iter.sh [game args, default "-q -m 0,1"] [--no-build]
# Target: WinUAE (default; the Amiga data dir "Syndicate Wars/amiga_data",
#   made by tools/amiga/make_amiga_data.sh, mounted as SWARS:) or a real Amiga
#   with TARGET=amiga1 (AmiAgent default config) or TARGET=amiga2
#   (amiagent-2.cfg), both WorkSD:Games/SyndWars.
# The game is started asynchronously; after WAIT seconds (default 40) its
# output is shown and it gets Ctrl-C. If AmiAgent stops answering in WinUAE,
# restart WinUAE (tools/amiga/winuae_restart.ps1).
cd "$(dirname "$0")/../.."
ARGS="${1:--q -m 0,1}"
TARGET=${TARGET:-winuae}
if [ "$TARGET" = "winuae" ]; then
  AR="python tools/amiga_remote.py --config C:/amiga-dev/amiagent-winuae.cfg"
  GDIR="SWARS:"
elif [ "$TARGET" = "amiga2" ]; then
  AR="python tools/amiga_remote.py --config C:/amiga-dev/amiagent-2.cfg"
  GDIR="WorkSD:Games/SyndWars"
else
  AR="python tools/amiga_remote.py"
  GDIR="WorkSD:Games/SyndWars"
fi
if [ "$2" != "--no-build" ]; then
  rm -f build/amiga/SyndWars
  sh tools/amiga/build_amiga.sh > build/amiga/build.log 2>&1
  tail -1 build/amiga/build.log
  [ -f build/amiga/SyndWars ] || { tail -15 build/amiga/build.log; exit 1; }
fi
if [ "$TARGET" = "winuae" ]; then
  cp build/amiga/SyndWars "Syndicate Wars/amiga_data/SyndWars" || exit 1
else
  $AR put build/amiga/SyndWars $GDIR/SyndWars > /dev/null || exit 1
fi
TMP=$(mktemp)
printf 'CD %s\nSyndWars %s >RAM:sw-out.txt *>RAM:sw-err.txt\n' "$GDIR" "$ARGS" > "$TMP"
$AR put "$TMP" RAM:sw.script > /dev/null
rm -f "$TMP"
$AR run "Delete RAM:SyndWars-#?.txt RAM:sw-out.txt QUIET" > /dev/null 2>&1
$AR start "Execute RAM:sw.script" > /dev/null
sleep ${WAIT:-40}
$AR titles 2>&1 | tail -3
if [ -n "$SHOT" ]; then $AR shot "$SHOT" > /dev/null; fi
$AR run "Type RAM:sw-out.txt" | tail -${LINES_OUT:-25}
echo "--- stubs called:"
$AR run "Type RAM:SyndWars-stubs.txt" | head -${LINES_STUBS:-40}
[ -n "$NOSTOP" ] || $AR stop SyndWars > /dev/null 2>&1
