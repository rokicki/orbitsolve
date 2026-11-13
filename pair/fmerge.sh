declare -A T
T=(w4s6_ws0 8.7 w4s6_ws1 7.4 w4s6_ws2 7.4 w16s10_ws0 114 w16s10_ws1 109 w16s10_ws2 102 w32s10_ws0 244 w32s10_ws1 226 w32s10_ws2 220 w64s10_ws0 557 w64s10_ws1 531 w64s10_ws2 510 w128s10_ws1 1778 w128s10_ws2 1550 w192s10_ws2 3781 w256s10_ws1 9991 w256s10_ws2 11394)
for tag in ${(k)T}; do
  ms=${T[$tag]}
  r=$(python3 combine.py gsol_$tag.txt 1 16 --bitmap --cube 10 --mbeam 64)
  echo "10 $tag $ms $(echo "$r" | grep 'beam-merge  ' | awk '{print $2}') $(echo "$r" | grep 'time beam' | awk '{print $5}')"
  r=$(python3 combine.py gsol_$tag.txt 1 4 --bitmap --cube 20 --mbeam 16)
  echo "20 $tag $ms $(echo "$r" | grep 'beam-merge  ' | awk '{print $2}') $(echo "$r" | grep 'time beam' | awk '{print $5}')"
  r=$(python3 combine.py gsol_$tag.txt 1 3 --bitmap --cube 40)
  echo "40 $tag $ms $(echo "$r" | grep 'greedy+lat  ' | awk '{print $2}') $(echo "$r" | grep 'time greedy' | awk '{print $5}')"
done
