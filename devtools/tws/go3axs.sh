# stage 3ax generation in the reduced move set F R B L 2U 3U 2D 3D on the side-piece puzzle
# 7pair-3axs.tws (U/D pieces dropped; pair two colours, mids four): 8 threads, 48 GB pruning table
# (pergola).  Output: 7pair-3axs.log.
build/bin/twsearch -t 8 --nowrite -M 48192 -c 1000000000000000000 --scramblealg ""  --quiet 7pair-3axs.tws | grep -v Trying | build/bin/twsearch --quiet --distinguishall -U 7pair-3axs-uniq.tws > 7pair-3axs.log
