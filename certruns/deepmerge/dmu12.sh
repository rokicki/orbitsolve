# dmu12.sh (2026-10-22): yield test of aDbD merges from its own log: 7pairup-2 lines <= 12 joined
# with the back index of lines <= 11 at overlap 4 (14 and 15 moves; the log is complete through 13),
# a 1/1000 slice, fed to aDbD as products (K = 10, no --worst).
cd ~/orbitsolve
V=/var/tmp/orbitsolve; S=aUbU..aDbD
tail24() { cat cert/obl23pair/cf-xfree/stages/$S/w*.txt | awk '{c[NF]++} END {for (n = 20; n <= 24; n++) if (c[n]) printf " %d:%d", n, c[n]; print ""}'; }
echo "== start ($(date '+%m-%d %H:%M')):$(tail24)"
devtools/deepmerge --src 12 --back 11 --overlap 4 --minlen 14 --part 1000:0 $V/7pairup-2-head12.log > $V/dmu12-0.log 2> certruns/deepmerge/dmu12-0.err
echo "== gen done ($(date '+%m-%d %H:%M')): $(tail -1 certruns/deepmerge/dmu12-0.err)"
CHUNK=5000000 CEARGS="--near 10 --band 1 --seenbits 29 --merge 6" devtools/pairfeed.sh uonly $V/dmu12-0.log $S || echo "== failed (stage restored)"
rm -f $V/dmu12-0.log $V/seen/dmu12-0.seen
echo "== end ($(date '+%m-%d %H:%M')):$(tail24)"
