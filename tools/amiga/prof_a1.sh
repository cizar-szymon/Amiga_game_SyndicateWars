#!/bin/bash
# Profile run on Amiga 1: install current build, run mission for WAIT s,
# print the last profiler report. usage: sh tools/amiga/prof_a1.sh [--no-build] [args]
cd "$(dirname "$0")/../.."
NB="$1"; ARGS="${2:--q -m 0,1}"
TARGET=amiga1 NOSTOP=1 WAIT=${WAIT:-40} sh tools/amiga/iter.sh "$ARGS" $NB 2>&1 | head -1
python tools/amiga_remote.py stop SyndWars > /dev/null
sleep 3
python tools/amiga_remote.py get WorkSD:Games/SyndWars/error.log build/a1_prof.log > /dev/null
n=$(grep -n "prof: [0-9]* frames" build/a1_prof.log | tail -1 | cut -d: -f1)
[ -n "$n" ] && tail -n +$n build/a1_prof.log | grep "prof:" | sed 's/^Warning: prof: *//'
grep -E "differs|Error" build/a1_prof.log | head -5
