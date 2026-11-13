# per-pair solver configs -> solution files -> merged per-pair cost at n = 10, 20, 40
run() {  # tag, n, fast args...
  tag=$1; n=$2; shift 2
  echo "[$tag] $(./fast -n $n -o gsol_$tag.txt "$@" 2>&1 | tail -1)"
  for c in 10 20 40; do
    T=8; [ $c = 40 ] && T=3
    echo "   cube $c: $(python3 combine.py gsol_$tag.txt 1 $T --bitmap --cube $c | grep -E 'greedy\+lat   ')"
  done
}
for ws in 0 1 2; do
  run w4s6_ws$ws 512 -w 4 -M 8 -ws $ws p8s6.txt
  run w16s10_ws$ws 256 -w 16 -M 8 -ws $ws p8s10.txt
  run w64s10_ws$ws 256 -w 64 -M 8 -ws $ws p8s10.txt
done
for ws in 1 2; do
  run w256s10_ws$ws 128 -w 256 -M 8 -ws $ws p8s10.txt
done
