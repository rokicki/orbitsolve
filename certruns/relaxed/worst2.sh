#!/bin/bash
# after worst1.sh: repeat --worst on 2ud (now at 23) until a pass gains nothing at the worst length
cd ~/orbitsolve
while pgrep -f worst1.sh > /dev/null; do sleep 30; done
for i in 1 2 3 4 5 6; do
  CEARGS="--worst" certruns/relaxed/rfeed.sh 2ud ud aUbU..aDbD /var/tmp/orbitsolve/7pair-2ud-to11.log --uonly > certruns/relaxed/r2ud-11w$i.log 2>&1
  grep -q "^ *0 classes shortened" certruns/relaxed/r2ud-11w$i.log && break
done
