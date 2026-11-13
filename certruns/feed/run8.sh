#!/bin/bash
# 2026-10-17: the new arena logs, smallest first, worst lengths only, merged and checked per chunk:
# 7pairudb-2 (bonly: aDbD, b ring), 7pairud-2 (ud: aDbD, a ring, b ring), the grown 7pairup-2 (uonly: aDbD)
cd ~/orbitsolve
export CEARGS="--near 12 --band 1 --worst --seenbits 29" CHUNK=20000000
devtools/pairfeed.sh bonly 7pairudb-2.log aUbU..aDbD aUaLaFaRaBaDbUbD..bLbFbRbB && echo "== done 7pairudb-2 $(date +%H:%M)" &&
devtools/pairfeed.sh ud 7pairud-2.log aUbU..aDbD aUaDbUbD..aLaFaRaB aUaLaFaRaBaDbUbD..bLbFbRbB && echo "== done 7pairud-2 $(date +%H:%M)" &&
devtools/pairfeed.sh uonly 7pairup-2.log aUbU..aDbD && echo "== done 7pairup-2 $(date +%H:%M)"
echo "== end $(date +%H:%M)"
