#!/bin/bash
# 2026-10-25: feed the ML agent's 2ud lines every 10 minutes (30 until 10-26), from ~/mlcube/adbd20ud_live.txt and
# (once it exists) ~/mlcube/adbd19ud_live.txt (one solving line per line, appended as they come):
# new whole lines past each file's offset (ml2udprocessed.tsv, ml2udprocessed-19.tsv) go through
# rfeed.sh (stage lock; pairlogcheck --keep ud, certextend --prop 8 --slices, certmerge, certcheck).
# After each feed: devtools/tws/adbd20ud-remaining.scr (and adbd19ud-remaining.scr) list the classes
# still at 20 (19).
#   rmlfeed2ud.sh   (stop: touch certruns/relaxed/rmlfeed2ud.stop)
cd ~/orbitsolve
R=certruns/relaxed; V=/var/tmp/orbitsolve; ST=$V/r2ud/stages/aUbU..aDbD; T=devtools/tws
while [ ! -e $R/rmlfeed2ud.stop ]; do
  for spec in "adbd20ud ml2udprocessed.tsv" "adbd19ud ml2udprocessed-19.tsv"; do
    set -- $spec; IN=$HOME/mlcube/$1_live.txt; PT=$R/$2; [ -f $PT ] || echo 0 > $PT
    OFF=$(cat $PT)
    [ -f $IN ] && [ $(wc -c < $IN) -gt $OFF ] || continue
    tail -c +$((OFF + 1)) $IN > $V/ml2ud.raw
    N=$(python3 -c "import sys; d=open(sys.argv[1],'rb').read(); print(d.rfind(b'\\n') + 1)" $V/ml2ud.raw)
    head -c $N $V/ml2ud.raw | awk 'NF{print " " $0}' > $V/ml2ud.log
    echo "== ml $1: $(wc -l < $V/ml2ud.log) new lines ($(date '+%m-%d %H:%M'))"
    CEARGS="--near 0 --band 0 --prop 8 --slices" $R/rfeed.sh 2ud ud aUbU..aDbD $V/ml2ud.log --uonly > $R/r2ud-mlfeed2.log 2>&1
    grep -E "^algorithms|shortened \(|^PASS|WARN" $R/r2ud-mlfeed2.log | cut -c1-160
    sed -n '/== lengths/,/== end/p' $R/r2ud-mlfeed2.log | awk 'NF==2{printf "%s:%s ", $2, $1} END{print ""}'
    if grep -q "^PASS" $R/r2ud-mlfeed2.log && ! grep -q WARNING $R/r2ud-mlfeed2.log; then
      echo $((OFF + N)) > $PT
      python3 $R/mlremaining2.py adbd20ud 20 $ST
      [ -f $T/adbd19ud.scr ] && python3 $R/mlremaining2.py adbd19ud 19 $ST
      python3 $R/rtable.py
    else echo "== ml feed failed; offset kept"; fi
  done
  sleep 600
done
