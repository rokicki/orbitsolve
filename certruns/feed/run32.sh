# run32 (2026-10-19): run28 again for the b ring's 29s: whole-table pair products (every 7pairudb-2
# image A in front of every certificate line of at most K moves, no --worst), K = 10, 11, 12, each
# propagated, then a --worst merge pass.  Uses the lock-free certextend (v8).
cd ~/orbitsolve
B=aUaLaFaRaBaDbUbD..bLbFbRbB
count() { cat cert/obl23pair/cf-xfree/stages/$B/w*.txt | awk 'NF == 29' > cert/bring-29.seq.txt.new && mv cert/bring-29.seq.txt.new cert/bring-29.seq.txt; wc -l < cert/bring-29.seq.txt; }
echo "== start: b ring at 29 $(count) classes ($(date +%H:%M))"
for k in 10 11 12; do
  ln -sf 7pairudb-2-full.log 7pairudb-2-r32k$k.log
  CHUNK=5000000 CEARGS="--near $k --band 1 --seenbits 29 --merge 6" devtools/pairfeed.sh bonly 7pairudb-2-r32k$k.log $B || { echo "== failed K $k"; break; }
  echo "== K $k done: b ring at 29 $(count) classes ($(date +%H:%M))"
done
ln -sf 7pairudb-2-full.log 7pairudb-2-r32w.log
CHUNK=5000000 CEARGS="--near 12 --band 1 --worst --seenbits 29 --merge 6" devtools/pairfeed.sh bonly 7pairudb-2-r32w.log $B
echo "== end: b ring at 29 $(count) classes ($(date +%H:%M))"
