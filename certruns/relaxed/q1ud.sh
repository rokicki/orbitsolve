#!/bin/bash
# after the 1u feed (r1u.sh) ends: 1ud through length 11
cd ~/orbitsolve
while pgrep -f "relaxed/r1u.sh" > /dev/null; do sleep 30; done
certruns/relaxed/rfeed.sh 1ud ud none..aUbU /var/tmp/orbitsolve/7pair-1ud-to11.log
