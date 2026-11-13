# run47: all of 7pairudb-2 (local full copy, then the tail from byte 936,984,406 pulled 10-22 10:14)
# to aDbD as products without --worst (K = 10); aDbD only ever had it with --worst or K = 2.
# Starts after the dm18 sample.
cd ~/orbitsolve
until grep -q "^== end\|failed" certruns/deepmerge/dm18sample.log; do sleep 60; done
S=aUbU..aDbD
for f in 7pairudb-2-full 7pairudb-2-t47; do
  ln -sf $f.log r47-$f.log
  CHUNK=5000000 CEARGS="--near 10 --band 1 --seenbits 29 --merge 6" devtools/pairfeed.sh uonly r47-$f.log $S || echo "== $f failed (stage restored)"
  rm -f r47-$f.log certruns/feed/r47-$f.seen
  echo "== $f done ($(date '+%m-%d %H:%M')):$(cat cert/obl23pair/cf-xfree/stages/$S/w*.txt | awk '{c[NF]++} END {for (n = 18; n <= 25; n++) if (c[n]) printf " %d:%d", n, c[n]}')"
done
rm -f 7pairudb-2-t47.log
echo "== end ($(date '+%m-%d %H:%M'))"
