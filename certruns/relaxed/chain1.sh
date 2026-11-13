#!/bin/bash
# 2026-10-23: first distance tables for the relaxed stages, one at a time (certextend --near 0):
# 1u (arena lines to 10), 1ud (to 11), 2ud (to 10).
cd ~/orbitsolve
CEARGS="--near 0 --band 0 --prop 0" certruns/relaxed/rfeed.sh 1u u none..aUbU /var/tmp/orbitsolve/7pair-1u-to10.log > certruns/relaxed/r1u-10.log 2>&1
CEARGS="--near 0 --band 0 --prop 0" certruns/relaxed/rfeed.sh 1ud ud none..aUbU /var/tmp/orbitsolve/7pair-1ud-to11.log > certruns/relaxed/r1ud-11.log 2>&1
CEARGS="--near 0 --band 0 --prop 0" certruns/relaxed/rfeed.sh 2ud ud aUbU..aDbD /var/tmp/orbitsolve/7pair-2ud-to10.log --uonly > certruns/relaxed/r2ud-10.log 2>&1
