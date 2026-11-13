#!/bin/bash
# the older logs again (fresh seen sets) against the rings improved by run8/run9:
# 7pairudb on the b ring, 7pair on the b ring and a ring, 7pairud on the a ring
cd ~/orbitsolve
export CEARGS="--near 12 --band 1 --worst --seenbits 29" CHUNK=20000000
devtools/pairfeed.sh bonly 7pairudb-r2.log aUaLaFaRaBaDbUbD..bLbFbRbB && echo "== done 7pairudb $(date +%H:%M)"
devtools/pairfeed.sh pair 7pair-r3.log aUaLaFaRaBaDbUbD..bLbFbRbB aUaDbUbD..aLaFaRaB && echo "== done 7pair $(date +%H:%M)"
devtools/pairfeed.sh ud 7pairud-r3.log aUaDbUbD..aLaFaRaB && echo "== done 7pairud $(date +%H:%M)"
echo "== end $(date +%H:%M)"
