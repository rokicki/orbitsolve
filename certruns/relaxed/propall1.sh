#!/bin/bash
# 2026-10-23: shortest-path pass over each relaxed stage (certextend --propall): every class is
# expanded cheapest first through the face turns and every allowed image of the arena lines and
# base pools of at most 6 moves.  Runs after worst2.sh.
cd ~/orbitsolve
while pgrep -f "worst1.sh|worst2.sh" > /dev/null; do sleep 30; done
CEARGS="--near 0 --band 0 --prop 6 --propall" certruns/relaxed/rfeed.sh 1u u none..aUbU /var/tmp/orbitsolve/7pair-1u-to10.log > certruns/relaxed/r1u-pa6.log 2>&1
CEARGS="--near 0 --band 0 --prop 6 --propall" certruns/relaxed/rfeed.sh 1ud ud none..aUbU /var/tmp/orbitsolve/7pair-1ud-to11.log > certruns/relaxed/r1ud-pa6.log 2>&1
CEARGS="--near 0 --band 0 --prop 6 --propall" certruns/relaxed/rfeed.sh 2ud ud aUbU..aDbD /var/tmp/orbitsolve/7pair-2ud-to11.log --uonly > certruns/relaxed/r2ud-pa6.log 2>&1
