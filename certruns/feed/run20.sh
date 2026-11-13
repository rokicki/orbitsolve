# run20 (2026-10-18): a ring re-feed with last-move merges, G = 4 (axis merges)
cd ~/orbitsolve
export CEARGS="--near 12 --band 1 --worst --seenbits 29 --merge 4" CHUNK=10000000
A=aUaDbUbD..aLaFaRaB
devtools/pairfeed.sh ud 7pairud-m4.log $A && echo "== done 7pairud a $(date +%H:%M)"
devtools/pairfeed.sh ud 7pairud-2-m4.log $A && echo "== done 7pairud-2 a $(date +%H:%M)"
echo "== end $(date +%H:%M)"
