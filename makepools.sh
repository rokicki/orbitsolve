#!/bin/sh
# Generate algorithm pools for the orbit solver.
#
# Each pool is `algutil -c -d 1 -pc` output: one line per algorithm, as
#   <move-count> (cycles...) <moves...>
# `-d 1` makes the cost field a plain move count rather than the packed
# v/h/o/c fields `-d 1000` produces.
#
# The expansion chain is:
#   -g -U   greedy conjugation expansion (18 face-move conjugations,
#           deduplicated globally by symmetry class)
#   -xu     expand to all 48 orientation conjugations
#   -m N    drop anything longer than N moves, in the cost-annotating pass
#
# The -m cap matters: without it the chain emits ~80.5M algorithms / 6.6 GB.
# Capping at 12 moves gives ~97K, which is the recommended operating pool --
# see README.md for why longer pools are not better.
#
# Usage:  ./makepools.sh [BROB_DIR] [OUT_DIR]

set -e
BROB="${1:-$HOME/brob-private}"
OUT="${2:-$(dirname "$0")/pools}"
mkdir -p "$OUT"
cd "$BROB"

for N in 12 14; do
    echo "generating conj${N}.costalgs (conjugated, <= ${N} moves) ..."
    ./algutil -g -U < bigseed.algs \
        | ./algutil -xu \
        | ./algutil -m "$N" -c -d 1 -pc \
        > "$OUT/conj${N}.costalgs"
    echo "  $(wc -l < "$OUT/conj${N}.costalgs") algorithms"
done

# Unconjugated reference pool: orientation expansion only.  Useful to show that
# conjugations matter far more than raw algorithm count.
echo "generating noconj.costalgs (orientations only) ..."
./algutil -xu < bigseed.algs | ./algutil -c -d 1 -pc > "$OUT/noconj.costalgs"
echo "  $(wc -l < "$OUT/noconj.costalgs") algorithms"

# Tiny pool from the 8-move commutator seed; too small to solve reliably, but
# it makes the pool-size scaling visible at the low end.
echo "generating seed.costalgs ..."
./algutil -xu < seed.algs | ./algutil -c -d 1 -pc > "$OUT/seed.costalgs"
echo "  $(wc -l < "$OUT/seed.costalgs") algorithms"
