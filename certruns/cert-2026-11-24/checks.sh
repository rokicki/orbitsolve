#!/bin/bash
# checks.sh (2026-11-24): every check of the P 79 certificate obl23pair-axis-2026-11-24; logs next to this script.
C=/var/tmp/orbitsolve/cert79/obl23pair-axis-2026-11-24; T=~/orbitsolve/devtools; L=$(cd "$(dirname "$0")" && pwd)
cd $C
$T/certcheck stages/none..aUbU --keep ud > $L/1ud.chk 2>&1; echo "1ud certcheck: $(tail -1 $L/1ud.chk)"
$T/certcheck stages/aUbU..aDbD --keep ud > $L/2ud.chk 2>&1; echo "2ud certcheck: $(tail -1 $L/2ud.chk)"
$T/axcheck 3ax stages/3ax > $L/3ax.chk 2>&1; echo "3ax axcheck: $(tail -1 $L/3ax.chk)"
$T/axcheck 4op stages/4op > $L/4op.chk 2>&1; echo "4op axcheck: $(tail -1 $L/4op.chk)"
pypy3 $T/udindcheck.py 1ud stages/none..aUbU 12 > $L/ind-1ud.chk 2>&1; echo "1ud udindcheck rc=$?: $(tail -1 $L/ind-1ud.chk)"
pypy3 $T/udindcheck.py 2ud stages/aUbU..aDbD 12 > $L/ind-2ud.chk 2>&1; echo "2ud udindcheck rc=$?: $(tail -1 $L/ind-2ud.chk)"
pypy3 $T/axindcheck.py 3ax stages/3ax 12 > $L/ind-3ax.chk 2>&1; echo "3ax axindcheck rc=$?: $(tail -1 $L/ind-3ax.chk)"
pypy3 $T/axindcheck.py 4op stages/4op 12 > $L/ind-4op.chk 2>&1; echo "4op axindcheck rc=$?: $(tail -1 $L/ind-4op.chk)"
echo "checks done $(date '+%m-%d %H:%M')"
