#!/bin/bash
# 2026-10-26 (rloop4: rloop3 without 1u, whose arena log stopped 10-24; 1u gained nothing in 24 rounds).
# 2026-10-25 (rloop3: rloop2 with --slices): keep feeding the growing arena relaxed logs (after feed12.sh): each round pulls every
# log's new tail past certruns/relaxed/processed.tsv and feeds it (--near 6), 2ud, 1ud, 1u in turn;
# a round starts as soon as the previous one ends (at least 20 minutes apart).
cd ~/orbitsolve
R=${1:-0}   # rloop4.sh [LASTROUND]
while [ ! -e certruns/relaxed/rloop.stop ]; do
  R=$((R + 1)); t0=$(date +%s)
  echo "== round $R ($(date '+%m-%d %H:%M'))"
  for spec in "2ud ud aUbU..aDbD 7pair-2ud.log --uonly" "1ud ud none..aUbU 7pair-1ud.log"; do
    set -- $spec
    if [ $1 = 2ud ]; then P=8; else P=6; fi
    CEARGS="--near 6 --band 0 --prop $P --slices" certruns/relaxed/rtail.sh $spec > certruns/relaxed/r$1-loop.log 2>&1
    grep -E "^== .*from|processed to|failed" certruns/relaxed/r$1-loop.log
    sed -n '/== lengths/,/== end/p' certruns/relaxed/r$1-loop.log | awk 'NF==2{printf "%s:%s ", $2, $1} END{print ""}'
  done
  python3 certruns/relaxed/rtable.py && echo "table updated"
  dt=$(( $(date +%s) - t0 )); [ $dt -lt 1200 ] && sleep $((1200 - dt))
done
