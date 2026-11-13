#!/bin/bash
# re-feed the older logs against the certificate improved by 7pairup-2 (fresh seen sets via the -r2 links):
# 7pair on aDbD and the a ring, then 7pairud on aDbD
cd ~/orbitsolve
export CEARGS="--near 12 --band 1 --worst --seenbits 29"
devtools/pairfeed.sh pair 7pair-r2.log aUbU..aDbD aUaDbUbD..aLaFaRaB &&
devtools/pairfeed.sh ud 7pairud-r2.log aUbU..aDbD
