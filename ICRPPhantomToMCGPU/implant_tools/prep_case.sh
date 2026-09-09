#!/bin/bash
# prep_case.sh <case>   e.g. prep_case.sh GRDN00MGH1ABQLE8_A
# Ensures the base volume + profile are available, then renders the axial
# montage over the vertex-anchored arch window for the visual arch-Z pick.
# Read-only w.r.t. every base volume.
set -euo pipefail
W=/home/colle/implant_work
PY=/home/colle/miniforge3/bin/python
c="$1"
case "$c" in
  GRDN*)  SRCDIR=/mnt/h/MICHELE_MCGPU/GRADIENTHEALTH_VOLUME_EXPORT; FROM=raw ;;
  CQ500*) SRCDIR=/mnt/h/MICHELE_MCGPU/QUREAI_VOLUME_EXPORT;         FROM=raw ;;
  MRCP*)  SRCDIR=/mnt/h/MICHELE_MCGPU/MCGPU_EXPORT;                 FROM=archive ;;
  *) echo "unknown case class: $c" >&2; exit 2 ;;
esac
BN="${c}_2134x2134x834byte.raw"
if [ -f "$W/raw/$BN" ]; then BASE="$W/raw/$BN"
elif [ "$FROM" = archive ]; then
  echo "decompressing $c ..."
  xz -dc -T 32 "$SRCDIR/${c}_2134x2134x834byte.tar.xz" | tar -x -C "$W/raw"
  BASE="$W/raw/$BN"
else
  BASE="$SRCDIR/$BN"     # read the base in place on H:; never copy, never modify
fi
[ -f "$BASE" ] || { echo "base not found: $BASE" >&2; exit 3; }
[ -f "$W/prof/$c.npz" ] || { echo "dumping profile ..."; $PY "$W/dump_profile.py" "$W/prof/$c.npz" < "$BASE"; }

read -r VTX Z0 Z1 CAND <<<"$($PY - "$W/prof/$c.npz" <<'PYEOF'
import numpy as np, sys
V=0.3; OFFZ=-125.10
d=np.load(sys.argv[1],allow_pickle=True); cols=list(d["cols"]); P=d["prof"]
z=d["k"]*V+OFFZ; vtx=z.max()
bone=P[:,cols.index("bone")]; aW=P[:,cols.index("archW")]
gap=P[:,cols.index("ant_bone_y")]-P[:,cols.index("ant_skin_y")]
lo,hi=vtx-190,vtx-125
m=(z>=lo)&(z<=hi)&(bone>4000)&(aW>=28)&(aW<=52)&(gap>=6)&(gap<=28)
cand=z[np.where(m)[0][0]] if m.any() else (lo+hi)/2
print(f"{vtx:.1f} {lo:.1f} {hi:.1f} {cand:.1f}")
PYEOF
)"
PNG="$W/png/${c}_arch_montage.png"
$PY "$W/montage.py" "$BASE" "$PNG" --z0 "$Z0" --z1 "$Z1" --step 6.5 --cols 4 --grid 10 >/dev/null
echo "CASE=$c"
echo "BASE=$BASE"
echo "VERTEX=$VTX  WINDOW=[$Z0,$Z1]  RULE_B_CANDIDATE=$CAND"
echo "MONTAGE=$PNG"
