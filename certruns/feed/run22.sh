# run22 (2026-10-19): the 7pairud-2 tail not yet fed to the rings (bytes 7,772,597,158 on, 37.8M 14s)
cd ~/orbitsolve
export CHUNK=5000000
T=7pairud-2.from-7772597158.log
CEARGS="--near 12 --band 1 --worst --seenbits 29 --merge 6" devtools/pairfeed.sh ud $T aUaLaFaRaBaDbUbD..bLbFbRbB && echo "== done tail b $(date +%H:%M)"
ln -sf $T 7pairud-2.tail-a.log
CEARGS="--near 12 --band 1 --worst --seenbits 29 --merge 2" devtools/pairfeed.sh ud 7pairud-2.tail-a.log aUaDbUbD..aLaFaRaB && echo "== done tail a $(date +%H:%M)"
echo "== end $(date +%H:%M)"
