#!/bin/bash
# aclose.sh STAGE (2026-11-13): close STAGE's last run (last row of optfs/<st>/outs) once it has finished or been
#   stopped (astop.sh): fetch its output a last time; if it stopped early, cut all.txt to what it searched (so
#   mktails.py offers the unsearched tails again) and record the count; mark the row as no longer fetched.
#   Run before mktails.py + alaunch.sh.  Refuses while the run is alive.
set -u
A=$(cd "$(dirname "$0")" && pwd)
echo "# aclose.sh $*"
[ $# -eq 1 ] || { echo "usage: aclose.sh 1ud|2ud|3ax|4op"; exit 1; }
st=$1; D=/var/tmp/orbitsolve/optfs/$st
case $st in 1ud|2ud|3ax|4op) ;; *) echo "unknown stage $st"; exit 1 ;; esac
read -r loc rem len < <(tail -1 $D/outs)
[ "$rem" = "-" ] && { echo "$st: last run $loc already closed"; exit 0; }
r=${loc%.out}
alive=$($A/ar "p=\$(cat fastsolve-x86/runs/$st-$r.pid 2>/dev/null); [ -n \"\$p\" ] && kill -0 \$p 2>/dev/null && echo alive; true") || { echo "ssh to arena failed"; exit 1; }
[ -z "$alive" ] || { echo "$st run $r is still running; astop.sh $st first (or wait for it to finish)"; exit 1; }
$A/aget "$rem" "$D/$loc.new" && mv "$D/$loc.new" "$D/$loc" || { echo "fetch $rem failed"; exit 1; }
c=$(awk '/^Solving/{cur=1; next} cur && (/^Found/ || /^No solution/){n++; cur=0} END{print n+0}' "$D/$loc")
[ "$c" -le "$len" ] || { echo "$loc has $c results for a list of $len; stop"; exit 1; }
tot=$(wc -l < $D/all.txt | tr -d ' ')
if [ "$c" -lt "$len" ]; then
  cp $D/all.txt $D/all.txt.bak; head -n $((tot - len + c)) $D/all.txt.bak > $D/all.txt
  echo "$st run $r stopped at $c of $len: all.txt cut to $((tot - len + c)) lines"
else
  echo "$st run $r finished: $c of $len"
fi
awk -v c="$c" '{r[NR]=$0} END{for(i=1;i<NR;i++) print r[i]; split(r[NR],f," "); print f[1], "-", c}' $D/outs > $D/outs.new && mv $D/outs.new $D/outs
