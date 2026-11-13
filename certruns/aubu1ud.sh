#!/bin/bash
# 2026-10-23: solve all 36,652 aUbU depth-19 classes under 1ud (devtools/tws/aubu19ud.tws), 30 GB
# table (user-approved for this run).  Output: /var/tmp/orbitsolve/aubu19ud.out
cd ~/orbitsolve/devtools/tws
~/twsearch2/build/bin/twsearch -M 30000 --nowrite aubu19ud.tws aubu19all.scr > /var/tmp/orbitsolve/aubu19ud.out 2>&1
