for pc in "6 1.0" "6 0.75" "4 0.75" "8 0.75" "8 1.0"; do echo "pre[$pc] $(./psolve2 -x -b -w 8 -n 64 -p $pc p8.txt c12x.txt 2>/dev/null)"; done
