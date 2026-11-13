#!/bin/bash
# refold.sh [STAGE...] (2026-11-13): fetch arena outputs and fold every stage whose results changed (default: all
#   four).  Per stage, /var/tmp/orbitsolve/optfs/<st>/ holds:
#     lines.txt  the stage lines the tails were taken from (fold.py LINES)
#     all.txt    every tail list given to fastsolve, concatenated in run order
#     outs       one row per run, same order: LOCAL_OUT REMOTE_OUT|- LISTLEN (only the last run may be unfinished)
#     lastfold   name of the stage's latest fold (fsfold/<st>-NAME), lastfold.sum its prod-improved.txt checksum
#   fold.py + vline.py -> prod-improved.txt; if that changed since the last fold: fsfold.sh (1ud, 2ud) or
#   fsfoldax.sh (3ax, 4op) makes the next fold f<N+1> and the previous fold dir is removed (its log is kept).
#   Then ubtable.py (upperbound5.md Working tables) and a copy of each prod-improved.txt in arena/results/.
#   Scripts: ~/orbitsolve/arena (git).  Fold logs: /var/tmp/orbitsolve/fsfold/<st>-<name>.log.  Summary on stdout.
set -u
A=$(cd "$(dirname "$0")" && pwd)   # this script's directory (~/orbitsolve/arena)
echo "# refold.sh $*"
P=$(command -v pypy3) || { echo "pypy3 not on PATH"; exit 1; }
T=~/orbitsolve/devtools/tws; O=/var/tmp/orbitsolve/optfs; F=/var/tmp/orbitsolve/fsfold
STAGES=${*:-1ud 2ud 4op 3ax}
tws() { case $1 in 1ud) echo 1ud-si ;; 2ud) echo 2ud-si ;; 3ax) echo obl8ax ;; 4op) echo obl8op ;; esac; }
mkdir -p $F
for st in $STAGES; do
  case $st in 1ud|2ud|3ax|4op) ;; *) echo "unknown stage $st"; exit 1 ;; esac
  D=$O/$st; outs=()
  [ -f $D/outs ] && [ -f $D/lines.txt ] && [ -f $D/all.txt ] && [ -f $D/lastfold ] || { echo "$st: missing outs/lines.txt/all.txt/lastfold in $D"; exit 1; }
  sum=0; n=$(wc -l < $D/outs)
  while read -r loc rem len; do
    if [ "$rem" != "-" ]; then $A/aget "$rem" "$D/$loc.new" && mv "$D/$loc.new" "$D/$loc" || { echo "$st: fetch $rem failed"; exit 1; }; fi
    outs+=("$D/$loc"); sum=$((sum + len))
  done < $D/outs
  [ "$sum" -eq "$(wc -l < $D/all.txt)" ] || { echo "$st: outs list lengths ($sum) != all.txt ($(wc -l < $D/all.txt)); stop"; exit 1; }
  $P $A/fold.py --pairs $D/prod-pairs.txt $D/lines.txt $D/all.txt "${outs[@]}" > $D/prod-improved.txt 2> $D/fold.err || { echo "$st: fold.py failed"; tail -3 $D/fold.err; exit 1; }
  echo "$st: $(grep -v "^# fold.py" $D/fold.err | tr -d '#' | tr '\n' ' ')"
  r=$($P $A/vline.py $T/$(tws $st).tws $D/prod-pairs.txt | tail -1); echo "$st vline: $r"
  case $r in PASS*) ;; *) echo "$st: vline failed; stop"; exit 1 ;; esac
  last=$(cat $D/lastfold); new=$(cksum < $D/prod-improved.txt)
  if [ -f $D/lastfold.sum ] && [ "$new" = "$(cat $D/lastfold.sum)" ] && [ -d $F/$st-$last ]; then echo "$st: no change, keeps $last"; continue; fi
  name=f$(( $(echo $last | tr -dc 0-9) + 1 ))
  case $st in 1ud|2ud) $A/fsfold.sh $st $name > $F/$st-$name.log 2>&1 ;; *) $A/fsfoldax.sh $st $name > $F/$st-$name.log 2>&1 ;; esac
  if grep -q "^PASS" $F/$st-$name.log; then
    echo "$name" > $D/lastfold; echo "$new" > $D/lastfold.sum; rm -rf $F/$st-$last
    echo "$st $name: $(grep -h "^PASS\|^end lengths\|classes by length" $F/$st-$name.log | cut -c1-200 | tr '\n' ' ')"
  else
    echo "$st $name: FAILED (no PASS; see $F/$st-$name.log); $last kept"; tail -5 $F/$st-$name.log; exit 1
  fi
done
cd $F && python3 $A/ubtable.py $(cat $O/1ud/lastfold) $(cat $O/2ud/lastfold) $(cat $O/4op/lastfold) $(cat $O/3ax/lastfold)
mkdir -p $A/results; for st in 1ud 2ud 3ax 4op; do cp $O/$st/prod-improved.txt $A/results/$st-improved.txt; done
echo "results: $A/results/*-improved.txt (old_len new_len line); commit and push them with journal.md and upperbound5.md"
