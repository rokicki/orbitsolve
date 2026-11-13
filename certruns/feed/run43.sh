# run43 (2026-10-21): all of dm16.log (run41's sample gave 18,640 classes in 142 s). run41: first deep-merge test.  devtools/deepmerge --src 13 --overlap 5 --minlen 16
# --maxlen 16 on 7pairudb-2-full.log: 121,536,017 distinct 16-move joins of two 13s (6.5 GB, 29 min);
# a 1/24 sample (5,064,001 lines) fed to the b ring as products (no --worst).
cd ~/orbitsolve
B=aUaLaFaRaBaDbUbD..bLbFbRbB
hist() { cat cert/obl23pair/cf-xfree/stages/$B/w*.txt | awk '{c[NF]++} END {for (n in c) printf " %d:%d", n, c[n]; print ""}' | tr ' ' '\n' | sort -n | tr '\n' ' '; }
echo "== start ($(date +%H:%M)):$(hist)"
CHUNK=5000000 CEARGS="--near 10 --band 1 --seenbits 29 --merge 6" devtools/pairfeed.sh bonly dm16.log $B || echo "== failed (stage restored)"
echo "== end ($(date +%H:%M)):$(hist)"
