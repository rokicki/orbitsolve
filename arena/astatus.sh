#!/bin/bash
# astatus.sh (2026-11-13): one look at arena: uptime/load, /dev/shm tables and mount, and per stage the current run
#   (last row of optfs/<st>/outs): alive?, searched / list length, shortenings found, elapsed, rough time left.
set -u
A=$(cd "$(dirname "$0")" && pwd); O=/var/tmp/orbitsolve/optfs
q=""
for st in 1ud 2ud 3ax 4op; do
  read -r loc rem len < <(tail -1 $O/$st/outs)
  [ "$rem" = "-" ] && { q="$q echo '$st: no current run (last: $loc, finished or stopped)';"; continue; }
  r=${loc%.out}
  q="$q f=fastsolve-x86/runs/$st-$r; p=\$(cat \$f.pid 2>/dev/null); s=dead; e=-; kill -0 \$p 2>/dev/null && { s=alive; e=\$(ps -o etimes= -p \$p | tr -d ' '); };
    awk -v st=$st -v r=$r -v len=$len -v s=\$s -v e=\$e '/^Solving/{cur=1; next} cur && /^Found/{n++; h++; cur=0} cur && /^No solution/{n++; cur=0}
      END{left = (s==\"alive\" && n>0) ? sprintf(\"%.1f h left\", e*(len-n)/n/3600) : \"\"; printf \"%s: run %s %s, %d / %d searched, %d shortened, %s\\n\", st, r, s, n, len, h, left}' \$f.out;"
done
$A/ar "uptime; mount | grep ' /dev/shm ' | sed 's/.*(\(.*\))/shm mount: \1/'; df -h /dev/shm | tail -1; ls -la /dev/shm 2>/dev/null | grep -- '-[0-9]*$' | tr '\n' ' '; echo; $q"
