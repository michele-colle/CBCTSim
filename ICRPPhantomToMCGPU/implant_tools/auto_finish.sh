#!/bin/bash
# auto_finish.sh <case> <z...>  -- score each candidate Z, build at the best.
# Best = most accepted sites, ties broken by mean bone fraction.
# No 'set -e': a zero-scoring candidate is normal, not a script failure.
W=/home/colle/implant_work
c="$1"; shift
BEST=""; BESTN=-1; BESTB=0
while read -r line; do
  echo "$line"
  z=$(sed -n 's/.*z=\([-0-9.]*\).*/\1/p' <<<"$line")
  n=$(sed -n 's/.*accepted=\([0-9]*\).*/\1/p' <<<"$line")
  b=$(sed -n 's/.*mean_bone=\([0-9.]*\).*/\1/p' <<<"$line")
  [ -z "$z" ] && continue
  if [ "$n" -gt "$BESTN" ] 2>/dev/null || { [ "$n" -eq "$BESTN" ] 2>/dev/null && awk "BEGIN{exit !($b>$BESTB)}"; }; then
    BEST=$z; BESTN=$n; BESTB=$b
  fi
done < <("$W/scorez.sh" "$c" "$@")
echo "CHOSEN Z=$BEST (accepted=$BESTN mean_bone=$BESTB)"
if [ "${BESTN:-0}" -lt 2 ]; then
  echo "RESULT=FLAGGED_NO_VIABLE_Z"; exit 0
fi
"$W/finish_case.sh" "$c" "$BEST"
