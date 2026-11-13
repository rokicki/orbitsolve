gen() { tag=$1; n=$2; shift 2; echo "[$tag] $(./fast -n $n -o gsol_$tag.txt "$@" 2>&1 | tail -1)"; }
for ws in 0 1 2; do gen w32s10_ws$ws 256 -w 32 -M 8 -ws $ws p8s10.txt; done
for ws in 1 2; do gen w128s10_ws$ws 128 -w 128 -M 8 -ws $ws p8s10.txt; done
gen w192s10_ws2 96 -w 192 -M 8 -ws 2 p8s10.txt
