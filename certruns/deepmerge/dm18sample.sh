# dm18sample.sh: one slice (1/64) of the 18-move deep merges, fed to the b ring, to measure yield.
cd ~/orbitsolve
B=aUaLaFaRaBaDbUbD..bLbFbRbB
tail29() { cat cert/obl23pair/cf-xfree/stages/$B/w*.txt | awk '{c[NF]++} END {for (n = 25; n <= 30; n++) if (c[n]) printf " %d:%d", n, c[n]; print ""}'; }
echo "== start dm18 sample ($(date '+%m-%d %H:%M')):$(tail29)"
devtools/deepmerge --src 14 --overlap 5 --minlen 18 --maxlen 18 --part 64:0 7pairudb-2-full.log > dm18-0.log 2> certruns/deepmerge/dm18-0.err
echo "== gen done ($(date '+%m-%d %H:%M')): $(grep -c . dm18-0.log) lines"
CHUNK=5000000 CEARGS="--near 10 --band 1 --seenbits 29 --merge 6" devtools/pairfeed.sh bonly dm18-0.log $B || echo "== failed (stage restored)"
rm -f dm18-0.log certruns/feed/dm18-0.seen
echo "== end ($(date '+%m-%d %H:%M')):$(tail29)"
