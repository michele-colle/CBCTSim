#!/bin/bash
# Round-9 rebuild: nose at the CBCT FOV centre.
# Per case: generate a shifted cfg -> build -> verify the nose landed at 0 ->
# positioning render -> compress -> prove -> reclaim the raw.
R=/home/colle/CBCTSim/ICRPPhantomToMCGPU; W=/home/colle/implant_work
PY=/home/colle/miniforge3/bin/python
OUT=/mnt/h/MICHELE_MCGPU/IMPLANT_V2_BASE
CHK=$R/positioning_checks_implant_v2
MAN=$R/batch_jobs_implant_v2_exported_raw.txt
[ -f "$MAN" ] || echo -e "# round-9 nose-centred bases\t<case>\t<nose_z>\t<margin>\t<archive>" > "$MAN"
while IFS=$'\t' read -r c nz; do
  grep -q "^$c	" "$MAN" && { echo "SKIP $c (already done)"; continue; }
  src=$(grep -F "$c|" $W/cfgmap.txt | head -1 | cut -d'|' -f2)
  [ -z "$src" ] && { echo "NO CFG $c"; continue; }
  margin=$($PY -c "print(round(10+($nz),2))")
  cfg=$R/params/generated_implant_v2/$c.cfg
  cp "$src" "$cfg"
  cat >> "$cfg" <<CFGEOF

# round-9: nose to the FOV centre. margin = 10 + nose_z, because
# targetTipIso = fovHalfZ - margin and the previous run used margin=10.
head_top_margin_mm = $margin
output_dir   = "$W/build"
mcgpu_in_dir = "$W/build"
CFGEOF
  echo "=== $c  nose_z=$nz  margin=$margin"
  LD_LIBRARY_PATH=/opt/Geant4/lib $R/build/dicom_to_mcgpu "$cfg" > "$W/build/$c.buildlog" 2>&1 || { echo "BUILD FAILED $c"; continue; }
  raw="$W/build/${c}_2134x2134x834byte.raw"
  [ -f "$raw" ] || { echo "NO RAW $c"; continue; }
  got=$($PY $W/nose_in_volume.py < "$raw" | awk '{print $1}')
  echo "   nose landed at $got (target 0.0)"
  $PY $W/fovcheck.py "$raw" "$CHK/${c}_positioning.png" --as-is >/dev/null 2>&1
  ( cd "$W/build" && tar --use-compress-program='xz -T 32 -9e -M 30G' -cf "${c}_2134x2134x834byte.tar.xz" "${c}_2134x2134x834byte.raw" )
  a="$W/build/${c}_2134x2134x834byte.tar.xz"
  if xz -t "$a" 2>/dev/null && [ "$(md5sum "$raw"|cut -d' ' -f1)" = "$(xz -dc -T 32 "$a"|tar -xO|md5sum|cut -d' ' -f1)" ]; then
    mv -f "$a" "$OUT/"; cp -f "$W/build/${c}_2134x2134x834byte.txt" "$OUT/" 2>/dev/null
    rm -f "$raw" "$W/build/${c}_2134x2134x834byte.txt"
    printf '%s\t%s\t%s\t%s\n' "$c" "$nz" "$margin" "$OUT/${c}_2134x2134x834byte.tar.xz" >> "$MAN"
    echo "   OK archived, raw reclaimed"
  else
    echo "   ARCHIVE PROOF FAILED -- keeping raw for $c"
  fi
done < $W/nose_final.tsv
echo ALL_BUILDS_DONE
