# run17 (2026-10-18): b ring re-feed with axis-block merges (G = 6)
cd ~/orbitsolve
export CEARGS="--near 12 --band 1 --worst --seenbits 29 --merge 6" CHUNK=20000000
B=aUaLaFaRaBaDbUbD..bLbFbRbB
devtools/pairfeed.sh bonly 7pairudb-ax.log $B && echo "== done 7pairudb b $(date +%H:%M)"
devtools/pairfeed.sh bonly 7pairudb-2-ax.log $B && echo "== done 7pairudb-2 b $(date +%H:%M)"
echo "== end $(date +%H:%M)"
