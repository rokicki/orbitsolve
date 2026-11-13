# stage 3ax generation in the reduced move set F,R,B,L,2U,3U,2D,3D (pair keeps U and D; every other
# oblique orbit kept): 8 threads, 48 GB pruning table (pergola).  Output: 7pair-3ax-2.log.
build/bin/twsearch --moves F,R,B,L,2U,3U,2D,3D -t 8 --nowrite -M 48192 -c 1000000000000000000 --scramblealg ""  --quiet 7pair-3ax.tws | grep -v Trying | build/bin/twsearch --quiet --distinguishall -U 7pair-3ax-uniq.tws > 7pair-3ax-2.log
