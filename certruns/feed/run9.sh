#!/bin/bash
# re-feed (fresh seen sets via -r2 links) against the stages improved by run8:
# 7pairud-2 on the a ring (72,804 classes shortened in run8), 7pairudb-2 on the b ring and aDbD
cd ~/orbitsolve
export CEARGS="--near 12 --band 1 --worst --seenbits 29" CHUNK=20000000
devtools/pairfeed.sh ud 7pairud-2-r2.log aUaDbUbD..aLaFaRaB && echo "== done 7pairud-2 a ring $(date +%H:%M)"
devtools/pairfeed.sh bonly 7pairudb-2-r2.log aUaLaFaRaBaDbUbD..bLbFbRbB aUbU..aDbD && echo "== done 7pairudb-2 $(date +%H:%M)"
echo "== end $(date +%H:%M)"
