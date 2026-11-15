#!/bin/sh
# The bug reports of Syndicate Wars (Help key in the game, version
# SyndWars-...) from the game server of the Amiga ports (the Settlers port's
# server/ on the Pi400) to private/reports/ (not in git: e-mail addresses
# of players). Only the new folders are copied; reports of the other games
# on the server are left out.
#   sh tools/amiga/server_reports.sh [cizar@192.168.5.237]
set -e
cd "$(dirname "$0")/../.."
T=${1:-cizar@192.168.5.237}
mkdir -p private/reports
for d in $(ssh "$T" 'cd ~/settlers-server/data/reports 2>/dev/null &&
    grep -l "\"version\": \"SyndWars-" */report.json | cut -d/ -f1'); do
  [ -d "private/reports/$d" ] && continue
  scp -q -r "$T:settlers-server/data/reports/$d" private/reports/
  echo "$d"
done
