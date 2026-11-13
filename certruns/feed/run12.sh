#!/bin/bash
# 2026-10-18: the grown 7pairup-2 (uonly: aDbD), new lines only; starts after run11
cd ~/orbitsolve
until grep -q "== end" certruns/feed/run11.log; do sleep 30; done
export CEARGS="--near 12 --band 1 --worst --seenbits 29" CHUNK=20000000
devtools/pairfeed.sh uonly 7pairup-2.log aUbU..aDbD && echo "== done 7pairup-2 $(date +%H:%M)"
echo "== end $(date +%H:%M)"
