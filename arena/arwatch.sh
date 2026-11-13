#!/bin/bash
# arwatch.sh: exits (with a report) when arena runs fewer than N solver processes (default 1: idle) or its
# 1-minute load is under 4.  ssh failures (comcast drops) count as "still busy"; polls every 60 s.
N=${1:-1}
A=$(dirname "$0")/ar
while true; do
  r=$($A 'echo $(pgrep -x "solve[0-9]*" | wc -l) $(cut -d" " -f1 /proc/loadavg)' </dev/null 2>/dev/null)
  if [ -n "$r" ]; then
    set -- $r; n=$1; l=$2
    if [ "$n" -lt "$N" ] || [ "${l%.*}" -lt 4 ]; then echo "$(date '+%H:%M') arena: $n solver processes, load $l"; exit 0; fi
  fi
  sleep 60
done
