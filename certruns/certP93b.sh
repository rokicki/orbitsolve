# certcheck of the 2026-10-22 snapshot (all four stages)
D=/Users/rokicki/orbitsolve-certsafe/obl23pair-cf-xfree-P93-2026-10-22
echo "# certcheck of $D ($(date '+%m-%d %H:%M'))"
for st in none..aUbU aUbU..aDbD aUaDbUbD..aLaFaRaB aUaLaFaRaBaDbUbD..bLbFbRbB; do
  echo "## $st"; devtools/certcheck $D/stages/$st 2>&1
done
echo "# end ($(date '+%m-%d %H:%M'))"
