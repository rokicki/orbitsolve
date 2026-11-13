# run44 (2026-10-21): run43's 16-move deep merges (dm16.log, 121.5M) on aDbD (uonly, products K = 10).
cd ~/orbitsolve
S=aUbU..aDbD
hist() { cat cert/obl23pair/cf-xfree/stages/$S/w*.txt | awk '{c[NF]++} END {for (n = 20; n <= 25; n++) if (c[n]) printf " %d:%d", n, c[n]; print ""}'; }
echo "== start ($(date +%H:%M)):$(hist)"
ln -f dm16.log dm16u.log
CHUNK=5000000 CEARGS="--near 10 --band 1 --seenbits 29 --merge 6" devtools/pairfeed.sh uonly dm16u.log $S || echo "== failed (stage restored)"
rm -f dm16u.log dm16.log certruns/feed/dm16u.seen certruns/feed/dm16.seen
echo "== end ($(date +%H:%M)):$(hist)"
