#!/bin/bash
# 2026-10-24: 2ud, harder (chain4 without its wait): alternate (a) the arena 12-13 lines
# streamed against the worst length (certextend --band 1, when that length has <= 3000 classes),
# (b) --proptop 21 with cost-10 actions (lower the 21s so the 22s have shorter neighbours), and
# (c) --proptop 22 with cost-11 actions again.
cd ~/orbitsolve
R=certruns/relaxed; V=/var/tmp/orbitsolve; S=$V/r2ud/stages/aUbU..aDbD
python3 $R/rtable.py
step() {  # LOGNAME POOL CEARGS
  echo "== $1 ($(date '+%m-%d %H:%M')): $3"
  CEARGS="$3" $R/rfeed.sh 2ud ud aUbU..aDbD $2 --uonly > $R/r2ud-$1.log 2>&1
  grep -E "proptop|band candidates|propagation:|^PASS|WARN" $R/r2ud-$1.log | cut -c1-200
  sed -n '/== lengths/,/== end/p' $R/r2ud-$1.log | awk 'NF==2&&$2>=20{printf "%s:%s ", $2, $1} END{print ""}'
  python3 $R/rtable.py
}
worstcount() { sed -n '/== lengths/,/== end/p' $(ls -t $R/r2ud-*.log | head -1) | awk 'NF==2{c=$1} END{print c}'; }
for round in 1 2; do
  [ $(worstcount) -le 3000 ] && step band$round $V/7pair-2ud-12to13.log "--near 0 --band 1 --prop 8"
  step pt21c10r$round $V/7pair-2ud-to11.log "--near 0 --band 0 --prop 10 --proptop 21"
  step pt22c11r$round $V/7pair-2ud-to11.log "--near 0 --band 0 --prop 11 --proptop 22"
done
echo "== done ($(date '+%m-%d %H:%M'))"
