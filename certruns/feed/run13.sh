#!/bin/bash
# 2026-10-18: certextend --merge 6 (slice-turn merges at the junction of A and line(q), counted exactly):
# every log again on the b ring (fresh seen sets via -m links), smallest first, then the a ring
cd ~/orbitsolve
export CEARGS="--near 12 --band 1 --worst --seenbits 29 --merge 6" CHUNK=20000000
B=aUaLaFaRaBaDbUbD..bLbFbRbB; A=aUaDbUbD..aLaFaRaB
devtools/pairfeed.sh bonly 7pairudb-m.log $B && echo "== done 7pairudb b $(date +%H:%M)"
devtools/pairfeed.sh bonly 7pairudb-2-m.log $B && echo "== done 7pairudb-2 b $(date +%H:%M)"
devtools/pairfeed.sh pair 7pair-m.log $B $A && echo "== done 7pair b a $(date +%H:%M)"
devtools/pairfeed.sh ud 7pairud-m.log $B $A && echo "== done 7pairud b a $(date +%H:%M)"
devtools/pairfeed.sh ud 7pairud-2-m.log $B $A && echo "== done 7pairud-2 b a $(date +%H:%M)"
echo "== end $(date +%H:%M)"
