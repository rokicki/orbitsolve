#!/bin/bash
# 7pairup-2 on aDbD, worst length only
cd ~/orbitsolve
export CEARGS="--near 12 --band 1 --worst --seenbits 29"
devtools/pairfeed.sh uonly 7pairup-2.log aUbU..aDbD
