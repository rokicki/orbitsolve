# original vs patched brobdicube, checked on the independent sticker simulator
SP=/private/tmp/claude-501/-Users-rokicki-orbitsolve/c9a56b76-0990-47fc-b388-530dd129aba9/scratchpad
chk() {  # binary N scramble...  -> prints ok / BAD
  bin=$1; n=$2; shift 2
  echo "$*" > $SP/s.txt
  (cd ~/brob-private && $bin -D algs3 -i -o -t 1 $n < $SP/s.txt > $SP/o.txt 2>&1)
  if ./brobcheck $n $SP/s.txt $SP/o.txt > /dev/null; then echo ok; else echo BAD; fi
}
for t in "8|2F U" "8|2F U'" "8|2F U2" "7|4U 4F" "7|4F 4U" "9|5U 5R" "7|4U R 2F 3L U' 4F"; do
  n=${t%%|*}; s=${t#*|}
  printf "%-3s %-22s original %-4s patched %s\n" $n "[$s]" $(chk ./brobdicube $n $s) $(chk $SP/brobfix/brobdicube $n $s)
done
o=0; p=0; tot=0
while read n s; do
  tot=$((tot+1))
  [ "$(chk ./brobdicube $n $s)" = ok ] && o=$((o+1))
  [ "$(chk $SP/brobfix/brobdicube $n $s)" = ok ] && p=$((p+1))
done < $SP/rand.txt
echo "random 60-move scrambles, N=7..12: original correct $o/$tot, patched correct $p/$tot"
