#!/bin/bash
# Compresses each .raw phantom into its OWN .tar.xz (the .raw ONLY -- no .txt,
# no .in) and uploads it individually to gs://mcgpu-data-gcp/phantom/, and
# optionally reclaims the disk afterwards by deleting each .raw once its
# archive is PROVEN recoverable.
#
# Compression command matches the known-good reference (already used for
# gs://mcgpu-data-gcp/output/PHAN_ZURIGO_*_978.tar.xz):
#   tar --use-compress-program='xz -T 32 -9e -M 30G' -cf NAME.tar.xz NAME
#
# Usage:
#   ./upload_phantoms_to_gcp.sh [options] <directory> [directory2 ...]
#
# Options:
#   --list FILE      Process ONLY the .raw files named in FILE (one per line,
#                    bare names resolved against <directory>; blank lines and
#                    '#' comments ignored) instead of every *.raw in it.
#                    Needed whenever a folder holds a MIX of phantoms you want
#                    published and phantoms you do not -- e.g. the CQ500 TEETH
#                    batch, where 26 of 66 were gantry-tilted (sheared, see
#                    README §10) and had to stay unpublished. Globbing the
#                    folder would have published them.
#   --reclaim        After a successful upload, verify the archive is
#                    recoverable and then DELETE the local .raw (see below).
#   --reclaim-only   Skip compression and upload entirely; only verify +
#                    delete .raw files whose archive and bucket object already
#                    exist. Use to reclaim space after an earlier run.
#   --dry-run        Report what would happen; compress/upload/delete nothing.
#
# Recoverability check before ANY .raw is deleted (all three must pass):
#   1. the local <base>.tar.xz exists;
#   2. its md5 equals the md5 GCS reports for the uploaded object
#      -> the bucket copy is byte-identical to the local archive;
#   3. `xz -t` passes -> the compressed stream and its checksums are intact.
# A .raw is only ever removed when it has two independent copies (local
# archive + bucket object). Anything that fails is skipped, never deleted.
#
# Runs one .raw at a time (not in parallel) so each compression gets the full
# 32 threads to itself. Assumes gcloud/gsutil are already authenticated
# (confirmed configured: account michele.colle@rartech.it, project
# cbct-simulation-gpu). Local .tar.xz files are always kept.
#
# NOTE: never check an upload with `gsutil ls ... 2>/dev/null` -- an expired
# credential then looks exactly like an empty bucket, i.e. a false "upload
# failed". This script lets gsutil's stderr through on purpose.

set -uo pipefail

GCS_DEST="gs://mcgpu-data-gcp/phantom/"
XZ_OPTS='xz -T 32 -9e -M 30G'

LIST=""
RECLAIM=0
RECLAIM_ONLY=0
DRY=0
dirs=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --list)         LIST="$2"; shift 2 ;;
        --reclaim)      RECLAIM=1; shift ;;
        --reclaim-only) RECLAIM_ONLY=1; RECLAIM=1; shift ;;
        --dry-run)      DRY=1; shift ;;
        -h|--help)      sed -n '2,48p' "$0"; exit 0 ;;
        -*)             echo "Unknown option: $1" >&2; exit 1 ;;
        *)              dirs+=("${1%/}"); shift ;;
    esac
done

if [[ ${#dirs[@]} -lt 1 ]]; then
    echo "Usage: $0 [--list FILE] [--reclaim|--reclaim-only] [--dry-run] <directory> ..." >&2
    exit 1
fi
if [[ -n "$LIST" && ! -f "$LIST" ]]; then
    echo "List file not found: $LIST" >&2
    exit 1
fi

# Every run is logged automatically (console output mirrored to a timestamped
# file) so there's a record of what was uploaded/deleted without relying on
# the caller to pipe through tee.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="$SCRIPT_DIR/logs"
mkdir -p "$LOG_DIR"
LOG_FILE="$LOG_DIR/upload_phantoms_to_gcp_$(date +%Y%m%d_%H%M%S).log"
exec > >(tee -a "$LOG_FILE") 2>&1
echo "=== log: $LOG_FILE ==="

# md5 of a bucket object, as hex (GCS reports base64). Empty if absent.
gcs_md5_hex() {
    local obj="$1" b64
    b64=$(gsutil ls -L "$obj" 2>/dev/null | awk -F': *' '/Hash \(md5\)/{print $2; exit}')
    [[ -z "$b64" ]] && return 0
    printf '%s' "$b64" | base64 -d 2>/dev/null | xxd -p | tr -d '\n'
}

# Verify <archive> is recoverable (bucket md5 match + xz integrity).
# Returns 0 = safe to delete the .raw, 1 = not safe (reason printed).
verify_recoverable() {
    local archive="$1" obj="$2" gcs local
    if [[ ! -f "$archive" ]]; then echo "    [keep] no local archive"; return 1; fi
    gcs=$(gcs_md5_hex "$obj")
    if [[ -z "$gcs" ]]; then echo "    [keep] object not in bucket (or no md5)"; return 1; fi
    local=$(md5sum "$archive" | cut -d' ' -f1)
    if [[ "$gcs" != "$local" ]]; then
        echo "    [keep] md5 MISMATCH (bucket=$gcs local=$local)"; return 1
    fi
    if ! xz -t "$archive" 2>/dev/null; then
        echo "    [keep] xz integrity test FAILED"; return 1
    fi
    return 0
}

total_freed=0
n_up=0; n_del=0; n_skip=0; n_fail=0

for dir in "${dirs[@]}"; do
    if [[ ! -d "$dir" ]]; then
        echo "  [skip] not a directory: $dir" >&2
        continue
    fi

    raws=()
    if [[ -n "$LIST" ]]; then
        while read -r line; do
            line="${line%%#*}"; line="${line//[[:space:]]/}"
            [[ -z "$line" ]] && continue
            [[ "$line" != /* ]] && line="$dir/$line"
            raws+=("$line")
        done < "$LIST"
    else
        shopt -s nullglob
        raws=("$dir"/*.raw)
        shopt -u nullglob
    fi
    if [[ ${#raws[@]} -eq 0 ]]; then
        echo "  [skip] no .raw files selected in: $dir" >&2
        continue
    fi

    echo "=== $dir: ${#raws[@]} .raw file(s)$([[ -n "$LIST" ]] && echo " from $LIST") ==="
    i=0
    for raw in "${raws[@]}"; do
        i=$((i+1))
        rawname="$(basename "$raw")"
        base="${rawname%.raw}"
        archive="${dir}/${base}.tar.xz"
        obj="${GCS_DEST}${base}.tar.xz"
        printf -- "--- [%d/%d] %s\n" "$i" "${#raws[@]}" "$base"

        if [[ $RECLAIM_ONLY -eq 0 ]]; then
            if [[ ! -f "$raw" ]]; then
                echo "    [skip] .raw not found: $raw"; n_skip=$((n_skip+1)); continue
            fi
            if [[ -f "$archive" ]]; then
                echo "    archive exists, reusing"
            elif [[ $DRY -eq 1 ]]; then
                echo "    would compress -> $(basename "$archive")"
            elif ! tar --use-compress-program="$XZ_OPTS" -cf "$archive" -C "$dir" "$rawname"; then
                echo "    !! compression FAILED"; rm -f "$archive"
                n_fail=$((n_fail+1)); continue
            fi
            if [[ $DRY -eq 1 ]]; then
                echo "    would upload -> $GCS_DEST"
            elif gsutil -m cp "$archive" "$GCS_DEST"; then
                echo "    uploaded ($(du -h "$archive" | cut -f1))"; n_up=$((n_up+1))
            else
                echo "    !! upload FAILED"; n_fail=$((n_fail+1)); continue
            fi
        fi

        if [[ $RECLAIM -eq 1 ]]; then
            if [[ ! -f "$raw" ]]; then
                echo "    .raw already removed"; continue
            fi
            if [[ $DRY -eq 1 ]]; then
                if verify_recoverable "$archive" "$obj"; then
                    echo "    would DELETE $rawname ($(du -h "$raw" | cut -f1) reclaimed)"
                fi
                continue
            fi
            if verify_recoverable "$archive" "$obj"; then
                sz=$(stat -c %s "$raw")
                rm -f "$raw" && echo "    verified recoverable -> deleted, freed $((sz/1024/1024)) MiB"
                total_freed=$((total_freed+sz)); n_del=$((n_del+1))
            else
                n_skip=$((n_skip+1))
            fi
        fi
    done
done

echo "=== uploaded $n_up, deleted $n_del, skipped $n_skip, failed $n_fail"
[[ $total_freed -gt 0 ]] && echo "=== reclaimed $((total_freed/1024/1024/1024)) GiB"
echo "=== log saved: $LOG_FILE ==="
exit $(( n_fail > 0 ? 1 : 0 ))
