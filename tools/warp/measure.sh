#!/bin/bash
# Measures the ARM build in mission 1: 320x200 and then 640x480 (F8), the
# last two profiler reports of each. PROF=1 adds the slots.
cd "$(dirname "$0")/../.."
AR="python tools/amiga_remote.py"
[ -n "$PROF" ] && $AR run "SetEnv SyndWarsProf $PROF" > /dev/null
LINES_OUT=0 python tools/warp/run.py --secs ${SECS:-110} --f8 ${F8:-60} -q -m 0,1 > /dev/null
[ -n "$PROF" ] && $AR run "Delete ENV:SyndWarsProf QUIET" > /dev/null
grep -E "screen |prof: [0-9]+ frames|busy per turn|frame \(avg\)|frames in|exit" build/warp/swarm-log.txt \
  | grep -v "extLoad" | tail -${TAIL:-40}
