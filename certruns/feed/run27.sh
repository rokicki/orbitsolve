# run27 (2026-10-19): the full udb product on the b ring.  --worst runs never used seeds there
# (the band is smaller), so positions solved by single new 7pairudb-2 algorithms were never
# recorded.  Pass 1: no --worst, --near 2 (records A^-1(q) for q within 2 moves of solved, and
# propagates); pass 2: --worst --merge 6 band pass against the filled table.
cd ~/orbitsolve
B=aUaLaFaRaBaDbUbD..bLbFbRbB
ln -sf 7pairudb-2-full.log 7pairudb-2-p1.log; ln -sf 7pairudb-2-full.log 7pairudb-2-p2.log
CHUNK=5000000 CEARGS="--near 2 --band 1 --seenbits 29 --merge 6" devtools/pairfeed.sh bonly 7pairudb-2-p1.log $B && echo "== pass 1 $(date +%H:%M)"
CHUNK=5000000 CEARGS="--near 12 --band 1 --worst --seenbits 29 --merge 6" devtools/pairfeed.sh bonly 7pairudb-2-p2.log $B && echo "== pass 2 $(date +%H:%M)"
cat cert/obl23pair/cf-xfree/stages/$B/w*.txt | awk 'NF == 30' > cert/bring-30.seq.txt.new && mv cert/bring-30.seq.txt.new cert/bring-30.seq.txt
echo "== end: b ring at 30 $(wc -l < cert/bring-30.seq.txt) classes ($(date +%H:%M))"
