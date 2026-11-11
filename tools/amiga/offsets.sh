#!/bin/bash
# Print field offsets of game structs as the x86 build sees them.
# Usage: tools/amiga/offsets.sh "struct Thing" "PTarget" "UPerson.BumpMode" ...
#        tools/amiga/offsets.sh -size "struct Thing"
# Needs a configured build dir (build/x86-release) for config.h.
set -e
cd "$(dirname "$0")/../.."
export PATH=/c/msys64/mingw32/bin:/mingw32/bin:$PATH
tmp=$(mktemp -d)
st="$1"; shift
{
  echo '#include <stdio.h>'
  echo '#include <stddef.h>'
  echo '#include "thing.h"'
  echo '#include "bigmap.h"'
  echo '#include "game_data.h"'
  echo "#include \"game_options.h\""
  echo "#include \"people.h\""
  echo "#include \"scanner.h\""
  echo "#include \"player.h\""
  echo 'int main(void) {'
  echo "  printf(\"sizeof(%s) = %u\\n\", \"$st\", (unsigned)sizeof($st));"
  for f in "$@"; do
    echo "  printf(\"%-30s %4u 0x%x\\n\", \"$f\", (unsigned)offsetof($st, $f), (unsigned)offsetof($st, $f));"
  done
  echo '  return 0; }'
} > "$tmp/o.c"
gcc -m32 -I build/x86-release -I build/x86-release/src -I src -I bflibrary/include \
  -I build/x86-release/bflibrary/include -I swrendersoft/include -I bfsoundlib/include \
  -I bfariadne/include -I build/x86-release/include/bflibrary \
  -DHAVE_CONFIG_H -o "$tmp/o.exe" "$tmp/o.c" 2>&1 | grep -v warning | head -20
"$tmp/o.exe"
rm -rf "$tmp"
