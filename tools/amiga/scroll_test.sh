#!/bin/bash
# Frame time while the map scrolls: mission 1, stand still, then hold the
# arrow keys (camera pan) through AmiAgent, one direction after another.
# usage: TARGET=amiga2 sh tools/amiga/scroll_test.sh [--build]
# Prints the 5 s frame-time windows of error.log with the phase they fall in.
# PROF=1 adds the profiler slots (ENV:SyndWarsProf), SHOT=file.ppm takes a
# CLUT screenshots while scrolling ($SHOT-N.ppm) (RAM:clutshot must be on the Amiga).
cd "$(dirname "$0")/../.."
AR="python tools/amiga_remote.py"
[ "$TARGET" = "amiga2" ] && AR="$AR --config C:/amiga-dev/amiagent-2.cfg"
GDIR="WorkSD:Games/SyndWars"
if [ "$1" = "--build" ]; then
  sh tools/amiga/build_amiga.sh > build/amiga/build.log 2>&1 || { tail -15 build/amiga/build.log; exit 1; }
  $AR put build/amiga/SyndWars $GDIR/SyndWars > /dev/null || exit 1
fi
if [ -n "$PROF" ]; then $AR run "SetEnv SyndWarsProf $PROF" > /dev/null
else $AR run "Delete ENV:SyndWarsProf QUIET" > /dev/null; fi
printf 'CD %s\nSyndWars -q -m 0,1 >RAM:sw-out.txt *>RAM:sw-err.txt\n' "$GDIR" > build/sw_scroll.script
$AR put build/sw_scroll.script RAM:sw.script > /dev/null
$AR start "Execute RAM:sw.script" > /dev/null
T0=$(date +%s)
mark() { echo "$(( $(date +%s) - T0 )) $1" >> build/scroll_phases.txt; }
: > build/scroll_phases.txt
sleep 30; mark still
sleep 15
# back and forth around the start position (a long run in one direction
# leaves the map: black screen); each key held 2 s at a time
SEQ=${SEQ:-"right right left left left left right right down down up up up up down down right right down down left left up up"}
n=0
for k in $SEQ; do
  n=$((n + 1))
  mark "scroll-$k"
  $AR key $k 2000 --window "Syndicate Wars" > /dev/null
  if [ -n "$SHOT" ] && [ $((n % 6)) = 3 ]; then
    $AR run "RAM:clutshot" > /dev/null; $AR get RAM:clutshot.ppm "$SHOT-$n.ppm" > /dev/null
  fi
done
mark still2
sleep 12
mark end
$AR stop SyndWars > /dev/null
sleep 3
[ -n "$PROF" ] && $AR run "Delete ENV:SyndWarsProf QUIET" > /dev/null
$AR get $GDIR/error.log build/scroll.log > /dev/null
cat build/scroll_phases.txt
grep -E "prof: [0-9]+ frames|frame (\(avg\))? " build/scroll.log | sed 's/^Warning: prof: *//'
