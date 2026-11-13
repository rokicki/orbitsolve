#!/bin/bash
# runner.sh STAGE (2026-11-12): unattended fastsolve queue runner for the arena week run.
#   ~/fastsolve-x86/week/STAGE/: env (settings, below), queue/ (pending chunks NNNNN.txt: generating tails, run in
#   name order), run/ (the chunk in progress), done/ (finished chunks with .out/.err), failed/, runner.log.
#   Loop: take the first queue/ chunk, run solve on it (the first run builds the table into FTSHM; later runs map it
#   in seconds), move it to done/, log one line.  Empty queue: wait 5 min.  touch STAGE/STOP: stop after this chunk.
#   On start, a chunk left in run/ (crash, kill) goes back to queue/ and is searched again.
#   PIDs: STAGE/runner.pid, STAGE/solve.pid (kill by these exact PIDs; never pkill/pgrep -f).
#   env sets: BIN TWS NODE FTGB LAYERGB MAXEST FTLEV THREADS, optional SHM (sourced; table file, default /dev/shm/STAGE-week).
# Start detached: setsid nohup ./runner.sh STAGE < /dev/null > /dev/null 2>&1 &
set -u
[ $# -eq 1 ] || { echo "usage: runner.sh STAGE"; exit 1; }
st=$1; W=$HOME/fastsolve-x86/week/$st; cd $HOME/fastsolve-x86 || exit 1
[ -f $W/env ] || { echo "no $W/env"; exit 1; }
set -a; . $W/env; set +a   # every setting is exported (e.g. FTNOBUILD=1 reaches the solver)
mkdir -p $W/queue $W/run $W/done $W/failed
log() { echo "$(date '+%m-%d %H:%M:%S') $*" >> $W/runner.log; }
for f in $W/run/*.txt; do [ -e "$f" ] || continue; mv "$f" $W/queue/; rm -f "${f%.txt}".out "${f%.txt}".err; log "requeued $(basename $f) (left in run/)"; done
echo $$ > $W/runner.pid
log "runner start: pid $$, $BIN $TWS node $NODE FTLEV $FTLEV"
while [ ! -e $W/STOP ]; do
  c=$(ls $W/queue 2>/dev/null | grep '^[0-9]*\.txt$' | sort | head -1)
  if [ -z "$c" ]; then sleep 300; continue; fi
  mv $W/queue/$c $W/run/$c || { sleep 10; continue; }
  b=${c%.txt}; t0=$(date +%s); n=$(wc -l < $W/run/$c)
  FTSHM=${SHM:-/dev/shm/$st-week} FTGB=$FTGB LAYERGB=$LAYERGB MAXEST=$MAXEST FTLEV=$FTLEV numactl --cpunodebind=$NODE --membind=$NODE \
    ./$BIN $TWS $W/run/$c 1000000 $THREADS > $W/run/$b.out 2> $W/run/$b.err < /dev/null &
  sp=$!; echo $sp > $W/solve.pid; wait $sp
  rc=$?; dt=$(( $(date +%s) - t0 )); rm -f $W/solve.pid
  s=$(grep -c '^Solving' $W/run/$b.out); fd=$(grep -c '^Found' $W/run/$b.out)
  if [ $rc -eq 0 ] && [ "$s" -eq "$n" ]; then
    mv $W/run/$c $W/run/$b.out $W/run/$b.err $W/done/
    log "done $b: $n tails, $fd found, ${dt}s, $(awk -v d=$dt -v n=$n 'BEGIN{printf "%.2f", d/n}') s/tail"
  else
    mv $W/run/$c $W/run/$b.out $W/run/$b.err $W/failed/
    log "FAILED $b: rc $rc, $s of $n searched, ${dt}s (moved to failed/)"; sleep 60
  fi
done
log "runner stop (STOP file)"; rm -f $W/runner.pid
