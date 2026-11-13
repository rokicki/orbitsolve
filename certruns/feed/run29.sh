#!/bin/bash
# run29: a ring push.  Round 1 records single 7pairud-2 algorithms against the whole a-ring table
# (no --worst), round 2 is the --worst merge pass; both stream arena's 7pairud-2.log in slices.
set -u
cd ~/orbitsolve
LOG=7pairud-2.log; TOTAL=12007884518; MAX=3000000000
R="ssh -c aes128-gcm@openssh.com -i $HOME/.ssh/id_gizmo -o BatchMode=yes 10.0.0.155"
ST=aUaDbUbD..aLaFaRaB
for round in 1 2; do
  case $round in
    1) CE="--near 2 --band 1 --seenbits 29 --merge 2" ;;
    2) CE="--near 12 --band 1 --worst --seenbits 29 --merge 4" ;;
  esac
  P=0
  while [ $P -lt $TOTAL ]; do
    T=7pairud-2.r$round.from-$P.log
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
