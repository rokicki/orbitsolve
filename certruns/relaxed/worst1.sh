#!/bin/bash
# 2026-10-23: composition passes at the worst length (certextend --worst: band = the worst classes),
# with the pools already pulled: 2ud (to 11), then 1ud (to 11).
cd ~/orbitsolve
CEARGS="--worst" certruns/relaxed/rfeed.sh 2ud ud aUbU..aDbD /var/tmp/orbitsolve/7pair-2ud-to11.log --uonly > certruns/relaxed/r2ud-11w.log 2>&1
CEARGS="--worst" certruns/relaxed/rfeed.sh 1ud ud none..aUbU /var/tmp/orbitsolve/7pair-1ud-to11.log > certruns/relaxed/r1ud-11w.log 2>&1
