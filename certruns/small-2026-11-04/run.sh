#!/bin/bash
# smallcert.py on all four stages of obl23pair-axis-2026-11-04 -> obl23pair-axis-2026-11-04-small
set -u
PY=$(ls ~/pypybin/*/bin/pypy3); C=~/orbitsolve-certsafe/obl23pair-axis-2026-11-04/stages
O=~/orbitsolve-certsafe/obl23pair-axis-2026-11-04-small/stages; mkdir -p $O
for s in "4op 4op" "2ud aUbU..aDbD" "3ax 3ax" "1ud none..aUbU"; do
  set -- $s; echo "== $1 $(date '+%H:%M')"
  $PY ~/orbitsolve/devtools/smallcert.py $1 $C/$2 $O/$2 12 || { echo "== FAIL $1"; exit 1; }
done
echo "== done $(date '+%H:%M')"
