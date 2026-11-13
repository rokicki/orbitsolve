# minimum-time comparison vs brobdicube, powers of two: our width-1 beam (--fast)
for n in 8 16 32 64 128 256 512 1024 2048 4096 8192 16384 32768 65536; do
  r=$( { /usr/bin/time -p ./gullicube --fast -b 1 --seed 1 $n; } 2>&1 )
  echo "ours N=$n $(echo "$r" | grep -o 'SOLVED\|NOT SOLVED' | head -1) moves $(echo "$r" | grep TOTAL | awk '{print $2}') real $(echo "$r" | grep ^real | awk '{print $2}') user $(echo "$r" | grep ^user | awk '{print $2}') $(echo "$r" | grep 'retries' | tr -s ' ' | tr '\n' ' ')"
  r=$( cd ~/brob-private && { /usr/bin/time -p ./brobdicube -D algs3 $n; } 2>&1 )
  echo "brob N=$n moves $(echo "$r" | grep '^N ' | awk '{print $4}') real $(echo "$r" | grep ^real | awk '{print $2}') user $(echo "$r" | grep ^user | awk '{print $2}')"
done
