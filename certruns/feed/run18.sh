# run18 (2026-10-18): aDbD re-feeds with axis merges (G = 6) against run15's gains (549 -> 4 at 25)
cd ~/orbitsolve
export CEARGS="--near 12 --band 1 --worst --seenbits 29 --merge 6" CHUNK=20000000
U=aUbU..aDbD
devtools/pairfeed.sh ud 7pairud-a6.log $U && echo "== done 7pairud aDbD $(date +%H:%M)"
devtools/pairfeed.sh ud 7pairud-2-a6.log $U && echo "== done 7pairud-2 aDbD $(date +%H:%M)"
devtools/pairfeed.sh uonly 7pairup-2-a6.log $U && echo "== done 7pairup-2 aDbD $(date +%H:%M)"
echo "== end $(date +%H:%M)"
