# run23 (2026-10-19): pull the arena logs' new tails (sizes as of the start) and feed them
cd ~/orbitsolve
R="ssh -i $HOME/.ssh/id_gizmo -o BatchMode=yes 10.0.0.155"
until ! pgrep -f "certruns/feed/run22.sh" > /dev/null; do sleep 30; done
for LOG in 7pairudb-2.log 7pairud-2.log 7pairup-2.log; do
  END=$($R "stat -c %s twsearch2/$LOG")
  echo "== $LOG: up to $END"
  while [ $(awk -v l=$LOG '$1 == l {print $2}' certruns/feed/processed.tsv) -lt $((END - 1000)) ]; do
    certruns/feed/pulltail.sh $LOG || { echo "== failed"; exit 1; }
  done
done
echo "== end $(date +%H:%M)"
