# run26 (2026-10-19): the b ring's worst classes by --deepband (each worst position moved by every
# action of at most C1 moves, then an image plus a line) over the whole of 7pairudb-2.log, C1 = 4, 6, 8
cd ~/orbitsolve
B=aUaLaFaRaBaDbUbD..bLbFbRbB
for c in 4 6 8; do
  ln -sf 7pairudb-2-full.log 7pairudb-2-db$c.log
  CHUNK=5000000 CEARGS="--near 12 --band 1 --worst --seenbits 29 --merge 6 --deepband $c" devtools/pairfeed.sh bonly 7pairudb-2-db$c.log $B || echo "== failed deepband $c"
  cat cert/obl23pair/cf-xfree/stages/$B/w*.txt | awk 'NF == 30' > cert/bring-30.seq.txt.new && mv cert/bring-30.seq.txt.new cert/bring-30.seq.txt
  echo "== deepband $c done: b ring at 30 $(wc -l < cert/bring-30.seq.txt) classes ($(date +%H:%M))"
done
echo "== end $(date +%H:%M)"
