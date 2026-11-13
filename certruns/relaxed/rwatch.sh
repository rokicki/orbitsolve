#!/bin/bash
# 2026-10-24: exit (so the session gets a notice) once no certextend, pairlogcheck, certmerge,
# certcheck, boundx, boundcheck or orbitbound55 (2026-10-27), opbound, axbound, axbfs or stagedump (2026-10-28) has run for 10 minutes: the machine has gone idle.  Matches process names exactly
# (pgrep -x), never script names, so shells that mention them don't count.
#   rwatch.sh [MINUTES]
M=${1:-10}; idle=0
while [ $idle -lt $((M * 3)) ]; do
  if pgrep -x certextend > /dev/null || pgrep -x pairlogcheck > /dev/null || pgrep -x certmerge > /dev/null || pgrep -x certcheck > /dev/null || pgrep -x boundx > /dev/null || pgrep -x boundcheck > /dev/null || pgrep -x orbitbound55 > /dev/null || pgrep -x opbound > /dev/null || pgrep -x axbound > /dev/null || pgrep -x axbfs > /dev/null || pgrep -x stagedump > /dev/null; then idle=0; else idle=$((idle + 1)); fi
  sleep 20
done
echo "IDLE: no feed tools running for $M minutes ($(date '+%m-%d %H:%M'))"
