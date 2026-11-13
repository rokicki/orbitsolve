#!/bin/bash
# 2026-10-22 boundaries on the P93-2026-10-22 snapshot: UD 42 (now only aUbU's 19s), then a 1/500
# sample of UD 41.  Written to /var/tmp/orbitsolve/bcert, checked independently.
cd ~/orbitsolve
until grep -q "^# end" certruns/cert-P93-2026-10-22.log; do sleep 30; done
C=$HOME/orbitsolve-certsafe/obl23pair-cf-xfree-P93-2026-10-22/stages
D=dist/gullicube/data
BASE="--base $D/pair3.cls --base $D/p8s10.cls --base $D/p9.cls --base $D/eg8.cls --base $D/c12x.cls"
O=/var/tmp/orbitsolve/bcert; mkdir -p $O
echo "== UD 42 boundx ($(date '+%m-%d %H:%M'))"
devtools/boundx $C/none..aUbU $C/aUbU..aDbD 42 --deep 1 --slack 5 --out $O/ud42.txt $BASE 2>&1 | grep -v "^class" | tail -4
echo "== UD 42 boundcheck ($(date '+%m-%d %H:%M'))"
devtools/boundcheck $C/none..aUbU $C/aUbU..aDbD $O/ud42.txt 42 2>&1 | tail -3
echo "== UD 41 sample 500 ($(date '+%m-%d %H:%M'))"
devtools/boundx $C/none..aUbU $C/aUbU..aDbD 41 --deep 1 --slack 5 --sample 500 $BASE 2>&1 | grep -v "^class" | tail -4
echo "== end ($(date '+%m-%d %H:%M'))"
