#!/bin/bash
# 2026-10-22 night, after ud41.sh: rings 46 in full (needed with the new stages), then rings 45 in
# full if its 1/500 sample was fully covered, then a 1/500 sample of UD 40.  P93-2026-10-22 snapshot.
cd ~/orbitsolve
until grep -q "^== end" certruns/boundary/ud41.log; do sleep 60; done
C=$HOME/orbitsolve-certsafe/obl23pair-cf-xfree-P93-2026-10-22/stages
D=dist/gullicube/data
BASE="--base $D/pair3.cls --base $D/p8s10.cls --base $D/p9.cls --base $D/eg8.cls --base $D/c12x.cls"
O=/var/tmp/orbitsolve/bcert
RA=$C/aUaDbUbD..aLaFaRaB; RB=$C/aUaLaFaRaBaDbUbD..bLbFbRbB
echo "== rings 46 boundx ($(date '+%m-%d %H:%M'))"
devtools/boundx $RA $RB 46 --deep 1 --slack 5 --out $O/ring46.txt $BASE 2>&1 | grep -v "^class" | tail -4
echo "== rings 46 boundcheck ($(date '+%m-%d %H:%M'))"
devtools/boundcheck $RA $RB $O/ring46.txt 46 2>&1 | tail -3
if grep -A4 "rings 45 sample" certruns/boundary/ud41.log | grep -q "X=45: 0 of"; then
  echo "== rings 45 boundx ($(date '+%m-%d %H:%M'))"
  devtools/boundx $RA $RB 45 --deep 1 --slack 5 --out $O/ring45.txt $BASE 2>&1 | grep -v "^class" | tail -4
  echo "== rings 45 boundcheck ($(date '+%m-%d %H:%M'))"
  devtools/boundcheck $RA $RB $O/ring45.txt 45 2>&1 | tail -3
else
  echo "== rings 45 sample not fully covered: skipped"
fi
echo "== UD 40 sample 500 ($(date '+%m-%d %H:%M'))"
devtools/boundx $C/none..aUbU $C/aUbU..aDbD 40 --deep 1 --slack 5 --sample 500 $BASE 2>&1 | grep -v "^class" | tail -4
echo "== end ($(date '+%m-%d %H:%M'))"
