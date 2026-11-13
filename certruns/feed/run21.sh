# run21 (2026-10-18): aDbD's last 4 classes at 25: two-level search (--miss 3: near misses become
# targets of a second pass over the whole log), axis merges G = 6, no chunks
cd ~/orbitsolve
export CEARGS="--near 12 --band 1 --worst --seenbits 29 --merge 6 --miss 3"
devtools/pairfeed.sh uonly 7pairup-2-miss.log aUbU..aDbD && echo "== done 7pairup-2 aDbD $(date +%H:%M)"
echo "== end $(date +%H:%M)"
