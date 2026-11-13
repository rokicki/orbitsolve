# run25 (2026-10-19): finish 7pairud-2 and 7pairup-2 (arena stopped them: 12,007,884,518 and
# 52,347,976,564 bytes)
cd ~/orbitsolve
for e in 7pairud-2.log:12007884518 7pairup-2.log:52347976564; do
  LOG=${e%:*}; END=${e#*:}
  while [ $(awk -v l=$LOG '$1 == l {print $2}' certruns/feed/processed.tsv) -lt $END ]; do
    certruns/feed/pulltail.sh $LOG || { echo "== failed"; exit 1; }
  done
  echo "== $LOG complete ($(date +%H:%M))"
done
echo "== end $(date +%H:%M)"
