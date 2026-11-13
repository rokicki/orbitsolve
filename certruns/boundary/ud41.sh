#!/bin/bash
# 2026-10-22: UD 41 in full, then a 1/500 sample of rings 45, on the P93-2026-10-22 snapshot.
cd ~/orbitsolve
C=$HOME/orbitsolve-certsafe/obl23pair-cf-xfree-P93-2026-10-22/stages
D=dist/gullicube/data
BASE="--base $D/pair3.cls --base $D/p8s10.cls --base $D/p9.cls --base $D/eg8.cls --base $D/c12x.cls"
O=/var/tmp/orbitsolve/bcert
echo "== UD 41 boundx ($(date '+%m-%d %H:%M'))"
devtools/boundx $C/none..aUbU $C/aUbU..aDbD 41 --deep 1 --slack 5 --out $O/ud41.txt $BASE 2>&1 | grep -v "^class" | tail -4
echo "== UD 41 boundcheck ($(date '+%m-%d %H:%M'))"
devtools/boundcheck $C/none..aUbU $C/aUbU..aDbD $O/ud41.txt 41 2>&1 | tail -3
echo "== rings 45 sample 500 ($(date '+%m-%d %H:%M'))"
devtools/boundx $C/aUaDbUbD..aLaFaRaB $C/aUaLaFaRaBaDbUbD..bLbFbRbB 45 --deep 1 --slack 5 --sample 500 $BASE 2>&1 | grep -v "^class" | tail -4
echo "== end ($(date '+%m-%d %H:%M'))"
