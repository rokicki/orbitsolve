# run48 (2026-10-22): aDbD's 24s (272 classes after run45/run47) with --deepband 6 (run38 tried it
# at 1,271 classes: too slow).  Pool = the local 7pairudb-2 copy.
cd ~/orbitsolve
S=aUbU..aDbD
count() { cat cert/obl23pair/cf-xfree/stages/$S/w*.txt | awk 'NF == 24' | wc -l; }
echo "== start: aDbD at 24 $(count) classes ($(date +%H:%M))"
ln -sf /var/tmp/orbitsolve/7pairudb-2-full.log /var/tmp/orbitsolve/7pairudb-2-r48.log
CHUNK=5000000 CEARGS="--near 2 --band 1 --seenbits 29 --merge 6 --deepband 6" devtools/pairfeed.sh uonly /var/tmp/orbitsolve/7pairudb-2-r48.log $S || echo "== failed (stage restored)"
rm -f /var/tmp/orbitsolve/7pairudb-2-r48.log /var/tmp/orbitsolve/seen/7pairudb-2-r48.seen
echo "== end: aDbD at 24 $(count) classes ($(date +%H:%M))"
