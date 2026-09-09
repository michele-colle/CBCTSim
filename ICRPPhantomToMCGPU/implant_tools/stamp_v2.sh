#!/bin/bash
# stamp_v2.sh [case_stem ...]
#
# Round-9 implant pass over the nose-centred bases in IMPLANT_V2_BASE.
# Per case, in one pass so each 3.8 GB volume is decompressed only once:
#
#   1. decompress the round-9 base archive
#   2. rename it to the current convention  <stem>_02_A_<dims>.raw  and rewrite
#      the .raw reference inside the .txt companion
#   3. compress + prove + publish the renamed BASE to $OUT (the control volume)
#   4. pick the alveolar arch Z: candidates are the round-8 pick carried through
#      the known round-9 translation, +-2 and +-4 mm; score each with place.py
#      and take the most accepted sites (ties -> mean bone).  If nothing reaches
#      2 sites, retry the same sweep with --small (3.3 x 8 mm paediatric/atrophic
#      fixture) before giving up.
#   5. stamp -> <stem>_02_A_imp_<dims>.raw, verify, render the review PNG
#   6. compress + prove + publish the IMPLANTED volume, reclaim both local raws
#
# Resumable: a case already in $MAN is skipped.  Never uploads, never touches
# IMPLANT_V2_BASE (that stays as the fallback copy).
#
# No `set -e`: a case that fails its own gates must be flagged and skipped, not
# abort the batch.
set -uo pipefail

R=/home/colle/CBCTSim/ICRPPhantomToMCGPU
W=/home/colle/implant_work
PY=/home/colle/miniforge3/bin/python
SRC=/mnt/h/MICHELE_MCGPU/IMPLANT_V2_BASE
OUT=/mnt/h/MICHELE_MCGPU/IMPLANT_V2_VOLUME_EXPORT
CHK=$R/positioning_checks_implant_v2_imp
PEND=$CHK/_pending
MAN=$R/batch_jobs_implant_v2_imp_exported_raw.txt
TBL=$R/implant_tools/arch_v2_predicted.tsv
DIMS=2134x2134x834byte
XZ='xz -T 32 -9e -M 30G'

mkdir -p "$OUT" "$CHK" "$PEND" "$W/build" "$W/json"
[ -f "$MAN" ] || printf '# round-9 dental set\t<stem>\t<arch_z>\t<sites>\t<base archive>\t<implant archive>\n' > "$MAN"

# compress $1 (a .raw in $W/build) and prove the archive round-trips, then move
# it plus the .txt to $OUT.  Returns 1 without deleting anything on any failure.
publish() {
  local raw="$1" bn arc m1 m2
  bn=$(basename "$raw" .raw); arc="$W/build/$bn.tar.xz"
  rm -f "$arc"
  ( cd "$W/build" && tar --use-compress-program="$XZ" -cf "$bn.tar.xz" "$bn.raw" ) || return 1
  xz -t "$arc" || return 1
  m1=$(md5sum "$raw" | cut -d' ' -f1)
  m2=$(xz -dc -T 32 "$arc" | tar -xO | md5sum | cut -d' ' -f1)
  [ "$m1" = "$m2" ] || { echo "    ROUND_TRIP_MD5_MISMATCH $bn"; return 1; }
  mv -f "$arc" "$OUT/" && cp -f "$W/build/$bn.txt" "$OUT/" 2>/dev/null
  echo "    published $bn.tar.xz  md5=$m1"
}

# score one arch-Z candidate -> "<accepted> <mean_bone>"
score_z() {
  local raw="$1" z="$2" extra="${3:-}" o
  o=$($PY "$W/place.py" "$raw" "$W/json/_scratch.json" --arch-z "$z" $extra 2>/dev/null)
  local n b
  n=$(grep -cE 'OK$' <<<"$o"); n=${n:-0}
  b=$(grep -oE 'bone=[0-9.]+' <<<"$o" | cut -d= -f2 | awk '{s+=$1;c++}END{printf "%.3f",(c?s/c:0)}')
  echo "$n ${b:-0}"
}

# pick the best candidate around the predicted arch; echoes "<z> <n> <extra>"
pick_arch() {
  local raw="$1" z0="$2" extra bz bn bb n b z
  for extra in "" "--small"; do
    bz=""; bn=-1; bb=0
    for d in 0 -2 2 -4 4; do
      z=$($PY -c "print(round($z0 + $d, 1))")
      read -r n b < <(score_z "$raw" "$z" "$extra")
      echo "      z=$z ${extra:-full} accepted=$n mean_bone=$b" >&2
      if [ "$n" -gt "$bn" ] 2>/dev/null || { [ "$n" -eq "$bn" ] 2>/dev/null && \
           awk "BEGIN{exit !($b>$bb)}"; }; then bz=$z; bn=$n; bb=$b; fi
    done
    [ "${bn:-0}" -ge 2 ] && { echo "$bz $bn $extra"; return 0; }
  done
  echo "$bz $bn --small"
}

mapfile -t ROWS < <(grep -v '^#' "$TBL")
sel=("$@")
for row in "${ROWS[@]}"; do
  IFS=$'\t' read -r OLD STEM AZ0 MARGIN <<<"$row"
  if [ ${#sel[@]} -gt 0 ]; then
    printf '%s\n' "${sel[@]}" | grep -qx "$STEM" || continue
  fi
  grep -q "^$STEM	" "$MAN" && { echo "SKIP $STEM (already done)"; continue; }
  echo "=== $STEM   predicted arch $AZ0 (margin $MARGIN)"

  BASE="$W/build/${STEM}_02_A_${DIMS}.raw"
  if [ ! -f "$BASE" ]; then
    echo "  decompress base"
    xz -dc -T 32 "$SRC/${OLD}_${DIMS}.tar.xz" | tar -x -C "$W/build" || { echo "  DECOMPRESS FAILED"; continue; }
    mv -f "$W/build/${OLD}_${DIMS}.raw" "$BASE" || { echo "  RENAME FAILED"; continue; }
    sed "s|${OLD}_${DIMS}.raw|${STEM}_02_A_${DIMS}.raw|" \
        "$SRC/${OLD}_${DIMS}.txt" > "$W/build/${STEM}_02_A_${DIMS}.txt"
  else
    echo "  base already present locally"
  fi

  if [ ! -f "$OUT/${STEM}_02_A_${DIMS}.tar.xz" ]; then
    echo "  publish base (control volume)"
    publish "$BASE" || { echo "  BASE PUBLISH FAILED -- keeping raw, skipping case"; continue; }
  fi

  echo "  arch sweep"
  read -r AZ NSITE EXTRA < <(pick_arch "$BASE" "$AZ0")
  echo "  chosen arch_z=$AZ sites=$NSITE ${EXTRA:-full}"
  JSON="$W/json/${STEM}.json"
  $PY "$W/place.py" "$BASE" "$JSON" --arch-z "$AZ" $EXTRA | sed 's/^/    /'
  if [ "${NSITE:-0}" -lt 2 ]; then
    echo "  FLAG: <2 accepted sites -> base kept, no implants stamped"
    $PY "$W/montage.py" "$BASE" "$PEND/${STEM}_arch_montage.png" \
        --z0 "$($PY -c "print($AZ0-12)")" --z1 "$($PY -c "print($AZ0+12)")" \
        --step 2 --cols 4 --grid 10 >/dev/null 2>&1
    printf '%s\t%s\t%s\t%s\t%s\n' "$STEM" "arch_z=$AZ" "FLAGGED_TOO_FEW_SITES" \
      "$OUT/${STEM}_02_A_${DIMS}.tar.xz" "-" >> "$MAN"
    rm -f "$BASE" "$W/build/${STEM}_02_A_${DIMS}.txt"
    continue
  fi

  IMP="$W/build/${STEM}_02_A_imp_${DIMS}.raw"
  echo "  stamp"
  rm -f "$IMP"
  $PY "$W/stamp.py" "$BASE" "$IMP" "$JSON" \
      --base-txt "$W/build/${STEM}_02_A_${DIMS}.txt" | sed 's/^/    /' || { echo "  STAMP FAILED"; continue; }

  echo "  verify"
  $PY "$W/verify_imp.py" "$BASE" "$IMP" | sed 's/^/    /'
  [ ${PIPESTATUS[0]} -eq 0 ] || { echo "  VERIFY FAILED -- implanted raw kept for inspection"; continue; }

  CZ=$($PY -c "import json,sys;d=[m for m in json.load(open(sys.argv[1])) if m['ok']];print(d[0]['cz_mm'],d[0]['cx_mm'],d[0]['cy_mm'])" "$JSON")
  set -- $CZ
  $PY "$W/view.py" "$IMP" "$CHK/${STEM}_02_A_imp_review.png" \
      --axial-z "$1" --sag-x "$2" --cor-y "$3" --pad 8 --grid 5 --implants "$JSON" >/dev/null

  publish "$IMP" || { echo "  IMPLANT PUBLISH FAILED -- keeping raws"; continue; }

  printf '%s\t%s\t%s\t%s\t%s\n' "$STEM" "arch_z=$AZ ${EXTRA:-full}" "sites=$NSITE" \
    "$OUT/${STEM}_02_A_${DIMS}.tar.xz" "$OUT/${STEM}_02_A_imp_${DIMS}.tar.xz" >> "$MAN"
  rm -f "$BASE" "$IMP" "$W/build/${STEM}_02_A_${DIMS}.txt" "$W/build/${STEM}_02_A_imp_${DIMS}.txt"
  echo "  OK  review=$CHK/${STEM}_02_A_imp_review.png"
done
echo ALL_DONE
