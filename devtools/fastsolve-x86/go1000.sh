#!/bin/bash
# go1000.sh: N = 1000 whole cubes, one at a time: fixed order and greedy --cand K, plain (P 80) and --bound (P 72)
cd ~/orbitsolve-cand/runs || exit 1
echo "# go1000.sh $*"
run() {  # name, options
  local n=$1; shift
  [ -s $n.log ] && grep -q "^total" $n.log && { echo "skip $n"; return; }
  echo "== $(date +%T) $n: $*"
  /usr/bin/time -v ../bin/certorder 1000 --seed 1000 --timing -t 40 "$@" > $n.log 2> $n.err
  grep -E "^order|^pairs solved|^total|moves" $n.log | head -6
  grep -E "Elapsed|Maximum resident" $n.err
}
for b in plain bound; do
  B=""; [ $b = bound ] && B="--bound"
  run fixed-$b $B
  for K in 64 256 1024 4096 16384; do run greedy$K-$b $B --order greedy --cand $K; done
done
for b in plain bound; do
  B=""; [ $b = bound ] && B="--bound"
  run greedyall-$b $B --order greedy
done
echo "== $(date +%T) done"
