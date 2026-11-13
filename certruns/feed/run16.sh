# run16 (2026-10-18): b ring re-feed with deeper last-move merges (G = 8) against run13's gains
cd ~/orbitsolve
export CEARGS="--near 12 --band 1 --worst --seenbits 29 --merge 8" CHUNK=20000000
B=aUaLaFaRaBaDbUbD..bLbFbRbB
devtools/pairfeed.sh bonly 7pairudb-m8.log $B && echo "== done 7pairudb b $(date +%H:%M)"
devtools/pairfeed.sh bonly 7pairudb-2-m8.log $B && echo "== done 7pairudb-2 b $(date +%H:%M)"
echo "== end $(date +%H:%M)"
