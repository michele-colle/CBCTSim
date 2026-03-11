#!/bin/bash
# run_all_mcgpu_materials.sh
# Generates MC-GPU material files for every MRCP phantom.
# Run from the project root (ICRPPhantomToMCGPU/).

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY="${SCRIPT_DIR}/build/mcrp_to_mcgpu_materials"
PENELOPE_DIR="${HOME}/penelope/pendbase/"
OUTPUT_BASE="${SCRIPT_DIR}/data/mcgpu_mcrp_materials"

if [ ! -f "${BINARY}" ]; then
    echo "ERROR: ${BINARY} not found. Build the project first."
    exit 1
fi

PHANTOMS=(
    MRCP-00F
    MRCP-00M
    MRCP-01F
    MRCP-01M
    MRCP-05F
    MRCP-05M
    MRCP-10F
    MRCP-10M
    MRCP-15F
    MRCP-15M
    MRCP_AF
    MRCP_AM
)

mkdir -p "${OUTPUT_BASE}"

for PHANTOM in "${PHANTOMS[@]}"; do
    OUTPUT_DIR="${OUTPUT_BASE}/${PHANTOM}/"
    echo "========================================"
    echo " Processing: ${PHANTOM}"
    echo " Output:     ${OUTPUT_DIR}"
    echo "========================================"
    "${BINARY}" "${PHANTOM}" "${PENELOPE_DIR}" "${OUTPUT_DIR}"
    echo "Done: ${PHANTOM}"
    echo ""
done

echo "All phantoms processed. Results in: ${OUTPUT_BASE}"
