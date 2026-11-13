#!/bin/bash
# 2026-10-24: feed the ML agent's 2ud lines (~/mlcube/adbd22ud_live.txt, one solving line per
# line, appended as they come) every 10 minutes: new lines past the offset in mlprocessed.tsv go
# through rfeed.sh (stage lock; pairlogcheck --keep ud, certextend --prop 8, certmerge, certcheck),
# then the classes still at the worst length are rewritten to devtools/tws/adbd22ud-remaining.scr
# (same names as adbd22ud.scr) so the agent can skip the ones already shortened.
#   rmlfeed.sh   (stop: touch certruns/relaxed/rmlfeed.stop)
cd ~/orbitsolve
R=certruns/relaxed; V=/var/tmp/orbitsolve; IN=$HOME/mlcube/adbd22ud_live.txt
PT=$R/mlprocessed.tsv; [ -f $PT ] || echo 0 > $PT
while [ ! -e $R/rmlfeed.stop ]; do
  OFF=$(cat $PT)
  if [ -f $IN ] && [ $(wc -c < $IN) -gt $OFF ]; then
    # whole lines only: up to the last newline past the offset
    tail -c +$((OFF + 1)) $IN > $V/ml22.raw
    N=$(python3 -c "import sys; d=open(sys.argv[1],'rb').read(); print(d.rfind(b'\\n') + 1)" $V/ml22.raw)
    head -c $N $V/ml22.raw | awk 'NF{print " " $0}' > $V/ml22.log
    echo "== ml: $(wc -l < $V/ml22.log) new lines ($(date '+%m-%d %H:%M'))"
    CEARGS="--near 0 --band 0 --prop 8 --slices" $R/rfeed.sh 2ud ud aUbU..aDbD $V/ml22.log --uonly > $R/r2ud-mlfeed.log 2>&1
    grep -E "^algorithms|shortened \(|^PASS|WARN" $R/r2ud-mlfeed.log | cut -c1-160
    if grep -q "^PASS" $R/r2ud-mlfeed.log && ! grep -q WARNING $R/r2ud-mlfeed.log; then
      echo $((OFF + N)) > $PT
      python3 $R/mlremaining.py && python3 $R/rtable.py
    else echo "== ml feed failed; offset kept"; fi
  fi
  sleep 600
done
