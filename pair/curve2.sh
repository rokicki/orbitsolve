for pools in "p6.txt single8.txt" "p7.txt single8.txt" "p8.txt c12x.txt"; do
  for w in 1 4; do echo "LB+EG [$pools] $(./psolve3 -x -w $w -l 3 -n 128 -E -M 14 -D 4 $pools 2>/dev/null)"; done
done
for w in 1 4 8 16; do echo "CB+EGk [p8] $(./psolve3 -x -b -w $w -n 64 -E -M 16 -D 8 p8.txt c12x.txt 2>/dev/null)"; done
