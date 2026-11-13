#!/bin/bash
# boundary certificates (written to scratch, checked independently): a ring + b ring <= 47, aUbU + aDbD <= 42
cd ~/orbitsolve
C=cert/obl23pair/cf-xfree/stages
D=dist/gullicube/data
BASE="--base $D/pair3.cls --base $D/p8s10.cls --base $D/p9.cls --base $D/eg8.cls --base $D/c12x.cls"
O=/private/tmp/claude-501/-Users-rokicki-orbitsolve/c9a56b76-0990-47fc-b388-530dd129aba9/scratchpad/bcert
mkdir -p $O
date '+start %H:%M'
devtools/boundx $C/aUaDbUbD..aLaFaRaB $C/aUaLaFaRaBaDbUbD..bLbFbRbB 47 --deep 1 --slack 5 --out $O/ring47.txt $BASE 2>&1 | grep -v "^class" | tail -3
date '+%H:%M'
devtools/boundx $C/none..aUbU $C/aUbU..aDbD 42 --deep 1 --slack 5 --out $O/ud42.txt $BASE 2>&1 | grep -v "^class" | tail -3
date '+%H:%M'
devtools/boundcheck $C/none..aUbU $C/aUbU..aDbD $O/ud42.txt 42 | tail -3
date '+%H:%M'
devtools/boundcheck $C/aUaDbUbD..aLaFaRaB $C/aUaLaFaRaBaDbUbD..bLbFbRbB $O/ring47.txt 47 | tail -3
date '+done %H:%M'
