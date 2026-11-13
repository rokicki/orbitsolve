# run28 (2026-10-19): whole-table pair products on the b ring: A (every 7pairudb-2 image) + every
# certificate line of at most K moves (seeds, no --worst), K = 9, 10, 11, 12, each pass propagated;
# then a --worst merge pass.  Each pass re-reads the table, so later passes use earlier gains.
cd ~/orbitsolve
until ! pgrep -f "certruns/feed/run27.sh" > /dev/null; do sleep 30; done
B=aUaLaFaRaBaDbUbD..bLbFbRbB
for k in 9 10 11 12; do
  ln -sf 7pairudb-2-full.log 7pairudb-2-k$k.log
  CHUNK=5000000 CEARGS="--near $k --band 1 --seenbits 29 --merge 6" devtools/pairfeed.sh bonly 7pairudb-2-k$k.log $B || { echo "== failed K $k"; break; }
  cat cert/obl23pair/cf-xfree/stages/$B/w*.txt | awk 'NF == 30' > cert/bring-30.seq.txt.new && mv cert/bring-30.seq.txt.new cert/bring-30.seq.txt
  echo "== K $k done: b ring at 30 $(wc -l < cert/bring-30.seq.txt) classes ($(date +%H:%M))"
done
ln -sf 7pairudb-2-full.log 7pairudb-2-w2.log
CHUNK=5000000 CEARGS="--near 12 --band 1 --worst --seenbits 29 --merge 6" devtools/pairfeed.sh bonly 7pairudb-2-w2.log $B
cat cert/obl23pair/cf-xfree/stages/$B/w*.txt | awk 'NF == 30' > cert/bring-30.seq.txt.new && mv cert/bring-30.seq.txt.new cert/bring-30.seq.txt
echo "== end: b ring at 30 $(wc -l < cert/bring-30.seq.txt) classes ($(date +%H:%M))"
