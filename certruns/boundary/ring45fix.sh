#!/bin/bash
# 2026-10-26: the classes that a ring45 part left uncovered (part-NN.tmp kept, verdict "K of N not
# covered"): find their stage lines (boundx's "class I" is line I of w00.txt, w01.txt, ... joined),
# rerun boundx on just those with --deep 3 --slack 7 (as for the rings 46 tail, 2026-10-17), and
# splice the new blocks into part-NN.tmp, which becomes part-NN.txt when every class is covered.
#   ring45fix.sh NN...
cd ~/orbitsolve
C=$HOME/orbitsolve-certsafe/obl23pair-cf-xfree-P93-2026-10-22/stages
D=dist/gullicube/data
BASE="--base $D/pair3.cls --base $D/p8s10.cls --base $D/p9.cls --base $D/eg8.cls --base $D/c12x.cls"
O=/var/tmp/orbitsolve/bcert/ring45
RA=$C/aUaDbUbD..aLaFaRaB; RB=$C/aUaLaFaRaBaDbUbD..bLbFbRbB
[ $# -ge 1 ] || { echo "usage: ring45fix.sh NN..."; exit 1; }
for n in "$@"; do
  [ -s $O/part-$n.tmp ] || { echo "== part $n: no part-$n.tmp"; continue; }
  ids=$(grep "colourings left" $O/part-$n.err | grep -v "> 0 colourings" | grep "^class " | awk '{print $2 + 1}' | tr -d : | paste -sd, -)
  cat $(ls $RA/w*.txt | sort) | awk -v ids="$ids" 'BEGIN{split(ids, a, ","); for (k in a) w[a[k]] = 1} NR in w' > $O/fix-$n
  echo "== part $n: $(wc -l < $O/fix-$n) classes, boundx --deep 3 --slack 7 ($(date '+%m-%d %H:%M'))"
  devtools/boundx $RA $RB 45 --deep 3 --slack 7 --only $O/fix-$n --out $O/fix-$n.txt $BASE > $O/fix-$n.out 2> $O/fix-$n.err
  grep "not covered" $O/fix-$n.out
  if grep -q "X=45: 0 of" $O/fix-$n.out; then
    python3 certruns/boundary/bsplice.py $O/part-$n.tmp $O/fix-$n.txt > $O/part-$n.spliced && mv $O/part-$n.spliced $O/part-$n.txt && rm $O/part-$n.tmp && echo "== part $n spliced: part-$n.txt"
  else echo "== part $n still not covered"; fi
done
