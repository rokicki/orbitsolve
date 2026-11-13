#!/bin/bash
# refold.sh U1 U2 O4 A3 (2026-11-12): fetch arena outputs, fold.py + vline per stage, then fsfold.sh 1ud U1, 2ud U2,
#   fsfoldax.sh 4op O4, 3ax A3, ubtable.py.  Logs /var/tmp/orbitsolve/fsfold/<st>-<name>.log.  Scripts: ~/orbitsolve/arena (git).  Summary on stdout.
set -u
[ $# -eq 4 ] || { echo "usage: refold.sh 1UD_NAME 2UD_NAME 4OP_NAME 3AX_NAME"; exit 1; }
echo "# refold.sh $*"; U1=$1; U2=$2; O4=$3; A3=$4
A=$(cd "$(dirname "$0")" && pwd)   # this script's directory (~/orbitsolve/arena)
P=$(command -v pypy3) || { echo "pypy3 not on PATH"; exit 1; }; T=~/orbitsolve/devtools/tws
cd /var/tmp/orbitsolve/optfs || exit 1
$A/aget fastsolve-x86/runs/1ud-prod2.out 1ud/prod2.out && $A/aget fastsolve-x86/runs/2ud-prod4.out 2ud/prod4.out &&
  $A/aget fastsolve-x86/runs/4op-nr.out 4op/nr.out && $A/aget fastsolve-x86/runs/3ax-nr.out 3ax/nr.out || { echo "fetch failed"; exit 1; }
$P $A/fold.py --pairs 1ud/prod-pairs.txt 1ud/1ud17-solving.txt 1ud/all.txt 1ud/t13.out 1ud/prod.out 1ud/prod2.out > 1ud/prod-improved.txt
$P $A/fold.py --pairs 2ud/prod-pairs.txt 2ud/2ud18-solving.txt 2ud/all.txt 2ud/prod.out 2ud/prod2.out 2ud/prod3.out 2ud/prod4.out > 2ud/prod-improved.txt
$P $A/fold.py --pairs 4op/prod-pairs.txt 4op/4op22-solving.txt 4op/all.txt 4op/prod-norot.out 4op/nr.out > 4op/prod-improved.txt
$P $A/fold.py --pairs 3ax/prod-pairs.txt 3ax/3ax23-solving.txt 3ax/all.txt 3ax/nr.out > 3ax/prod-improved.txt
for v in "1ud-si 1ud" "2ud-si 2ud" "obl8op 4op" "obl8ax 3ax"; do set -- $v
  r=$($P $A/vline.py $T/$1.tws $2/prod-pairs.txt | tail -1); echo "$2 vline: $r"; case $r in PASS*) ;; *) echo "vline failed"; exit 1;; esac
done
mkdir -p /var/tmp/orbitsolve/fsfold; cd /var/tmp/orbitsolve/fsfold

$A/fsfold.sh 1ud $U1 > 1ud-$U1.log 2>&1; $A/fsfold.sh 2ud $U2 > 2ud-$U2.log 2>&1
# an existing 4op/3ax fold dir is reused (pass the last name when that stage has no new finds)
[ -d 4op-$O4 ] || $A/fsfoldax.sh 4op $O4 > 4op-$O4.log 2>&1; [ -d 3ax-$A3 ] || $A/fsfoldax.sh 3ax $A3 > 3ax-$A3.log 2>&1
grep -H "end lengths\|PASS\|FAIL" 1ud-$U1.log 2ud-$U2.log; tail -qn1 4op-$O4.log 3ax-$A3.log | cut -c1-200
python3 $A/ubtable.py $U1 $U2 $O4 $A3
