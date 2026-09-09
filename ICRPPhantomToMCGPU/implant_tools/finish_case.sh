#!/bin/bash
# finish_case.sh <case> <arch_z>
# Places + stamps implants, verifies, renders the review PNG, compresses,
# publishes the archive to H: and reclaims the local .raw.  NEVER uploads.
set -euo pipefail
W=/home/colle/implant_work
PY=/home/colle/miniforge3/bin/python
c="$1"; AZ="$2"; EXTRA="${3:-}"
case "$c" in
  GRDN*)  SRCDIR=/mnt/h/MICHELE_MCGPU/GRADIENTHEALTH_VOLUME_EXPORT
          OUTDIR=/mnt/h/MICHELE_MCGPU/GRADIENTHEALTH_IMPLANT_VOLUME_EXPORT; B=gradienthealth ;;
  CQ500*) SRCDIR=/mnt/h/MICHELE_MCGPU/QUREAI_VOLUME_EXPORT
          OUTDIR=/mnt/h/MICHELE_MCGPU/QUREAI_IMPLANT_VOLUME_EXPORT;         B=qureai ;;
  MRCP*)  SRCDIR=/mnt/h/MICHELE_MCGPU/MCGPU_EXPORT
          OUTDIR=/mnt/h/MICHELE_MCGPU/MCGPU_IMPLANT_VOLUME_EXPORT;          B=mcrp ;;
esac
REPO=/home/colle/CBCTSim/ICRPPhantomToMCGPU
CHK="$REPO/positioning_checks_${B}_implant"; PEND="${CHK}_pending"
JDIR="$REPO/params/generated_${B}_implant"
MAN="$REPO/batch_jobs_${B}_implant_exported_raw.txt"
mkdir -p "$OUTDIR" "$CHK" "$PEND" "$JDIR"
[ -f "$MAN" ] || echo -e "# implanted .raw archives\t<case>\t<archive>" > "$MAN"

BN="${c}_2134x2134x834byte.raw"
BASE="$W/raw/$BN"; [ -f "$BASE" ] || BASE="$SRCDIR/$BN"
OUTN="${c}_imp_2134x2134x834byte.raw"        # <case ends in _A>_imp_<dims>
OUT="$W/raw/$OUTN"
JSON="$JDIR/${c}.json"

echo "--- place ---"
$PY "$W/place.py" "$BASE" "$JSON" --arch-z "$AZ" $EXTRA
NOK=$($PY -c "import json,sys;print(sum(1 for m in json.load(open(sys.argv[1])) if m['ok']))" "$JSON")
echo "accepted sites: $NOK"
if [ "$NOK" -lt 2 ]; then
  echo "FLAG: fewer than 2 accepted sites -> not stamping, montage copied to _pending"
  cp -f "$W/png/${c}_arch_montage.png" "$PEND/" 2>/dev/null || true
  echo "RESULT=FLAGGED_TOO_FEW_SITES"; exit 0
fi

echo "--- stamp ---"
$PY "$W/stamp.py" "$BASE" "$OUT" "$JSON" --base-txt "$SRCDIR/${c}_2134x2134x834byte.txt"

echo "--- verify ---"
$PY - "$BASE" "$OUT" <<'PYEOF'
import numpy as np, sys
NX=NY=2134; NZ=834
b=np.memmap(sys.argv[1],dtype=np.uint8,mode="r",shape=(NZ,NY,NX))
a=np.memmap(sys.argv[2],dtype=np.uint8,mode="r",shape=(NZ,NY,NX))
hb=np.zeros(256,np.int64); ha=np.zeros(256,np.int64); nd=bad=0
for k in range(NZ):
    sb=np.asarray(b[k]).ravel(); sa=np.asarray(a[k]).ravel()
    hb+=np.bincount(sb,minlength=256); ha+=np.bincount(sa,minlength=256)
    d=sb!=sa
    if d.any(): nd+=int(d.sum()); bad+=int((sa[d]!=5).sum())
errs=[]; warn=[]
if bad: errs.append(f"{bad} changed voxels are not label 5")
# Carbon/foam are absolute: an implant must never touch the stretcher.
for lab,nm in ((10,"carbon"),(11,"foam")):
    if hb[lab]!=ha[lab]: errs.append(f"{nm} count changed {hb[lab]}->{ha[lab]}")
# Air: a handful of voxels at a cylinder's boundary is discretisation, not a
# floating implant. Tolerate <=0.1% of the stamped volume, but always report it.
dair=int(hb[0]-ha[0])
if dair:
    tol=max(20,int(0.001*ha[5]))
    (warn if 0 < dair <= tol else errs).append(f"air->implant {dair} voxels (tol {tol})")
for w in warn: print("VERIFY_WARN: "+w)
if hb.sum()!=ha.sum(): errs.append("total voxel count changed")
if ha[5]!=nd: errs.append(f"label5={ha[5]} != changed={nd}")
print(f"changed={nd} label5={ha[5]} air/carbon/foam preserved={not errs}")
if errs: print("VERIFY_FAIL: "+"; ".join(errs)); sys.exit(9)
print("VERIFY_OK")
PYEOF

echo "--- review PNG ---"
CZ=$($PY -c "import json,sys;d=[m for m in json.load(open(sys.argv[1])) if m['ok']];print(f\"{d[0]['cz_mm']} {d[0]['cx_mm']} {d[0]['cy_mm']}\")" "$JSON")
set -- $CZ
$PY "$W/view.py" "$OUT" "$CHK/${c}_imp_review.png" \
    --axial-z "$1" --sag-x "$2" --cor-y "$3" --pad 8 --grid 5 --implants "$JSON" >/dev/null

echo "--- compress + prove ---"
( cd "$W/raw" && tar --use-compress-program='xz -T 32 -9e -M 30G' -cf "${OUTN%.raw}.tar.xz" "$OUTN" )
ARC="$W/raw/${OUTN%.raw}.tar.xz"
xz -t "$ARC"
M1=$(md5sum "$OUT" | cut -d' ' -f1)
M2=$(xz -dc -T 32 "$ARC" | tar -xO | md5sum | cut -d' ' -f1)
[ "$M1" = "$M2" ] || { echo "ROUND_TRIP_MD5_MISMATCH"; exit 10; }
echo "round-trip md5 OK ($M1)"

mv -f "$ARC" "$OUTDIR/"
cp -f "${OUT%.raw}.txt" "$OUTDIR/" 2>/dev/null || true
rm -f "$OUT" "${OUT%.raw}.txt"       # archive proven; base never touched
printf '%s\t%s\t%s\n' "$c" "arch_z=$AZ sites=$NOK" "$OUTDIR/${OUTN%.raw}.tar.xz" >> "$MAN"
echo "RESULT=OK  archive=$OUTDIR/${OUTN%.raw}.tar.xz  review=$CHK/${c}_imp_review.png"
# MCRP base cleanup: the base was decompressed locally just for this case and is
# always recoverable from its archive on H:. GRDN/CQ500 bases live on H: and are
# never copied, so there is nothing to clean for those.
case "$c" in MRCP*) rm -f "$W/raw/$BN" ;; esac
