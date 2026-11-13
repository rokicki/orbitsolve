# run38 (2026-10-21): aDbD's 24s after every arena log was fed (1,271 classes): --deepband 6, then
# products --near 10 (no --worst), pool = the local 7pairudb-2 copy (any pool of short lines works).
cd ~/orbitsolve
S=aUbU..aDbD
count() { cat cert/obl23pair/cf-xfree/stages/$S/w*.txt | awk 'NF == 24' | wc -l; }
echo "== start: aDbD at 24 $(count) classes ($(date +%H:%M))"
for step in "db6:--near 2 --band 1 --seenbits 29 --merge 6 --deepband 6" "k10:--near 10 --band 1 --seenbits 29 --merge 6"; do
  n=${step%%:*}; x=${step#*:}
  ln -sf 7pairudb-2-full.log 7pairudb-2-r38$n.log
  CHUNK=5000000 CEARGS="$x" devtools/pairfeed.sh uonly 7pairudb-2-r38$n.log $S || echo "== $n failed (stage restored)"
  echo "== $n done: aDbD at 24 $(count) classes ($(date +%H:%M))"
done
echo "== end ($(date +%H:%M))"
