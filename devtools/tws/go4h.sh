# stage 4h generation ("horseshoe": aL aR aB bL bR bB after U, D, F of the pair; every other
# oblique orbit kept): 20 threads, 400 GB pruning table (as go2ud.sh; lower -M if 2ud still runs).
build/bin/twsearch -t 20 --nowrite -M 400000 -c 1000000000000000000 --scramblealg ""  --quiet 7pair-4h.tws | build/bin/twsearch --quiet --distinguishall -U 7pair-4h-uniq.tws > 7pair-4h.log
