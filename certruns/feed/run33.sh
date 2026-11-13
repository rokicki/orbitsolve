# run33: products (--near 10) for the 7pairudb-2 bytes fed with --worst only after run32's copy (604603712..626945789).
cd ~/orbitsolve
A=604603712; Z=626945789; T=7pairudb-2.gap-$A.log
ssh -c aes128-gcm@openssh.com -i $HOME/.ssh/id_gizmo -o BatchMode=yes 10.0.0.155 "tail -c +$((A + 1)) twsearch2/7pairudb-2.log | head -c $((Z - A))" > $T || exit 1
[ $(stat -f %z $T) -eq $((Z - A)) ] && [ "$(tail -c 1 $T | od -An -tx1 | tr -d ' ')" = 0a ] || { echo "bad slice"; exit 1; }
CHUNK=5000000 CEARGS="--near 10 --band 1 --seenbits 29 --merge 6" devtools/pairfeed.sh bonly $T aUaLaFaRaBaDbUbD..bLbFbRbB
rm -f $T certruns/feed/$(basename $T .log).seen
echo "== end ($(date +%H:%M))"
