#!/bin/bash
# 2026-10-25: one lane of the rings 45 run: the parts NN with NN mod NLANES = LANE (same steps as
# ring45.sh, which ran parts 00 and 01 one at a time; about 6 h and 2.4 cores each, so four lanes
# finish the other 18 in about a day and a half).  ring45join.sh joins the parts and runs boundcheck.
#   ring45p.sh LANE NLANES   (stop after the current part: touch certruns/boundary/ring45.stop)
[ $# -eq 2 ] || { echo "usage: ring45p.sh LANE NLANES"; exit 1; }
cd ~/orbitsolve
C=$HOME/orbitsolve-certsafe/obl23pair-cf-xfree-P93-2026-10-22/stages
D=dist/gullicube/data
BASE="--base $D/pair3.cls --base $D/p8s10.cls --base $D/p9.cls --base $D/eg8.cls --base $D/c12x.cls"
O=/var/tmp/orbitsolve/bcert/ring45
RA=$C/aUaDbUbD..aLaFaRaB; RB=$C/aUaLaFaRaBaDbUbD..bLbFbRbB
for n in $(seq -w 0 19); do
  [ $((10#$n % $2)) -eq $1 ] || continue
  [ -e certruns/boundary/ring45.stop ] && { echo "== lane $1 stopped ($(date '+%m-%d %H:%M'))"; exit 0; }
  [ -s $O/part-$n.txt ] && continue
  # parts run elsewhere (armory.local, 2026-10-26) have part-NN.remote
  [ -e $O/part-$n.remote ] && continue
  # a finished part with uncovered classes (part-NN.tmp + verdict) waits for ring45fix.sh, not a rerun
  grep -q "classes not covered" $O/part-$n.out 2>/dev/null && continue
  echo "== part $n ($(date '+%m-%d %H:%M'))"
  /usr/bin/time -l devtools/boundx $RA $RB 45 --deep 1 --slack 5 --only $O/only-$n --out $O/part-$n.tmp $BASE > $O/part-$n.out 2> $O/part-$n.err
  grep -v "^class" $O/part-$n.err | grep -E "only:|classes:" | cut -c1-200; grep "not covered" $O/part-$n.out
  # the verdict line is on stdout
  if grep -q "X=45: 0 of" $O/part-$n.out; then mv $O/part-$n.tmp $O/part-$n.txt
  else echo "== part $n not fully covered: kept as part-$n.tmp"; fi
done
echo "== lane $1 done ($(date '+%m-%d %H:%M'))"
