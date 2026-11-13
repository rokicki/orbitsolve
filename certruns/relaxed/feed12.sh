#!/bin/bash
# 2026-10-23: the arena logs past the <= 11 files (2ud: all of 12 and some 13; 1ud, 1u: 12 so far),
# each line directly and in front of every certificate line of <= 6 moves (--near 6).
cd ~/orbitsolve
export CEARGS="--near 6 --band 0 --prop 0"
certruns/relaxed/rtail.sh 2ud ud aUbU..aDbD 7pair-2ud.log --uonly > certruns/relaxed/r2ud-12.log 2>&1
certruns/relaxed/rtail.sh 1ud ud none..aUbU 7pair-1ud.log > certruns/relaxed/r1ud-12.log 2>&1
certruns/relaxed/rtail.sh 1u u none..aUbU 7pair-1u.log > certruns/relaxed/r1u-12.log 2>&1
