#!/bin/bash
# Launcher for the dicom_to_mcgpu binary (avoids VSCode/GDB F5 issues).
#
# List one or more .cfg files below and run:  ./run_dicom_to_mcgpu.sh
# Paths may be pasted as Windows paths (F:\...) — they are translated to WSL,
# same rule as the C++ toLinuxPath().  Relative paths are resolved from the
# repo root (this script's folder), so the cfg's relative refs (params/...,
# results/, phantom/...) work.
set -uo pipefail

# ======================= EDIT THESE =========================================
CFG_FILES=(
    'params/dicom_to_mcgpu_template.cfg'
    # 'params/generated/GRDN00MGH1ABQLE8_A.cfg'
    # 'params/generated/GRDN00MGH1ABQLE8_B.cfg'
)
BINARY='build/dicom_to_mcgpu'
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"   # run from repo root so cfg relative paths resolve

# --- Windows/WSL path translation (mirrors dicom_to_mcgpu.cpp toLinuxPath) ---
to_wsl() {
    local p="$1"
    p="${p%$'\r'}"; p="${p#\"}"; p="${p%\"}"; p="${p#\'}"; p="${p%\'}"
    p="${p//\\//}"
    if [[ "$p" =~ ^([A-Za-z]):(/.*)?$ ]]; then
        local drive; drive="$(printf '%s' "${BASH_REMATCH[1]}" | tr '[:upper:]' '[:lower:]')"
        p="/mnt/${drive}${BASH_REMATCH[2]}"
    fi
    printf '%s' "$p"
}

# Allow overriding the cfg list from the command line: ./run_dicom_to_mcgpu.sh a.cfg b.cfg
if [[ $# -ge 1 ]]; then
    CFG_FILES=("$@")
fi

if [[ ! -x "$BINARY" ]]; then
    echo "ERROR: binary not found or not executable: $BINARY" >&2
    echo "Build it first (e.g. cmake --build build --target dicom_to_mcgpu)." >&2
    exit 1
fi

rc=0
for raw in "${CFG_FILES[@]}"; do
    cfg="$(to_wsl "$raw")"
    echo "==================================================================="
    echo "=== dicom_to_mcgpu  $cfg"
    echo "==================================================================="
    if [[ ! -f "$cfg" ]]; then
        echo "  SKIP: cfg not found: $cfg" >&2
        rc=1
        continue
    fi
    if ! "$BINARY" "$cfg"; then
        echo "  FAILED: $cfg" >&2
        rc=1
    fi
done

exit "$rc"
