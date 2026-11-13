P8="p8.txt c12x.txt"
for u in 0 0.5 1 2; do ./psolve -x -b -w 8 -u $u -n 64 $P8; done
./psolve -x -b -w 32 -n 64 $P8
./psolve -x -b -w 32 -n 64 $P8 -F p9.txt -F c12x.txt
./psolve -x -b -w 16 -n 32 p9.txt c12x.txt -F p9.txt -F c12x.txt
