#!/usr/bin/env bash
# Run mcrp_to_vox with a parameter file.
# Usage:  ./run_mcrp_to_vox.sh [path/to/params.cfg]
# Default cfg: params/run.cfg

set -euo pipefail
cd "$(dirname "$0")"

CFG="${1:-params/run.cfg}"
BINARY="./build/mcrp_to_vox"

if [[ ! -f "$BINARY" ]]; then
    echo "Binary not found – building first..."
    cmake --build build --target mcrp_to_vox -j"$(nproc)"
fi

echo "Running: $BINARY $CFG"
"$BINARY" "$CFG"
