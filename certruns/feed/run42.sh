# run42 (2026-10-21): run40 on 7pairup-2's 13s (231,489,034 lines). run40: singles only (--near 0 --band 0: each image A of a line gives A^-1(solved)
# the line A) for 7pairup-2's lines of length <= 12, on aDbD.  Every earlier 7pairup-2 feed used
# --worst, which skips the seeds (and so the single) of an image whenever the classes within
# W - 1 - |A| outnumber the band; since P93, 92 classes went 12/13 -> 11 and 1,235 13 -> 12.
cd ~/orbitsolve
S=aUbU..aDbD
hist() { cat cert/obl23pair/cf-xfree/stages/$S/w*.txt | awk '{c[NF]++} END {for (n in c) printf " %d:%d", n, c[n]; print ""}' | tr ' ' '\n' | sort -n | tr '\n' ' '; }
echo "== start ($(date +%H:%M)):$(hist)"
ln -sf 7pairup-2-13s.log 7pairup-2-r42s.log
CHUNK=5000000 CEARGS="--near 0 --band 0 --seenbits 29" devtools/pairfeed.sh uonly 7pairup-2-r42s.log $S || echo "== failed (stage restored)"
echo "== end ($(date +%H:%M)):$(hist)"
