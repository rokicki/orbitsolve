#!/bin/bash
# run30: a ring products: every 7pairud-2 image in front of every a-ring line of at most K moves
# (--band 0: no band, the band of 14K classes at 21 made each 5M chunk take 29 min), propagated.
set -u
cd ~/orbitsolve
LOG=7pairud-2.log; TOTAL=12007884518; MAX=3000000000
R="ssh -c aes128-gcm@openssh.com -i $HOME/.ssh/id_gizmo -o BatchMode=yes 10.0.0.155"
ST=aUaDbUbD..aLaFaRaB; echo "# rounds=1"
for round in 1; do
  case $round in
    1) CE="--near 10 --band 0 --seenbits 29 --merge 6" ;;
  esac
  P=0
  while [ $P -lt $TOTAL ]; do
    T=7pairud-2.k10.from-$P.log
    $R "tail -c +$((P + 1)) twsearch2/$LOG | head -c $((TOTAL - P < MAX ? TOTAL - P : MAX))" > $T || exit 1
    K=$(python3 -c "import sys; d=open(sys.argv[1],'rb').read(); print(d.rfind(b'\n') + 1)" $T)
    [ $K -gt 0 ] || exit 1
    python3 -c "import sys, os; os.truncate(sys.argv[1], int(sys.argv[2]))" $T $K
    echo "# round $round slice bytes $P $((P + K)) of $TOTAL ($(date +%H:%M))"
    CHUNK=5000000 CEARGS="$CE" devtools/pairfeed.sh ud $T $ST || exit 1
    rm -f $T certruns/feed/$(basename $T .log).seen
    P=$((P + K))
    cat cert/obl23pair/cf-xfree/stages/$ST/w*.txt | awk 'NF == 21' > cert/aring-21.seq.txt.new && mv cert/aring-21.seq.txt.new cert/aring-21.seq.txt
    echo "== slice done: a ring at 21 $(wc -l < cert/aring-21.seq.txt) classes ($(date +%H:%M))"
  done
  echo "== round $round done: a ring at 21 $(wc -l < cert/aring-21.seq.txt) classes ($(date +%H:%M))"
done
echo "== end ($(date +%H:%M))"
