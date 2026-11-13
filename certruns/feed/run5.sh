#!/bin/bash
# the grown 7pairup-2.log (8.5 GB) on aDbD, worst length only, 20M lines per merged chunk
cd ~/orbitsolve
export CEARGS="--near 12 --band 1 --worst" CHUNK=20000000
devtools/pairfeed.sh uonly 7pairup-2.log aUbU..aDbD
