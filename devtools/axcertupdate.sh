#!/bin/bash
# axcertupdate.sh CERTDIR STAGE TABLE: write an axis-split certificate stage (STAGE 3ax or 4op) from an
# axbound/opbound table (--save: one line per class, LEN<TAB>moves) as CERTDIR/stages/STAGE/lNN.txt,
# one file per length.  Only files whose contents change are replaced (the rest keep their dates), and
# files for lengths no longer present are removed.  Then CERTDIR/MANIFEST is rewritten from every stage
# present.  Check afterwards with: devtools/axcheck STAGE CERTDIR/stages/STAGE
set -e
if [ $# -ne 3 ] || { [ "$2" != 3ax ] && [ "$2" != 4op ]; }; then
  echo "usage: axcertupdate.sh CERTDIR 3ax|4op TABLE" >&2; exit 1
fi
echo "# axcertupdate.sh $*"
C=$1; S=$2; T=$3
D=$C/stages/$S
mkdir -p "$D"
N=$(mktemp -d "${TMPDIR:-/tmp}/axcert.XXXXXX")
awk -F'\t' -v d="$N" '{ printf "%s\n", $2 > sprintf("%s/l%02d.txt", d, $1) }' "$T"
changed=0; kept=0
made=" $(cd "$N" && echo l*.txt) "
for f in "$N"/l*.txt; do
  b=$(basename "$f")
  if [ -f "$D/$b" ] && cmp -s "$f" "$D/$b"; then kept=$((kept+1)); else mv "$f" "$D/$b"; changed=$((changed+1)); echo "  $S/$b written"; fi
done
for f in "$D"/l*.txt; do
  b=$(basename "$f")
  case "$made" in *" $b "*) ;; *) rm "$f"; echo "  $S/$b removed"; changed=$((changed+1));; esac
done
rm -rf "$N"
echo "$S: $changed files written or removed, $kept unchanged"
# MANIFEST: every stage present
{
  for st in 3ax 4op; do
    [ -d "$C/stages/$st" ] || continue
    for f in "$C/stages/$st"/l*.txt; do
      n=$(wc -l < "$f" | tr -d ' ')
      m=$(awk '{ if (NF > m) m = NF } END { print m + 0 }' "$f")
      echo "stages/$st/$(basename "$f") $n lines, longest $m"
    done
    echo "stages/$st: check with devtools/axcheck $st stages/$st"
  done
} > "$C/MANIFEST"
echo "MANIFEST rewritten"
