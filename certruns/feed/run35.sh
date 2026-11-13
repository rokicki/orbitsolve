# run35: run34 without the --miss step (nothing in 51 min on chunk 1). run34 (2026-10-19): the b ring's 29s by two-step searches on all of 7pairudb-2 fed so far
# (636,330,732 bytes): --miss 3 (band near misses q = A(p), then the pool again to shorten q),
# then --deepband 6 (each band position moved by every action of at most 6 moves).  No --worst
# (with --worst the seeds of length <= 2 are fewer than the band, so the band would be skipped).
cd ~/orbitsolve
B=aUaLaFaRaBaDbUbD..bLbFbRbB
count() { cat cert/obl23pair/cf-xfree/stages/$B/w*.txt | awk 'NF == 29' > cert/bring-29.seq.txt.new && mv cert/bring-29.seq.txt.new cert/bring-29.seq.txt; wc -l < cert/bring-29.seq.txt; }
echo "== start: b ring at 29 $(count) classes ($(date +%H:%M))"
for step in "db6:--deepband 6"; do
  n=${step%%:*}; x=${step#*:}
  ln -sf 7pairudb-2-full.log 7pairudb-2-r35$n.log
  CHUNK=5000000 CEARGS="--near 2 --band 1 --seenbits 29 --merge 6 $x" devtools/pairfeed.sh bonly 7pairudb-2-r35$n.log $B || echo "== $n failed (stage restored)"
  echo "== $n done: b ring at 29 $(count) classes ($(date +%H:%M))"
done
echo "== end ($(date +%H:%M))"
