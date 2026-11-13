# run39 (2026-10-21): re-feed 7pairup-2's lines of length <= 12 (first 34,671,560 lines, pulled with
# head -n) to aDbD with run37's arguments.  7pairupu-2 (the same short lines, fed 10-20) took aDbD's
# positions at 24 from 29,156 to 14,940; 7pairup-2's short lines were last fed in run15, against a
# much worse table.  Tests whether re-feeding alone gives the gain.
cd ~/orbitsolve
S=aUbU..aDbD
count() { cat cert/obl23pair/cf-xfree/stages/$S/w*.txt | awk 'NF == 24' | wc -l; }
echo "== start: aDbD at 24 $(count) classes ($(date +%H:%M))"
CHUNK=5000000 CEARGS="--near 12 --band 1 --worst --seenbits 29 --merge 2" devtools/pairfeed.sh uonly 7pairup-2-head12.log $S || echo "== failed (stage restored)"
echo "== end: aDbD at 24 $(count) classes ($(date +%H:%M))"
