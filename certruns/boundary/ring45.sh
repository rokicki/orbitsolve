#!/bin/bash
# 2026-10-25: rings 45 (a ring + b ring <= 45) on the P93-2026-10-22 ring stages, in 20 resumable parts:
# the a-ring lines longer than 16 (bcert/ring45/alines.txt) split into only-00..19 (every 20th line);
# each part is boundx --only, kept as part-NN.txt only when every class is covered.  Rerunning skips
# finished parts.  Then the parts are joined into ring45.txt and boundcheck checks it.
#   ring45.sh   (stop after the current part: touch certruns/boundary/ring45.stop)
cd ~/orbitsolve
C=$HOME/orbitsolve-certsafe/obl23pair-cf-xfree-P93-2026-10-22/stages
D=dist/gullicube/data
BASE="--base $D/pair3.cls --base $D/p8s10.cls --base $D/p9.cls --base $D/eg8.cls --base $D/c12x.cls"
O=/var/tmp/orbitsolve/bcert/ring45
RA=$C/aUaDbUbD..aLaFaRaB; RB=$C/aUaLaFaRaBaDbUbD..bLbFbRbB
for n in $(seq -w 0 19); do
  [ -e certruns/boundary/ring45.stop ] && { echo "== stopped ($(date '+%m-%d %H:%M'))"; exit 0; }
  [ -s $O/part-$n.txt ] && continue
  echo "== part $n ($(date '+%m-%d %H:%M'))"
  /usr/bin/time -l devtools/boundx $RA $RB 45 --deep 1 --slack 5 --only $O/only-$n --out $O/part-$n.tmp $BASE > $O/part-$n.out 2> $O/part-$n.err
  grep -v "^class" $O/part-$n.err | grep -E "only:|classes:" | cut -c1-200; grep "not covered" $O/part-$n.out
  # the verdict line is on stdout
  if grep -q "X=45: 0 of" $O/part-$n.out; then mv $O/part-$n.tmp $O/part-$n.txt
  else echo "== part $n not fully covered: kept as part-$n.tmp"; fi
done
ls $O/part-*.txt | wc -l | grep -q "^ *20$" || { echo "== not every part covered"; exit 1; }
cat $O/part-*.txt > /var/tmp/orbitsolve/bcert/ring45.txt
echo "== rings 45 boundcheck ($(date '+%m-%d %H:%M'))"
devtools/boundcheck $RA $RB /var/tmp/orbitsolve/bcert/ring45.txt 45 > $O/check.log 2>&1
tail -3 $O/check.log
echo "== end ($(date '+%m-%d %H:%M'))"
