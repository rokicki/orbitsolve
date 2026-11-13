# dmu.sh LEN P FIRST: feed dmloop.sh's slices to aDbD as well (uonly filter, products K = 10, no
# --worst).  Each slice dm$LEN-p.log is hard-linked once deepmerge has finished it (its .err has the
# final count), so dmloop's rm after the b-ring feed does not free it before this feed is done.
cd ~/orbitsolve
LEN=$1; P=$2; F=$3
S=aUbU..aDbD
hist() { cat cert/obl23pair/cf-xfree/stages/$S/w*.txt | awk '{c[NF]++} END {for (n = 20; n <= 25; n++) if (c[n]) printf " %d:%d", n, c[n]; print ""}'; }
echo "== start aDbD dm$LEN from slice $F ($(date '+%m-%d %H:%M')):$(hist)"
for ((p = F; p < P; p++)); do
  [ -e certruns/deepmerge/dmloop.stop ] && { echo "== stopped before slice $p"; break; }
  until grep -q "distinct merged" certruns/deepmerge/dm$LEN-$p.err 2>/dev/null && [ -e dm$LEN-$p.log ]; do sleep 60; done
  ln -f dm$LEN-$p.log dm$LEN-$p.u.log || { echo "== slice $p: link failed"; continue; }
  CHUNK=5000000 CEARGS="--near 10 --band 1 --seenbits 29 --merge 6" devtools/pairfeed.sh uonly dm$LEN-$p.u.log $S || echo "== slice $p failed (stage restored)"
  rm -f dm$LEN-$p.u.log certruns/feed/dm$LEN-$p.u.seen
  echo "== slice $p done ($(date '+%m-%d %H:%M')):$(hist)"
done
echo "== end ($(date '+%m-%d %H:%M')):$(hist)"
