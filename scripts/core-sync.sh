#!/usr/bin/env bash
# Compare (default) or push (--push) the shared contract files across the sibling fork checkouts.
#
# The files listed in test/host/core-files.txt are byte-identical in the RSeries, PSI Pro and
# Flthy HP forks. RSeries is where they are edited (docs/CHANGING_THE_CORE.md). This script is
# the check that they still match, and the only way they are copied.
#
#   bash scripts/core-sync.sh           compare; exit 1 on any difference
#   bash scripts/core-sync.sh --push    copy the shared files into the siblings, write each
#                                       sibling's src/contract/CORE_PIN (the RSeries commit they
#                                       came from), and copy the parity vectors to Studio.
#                                       Nothing is committed anywhere.
#
# Sibling checkouts: PSI_DIR, FLTHY_DIR, STUDIO_DIR (defaults: next to this repository).
set -euo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
PSI_DIR="${PSI_DIR:-$here/../DroidNet-PSIPro}"
FLTHY_DIR="${FLTHY_DIR:-$here/../DroidNet-FlthyHPs}"
STUDIO_DIR="${STUDIO_DIR:-$here/../DroidNetSignalBooster}"
mode="${1:-check}"
list="$here/test/host/core-files.txt"
bad=0

files="$(grep -v '^[[:space:]]*$' "$list" | tr '\n' ' ')"
# shellcheck disable=SC2086
pin="$(git -C "$here" log -1 --format=%H -- $files)"
# shellcheck disable=SC2086
if [ "$mode" = "--push" ] && ! git -C "$here" diff --quiet HEAD -- $files; then
  echo "FAIL: a shared file has uncommitted changes here. Commit first: CORE_PIN must name a"
  echo "      commit that holds exactly the bytes being pushed."
  exit 1
fi

for sib in "$PSI_DIR" "$FLTHY_DIR"; do
  if [ ! -d "$sib" ]; then echo "MISSING checkout: $sib"; bad=1; continue; fi
  while IFS= read -r f; do
    [ -n "$f" ] || continue
    if [ "$mode" = "--push" ]; then
      mkdir -p "$sib/$(dirname "$f")"
      cp "$here/$f" "$sib/$f"
    elif ! cmp -s "$here/$f" "$sib/$f"; then
      echo "DRIFT: $f differs in $(basename "$sib")"
      bad=1
    fi
  done < "$list"
  if [ "$mode" = "--push" ]; then
    echo "$pin" > "$sib/src/contract/CORE_PIN"
    echo "pushed to $(basename "$sib") (CORE_PIN $pin)"
  else
    echo "$(basename "$sib"): CORE_PIN $(cat "$sib/src/contract/CORE_PIN" 2>/dev/null || echo none); RSeries core is at $pin"
  fi
done

vec="$here/test/host/vectors/contract_vectors.json"
studio="$STUDIO_DIR/web/studio/src/__tests__/fixtures/contract-vectors.json"
if [ "$mode" = "--push" ] && [ -d "$STUDIO_DIR" ]; then
  mkdir -p "$(dirname "$studio")"
  cp "$vec" "$studio"
  echo "pushed the parity vectors to Studio"
elif [ -f "$studio" ] && ! cmp -s "$vec" "$studio"; then
  echo "DRIFT: Studio's contract-vectors.json is not the generated one"
  bad=1
elif [ ! -f "$studio" ]; then
  echo "Studio has no contract-vectors.json yet ($studio)"
fi
exit $bad
