# level beam (fast end)
for pools in "p6.txt single8.txt" "p7.txt single8.txt" "p8.txt c12x.txt"; do
  for w in 1 2 4; do echo "LB [$pools] $(./psolve2 -x -w $w -l 3 -n 128 $pools 2>/dev/null)"; done
done
# move-count beam
for w in 1 2 4 8; do echo "CB [p8] $(./psolve2 -x -b -w $w -n 64 p8.txt c12x.txt 2>/dev/null)"; done
# + endgame
for w in 1 2 4 8 16; do echo "CB+EG [p8] $(./psolve2 -x -b -w $w -n 64 -E -M 14 p8.txt c12x.txt 2>/dev/null)"; done
for w in 8 16; do echo "CB+EG2 [p8] $(./psolve2 -x -b -w $w -n 32 -E -M 14 -D 3 p8.txt c12x.txt 2>/dev/null)"; done
