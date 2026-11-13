#!/bin/bash
# 2026-10-25: feed the ML agent's 1ud lines for the 1ud 18s (devtools/tws/aubu18ud.scr) from
# ~/mlcube/aubu18ud_live.txt (one solving line per line, appended as they come) every 30 minutes:
# new whole lines past the offset in ml1udprocessed.tsv go through rfeed.sh (stage lock; pairlogcheck
# --keep ud, certextend --prop 8 --slices, certmerge, certcheck), then the table is rebuilt.
#   rmlfeed1ud.sh   (stop: touch certruns/relaxed/rmlfeed1ud.stop)
cd ~/orbitsolve
R=certruns/relaxed; V=/var/tmp/orbitsolve; IN=$HOME/mlcube/aubu18ud_live.txt
PT=$R/ml1udprocessed.tsv; [ -f $PT ] || echo 0 > $PT
while [ ! -e $R/rmlfeed1ud.stop ]; do
  OFF=$(cat $PT)
  if [ -f $IN ] && [ $(wc -c < $IN) -gt $OFF ]; then
    tail -c +$((OFF + 1)) $IN > $V/ml18ud.raw
    N=$(python3 -c "import sys; d=open(sys.argv[1],'rb').read(); print(d.rfind(b'\\n') + 1)" $V/ml18ud.raw)
    head -c $N $V/ml18ud.raw | awk 'NF{print " " $0}' > $V/ml18ud.log
    echo "== ml: $(wc -l < $V/ml18ud.log) new lines ($(date '+%m-%d %H:%M'))"
    CEARGS="--near 0 --band 0 --prop 8 --slices" $R/rfeed.sh 1ud ud none..aUbU $V/ml18ud.log > $R/r1ud-mlfeed.log 2>&1
    grep -E "^algorithms|shortened \(|^PASS|WARN" $R/r1ud-mlfeed.log | cut -c1-160
    sed -n '/== lengths/,/== end/p' $R/r1ud-mlfeed.log | awk 'NF==2{printf "%s:%s ", $2, $1} END{print ""}'
    if grep -q "^PASS" $R/r1ud-mlfeed.log && ! grep -q WARNING $R/r1ud-mlfeed.log; then
      echo $((OFF + N)) > $PT
      python3 $R/rtable.py
    else echo "== ml feed failed; offset kept"; fi
  fi
  sleep 1800
done
