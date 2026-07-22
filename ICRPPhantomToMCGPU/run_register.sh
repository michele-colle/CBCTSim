#!/bin/bash
# Launcher for register_dicom_to_dicom.py.
#
# Edit the paths below (paste Windows paths straight in, e.g.
#   F:\Michele_diskF\case\dicom   or   C:/data/ref ) then run:  ./run_register.sh
# They are translated to WSL paths the same way the C++ tool's toLinuxPath()
# does:  X:\...  ->  /mnt/x/...   backslashes -> forward slashes.
set -uo pipefail

# ======================= EDIT THESE =========================================
FIXED='F:\Michele_diskF\Test Fantoccio Zurigo-CBCTvsCT\CBCT_DE\NonameNoname_01-01-1900_09-06-2026-1824_DualEnergy\ExportImages\export_VMI_60.0keV_bone'          # reference (fixed) DICOM dir
MOVING='F:\Michele_diskF\Test Fantoccio Zurigo-CBCTvsCT\Scheitel-MG_nat_0.40_Hr84_Q3_hr'           # convert-target (moving) DICOM dir
MASK='F:\Michele_diskF\Test Fantoccio Zurigo-CBCTvsCT\Segmentation-Segment_1-label.nrrd'       # moving segmentation .nrrd ('' to skip)
OUT='F:\Michele_diskF\Test Fantoccio Zurigo-CBCTvsCT\patient_ct_registered'   # output registered DICOM dir

REGTYPE='rigid'          # rigid | affine | bspline
MARGIN='20'              # air margin around anatomy, mm
OUT_SPACING='0.3 0.3 0.3'  # output voxel size X Y Z (mm); '' = match fixed volume
REG_SPACING='1.5'        # registration voxel size (isotropic, mm) — coarse is fine for rigid
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PY="${SCRIPT_DIR}/register_dicom_to_dicom.py"

# --- Windows/WSL path translation (mirrors dicom_to_mcgpu.cpp toLinuxPath) ---
to_wsl() {
    local p="$1"
    p="${p%$'\r'}"                       # strip trailing CR from pasted text
    p="${p#\"}"; p="${p%\"}"             # strip surrounding double quotes
    p="${p#\'}"; p="${p%\'}"             # strip surrounding single quotes
    p="${p//\\//}"                       # backslashes -> forward slashes
    if [[ "$p" =~ ^([A-Za-z]):(/.*)?$ ]]; then
        local drive rest
        drive="$(printf '%s' "${BASH_REMATCH[1]}" | tr '[:upper:]' '[:lower:]')"
        rest="${BASH_REMATCH[2]}"
        p="/mnt/${drive}${rest}"
    fi
    printf '%s' "$p"
}

FIXED="$(to_wsl "$FIXED")"
MOVING="$(to_wsl "$MOVING")"
MASK="$(to_wsl "$MASK")"
OUT="$(to_wsl "$OUT")"

echo "  fixed       : $FIXED"
echo "  moving      : $MOVING"
echo "  mask        : ${MASK:-<none>}"
echo "  out         : $OUT"
echo "  reg type    : $REGTYPE   margin: $MARGIN mm"
echo "  out spacing : ${OUT_SPACING:-<match fixed>} mm"
echo "  reg spacing : $REG_SPACING mm (isotropic)"
echo

ARGS=(--fixed "$FIXED" --moving "$MOVING" --out "$OUT"
      --reg-type "$REGTYPE" --margin-mm "$MARGIN"
      --reg-spacing "$REG_SPACING" --save-nrrd)
[[ -n "$MASK" ]] && ARGS+=(--mask "$MASK")
# OUT_SPACING is three numbers -> pass unquoted so it splits into SX SY SZ
[[ -n "$OUT_SPACING" ]] && ARGS+=(--out-spacing $OUT_SPACING)

set -x
python3 "$PY" "${ARGS[@]}"
