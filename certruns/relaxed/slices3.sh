#!/bin/bash
# 2026-10-24: slice closure with short propagation (--propall --slices --prop 3) on 1ud and 2ud
# side by side (separate stage locks), then 1u; the table after each.
cd ~/orbitsolve
R=certruns/relaxed; E=/var/tmp/orbitsolve/empty.log; A="--near 0 --band 0 --prop 3 --propall --slices"
CEARGS="$A" $R/rfeed.sh 2ud ud aUbU..aDbD $E --uonly > $R/r2ud-slices3.log 2>&1 &
CEARGS="$A" $R/rfeed.sh 1ud ud none..aUbU $E > $R/r1ud-slices3.log 2>&1
python3 $R/rtable.py
CEARGS="$A" $R/rfeed.sh 1u u none..aUbU $E > $R/r1u-slices3.log 2>&1
wait
python3 $R/rtable.py
echo "== slices3 done ($(date '+%m-%d %H:%M'))"
