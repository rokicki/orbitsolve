# run15 (2026-10-18): aDbD re-feed of 7pairup-2 with last-move merges, G = 6 (test: 5M 13s took 25 from 4,348 to 2,724 positions)
cd ~/orbitsolve
export CEARGS="--near 12 --band 1 --worst --seenbits 29 --merge 6" CHUNK=20000000
devtools/pairfeed.sh uonly 7pairup-2-m6.log aUbU..aDbD && echo "== done 7pairup-2 aDbD $(date +%H:%M)"
echo "== end $(date +%H:%M)"
