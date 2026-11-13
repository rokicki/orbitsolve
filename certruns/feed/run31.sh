# run31: re-run of run28's final --worst merge pass on the b ring (it aborted with the certextend heap crash).
cd ~/orbitsolve
B=aUaLaFaRaBaDbUbD..bLbFbRbB
rm -f certruns/feed/7pairudb-2-w2.seen 7pairudb-2-w2.log
ln -sf 7pairudb-2-full.log 7pairudb-2-w3.log
CHUNK=5000000 CEARGS="--near 12 --band 1 --worst --seenbits 29 --merge 6" devtools/pairfeed.sh bonly 7pairudb-2-w3.log $B
cat cert/obl23pair/cf-xfree/stages/$B/w*.txt | awk 'NF == 30' > cert/bring-30.seq.txt.new && mv cert/bring-30.seq.txt.new cert/bring-30.seq.txt
echo "== end: b ring at 30 $(wc -l < cert/bring-30.seq.txt) classes ($(date +%H:%M))"
