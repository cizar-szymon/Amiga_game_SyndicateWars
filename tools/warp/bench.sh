#!/bin/bash
# Benchmark of the ARM build on a fixed scene: the mission 1 recording
# (qdata/savegame/rc00m001.s01, agents walking and shooting, the camera
# following them) in 640x480 (F8 after 10 s). Prints the average "busy per
# turn" of the reports from 30 s on and the last report with the slots.
#   sh tools/warp/bench.sh            - profiler level 1 (cheap slots)
#   PROF=2 sh tools/warp/bench.sh     - with the fine slots and counters
#   PROF=0 sh tools/warp/bench.sh     - only the busy time
# The recording must be in WorkSD:Games/SyndWars/qdata/savegame on the Amiga.
cd "$(dirname "$0")/../.."
AR="python tools/amiga_remote.py"
PROF=${PROF:-1}
if [ "$PROF" != "0" ]; then
  $AR run "SetEnv SyndWarsProf $PROF" > /dev/null
else
  $AR run "Delete ENV:SyndWarsProf QUIET" > /dev/null
fi
python tools/warp/run.py --secs ${SECS:-100} --f8 10 -q -W -m 0,1 -p 1 > /dev/null 2>&1
$AR run "Delete ENV:SyndWarsProf QUIET" > /dev/null
L=build/warp/swarm-log.txt
grep "busy per turn" "$L" | sed -n '6,$p' | \
  awk '{s += $5; n++; if (n == 1 || $5 < lo) lo = $5; if ($5 > hi) hi = $5}
    END {if (n) printf("busy per turn: avg %.1f ms (min %.1f, max %.1f, %d reports)\n", s / n, lo, hi, n)}'
n=$(grep -n "prof: [0-9]* frames" "$L" | tail -1 | cut -d: -f1)
[ -n "$n" ] && sed -n "$n,$((n+75))p" "$L" | grep "prof:" | sed 's/prof: *//' | grep -v " 0.0 ms" | head -${LINES_SLOTS:-60}
