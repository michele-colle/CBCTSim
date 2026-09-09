#!/bin/bash
# scorez.sh <case> <z...>  -> accepted-site count + mean bone for each Z
W=/home/colle/implant_work; PY=/home/colle/miniforge3/bin/python
c="$1"; shift
case "$c" in
  GRDN*)  SRC=/mnt/h/MICHELE_MCGPU/GRADIENTHEALTH_VOLUME_EXPORT ;;
  CQ500*) SRC=/mnt/h/MICHELE_MCGPU/QUREAI_VOLUME_EXPORT ;;
  MRCP*)  SRC=/mnt/h/MICHELE_MCGPU/MCGPU_EXPORT ;;
esac
BN="${c}_2134x2134x834byte.raw"; R="$W/raw/$BN"; [ -f "$R" ] || R="$SRC/$BN"
T=$(mktemp)
for z in "$@"; do
  OUT=$($PY $W/place.py "$R" "$T" --arch-z "$z" 2>/dev/null)
  N=$(grep -c 'OK$' <<<"$OUT"); N=${N:-0}
  B=$(grep -oE 'bone=[0-9.]+' <<<"$OUT" | cut -d= -f2 | awk '{s+=$1;n++}END{printf "%.2f",(n?s/n:0)}')
  echo "  z=$z accepted=$N mean_bone=$B"
done
rm -f "$T"
