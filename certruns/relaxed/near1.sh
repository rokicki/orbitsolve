#!/bin/bash
# 2026-10-23: after propall1.sh: every arena line (to 11) as a first block in front of every
# certificate line of at most K moves (certextend --near K), then propagation through face turns.
cd ~/orbitsolve
while pgrep -f propall1.sh > /dev/null; do sleep 30; done
CEARGS="--near 4 --band 0 --prop 0" certruns/relaxed/rfeed.sh 1u u none..aUbU /var/tmp/orbitsolve/7pair-1u-to11.log > certruns/relaxed/r1u-11n4.log 2>&1
CEARGS="--near 4 --band 0 --prop 0" certruns/relaxed/rfeed.sh 1ud ud none..aUbU /var/tmp/orbitsolve/7pair-1ud-to11.log > certruns/relaxed/r1ud-11n4.log 2>&1
CEARGS="--near 7 --band 0 --prop 0" certruns/relaxed/rfeed.sh 2ud ud aUbU..aDbD /var/tmp/orbitsolve/7pair-2ud-to11.log --uonly > certruns/relaxed/r2ud-11n7.log 2>&1
