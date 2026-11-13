# mlloop.sh (2026-10-19): every 15 minutes, merge the ML agent's new b-ring lines
# (~/mlcube/bring30_live.txt, appended as found; complete lines only, tracked by byte offset in
# certruns/feed/mlloop.off) under the stage lock, certcheck, and refresh cert/bring-30.seq.txt.
# Stop with: touch certruns/feed/mlloop.stop
cd ~/orbitsolve
F=$HOME/mlcube/bring30_live.txt; O=certruns/feed/mlloop.off
B=aUaLaFaRaBaDbUbD..bLbFbRbB; ST=cert/obl23pair/cf-xfree/stages/$B; L=certruns/feed/locks/$B
T=${TMPDIR:-/tmp}/mlloop.$$; mkdir -p $T
while [ ! -e certruns/feed/mlloop.stop ]; do
  off=$(cat $O 2>/dev/null || echo 0)
  if [ -f $F ]; then
    sz=$(stat -f %z $F)
    if [ $sz -gt $off ]; then
      tail -c +$((off + 1)) $F | head -c $((sz - off)) > $T/new.txt
      k=$(python3 -c "import sys; d=open(sys.argv[1],'rb').read(); print(d.rfind(b'\n') + 1)" $T/new.txt)
      if [ $k -gt 0 ]; then
        python3 -c "import sys, os; os.truncate(sys.argv[1], int(sys.argv[2]))" $T/new.txt $k
        until mkdir $L 2>/dev/null; do sleep 10; done
        rm -rf $T/bak; cp -a $ST $T/bak
        devtools/certmerge $ST $T/new.txt > $T/merge.out 2>&1
        if grep -q "^replaced" $T/merge.out && devtools/certcheck $ST > $T/cc.out 2>&1 && grep -q "^PASS" $T/cc.out; then
          echo "$(date '+%m-%d %H:%M') ML: $(grep -c . $T/new.txt) lines; $(grep '^replaced' $T/merge.out)"
          echo $((off + k)) > $O
        else
          echo "$(date '+%m-%d %H:%M') ML merge failed; restored"; cat $T/merge.out | tail -5
          rm -rf $ST; cp -a $T/bak $ST
          echo $((off + k)) > $O
        fi
        rmdir $L
        cat $ST/w*.txt | awk 'NF == 30' > cert/bring-30.seq.txt.new && mv cert/bring-30.seq.txt.new cert/bring-30.seq.txt
        echo "  b ring at 30: $(wc -l < cert/bring-30.seq.txt) classes"
      fi
    fi
  fi
  for i in $(seq 90); do [ -e certruns/feed/mlloop.stop ] && break; sleep 10; done
done
rm -rf $T certruns/feed/mlloop.stop
