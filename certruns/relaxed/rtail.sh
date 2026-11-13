#!/bin/bash
# 2026-10-23: pull the part of an arena relaxed log after its processed offset (certruns/relaxed/
# processed.tsv), up to the last complete line, and feed it with the direct pass (certextend
# --near 0 --band 0 --prop 0, or CEARGS).  Advances the offset only when the feed's certcheck passes.
#   rtail.sh NAME KEEP STAGE LOG [PAIRFLAG]
# 2026-10-26: at most MAXBYTES (default 3 GB) per call, so a big backlog doesn't hold the stage lock
# for hours (the ML feeds wait on it); the rest carries over to the next round.
set -u
cd ~/orbitsolve
NAME=$1 KEEP=$2 STG=$3 LOG=$4 PF=${5:-}
PT=certruns/relaxed/processed.tsv
OFF=$(awk -v l=$LOG '$1==l{print $2}' $PT)
T=/var/tmp/orbitsolve/${LOG%.log}.from-$OFF.log
SSH="ssh -i $HOME/.ssh/id_gizmo -o BatchMode=yes 10.0.0.155"
$SSH "bash -c 'cd twsearch2; tail -c +$((OFF + 1)) $LOG | head -c ${MAXBYTES:-3000000000}'" > $T.part
# keep whole lines only: cut after the last newline (only the file's end is read)
python3 - "$T.part" <<'PY'
import os, sys
p = sys.argv[1]; n = os.path.getsize(p); f = open(p, 'r+b'); k = n
while k > 0:
    a = max(0, k - 65536); f.seek(a); blk = f.read(k - a); i = blk.rfind(b'\n')
    if i >= 0: k = a + i + 1; break
    k = a
f.truncate(k); f.close()
PY
mv $T.part $T
N=$(wc -c < $T)
echo "== $NAME: $LOG from $OFF, $N bytes, $(wc -l < $T) lines ($(date '+%m-%d %H:%M'))"
CEARGS="${CEARGS:---near 0 --band 0 --prop 0}" certruns/relaxed/rfeed.sh $NAME $KEEP $STG $T $PF | tee /var/tmp/orbitsolve/rtail.$$.out
if grep -q "^PASS" /var/tmp/orbitsolve/rtail.$$.out && ! grep -q WARNING /var/tmp/orbitsolve/rtail.$$.out; then
  awk -v l=$LOG -v n=$((OFF + N)) 'BEGIN{OFS="\t"} $1==l{$2=n} {print}' $PT > $PT.new && mv $PT.new $PT
  echo "== $LOG processed to $((OFF + N))"
  rm -f $T
else
  echo "== feed failed; offset not advanced ($T kept)"
fi
rm -f /var/tmp/orbitsolve/rtail.$$.out
