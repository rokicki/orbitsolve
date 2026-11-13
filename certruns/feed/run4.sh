#!/bin/bash
# redo 7pairup-2 on aDbD (its merge was undone by a stale restore), then 7pairud on aDbD, a ring, b ring
cd ~/orbitsolve
export CEARGS="--near 12 --band 1 --worst --seenbits 29"
devtools/pairfeed.sh uonly 7pairup-2.log aUbU..aDbD &&
devtools/pairfeed.sh ud 7pairud.log aUbU..aDbD aUaDbUbD..aLaFaRaB aUaLaFaRaBaDbUbD..bLbFbRbB
