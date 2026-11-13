#!/bin/bash
# astop.sh STAGE (2026-11-13): stop STAGE's running fastsolve on arena: kill the pid in each live runs/<st>-*.pid
#   (exact pid only; never pkill -f), wait for it to exit.  The output so far stays; alaunch.sh / refold.sh use
#   the results it finished.
set -u
A=$(cd "$(dirname "$0")" && pwd)
echo "# astop.sh $*"
[ $# -eq 1 ] || { echo "usage: astop.sh 1ud|2ud|3ax|4op"; exit 1; }
case $1 in 1ud|2ud|3ax|4op) ;; *) echo "unknown stage $1"; exit 1 ;; esac
$A/ar "cd fastsolve-x86; n=0; for f in runs/$1-*.pid; do [ -f \$f ] || continue; p=\$(cat \$f)
  if kill -0 \$p 2>/dev/null; then echo \"\$f: \$(ps -o args= -p \$p | cut -c1-80)\"; kill \$p; n=\$((n+1))
    for i in 1 2 3 4 5 6 7 8 9 10; do kill -0 \$p 2>/dev/null || break; sleep 3; done
    kill -0 \$p 2>/dev/null && echo \"  pid \$p still alive\" || echo \"  pid \$p stopped\"; fi; done
  [ \$n -eq 0 ] && echo 'no live $1 run'; true"
