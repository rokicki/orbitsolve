#!/bin/bash
# 2026-10-24: 2ud, harder, after the two-block pass on the last 23 (rfeed's stage lock orders the
# steps; no pgrep waits): --proptop 21 cost 10, --proptop 22 cost 11, the arena 12-13 lines against
# the worst length (--band 1, when it has <= 3000 classes), twice; then 1ud --proptop 19 cost 9,
# then the feed loop (rloop2.sh).
cd ~/orbitsolve
R=certruns/relaxed; V=/var/tmp/orbitsolve
step() {  # NAME KEEP STAGE LOGNAME POOL CEARGS [PAIRFLAG]
  echo "== $1 $4 ($(date '+%m-%d %H:%M')): $6"
  CEARGS="$6" $R/rfeed.sh $1 $2 $3 $5 $7 > $R/r$1-$4.log 2>&1
  grep -E "proptop|band candidates|propagation:|^PASS|WARN" $R/r$1-$4.log | cut -c1-200
  sed -n '/== lengths/,/== end/p' $R/r$1-$4.log | awk 'NF==2&&$2>=18{printf "%s:%s ", $2, $1} END{print ""}'
  python3 $R/rtable.py
  LAST=$R/r$1-$4.log
}
worstcount() { sed -n '/== lengths/,/== end/p' $LAST | awk 'NF==2{c=$1} END{print c+0}'; }
for r in 1 2; do
  step 2ud ud aUbU..aDbD pt21c10r$r $V/7pair-2ud-to11.log "--near 0 --band 0 --prop 10 --proptop 21" --uonly
  step 2ud ud aUbU..aDbD pt22c11r$r $V/7pair-2ud-to11.log "--near 0 --band 0 --prop 11 --proptop 22" --uonly
  c=$(worstcount); echo "== worst length has $c classes"
  [ $c -le 3000 ] && step 2ud ud aUbU..aDbD band$((r + 1)) $V/7pair-2ud-12to13.log "--near 0 --band 1 --prop 8" --uonly
done
step 1ud ud none..aUbU pt9 $V/7pair-1ud-to11.log "--near 0 --band 0 --prop 9 --proptop 19"
rm -f $R/rloop.stop
echo "== chain6 done ($(date '+%m-%d %H:%M')); rloop2"
exec $R/rloop2.sh >> $R/rloop.log 2>&1
