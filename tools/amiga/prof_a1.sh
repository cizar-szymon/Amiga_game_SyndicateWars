#!/bin/bash
# Profile run on Amiga 1: install current build, run mission for WAIT s,
# print the last profiler report. usage: sh tools/amiga/prof_a1.sh [--no-build] [args]
# TARGET=amiga2 measures on Amiga 2 (A4000 + Warp 7060) instead.
# PROF=1 (slots) or PROF=2 (also fine slots) sets ENV:SyndWarsProf for the run;
# without it only the average frame time is measured (no profiler cost).
cd "$(dirname "$0")/../.."
AR="python tools/amiga_remote.py"
[ "$TARGET" = "amiga2" ] && AR="$AR --config C:/amiga-dev/amiagent-2.cfg"
NB="$1"; ARGS="${2:--q -m 0,1}"
if [ -n "$PROF" ]; then $AR run "SetEnv SyndWarsProf $PROF" > /dev/null; else $AR run "Delete ENV:SyndWarsProf QUIET" > /dev/null; fi
TARGET=${TARGET:-amiga1} NOSTOP=1 WAIT=${WAIT:-40} sh tools/amiga/iter.sh "$ARGS" $NB 2>&1 | head -1
$AR stop SyndWars > /dev/null
sleep 3
[ -n "$PROF" ] && $AR run "Delete ENV:SyndWarsProf QUIET" > /dev/null
$AR get WorkSD:Games/SyndWars/error.log build/a1_prof.log > /dev/null
n=$(grep -n "prof: [0-9]* frames" build/a1_prof.log | tail -1 | cut -d: -f1)
[ -n "$n" ] && tail -n +$n build/a1_prof.log | grep "prof:" | sed 's/^Warning: prof: *//'
grep -E "differs|Error" build/a1_prof.log | head -5
