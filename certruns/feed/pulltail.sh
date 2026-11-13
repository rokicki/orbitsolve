#!/bin/bash
# pulltail.sh LOG [MAXBYTES]: bring over the next unprocessed slice of an arena twsearch log
# (arena:twsearch2/LOG, append-only), feed it, and advance certruns/feed/processed.tsv.
# The slice starts right after the processed byte count (which must end on a newline there),
# holds at most MAXBYTES (default 3e9), and is cut back to its last complete line.  It is fed
# as a log of its own, then deleted.  PT_CEARGS (environment) replaces the certextend options.
# Run from the orbitsolve directory.
set -u
LOG=$1; MAX=${2:-3000000000}
R="ssh -i $HOME/.ssh/id_gizmo -o BatchMode=yes 10.0.0.155"
F=certruns/feed/processed.tsv
P=$(awk -v l="$LOG" '$1 == l {print $2}' $F)
[ -n "$P" ] || { echo "no processed entry for $LOG" >&2; exit 1; }
case $LOG in
  7pairud-2.log) MODE=ud; STAGES="aUaDbUbD..aLaFaRaB:2 aUaLaFaRaBaDbUbD..bLbFbRbB:6" ;;
  7pairudb-2.log) MODE=bonly; STAGES="aUaLaFaRaBaDbUbD..bLbFbRbB:6" ;;
  7pairup-2.log) MODE=uonly; STAGES="aUbU..aDbD:2" ;;
  7pairupu-2.log) MODE=uonly; STAGES="aUbU..aDbD:2" ;;  # stopped on arena 2026-10-15, first fed 2026-10-20  # G 2: at worst 24 the band is 1M and G 6 tries every seed
  *) echo "no feed plan for $LOG" >&2; exit 1 ;;
esac
SZ=$($R "stat -c %s twsearch2/$LOG") || exit 1
echo "# $LOG: arena $SZ bytes, processed $P, new $((SZ - P))"
[ $SZ -gt $P ] || { echo "nothing new"; exit 0; }
if [ $P -gt 0 ]; then
  B=$($R "tail -c +$P twsearch2/$LOG | head -c 1 | od -An -tx1" | tr -d ' ')
  [ "$B" = 0a ] || { echo "byte $P of $LOG is not a newline ($B); log changed?" >&2; exit 1; }
fi
T=/var/tmp/orbitsolve/${LOG%.log}.from-$P.log  # out of Time Machine
$R "tail -c +$((P + 1)) twsearch2/$LOG | head -c $MAX" > $T || { rm -f $T; exit 1; }
K=$(python3 -c "import sys; d=open(sys.argv[1],'rb').read(); print(d.rfind(b'\n') + 1)" $T)
[ $K -gt 0 ] || { echo "no complete line" >&2; rm -f $T; exit 1; }
python3 -c "import sys, os; os.truncate(sys.argv[1], int(sys.argv[2]))" $T $K
echo "# slice $(basename $T): bytes $P..$((P + K)), $(wc -l < $T) lines"
for sg in $STAGES; do
  st=${sg%:*}; g=${sg#*:}
  ln -sf $T $T.$g.log
  CHUNK=5000000 CEARGS="${PT_CEARGS:---near 12 --band 1 --worst --seenbits 29 --merge $g}" devtools/pairfeed.sh $MODE $T.$g.log $st || exit 1
  rm -f $T.$g.log /var/tmp/orbitsolve/seen/$(basename $T.$g .log).seen
done
awk -v l="$LOG" -v n=$((P + K)) 'BEGIN{OFS="\t"} $1 == l {$2 = n} {print}' $F > $F.new && mv $F.new $F
rm -f $T
echo "== $LOG processed to $((P + K)) ($(date +%H:%M))"
