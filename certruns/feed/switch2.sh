#!/bin/bash
# after the 7pair aUbU stage (seed mode) passes, stop run1 and redo the remaining stages band-only
cd ~/orbitsolve
until [ $(grep -c "^PASS" certruns/feed/run1.log) -ge 4 ] || grep -q restoring certruns/feed/run1.log; do sleep 20; done
grep -q restoring certruns/feed/run1.log && { echo "run1 restored a stage; not switching"; exit 1; }
until pgrep -f "certextend cert/obl23pair/cf-xfree/stages/aUbU..aDbD" >/dev/null; do sleep 2; done
pkill -f "certextend cert/obl23pair/cf-xfree/stages/aUbU..aDbD"
while pgrep -f "pairfeed.sh" >/dev/null; do sleep 2; done
echo "run1 stopped after 7pair aUbU"
export CEARGS="--near 0 --band 1"
devtools/pairfeed.sh pair 7pair.log aUbU..aDbD aUaDbUbD..aLaFaRaB aUaLaFaRaBaDbUbD..bLbFbRbB &&
devtools/pairfeed.sh ud 7pairud.log aUbU..aDbD aUaDbUbD..aLaFaRaB aUaLaFaRaBaDbUbD..bLbFbRbB
