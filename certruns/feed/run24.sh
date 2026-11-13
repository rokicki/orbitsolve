# run24 (2026-10-19): aDbD's last 4 at 25: --deepband 4 (y = b(p) for the 99 actions of at most
# 4 moves, 1,576 targets) with merges, over every 7pairup-2 line of at most 13 moves (the log's first 12.08 GB, read with dd; its last block also holds a few 14s), 20M-line slices
cd ~/orbitsolve
SL=certruns/feed/up13slices; mkdir -p $SL
ssh -c aes128-gcm@openssh.com -i $HOME/.ssh/id_gizmo -o BatchMode=yes 10.0.0.155 "dd if=twsearch2/7pairup-2.log bs=1M count=11519 2>/dev/null" | python3 devtools/lsplit.py 20000000 $SL/up13.
echo "slices: $(ls $SL | wc -l) ($(date +%H:%M))"
for f in $SL/up13.*; do
  CEARGS="--near 12 --band 1 --worst --seenbits 29 --merge 6 --deepband 4" devtools/pairfeed.sh uonly $f aUbU..aDbD || { echo "== failed $f"; exit 1; }
  rm -f $f certruns/feed/$(basename $f).seen
  echo "== done $f $(date +%H:%M)"
done
rmdir $SL
echo "== end $(date +%H:%M)"
