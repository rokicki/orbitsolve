# run45 (2026-10-22): aDbD products without --worst (K = 10, as run32 for the b ring) over
# 7pairup-2's lines <= 12, then its 13s.  The deep merges of 7pairudb-2 lines (dmu.sh, run44)
# shortened almost nothing on aDbD.
cd ~/orbitsolve
S=aUbU..aDbD
hist() { cat cert/obl23pair/cf-xfree/stages/$S/w*.txt | awk '{c[NF]++} END {for (n = 18; n <= 25; n++) if (c[n]) printf " %d:%d", n, c[n]; print ""}'; }
echo "== start ($(date '+%m-%d %H:%M')):$(hist)"
for f in 7pairup-2-head12.log 7pairup-2-13s.log; do
  ln -sf $f r45-$f
  CHUNK=5000000 CEARGS="--near 10 --band 1 --seenbits 29 --merge 6" devtools/pairfeed.sh uonly r45-$f $S || echo "== $f failed (stage restored)"
  rm -f r45-$f certruns/feed/r45-${f%.log}.seen
  echo "== $f done ($(date '+%m-%d %H:%M')):$(hist)"
done
echo "== end ($(date '+%m-%d %H:%M')):$(hist)"
