# udbloop.sh (2026-10-19): every hour, pull 7pairudb-2.log's new tail from arena and feed it to the
# b ring (pulltail.sh) as whole-table products (every new image in front of every line of at most
# 10 moves, plus the band at the worst length; since 20:25, was --worst only), then refresh
# cert/bring-WORST.seq.txt.  Stop with: touch certruns/feed/udbloop.stop
cd ~/orbitsolve
B=cert/obl23pair/cf-xfree/stages/aUaLaFaRaBaDbUbD..bLbFbRbB
while [ ! -e certruns/feed/udbloop.stop ]; do
  PT_CEARGS="--near 10 --band 1 --seenbits 29 --merge 6" certruns/feed/pulltail.sh 7pairudb-2.log
  W=$(cat $B/w*.txt | awk '{ if (NF > w) w = NF } END { print w }')
  cat $B/w*.txt | awk -v w=$W 'NF == w' > cert/bring-$W.seq.txt.new && mv cert/bring-$W.seq.txt.new cert/bring-$W.seq.txt
  echo "== b ring at $W: $(wc -l < cert/bring-$W.seq.txt) classes ($(date '+%m-%d %H:%M'))"
  for i in $(seq 360); do [ -e certruns/feed/udbloop.stop ] && break; sleep 10; done
done
rm -f certruns/feed/udbloop.stop
