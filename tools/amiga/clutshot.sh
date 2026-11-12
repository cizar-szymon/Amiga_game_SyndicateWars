#!/bin/bash
# Correct screenshot of the front 8-bit (CLUT) screen in WinUAE -> PNG
# usage: sh tools/amiga/clutshot.sh OUT.png
cd "$(dirname "$0")/../.."
AR="python tools/amiga_remote.py --config C:/amiga-dev/amiagent-winuae.cfg"
[ -f "Syndicate Wars/syndwarsfx/clutshot" ] || cp ../OpenDUNE/build/amiga/clutshot "Syndicate Wars/syndwarsfx/clutshot"
$AR run "SWARS:clutshot" > /dev/null
$AR get RAM:clutshot.ppm "$1.ppm" > /dev/null
python -c "from PIL import Image; import sys; Image.open(sys.argv[1]).save(sys.argv[2])" "$1.ppm" "$1" && rm -f "$1.ppm"
