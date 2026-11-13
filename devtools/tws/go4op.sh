# stage 4op generation (F vs B and L vs R in both pair orbits after U, D and the axis split; every other
# oblique orbit kept): 20 threads, 400 GB pruning table (as go4h.sh).
build/bin/twsearch -t 20 --nowrite -M 400000 -c 1000000000000000000 --scramblealg ""  --quiet 7pair-4op.tws | build/bin/twsearch --quiet --distinguishall -U 7pair-4op-uniq.tws > 7pair-4op.log
