# ubloop.sh (2026-10-20): keep upperbound.md current.  Every 5 minutes, when the certificate's
# MANIFEST has changed (any stage merged), regenerate the length table and the Live status block
# (certruns/feed/ubstatus.py).  Stop with: touch certruns/feed/ubloop.stop
cd ~/orbitsolve
M=cert/obl23pair/cf-xfree/MANIFEST; seen=
while [ ! -e certruns/feed/ubloop.stop ]; do
  t=$(stat -f %m $M)
  if [ "$t" != "$seen" ]; then python3 certruns/feed/ubstatus.py && seen=$t; fi
  for i in $(seq 30); do [ -e certruns/feed/ubloop.stop ] && break; sleep 10; done
done
rm -f certruns/feed/ubloop.stop
