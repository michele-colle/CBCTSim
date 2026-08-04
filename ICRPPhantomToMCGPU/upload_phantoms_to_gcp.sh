#!/bin/bash
# Compresses each .raw file in a directory into its OWN .tar.xz (the .raw
# ONLY -- no .txt, no .in) and uploads it individually to
# gs://mcgpu-data-gcp/phantom/.
#
# Compression command matches the known-good reference (already used for
# gs://mcgpu-data-gcp/output/PHAN_ZURIGO_*_978.tar.xz):
#   tar --use-compress-program='xz -T 32 -9e -M 30G' -cf NAME.tar.xz NAME
#
# Usage:
#   ./upload_phantoms_to_gcp.sh <directory> [directory2 ...]
#
# Every "*.raw" directly inside each <directory> becomes its own archive,
# "<raw-stem>.tar.xz" (written next to the .raw, on the same drive -- make
# sure there's room for one extra compressed copy at a time), then uploaded.
# Runs one .raw at a time (not in parallel) so each compression gets the
# full 32 threads to itself. Assumes gcloud/gsutil are already authenticated
# (confirmed configured: account michele.colle@rartech.it, project
# cbct-simulation-gpu). Local .tar.xz files are NOT deleted after upload.

set -euo pipefail

GCS_DEST="gs://mcgpu-data-gcp/phantom/"
XZ_OPTS='xz -T 32 -9e -M 30G'

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <directory> [directory2 ...]" >&2
    exit 1
fi

for dir in "$@"; do
    dir="${dir%/}"   # strip trailing slash, if any
    if [[ ! -d "$dir" ]]; then
        echo "  [skip] not a directory: $dir" >&2
        continue
    fi

    shopt -s nullglob
    raws=("$dir"/*.raw)
    shopt -u nullglob
    if [[ ${#raws[@]} -eq 0 ]]; then
        echo "  [skip] no .raw files in: $dir" >&2
        continue
    fi

    echo "=== $dir: ${#raws[@]} .raw file(s) ==="
    for raw in "${raws[@]}"; do
        rawname="$(basename "$raw")"
        base="${rawname%.raw}"
        archive="${dir}/${base}.tar.xz"

        echo "--- Compressing: $rawname -> $(basename "$archive")"
        tar --use-compress-program="$XZ_OPTS" -cf "$archive" -C "$dir" "$rawname"

        echo "--- Uploading: $(basename "$archive") -> $GCS_DEST"
        gsutil -m cp "$archive" "$GCS_DEST"

        echo "Done: $base"
    done
done
