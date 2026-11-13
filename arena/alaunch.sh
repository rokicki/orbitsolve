#!/bin/bash
# alaunch.sh [--build] STAGE NAME LIST (2026-11-13): start fastsolve on arena for STAGE (1ud 2ud 3ax 4op) on LIST (generating
#   tails, one per line; e.g. from mktails.py) as run NAME.  Refuses if a run of STAGE is still alive.  Steps:
#   1. the stage's previous run must be closed (aclose.sh);
#   2. LIST goes to arena ~/fastsolve-x86/<st>/NAME.txt and solve8b starts detached on the stage's NUMA node with
#      FTNOBUILD=1 (it maps the existing /dev/shm/<st>-300 table and exits if there is none; it never builds);
#      pid in runs/<st>-NAME.pid, output runs/<st>-NAME.out / .err.  --build (after an arena reboot only): solve8
#      without FTNOBUILD, which first builds the D=9 table in /dev/shm (hours), then searches LIST;
#   3. only after a good start: LIST is appended to all.txt and a row to outs, so refold.sh folds it.
set -u
A=$(cd "$(dirname "$0")" && pwd)
echo "# alaunch.sh $*"
bin=solve8b nb="FTNOBUILD=1"; [ "${1:-}" = "--build" ] && { bin=solve8; nb=""; shift; }
case ${1:-} in -*) echo "unknown option $1"; exit 1 ;; esac
[ $# -eq 3 ] || { echo "usage: alaunch.sh [--build] 1ud|2ud|3ax|4op NAME LIST"; exit 1; }
st=$1; name=$2; list=$3; D=/var/tmp/orbitsolve/optfs/$st
case $st in
  1ud) node=0 tws=1ud-si.tws lg=8 me=1e12 ;;
  2ud) node=1 tws=2ud-si.tws lg=8 me=1e12 ;;
  3ax) node=0 tws=obl8ax-nr.tws lg=50 me=1e13 ;;
  4op) node=1 tws=obl8op-nr.tws lg=24 me=1e13 ;;
  *) echo "unknown stage $st"; exit 1 ;;
esac
case $name in *[!A-Za-z0-9_-]*|"") echo "NAME: letters, digits, - and _ only"; exit 1 ;; esac
[ -s "$list" ] || { echo "empty or missing list $list"; exit 1; }
grep -q "^$name.out " $D/outs && { echo "run $name already in $D/outs"; exit 1; }
n=$(wc -l < "$list" | tr -d ' ')
dup=$(awk 'NR==FNR{s[$0]=1; next} ($0 in s){c++} END{print c+0}' $D/all.txt "$list")
[ "$dup" -eq 0 ] || { echo "$dup tails of $list are already in all.txt (queued before); remake the list with mktails.py"; exit 1; }
alive=$($A/ar "cd fastsolve-x86; for f in runs/$st-*.pid; do [ -f \$f ] && kill -0 \$(cat \$f) 2>/dev/null && echo \$f; done; true") || { echo "ssh to arena failed"; exit 1; }
[ -z "$alive" ] || { echo "a $st run is alive ($alive); stop it first (astop.sh $st)"; exit 1; }
read -r loc rem len < <(tail -1 $D/outs)
[ "$rem" = "-" ] || { echo "previous $st run ($loc) not closed: aclose.sh $st first"; exit 1; }
# 2. start
$A/ar "cat > fastsolve-x86/$st/$name.txt" < "$list" || { echo "copy to arena failed"; exit 1; }
$A/ar "cd fastsolve-x86 || exit 1; [ \$(wc -l < $st/$name.txt) -eq $n ] || { echo list copy incomplete; exit 1; }
  FTSHM=/dev/shm/$st-300 FTGB=300 LAYERGB=$lg MAXEST=$me $nb setsid nohup numactl --cpunodebind=$node \
  --membind=$node ./$bin $tws $st/$name.txt 1000000 20 > runs/$st-$name.out 2> runs/$st-$name.err < /dev/null &
  echo \$! > runs/$st-$name.pid" || { echo "start failed"; exit 1; }
sleep 30
ok=$($A/ar "cd fastsolve-x86; kill -0 \$(cat runs/$st-$name.pid) 2>/dev/null && echo alive; grep -c Solving runs/$st-$name.out; tail -2 runs/$st-$name.err")
echo "$ok"
case $ok in alive*) ;; *) echo "solver not running: see arena runs/$st-$name.err (table missing after a reboot? see RUNBOOK.md)"; exit 1 ;; esac
# 3. register
cat "$list" >> $D/all.txt
echo "$name.out fastsolve-x86/runs/$st-$name.out $n" >> $D/outs
echo "$st run $name started on node $node: $n tails (pid $($A/ar "cat fastsolve-x86/runs/$st-$name.pid"))"
