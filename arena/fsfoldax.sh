#!/bin/bash
# fsfoldax.sh 3ax|4op NAME (2026-11-12; generalizes fsfold4op.sh): fold arena fastsolve results into the P 80 axis
#   table (3ax: ax-m3 with axbound --prop 9 --from 23; 4op: 4op-m1 with opbound --prop 10 --from 22).
#   Lines: optfs/<st>/prod-improved.txt (fold.py on rotation-free finds; vline PASS) and all their suffixes, kept
#   only if linecheck-valid (lfilter.py).  --rounds 0, then the table goes into a copy of the expanded stage
#   (axcertupdate.sh) and axcheck.  Out: fsfold/<st>-NAME/ (<st>-NAME.tab, cert/stages/<st>).  Log on stdout.
set -u
[ $# -eq 2 ] || { echo "usage: fsfoldax.sh 3ax|4op NAME"; exit 1; }
echo "# fsfoldax.sh $*"
A=$(cd "$(dirname "$0")" && pwd)
st=$1; D=$HOME/orbitsolve/devtools
case $st in
  3ax) ENG=axbound; BASE=/var/tmp/orbitsolve/opt3ax/merge/ax-m3.tab; PROP=9; FROM=23; T=obl8ax; MODE=ax ;;
  4op) ENG=opbound; BASE=/var/tmp/orbitsolve/opt4op/merge/4op-m1.tab; PROP=10; FROM=22; T=obl8op; MODE=op ;;
  *) echo "unknown stage $st"; exit 1 ;;
esac
R=/var/tmp/orbitsolve/fsfold/$st-$2; TW=$D/tws/$T.tws
mkdir -p $R
cut -d' ' -f3- /var/tmp/orbitsolve/optfs/$st/prod-improved.txt > $R/full.txt
grep -q "[xyz]" $R/full.txt && { echo "rotation in lines; stop"; exit 1; }
awk '{for(i=1;i<=NF;i++){s=$i; for(j=i+1;j<=NF;j++) s=s" "$j; print s}}' $R/full.txt | sort -u > $R/load.txt
python3 $A/lfilter.py $TW $MODE $R/load.txt > $R/good.txt 2> $R/lfilter.err
echo "lines $(wc -l < $R/full.txt), suffixes $(wc -l < $R/load.txt), valid $(wc -l < $R/good.txt)"
cd /var/tmp/orbitsolve/fold
$D/$ENG --diagok --prop $PROP --rounds 0 --threads 6 --base $BASE --load $R/good.txt --save $R/$st-$2.tab \
  --worst $R/$st-$2.worst --from $FROM pools/pair3.cls pools/p8s10.cls pools/p9.cls pools/eg8.cls pools/c12x.cls \
  pools/ud.cls pools/b1all.cls > $R/eng.out 2> $R/eng.err
grep "replayed" $R/eng.err; grep "classes by length" $R/eng.out
rm -rf $R/cert; mkdir -p $R/cert/stages; cp -a /var/tmp/orbitsolve/optfs/full/stages/$st $R/cert/stages/; chmod -R u+w $R/cert
$D/axcertupdate.sh $R/cert $st $R/$st-$2.tab > $R/axcert.out 2>&1
(cd $R/cert && $D/axcheck $st stages/$st | tail -1)
