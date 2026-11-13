# dmloop.sh LEN P [FIRST]: deep merges of exactly LEN moves (sources <= 14 moves of 7pairudb-2-full.log,
# overlap 5) in P slices (deepmerge --part P:p); slice p+1 is generated while slice p is fed to
# the b ring (products, K = 10); slice files go in /var/tmp/orbitsolve (out of Time Machine) and
# are deleted after their feed.  Stop: touch
# certruns/deepmerge/dmloop.stop (between slices).
cd ~/orbitsolve
LEN=$1; P=$2; F=${3:-0}
B=aUaLaFaRaBaDbUbD..bLbFbRbB
gen() { devtools/deepmerge --src 14 --overlap 5 --minlen $LEN --maxlen $LEN --part $P:$1 7pairudb-2-full.log > /var/tmp/orbitsolve/dm$LEN-$1.log 2> certruns/deepmerge/dm$LEN-$1.err; }
tail29() { cat cert/obl23pair/cf-xfree/stages/$B/w*.txt | awk '{c[NF]++} END {for (n = 25; n <= 30; n++) if (c[n]) printf " %d:%d", n, c[n]; print ""}'; }
echo "== start dm$LEN in $P slices from $F ($(date '+%m-%d %H:%M')):$(tail29)"
gen $F
for ((p = F; p < P; p++)); do
  [ -e certruns/deepmerge/dmloop.stop ] && { echo "== stopped before slice $p"; break; }
  if [ $((p + 1)) -lt $P ]; then gen $((p + 1)) & G=$!; else G=; fi
  echo "== slice $p: $(grep -c . /var/tmp/orbitsolve/dm$LEN-$p.log) lines; $(tail -2 certruns/deepmerge/dm$LEN-$p.err | head -1)"
  CHUNK=5000000 CEARGS="--near 10 --band 1 --seenbits 29 --merge 6" devtools/pairfeed.sh bonly /var/tmp/orbitsolve/dm$LEN-$p.log $B || echo "== slice $p failed (stage restored)"
  rm -f /var/tmp/orbitsolve/dm$LEN-$p.log /var/tmp/orbitsolve/seen/dm$LEN-$p.seen
  echo "== slice $p done ($(date '+%m-%d %H:%M')):$(tail29)"
  [ -n "$G" ] && wait $G
done
echo "== end dm$LEN ($(date '+%m-%d %H:%M')):$(tail29)"
