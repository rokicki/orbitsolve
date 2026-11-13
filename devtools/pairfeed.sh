#!/bin/bash
# pairfeed.sh: feed a (growing) twsearch pair log into the chiral-pair certificate stages.
#   pairfeed.sh MODE LOG STAGE...
# MODE is the pairlogcheck filter (pair, ud, uonly, bonly); STAGE is a stage directory name under
# cert/obl23pair/cf-xfree/stages (e.g. none..aUbU).  Only lines of LOG not seen by an earlier run
# (/var/tmp/orbitsolve/seen/<log>.seen) are validated and streamed; each stage runs certextend on them,
# merges with certmerge and must pass certcheck.
# On any failure the stage directory is restored and the script stops (the
# log's seen set is only updated when every stage succeeded).  A stage added later never sees the
# lines already recorded: run certextend on the whole pool for it.
# CEARGS (environment) replaces the default certextend search options "--near 12", e.g.
# CEARGS="--near 0 --band 1" only tries to shorten the classes at each stage's worst length.
# CHUNK=N (environment) feeds the new lines N at a time, each chunk merged and checked on its own
# (gains land as it goes; a failure restores only the current chunk's stage state).
# A stage is locked (certruns/feed/locks) while it is extended, so feeds can run side by side.
# Run from the orbitsolve directory.
set -u
if [ $# -lt 3 ]; then echo "usage: pairfeed.sh pair|ud|uonly|bonly LOG STAGE..." >&2; exit 1; fi
echo "# pairfeed.sh $*${CEARGS:+ (CEARGS=$CEARGS)}"
MODE=$1; LOG=$2; shift 2
case $MODE in pair) FLAG= ;; ud) FLAG=--ud ;; uonly) FLAG=--uonly ;; bonly) FLAG=--bonly ;; *) echo "unknown mode $MODE" >&2; exit 1 ;; esac
CERT=cert/obl23pair/cf-xfree/stages
FEED=certruns/feed
SEEN=/var/tmp/orbitsolve/seen
TMP=/var/tmp/orbitsolve/pairfeed.$$
mkdir -p $FEED $SEEN $TMP || exit 1
LOCK=
trap 'rm -rf $TMP; [ -n "$LOCK" ] && rmdir $LOCK' EXIT
mkdir -p $FEED/locks || exit 1
D=dist/gullicube/data
BASE="--base $D/pair3.cls --base $D/p8s10.cls --base $D/p9.cls --base $D/eg8.cls --base $D/c12x.cls"
L=$(basename $LOG .log)
for st in "$@"; do [ -d $CERT/$st ] || { echo "no stage $st" >&2; exit 1; }; done
cp $SEEN/$L.seen $TMP/seen.new 2>/dev/null || rm -f $TMP/seen.new
devtools/pairlogcheck $FLAG --emit --new $TMP/seen.new 7 10 < $LOG > $TMP/new.cls 2> $TMP/check.err || { cat $TMP/check.err; exit 1; }
cat $TMP/check.err
if grep -q "another oblique [1-9]\|break U/D [1-9]\|unparsable [1-9]" $TMP/check.err; then echo "note: some lines of $LOG were rejected (not emitted)" >&2; fi
NEW=$(grep -vc "^#" $TMP/new.cls)
echo "$NEW new lines"
if [ -n "${CHUNK:-}" ]; then grep -v "^#" $TMP/new.cls | python3 devtools/lsplit.py $CHUNK $TMP/chunk.; rm $TMP/new.cls; CH=$(ls $TMP/chunk.* 2>/dev/null); else CH=$TMP/new.cls; fi
NCH=$(( $(echo $CH | wc -w) ))
for st in "$@"; do
 IC=0
 for chunk in $CH; do
  IC=$((IC + 1))
  [ $NCH -gt 1 ] && echo "$st: chunk $IC of $NCH ($(date '+%H:%M'))"
  until mkdir $FEED/locks/$st 2>/dev/null; do sleep 30; done
  LOCK=$FEED/locks/$st
  rm -rf $TMP/stage.bak; cp -a $CERT/$st $TMP/stage.bak
  restore() { echo "$st: $1; restoring" >&2; rm -rf $CERT/$st; cp -a $TMP/stage.bak $CERT/$st; exit 1; }
  devtools/certextend $CERT/$st ${CEARGS:---near 12} $BASE $chunk > $TMP/out.txt 2> $TMP/ext.err || { cat $TMP/ext.err; restore "certextend failed"; }
  grep -v "^  length\|^base" $TMP/ext.err
  grep -v "^#" $TMP/out.txt > $TMP/lines.txt
  if [ -s $TMP/lines.txt ]; then
    devtools/certmerge $CERT/$st $TMP/lines.txt > $TMP/merge.out 2>&1 || { cat $TMP/merge.out; restore "certmerge failed"; }
    tail -1 $TMP/merge.out
    grep -q " 0 not shorter, 0 classes not in" $TMP/merge.out || restore "certmerge rejected lines"
    devtools/certcheck $CERT/$st > $TMP/cc.out 2>&1
    tail -1 $TMP/cc.out
    grep -q "^PASS" $TMP/cc.out || restore "certcheck failed"
  fi
  rmdir $LOCK; LOCK=
 done
done
mv $TMP/seen.new $SEEN/$L.seen
echo "done"
