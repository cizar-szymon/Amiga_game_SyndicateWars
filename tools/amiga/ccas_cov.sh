#!/bin/bash
# CCAS replacement for the asm coverage build (see build_x86.sh cov):
# gcc -E (keeps -MD dependency output) | asm_cov_filter.py | gcc -c -x assembler
set -e
here="$(cd "$(dirname "$0")" && pwd)"
args=() out="" src="" asflags=()
while [ $# -gt 0 ]; do
  case "$1" in
    -c) ;;
    -o) out="$2"; shift ;;
    *.sx|*.S) src="$1" ;;
    -m32) args+=("$1"); asflags+=("$1") ;;
    *) args+=("$1") ;;
  esac
  shift
done
if [ -z "$src" ]; then exec gcc "${args[@]}" ${out:+-o "$out"} -c; fi
gcc "${args[@]}" -E "$src" | python3 "$here/asm_cov_filter.py" \
  | gcc "${asflags[@]}" -c -x assembler -o "$out" -
