#!/bin/bash
# Makes the Amiga game data directory from the PC one:
#   sh tools/amiga/make_amiga_data.sh [PC_DIR] [AMIGA_DIR]
# (default "Syndicate Wars/syndwarsfx" -> "Syndicate Wars/amiga_data").
# Everything is hard linked from the PC directory, except the sound banks
# (8-bit signed samples for Paula, tools/amiga/make_sound_bank.py) and the
# music (PCM tracks, tools/amiga/make_music.py, needs ffmpeg), which are
# converted.  The game writes to qdata (saves) and the config files, so
# those are copied, not linked.  PC executables, DLLs and logs are left out.
set -e
cd "$(dirname "$0")/../.."
PC="${1:-Syndicate Wars/syndwarsfx}"
AM="${2:-Syndicate Wars/amiga_data}"
mkdir -p "$AM"
for f in "$PC"/*; do
  name=$(basename "$f")
  case "$name" in
    sound|music|*.exe|*.dll|*.log|*.txt|clutshot|SyndWars) ;;
    qdata|conf|*.ini) [ -e "$AM/$name" ] || cp -r "$f" "$AM/$name" ;;
    *) [ -e "$AM/$name" ] || cp -rl "$f" "$AM/$name" ;;
  esac
done
mkdir -p "$AM/sound"
for f in "$PC"/sound/*; do
  name=$(basename "$f")
  case "$name" in
    sound_*.dat|syncreds.dat)
      [ -e "$AM/sound/$name" ] || python tools/amiga/make_sound_bank.py "$f" "$AM/sound/$name" ;;
    *) [ -e "$AM/sound/$name" ] || cp -l "$f" "$AM/sound/$name" ;;
  esac
done
if [ ! -e "$AM/music" ]; then
  python tools/amiga/make_music.py "$PC/music" "$AM/music"
fi
echo "done: $AM"
