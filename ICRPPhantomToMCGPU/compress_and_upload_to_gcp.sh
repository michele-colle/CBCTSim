#!/bin/bash
# Compresses one or more MC-GPU output folders (.raw + .txt only -- .in files
# are excluded, see the --exclude below) with xz, then uploads the resulting
# .tar.xz to the mcgpu-data-gcp bucket's output/ folder.
#
# Mirrors this known-good command (already used for PHAN_ZURIGO_*_978.tar.xz,
# both present in gs://mcgpu-data-gcp/output/):
#   tar --use-compress-program='xz -T 32 -9e -M 30G' -cf NAME.tar.xz NAME/
#
# Usage:
#   ./compress_and_upload_to_gcp.sh <folder1> [folder2 ...]
#
# Each <folder> is compressed to "<folder>.tar.xz" next to it, then uploaded
# to gs://mcgpu-data-gcp/output/ (assumes gcloud/gsutil are already
# authenticated -- confirmed configured: account michele.colle@rartech.it,
# project cbct-simulation-gpu).  The local .tar.xz is left on disk after
# upload (not deleted) -- remove it yourself if you want the space back.

set -euo pipefail

GCS_DEST="gs://mcgpu-data-gcp/output/"
XZ_OPTS='xz -T 32 -9e -M 30G'

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <folder1> [folder2 ...]" >&2
    exit 1
fi

for dir in "$@"; do
    dir="${dir%/}"   # strip trailing slash, if any
    if [[ ! -d "$dir" ]]; then
        echo "  [skip] not a directory: $dir" >&2
        continue
    fi

    name="$(basename "$dir")"
    parent="$(dirname "$dir")"
    archive="${parent}/${name}.tar.xz"

    echo "=== Compressing: $dir -> $archive (raw + txt only, .in excluded) ==="
    tar --exclude='*.in' --use-compress-program="$XZ_OPTS" \
        -cf "$archive" -C "$parent" "$name"

    echo "=== Uploading: $archive -> $GCS_DEST ==="
    gsutil -m cp "$archive" "$GCS_DEST"

    echo "Done: $name -> ${GCS_DEST}$(basename "$archive")"
done
