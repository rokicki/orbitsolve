# run37 (2026-10-20): feed all of arena's 7pairupu-2.log (1,410,987,175 bytes, never fed) to aDbD
# with pulltail (uonly, G 2), in 3 GB slices; waits for the aDbD lock like any feed.
cd ~/orbitsolve
until [ $(awk '$1=="7pairupu-2.log"{print $2}' certruns/feed/processed.tsv) -ge 1410987175 ]; do
  certruns/feed/pulltail.sh 7pairupu-2.log || { echo "== failed"; exit 1; }
done
echo "== end ($(date '+%m-%d %H:%M'))"
