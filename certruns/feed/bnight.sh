# bnight.sh (2026-10-19 night): keep pushing the b ring's 29s until none are left.  Each cycle:
# extend the local 7pairudb-2 copy to what udbloop has fed, then whole-table products K = 10 and
# (the table keeps changing, so repeated passes find more), --deepband 6 first (cycle 1: 240 -> 196;
# K = 10 and 11 found nothing at 29 in cycles 1-2, K = 11 dropped) (deepband 8: 7.3 h for one 5M chunk, 714K near-miss positions,
# 0 candidates; deepband 10 would be larger still).  When a
# whole cycle gains nothing at 29, one long product pass at K = 13, then cycle again.  Stops at 0
# classes at 29, after two cycles in a row with no gain at 29 (leaving the CPU to the a ring), or
# with: touch certruns/feed/bnight.stop
cd ~/orbitsolve
B=aUaLaFaRaBaDbUbD..bLbFbRbB
count() { cat cert/obl23pair/cf-xfree/stages/$B/w*.txt | awk 'NF == 29' > cert/bring-29.seq.txt.new && mv cert/bring-29.seq.txt.new cert/bring-29.seq.txt; wc -l < cert/bring-29.seq.txt | tr -d ' '; }
pass() {  # name, certextend options
  [ -e certruns/feed/bnight.stop ] && return 1
  ln -sf 7pairudb-2-full.log 7pairudb-2-bn$1.log
  CHUNK=5000000 CEARGS="$2" devtools/pairfeed.sh bonly 7pairudb-2-bn$1.log $B || echo "== $1 failed (stage restored)"
  rm -f 7pairudb-2-bn$1.log /var/tmp/orbitsolve/seen/7pairudb-2-bn$1.seen
  echo "== $1 done: b ring at 29 $(count) classes ($(date '+%m-%d %H:%M'))"
}
cyc=0; dry=0
while [ ! -e certruns/feed/bnight.stop ]; do
  cyc=$((cyc + 1)); c0=$(count)
  echo "== cycle $cyc: b ring at 29 $c0 classes ($(date '+%m-%d %H:%M'))"
  [ $c0 -eq 0 ] && break
  P=$(awk '$1=="7pairudb-2.log"{print $2}' certruns/feed/processed.tsv); O=$(stat -f %z 7pairudb-2-full.log)
  if [ $P -gt $O ]; then
    ssh -c aes128-gcm@openssh.com -i $HOME/.ssh/id_gizmo -o BatchMode=yes 10.0.0.155 "tail -c +$((O + 1)) twsearch2/7pairudb-2.log | head -c $((P - O))" > 7pairudb-2-full.add &&
      [ $(stat -f %z 7pairudb-2-full.add) -eq $((P - O)) ] && cat 7pairudb-2-full.add >> 7pairudb-2-full.log
    rm -f 7pairudb-2-full.add; echo "# copy now $(stat -f %z 7pairudb-2-full.log) bytes"
  fi
  pass c${cyc}db6 "--near 2 --band 1 --seenbits 29 --merge 6 --deepband 6"
  pass c${cyc}k10 "--near 10 --band 1 --seenbits 29 --merge 6"
  c1=$(count)
  if [ $c1 -ge $c0 ]; then pass c${cyc}k13 "--near 13 --band 1 --seenbits 29 --merge 6"; fi
  if [ $(count) -ge $c0 ]; then dry=$((dry + 1)); else dry=0; fi
  [ $dry -ge 2 ] && { echo "== two cycles without a gain at 29; stopping"; break; }
done
echo "== end: b ring at 29 $(count) classes ($(date '+%m-%d %H:%M'))"
rm -f certruns/feed/bnight.stop
