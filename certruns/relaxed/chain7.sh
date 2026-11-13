#!/bin/bash
# 2026-10-24: after the slice relaxation, CPU attack on the worst classes before the ML agent:
# 2ud (21s): --proptop 21 with every action of <= 11 moves, then the arena 12-13 lines against
# the worst length (--band 1) if <= 500 classes; 1u (19s), side by side: --proptop 19 with every
# action of <= 10 moves, then two blocks (--proptop2: <= 10 then <= 6).  --slices throughout.
cd ~/orbitsolve
R=certruns/relaxed; V=/var/tmp/orbitsolve
step() {  # NAME KEEP STAGE LOGNAME POOL CEARGS [PAIRFLAG]
  echo "== $1 $4 ($(date '+%m-%d %H:%M')): $6"
  CEARGS="$6" $R/rfeed.sh $1 $2 $3 $5 $7 > $R/r$1-$4.log 2>&1
  grep -E "proptop|band candidates|propagation:|^PASS|WARN" $R/r$1-$4.log | cut -c1-200
  sed -n '/== lengths/,/== end/p' $R/r$1-$4.log | awk 'NF==2&&$2>=17{printf "%s:%s ", $2, $1} END{print ""}'
  python3 $R/rtable.py
}
worstcount() { sed -n '/== lengths/,/== end/p' $1 | awk 'NF==2{c=$1} END{print c+0}'; }
(
  step 2ud ud aUbU..aDbD pt21c11s $V/7pair-2ud-to11.log "--near 0 --band 0 --prop 11 --proptop 21 --slices" --uonly
  c=$(worstcount $R/r2ud-pt21c11s.log); echo "== 2ud worst length has $c classes"
  [ $c -le 500 ] && step 2ud ud aUbU..aDbD band21s $V/7pair-2ud-12to13.log "--near 0 --band 1 --prop 3 --slices" --uonly
) &
step 1u u none..aUbU pt19c10s $V/7pair-1u-to11.log "--near 0 --band 0 --prop 10 --proptop 19 --slices"
step 1u u none..aUbU pt19x6s $V/7pair-1u-to11.log "--near 0 --band 0 --prop 10 --proptop 19 --proptop2 6 --slices"
wait
echo "== chain7 done ($(date '+%m-%d %H:%M'))"
