#!/bin/bash
# 2026-10-24: target-side propagation (certextend --proptop L: every class of length >= L through
# every relaxed action of cost <= C), 2ud then 1u and 1ud, then restart the feed loop (rloop2.sh).
cd ~/orbitsolve
R=certruns/relaxed; V=/var/tmp/orbitsolve
run() {  # NAME KEEP STAGE POOL C L [PAIRFLAG]
  CEARGS="--near 0 --band 0 --prop $5 --proptop $6" $R/rfeed.sh $1 $2 $3 $4 $7 > $R/r$1-pt$5.log 2>&1
  grep -E "proptop|propagation:|^PASS|WARN" $R/r$1-pt$5.log | cut -c1-160
  python3 $R/rtable.py
}
run 2ud ud aUbU..aDbD $V/7pair-2ud-to11.log 10 22 --uonly
run 2ud ud aUbU..aDbD $V/7pair-2ud-to11.log 11 22 --uonly
run 1u u none..aUbU $V/7pair-1u-to11.log 9 19
run 1ud ud none..aUbU $V/7pair-1ud-to11.log 9 19
run 1u u none..aUbU $V/7pair-1u-to11.log 10 19
rm -f $R/rloop.stop
exec $R/rloop2.sh >> $R/rloop.log 2>&1
