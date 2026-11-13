#!/bin/bash
# 2026-10-23: solve all 36,652 aUbU depth-19 classes under 1u (devtools/tws/aubu19u.tws), after the
# 10-class sample finishes.  Output: /var/tmp/orbitsolve/aubu19u-rest.out
cd ~/orbitsolve/devtools/tws
while pgrep -f "twsearch -M 7000 --nowrite aubu19u.tws" > /dev/null; do sleep 30; done
~/twsearch2/build/bin/twsearch -M 7000 --nowrite aubu19u.tws /var/tmp/orbitsolve/aubu19rest.scr > /var/tmp/orbitsolve/aubu19u-rest.out 2>&1
