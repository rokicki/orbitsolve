#!/bin/bash
# 7pair then 7pairud on aDbD, a ring, b ring: only the worst lengths (aUbU needs another method)
cd ~/orbitsolve
export CEARGS="--near 12 --band 1 --worst"
devtools/pairfeed.sh pair 7pair.log aUbU..aDbD aUaDbUbD..aLaFaRaB aUaLaFaRaBaDbUbD..bLbFbRbB &&
devtools/pairfeed.sh ud 7pairud.log aUbU..aDbD aUaDbUbD..aLaFaRaB aUaLaFaRaBaDbUbD..bLbFbRbB
