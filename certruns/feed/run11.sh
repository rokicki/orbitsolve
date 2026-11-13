#!/bin/bash
# 2026-10-18: the grown 7pairudb-2 (bonly: b ring, aDbD) and 7pairud-2 (ud: a ring, b ring, aDbD), new lines only
cd ~/orbitsolve
export CEARGS="--near 12 --band 1 --worst --seenbits 29" CHUNK=20000000
devtools/pairfeed.sh bonly 7pairudb-2.log aUaLaFaRaBaDbUbD..bLbFbRbB aUbU..aDbD && echo "== done 7pairudb-2 $(date +%H:%M)"
devtools/pairfeed.sh ud 7pairud-2.log aUaDbUbD..aLaFaRaB aUaLaFaRaBaDbUbD..bLbFbRbB aUbU..aDbD && echo "== done 7pairud-2 $(date +%H:%M)"
echo "== end $(date +%H:%M)"
