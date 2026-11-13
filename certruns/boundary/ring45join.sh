#!/bin/bash
# 2026-10-25: after the ring45p.sh lanes: join the 20 parts into ring45.txt and run boundcheck.
cd ~/orbitsolve
C=$HOME/orbitsolve-certsafe/obl23pair-cf-xfree-P93-2026-10-22/stages
O=/var/tmp/orbitsolve/bcert/ring45
RA=$C/aUaDbUbD..aLaFaRaB; RB=$C/aUaLaFaRaBaDbUbD..bLbFbRbB
ls $O/part-*.txt | wc -l | grep -q "^ *20$" || { echo "== not every part covered"; exit 1; }
cat $O/part-*.txt > /var/tmp/orbitsolve/bcert/ring45.txt
echo "== rings 45 boundcheck ($(date '+%m-%d %H:%M'))"
devtools/boundcheck $RA $RB /var/tmp/orbitsolve/bcert/ring45.txt 45 --threads 8 > $O/check.log 2>&1
tail -3 $O/check.log
echo "== end ($(date '+%m-%d %H:%M'))"
