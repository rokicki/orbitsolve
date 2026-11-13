#!/bin/bash
# 2026-10-23: feed a relaxed-stage arena log into a copy of a 2026-10-22 stage.
#   rfeed.sh NAME KEEP STAGE LOG [PAIRFLAG]
# NAME: relaxed stage (1u, 1ud, 2ud, ...); the copy is /var/tmp/orbitsolve/r$NAME/stages/STAGE
# (made from the P93-2026-10-22 snapshot on first use).  KEEP: pairlogcheck/certcheck --keep (u, ud).
# PAIRFLAG: extra pairlogcheck filter for the pair itself (e.g. --uonly for stage 2).
# One feed per stage at a time: the lock directory r$NAME/lock (holding the owner's pid) is taken
# first; a feed on a locked stage waits for it (logging the owner) and takes over a stale lock.
# Each run's work files are in r$NAME/run.PID, removed at the end.
# Steps: pairlogcheck --keep KEEP --emit, certextend (CEARGS, default --near 0: each line used directly, then propagated), certmerge, certcheck --keep KEEP.
set -u
cd ~/orbitsolve
NAME=$1 KEEP=$2 STG=$3 LOG=$4 PF=${5:-}
R=/var/tmp/orbitsolve/r$NAME; ST=$R/stages/$STG
D=dist/gullicube/data
BASE="--base $D/pair3.cls --base $D/p8s10.cls --base $D/p9.cls --base $D/eg8.cls --base $D/c12x.cls"
mkdir -p $R/stages
L=$R/lock
until mkdir $L 2>/dev/null; do
  o=$(cat $L/pid 2>/dev/null)
  if [ -n "$o" ] && ! kill -0 $o 2>/dev/null; then echo "== stale lock of pid $o removed"; rm -rf $L; continue; fi
  [ -n "${w:-}" ] || { echo "== waiting for the r$NAME lock (pid $o: $(ps -o command= -p $o 2>/dev/null | cut -c1-80)) ($(date '+%m-%d %H:%M'))"; w=1; }
  sleep 20
done
echo $$ > $L/pid
W=$R/run.$$; mkdir -p $W
trap 'rm -rf $W $L' EXIT
[ -d $ST ] || { cp -a ~/orbitsolve-certsafe/obl23pair-cf-xfree-P93-2026-10-22/stages/$STG $ST; chmod -R u+w $ST; }
echo "== $NAME: pairlogcheck --keep $KEEP $PF ($(date '+%m-%d %H:%M')) $(basename $LOG)"
devtools/pairlogcheck --keep $KEEP $PF --emit 7 10 < $LOG > $W/pool.cls 2> $W/check.err
grep -v "^  " $W/check.err | tail -3
echo "== certextend ${CEARGS:---near 0} ($(date '+%m-%d %H:%M'))"
devtools/certextend $ST ${CEARGS:---near 0} --keep $KEEP $BASE $W/pool.cls > $W/out.txt 2> $W/ext.err
grep -v "^  length\|^base" $W/ext.err | tail -8
grep -v "^#" $W/out.txt > $W/lines.txt
echo "== certmerge ($(date '+%m-%d %H:%M'))"
devtools/certmerge $ST $W/lines.txt --keep $KEEP > $W/merge.out 2>&1
grep "^improvements" $W/merge.out; tail -1 $W/merge.out
grep -q " 0 disturb another centre" $W/merge.out || echo "WARNING: certmerge rejected lines for side effects"
echo "== certcheck --keep $KEEP ($(date '+%m-%d %H:%M'))"
devtools/certcheck $ST --keep $KEEP 2>&1 | tail -4
echo "== lengths"
for f in $ST/w*.txt; do awk '{print NF}' $f; done | sort -n | uniq -c
echo "== end ($(date '+%m-%d %H:%M'))"
