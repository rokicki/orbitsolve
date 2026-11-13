#!/bin/bash
# 2026-10-24: after rloop.sh stops, propagate the arena gains (certextend --propall: every class
# through every fresh relaxed action of cost <= C, then face-turn propagation), 2ud C = 9, 1u and
# 1ud C = 7, then restart the feed loop (rloop2.sh) with propagation on.
cd ~/orbitsolve
while pgrep -f rloop.sh > /dev/null; do sleep 30; done
R=certruns/relaxed; V=/var/tmp/orbitsolve
CEARGS="--near 0 --band 0 --prop 9 --propall" $R/rfeed.sh 2ud ud aUbU..aDbD $V/7pair-2ud-to11.log --uonly > $R/r2ud-pa9.log 2>&1
python3 $R/rtable.py
CEARGS="--near 0 --band 0 --prop 7 --propall" $R/rfeed.sh 1u u none..aUbU $V/7pair-1u-to11.log > $R/r1u-pa7.log 2>&1
python3 $R/rtable.py
CEARGS="--near 0 --band 0 --prop 7 --propall" $R/rfeed.sh 1ud ud none..aUbU $V/7pair-1ud-to11.log > $R/r1ud-pa7.log 2>&1
python3 $R/rtable.py
rm -f $R/rloop.stop
exec $R/rloop2.sh >> $R/rloop.log 2>&1
