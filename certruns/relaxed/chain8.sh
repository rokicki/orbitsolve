#!/bin/bash
# 2026-10-25: 2ud's 20s (309K after the ML 21s): every 20 through every action of <= 10 moves, then <= 11.
cd ~/orbitsolve
R=certruns/relaxed V=/var/tmp/orbitsolve
for C in 10 11; do
  CEARGS="--near 0 --band 0 --prop $C --proptop 20 --slices" $R/rfeed.sh 2ud ud aUbU..aDbD $V/7pair-2ud-to11.log --uonly > $R/r2ud-pt20c${C}s.log 2>&1
  python3 $R/rtable.py >> $R/r2ud-pt20c${C}s.log 2>&1
done
