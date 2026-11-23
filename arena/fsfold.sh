#!/bin/bash
# fsfold.sh STAGE NAME (2026-11-11): fold arena fastsolve tail results into a copy of the expanded P 80 stage.
#   STAGE 1ud|2ud.  Lines: optfs/<st>/prod-improved.txt (fold.py, vline PASS).  Pool: every fastsolve find
#   (optfs/<st>/*.out) -> pairlogcheck --emit 7 10.  Then: certmerge the lines; certextend with the pool;
#   up to 3 --proptop 16 pulls; certcheck --keep ud; lengths.
#   Out: /var/tmp/orbitsolve/fsfold/<st>-<NAME>/stages/<stagedir>.  Log on stdout.
set -u
[ $# -eq 2 ] || { echo "usage: fsfold.sh 1ud|2ud NAME"; exit 1; }
cd ~/orbitsolve
st=$1; case $st in 1ud) SD=none..aUbU; T=1ud-si ;; 2ud) SD=aUbU..aDbD; T=2ud-si ;; *) echo "unknown stage $st"; exit 1 ;; esac
O=/var/tmp/orbitsolve/optfs/$st; R=/var/tmp/orbitsolve/fsfold/$st-$2; ST=$R/stages/$SD
PY=$(command -v pypy3) || { echo "pypy3 not on PATH"; exit 1; }; TWS=devtools/tws/$T.tws
D=dist/gullicube/data
BASE="--base $D/pair3.cls --base $D/p8s10.cls --base $D/p9.cls --base $D/eg8.cls --base $D/c12x.cls"
log() { echo "== $* ($(date '+%m-%d %H:%M'))"; }
lens() { for f in $ST/w*.txt; do awk '{print NF}' $f; done | sort -n | uniq -c | awk '$2>=14{printf "%s:%s ", $2, $1}'; echo; }
echo "# fsfold.sh $*"
mkdir -p $R/stages; rm -rf $ST; cp -a /var/tmp/orbitsolve/optfs/full/stages/$SD $ST; chmod -R u+w $ST
echo "start lengths: $(lens)"
log "merge fold.py lines"
cut -d' ' -f3- $O/prod-improved.txt > $R/fold.txt; wc -l < $R/fold.txt
devtools/certmerge $ST $R/fold.txt --keep ud > $R/merge0.out 2>&1; grep "^improvements" $R/merge0.out
# carry forward (11-23, user-approved): merge the previous fold's lines, so a class shortened once cannot slip back
# when certextend (not deterministic) misses it; plus any extra stage dirs listed in $O/carry.dirs (one-shot recovery).
log "carry forward"
PREV=$(cat $O/lastfold 2>/dev/null); PD=/var/tmp/orbitsolve/fsfold/$st-$PREV/stages/$SD
for cd in $PD $(cat $O/carry.dirs 2>/dev/null); do
  [ -d "$cd" ] || { echo "carry: $cd missing, skipped"; continue; }
  cat $cd/w*.txt > $R/carry.txt; echo "carry $cd: $(wc -l < $R/carry.txt) lines"
  devtools/certmerge $ST $R/carry.txt --keep ud > $R/mergeC.out 2>&1; grep "^improvements" $R/mergeC.out
done
rm -f $R/carry.txt
log "pool"
cat $O/*.out | awk '/^Solving/{next} /^ [^ ]/{print}' > $R/finds.txt; wc -l < $R/finds.txt
devtools/pairlogcheck --keep ud --emit 7 10 < $R/finds.txt > $R/pool.cls 2> $R/check.err; grep -v "^  " $R/check.err | tail -2
ext() {
  log "certextend $1 $2"
  devtools/certextend $ST --near 0 --band 0 --keep ud --slices --prop 6 $2 --threads 6 $BASE $R/pool.cls > $R/out$1.txt 2> $R/ext$1.err
  grep -v "^#" $R/out$1.txt > $R/lines$1.txt; wc -l < $R/lines$1.txt
  devtools/certmerge $ST $R/lines$1.txt --keep ud > $R/merge$1.out 2>&1; grep "^improvements" $R/merge$1.out
}
ext E ""
for pt in 1 2 3; do ext P$pt "--proptop 16"; [ $(wc -l < $R/linesP$pt.txt) -lt 1000 ] && break; done
log "certcheck"
devtools/certcheck $ST --keep ud > $R/certcheck.out 2>&1; tail -1 $R/certcheck.out
echo "end lengths: $(lens)"
log end
