#!/usr/bin/env python3
"""Stretcher-position augmentation by INPAINTING existing MC-GPU label volumes.

Takes each base "A" volume in /mnt/h/MICHELE_MCGPU/MCGPU_EXPORT (built by
dicom_to_mcgpu from the MCRP tight-crop DICOM exports, stretcher auto-seated
at the base position), zeroes the stretcher labels (10=carbon, 11=foam) and
re-stamps the shell at random positions — no DICOM reload, no resample; the
anatomy voxels are untouched.  Valid because the base shell was stamped only
into air (verified: >=2 mm clearance everywhere), so 10/11 -> 0 restores the
stretcher-free volume bit-exactly, and every new position is checked against
the full Z-projection of tissue before stamping.

Constraints on each sampled (cx, cy):
  - shell fully inside the output volume in XY;
  - shell polygon at least GAP_MM away from *any* tissue voxel in *any* slice
    (tested against the gap-dilated Z-projection of labels 1..4, then
    re-verified voxel-exactly against the 3D volume before stamping).

Per variant (letters B..F): raw is written to a local temp dir, the
positioning-check PNG is rendered, the raw is compressed with the project's
tested command (upload_phantoms_to_gcp.sh):
      tar --use-compress-program='xz -T 32 -9e -M 30G' -cf NAME.tar.xz NAME
the archive is verified by ROUND-TRIP md5 (decompress -> md5 == raw md5),
moved next to the base archives, and only then is the raw deleted.  The .txt
companion and per-template .in files are generated alongside (copy of the
base ones with the raw name swapped).  Uploading to GCP is a separate step.

After a phantom's 5 variants are done, the base "A" .raw is deleted too —
gated on the same round-trip proof against its existing local archive (a
stale archive is refreshed from the current raw first and flagged).

Positioning-check PNGs go to positioning_checks_mcrp_inpaint/ (this batch's
own folder); anything the script flags is ALSO copied to
positioning_checks_mcrp_inpaint_pending/ so only those need eyes.

Usage:
    python3 inpaint_stretcher_variants.py                 # all 12 phantoms
    python3 inpaint_stretcher_variants.py --only MRCP-00M # substring filter
    python3 inpaint_stretcher_variants.py --dry-run       # sample+report only
    python3 inpaint_stretcher_variants.py --validate-port # polygon-port check
Resume-safe: a variant whose archive+png+txt+in already exist is skipped, and
sampling is seeded per (phantom, letter), so a rerun reproduces the same
positions regardless of what was already done.
"""

import argparse
import hashlib
import json
import math
import re
import shutil
import subprocess
import sys
import zlib
from datetime import datetime
from pathlib import Path

import numpy as np

REPO = Path(__file__).resolve().parent
EXPORT_DIR = Path("/mnt/h/MICHELE_MCGPU/MCGPU_EXPORT")
IN_DIR = Path("/mnt/h/MICHELE_MCGPU/MCGPU_IN_FILES")
PNG_DIR = REPO / "positioning_checks_mcrp_inpaint"
PNG_PENDING_DIR = REPO / "positioning_checks_mcrp_inpaint_pending"
GEN_DIR = REPO / "params" / "generated_mcrp_inpaint"
MANIFEST = GEN_DIR / "variants_manifest.json"
EXPORTED_LIST = GEN_DIR / "exported_archives.txt"

VARIANT_LETTERS = ["B", "C", "D", "E", "F"]
BASE_LETTER = "A"
GAP_MM = 2.0          # min shell-to-tissue clearance, same as the base auto-seat
SEED = 20260805
MAX_TRIES = 200

# Tested compression command (upload_phantoms_to_gcp.sh, known-good reference)
XZ_OPTS = "xz -T 32 -9e -M 30G"

# ── Stretcher shell cross-section: exact port of dicom_to_mcgpu.cpp:1409-1445 ─
S_TOP_W, S_BOT_W, S_H = 440.0, 396.0, 57.0
S_STR_H, S_WALL_T, S_CHAM, S_CORNER_R = 20.0, 1.5, 4.0, 5.0
HW_T, HW_B, HT = S_TOP_W / 2.0, S_BOT_W / 2.0, S_H / 2.0
KY = HT - S_STR_H


def sharp_outer():
    c = S_CHAM
    return [(-HW_B, -HT), (HW_B, -HT), (HW_T, KY), (HW_T, HT - c),
            (HW_T - c, HT), (-HW_T + c, HT), (-HW_T, HT - c), (-HW_T, KY)]


def inward_offset(poly, d):
    """Port of dicom_to_mcgpu.cpp inwardOffset()."""
    n = len(poly)
    norms = []
    for i in range(n):
        dx = poly[(i + 1) % n][0] - poly[i][0]
        dy = poly[(i + 1) % n][1] - poly[i][1]
        L = math.hypot(dx, dy)
        norms.append((-dy / L, dx / L))
    inner = []
    for i in range(n):
        prev = (i + n - 1) % n
        p1x = poly[prev][0] + d * norms[prev][0]
        p1y = poly[prev][1] + d * norms[prev][1]
        d1x = poly[i][0] - poly[prev][0]
        d1y = poly[i][1] - poly[prev][1]
        p2x = poly[i][0] + d * norms[i][0]
        p2y = poly[i][1] + d * norms[i][1]
        d2x = poly[(i + 1) % n][0] - poly[i][0]
        d2y = poly[(i + 1) % n][1] - poly[i][1]
        det = d1x * (-d2y) - (-d2x) * d1y
        if abs(det) < 1e-10:
            inner.append((p2x, p2y))
            continue
        bx, by = p2x - p1x, p2y - p1y
        t = (bx * (-d2y) - by * (-d2x)) / det
        inner.append((p1x + t * d1x, p1y + t * d1y))
    return inner


def round_polygon(poly, r, n_arc=20):
    """Port of dicom_to_mcgpu.cpp roundPolygon()."""
    if r < 1e-9:
        return list(poly)
    n = len(poly)
    result = []
    for i in range(n):
        p0, p1, p2 = poly[(i + n - 1) % n], poly[i], poly[(i + 1) % n]
        dxi, dyi = p1[0] - p0[0], p1[1] - p0[1]
        Li = math.hypot(dxi, dyi)
        uxi, uyi = dxi / Li, dyi / Li
        dxo, dyo = p2[0] - p1[0], p2[1] - p1[1]
        Lo = math.hypot(dxo, dyo)
        uxo, uyo = dxo / Lo, dyo / Lo
        cos_a = max(-1.0, min(1.0, uxi * uxo + uyi * uyo))
        half_ext = math.acos(cos_a) / 2.0
        if half_ext < 1e-9:
            result.append(p1)
            continue
        d = r * math.tan(half_ext)
        tp1x, tp1y = p1[0] - d * uxi, p1[1] - d * uyi
        tp2x, tp2y = p1[0] + d * uxo, p1[1] + d * uyo
        px, py = -uyi, uxi
        if uxi * uyo - uyi * uxo < 0:
            px, py = -px, -py
        cx, cy = tp1x + r * px, tp1y + r * py
        a1 = math.atan2(tp1y - cy, tp1x - cx)
        a2 = math.atan2(tp2y - cy, tp2x - cx)
        if a2 < a1:
            a2 += 2.0 * math.pi
        r_arc = math.hypot(tp1x - cx, tp1y - cy)
        for j in range(n_arc):
            t = a1 + (a2 - a1) * j / (n_arc - 1)
            result.append((cx + r_arc * math.cos(t), cy + r_arc * math.sin(t)))
    return result


OUTER_POLY = round_polygon(sharp_outer(), S_CORNER_R)
INNER_POLY = round_polygon(inward_offset(sharp_outer(), S_WALL_T),
                           max(S_CORNER_R - S_WALL_T, 0.0))


def in_convex(poly, X, Y):
    """Vectorized port of dicom_to_mcgpu.cpp inConvex() (CCW, >= 0 = inside)."""
    inside = np.ones(X.shape, dtype=bool)
    n = len(poly)
    for i in range(n):
        ax, ay = poly[i]
        bx, by = poly[(i + 1) % n]
        ex, ey = bx - ax, by - ay
        inside &= ((X - ax) * (-ey) + (Y - ay) * ex) >= 0.0
        if not inside.any():
            break
    return inside


def shell_mask(nx, ny, vxy, cx, cy):
    """2D mask (ny, nx) with 0/10/11, same voxel-centre convention as the C++."""
    mask = np.zeros((ny, nx), dtype=np.uint8)
    xs = (np.arange(nx) + 0.5) * vxy - nx * vxy / 2.0    # voxel centre x
    ys = (np.arange(ny) + 0.5) * vxy - ny * vxy / 2.0    # voxel centre y
    dx = xs - cx                                          # (nx,)
    dy = cy - ys                                          # (ny,)  NB: cy - y
    ii = np.where((dx >= -HW_T) & (dx <= HW_T))[0]
    jj = np.where((dy >= -HT) & (dy <= HT))[0]
    if len(ii) == 0 or len(jj) == 0:
        return mask
    DX, DY = np.meshgrid(dx[ii], dy[jj])                  # (nj, ni)
    outer = in_convex(OUTER_POLY, DX, DY)
    inner = in_convex(INNER_POLY, DX, DY)
    sub = np.zeros(DX.shape, dtype=np.uint8)
    sub[outer] = 10
    sub[outer & inner] = 11
    mask[np.ix_(jj, ii)] = sub
    return mask


def dilate_disk(mask2d, r_vox):
    """Binary dilation with a circular structuring element (no scipy needed)."""
    out = mask2d.copy()
    for dj in range(-r_vox, r_vox + 1):
        for di in range(-r_vox, r_vox + 1):
            if di == 0 and dj == 0:
                continue
            if di * di + dj * dj > r_vox * r_vox:
                continue
            shifted = np.roll(mask2d, (dj, di), axis=(0, 1))
            # kill wraparound
            if dj > 0:
                shifted[:dj, :] = False
            elif dj < 0:
                shifted[dj:, :] = False
            if di > 0:
                shifted[:, :di] = False
            elif di < 0:
                shifted[:, di:] = False
            out |= shifted
    return out


# ── small helpers ────────────────────────────────────────────────────────────

def md5_of_array(arr):
    h = hashlib.md5()
    mv = memoryview(arr.reshape(-1))
    step = 64 << 20
    for off in range(0, len(mv), step):
        h.update(mv[off:off + step])
    return h.hexdigest()


def md5_of_file(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(64 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def compress_raw(raw_path: Path, archive_path: Path):
    """The tested compression command, byte-for-byte the upload script's."""
    subprocess.run(
        ["tar", f"--use-compress-program={XZ_OPTS}", "-cf", str(archive_path),
         "-C", str(raw_path.parent), raw_path.name],
        check=True)


def roundtrip_md5(archive_path: Path, member: str):
    """md5 of the raw as reconstructed from the archive (proof of recoverability)."""
    r = subprocess.run(
        ["bash", "-o", "pipefail", "-c",
         'xz -dc -T0 -- "$0" | tar -xO -f - -- "$1" | md5sum',
         str(archive_path), member],
        capture_output=True, text=True, check=True)
    return r.stdout.split()[0]


def extract_raw(archive_path: Path, dest_dir: Path):
    subprocess.run(
        ["bash", "-o", "pipefail", "-c",
         'xz -dc -T0 -- "$0" | tar -x -f - -C "$1"',
         str(archive_path), str(dest_dir)],
        check=True)


def render_png(in_path: Path, raw_path: Path, png_path: Path):
    sys.path.insert(0, str(REPO))
    from expand_in_kv import parse_in_geometry, render_positioning_check
    geom = parse_in_geometry(in_path)
    render_positioning_check(in_path, raw_path, png_path, geom, {})


def load_manifest():
    if MANIFEST.exists():
        return json.loads(MANIFEST.read_text())
    return {"seed": SEED, "gap_mm": GAP_MM, "xz_opts": XZ_OPTS,
            "created": datetime.now().isoformat(timespec="seconds"),
            "phantoms": {}}


def save_manifest(man):
    GEN_DIR.mkdir(parents=True, exist_ok=True)
    MANIFEST.write_text(json.dumps(man, indent=1) + "\n")


def append_exported(tag, path):
    GEN_DIR.mkdir(parents=True, exist_ok=True)
    lines = []
    if EXPORTED_LIST.exists():
        lines = EXPORTED_LIST.read_text().splitlines()
    else:
        lines = ["# inpainted stretcher-variant archives\t<phantom [letter]>\t<archive>"]
    entry = f"{tag}\t{path}"
    if entry not in lines:
        lines.append(entry)
        EXPORTED_LIST.write_text("\n".join(lines) + "\n")


# ── per-phantom driver ───────────────────────────────────────────────────────

BASE_RE = re.compile(r"^(?P<case>.+)_A_(?P<nx>\d+)x(?P<ny>\d+)x(?P<nz>\d+)byte\.raw$")


def find_bases():
    """Base cases from raws present AND from archives (raw may be reclaimed)."""
    found = {}
    for p in sorted(EXPORT_DIR.iterdir()):
        name = p.name
        if name.endswith(".tar.xz"):
            name = name[:-len(".tar.xz")] + ".raw"
        m = BASE_RE.match(name)
        if m:
            found[m.group("case")] = (int(m.group("nx")), int(m.group("ny")),
                                      int(m.group("nz")))
    return found


def variant_paths(case, letter, dims):
    nx, ny, nz = dims
    stem = f"{case}_{letter}_{nx}x{ny}x{nz}byte"
    return {
        "stem": stem,
        "raw_name": stem + ".raw",
        "archive": EXPORT_DIR / (stem + ".tar.xz"),
        "txt": EXPORT_DIR / (stem + ".txt"),
        "png": PNG_DIR / (stem + ".png"),
    }


def variant_done(case, letter, dims):
    vp = variant_paths(case, letter, dims)
    ins = list(IN_DIR.glob(f"{case}_{letter}_CBCT_*.in"))
    return vp["archive"].exists() and vp["txt"].exists() and vp["png"].exists() \
        and len(ins) > 0


def flag(man, case, msg, png=None):
    print(f"    [FLAG] {case}: {msg}")
    man["phantoms"].setdefault(case, {}).setdefault("flags", []).append(msg)
    if png is not None and Path(png).exists():
        PNG_PENDING_DIR.mkdir(parents=True, exist_ok=True)
        shutil.copy2(png, PNG_PENDING_DIR / Path(png).name)


def process_phantom(case, dims, tmp_dir, man, dry_run=False):
    nx, ny, nz = dims
    vxy = 0.3
    half_x, half_y = nx * vxy / 2.0, ny * vxy / 2.0
    cx_lim = half_x - HW_T - vxy          # shell fully inside in X
    cy_max = half_y - HT - vxy            # shell fully inside in +Y
    gap_vox = math.ceil(GAP_MM / vxy)

    base_stem = f"{case}_{BASE_LETTER}_{nx}x{ny}x{nz}byte"
    base_raw = EXPORT_DIR / (base_stem + ".raw")
    base_archive = EXPORT_DIR / (base_stem + ".tar.xz")
    base_txt = EXPORT_DIR / (base_stem + ".txt")
    base_ins = sorted(IN_DIR.glob(f"{case}_{BASE_LETTER}_CBCT_*.in"))

    todo = [L for L in VARIANT_LETTERS if not variant_done(case, L, dims)]
    if not todo and not base_raw.exists():
        print(f"  {case}: all variants done, base raw already reclaimed — skip")
        return
    if not base_txt.exists() or not base_ins:
        flag(man, case, "missing base .txt or .in files — skipped")
        return

    rec = man["phantoms"].setdefault(case, {})
    rec.setdefault("variants", {})

    # ── source volume: the base raw, or restore it from its archive ──────────
    restored = None
    src_raw = base_raw
    if not base_raw.exists():
        print(f"  {case}: base raw absent, restoring from {base_archive.name}")
        extract_raw(base_archive, tmp_dir)
        restored = tmp_dir / (base_stem + ".raw")
        src_raw = restored

    print(f"  {case}: loading {src_raw.name} ({nx}x{ny}x{nz})")
    vol = np.fromfile(src_raw, dtype=np.uint8)
    if vol.size != nx * ny * nz:
        flag(man, case, f"size mismatch: {vol.size} != {nx*ny*nz} — skipped")
        return
    vol = vol.reshape(nz, ny, nx)
    base_md5 = md5_of_array(vol)

    # zero the existing shell (base stamped only into air -> exact restore)
    n_shell = int(((vol == 10) | (vol == 11)).sum())
    vol[(vol == 10) | (vol == 11)] = 0
    print(f"    zeroed {n_shell} base shell voxels")

    # tissue Z-projection (labels 1..4), then dilate by the required gap
    proj = np.zeros((ny, nx), dtype=bool)
    for k in range(nz):
        sl = vol[k]
        proj |= (sl >= 1) & (sl <= 4)
    dproj = dilate_disk(proj, gap_vox)
    ys = (np.arange(ny) + 0.5) * vxy - half_y
    xs = (np.arange(nx) + 0.5) * vxy - half_x

    base_txt_text = base_txt.read_text()
    pending_compress = []

    for letter in VARIANT_LETTERS:
        if letter not in todo:
            print(f"    {letter}: already done — skip")
            continue
        rng = np.random.default_rng([SEED, zlib.crc32(case.encode()), ord(letter)])
        chosen = None
        for attempt in range(1, MAX_TRIES + 1):
            cx = float(rng.uniform(-cx_lim, cx_lim))
            cols = np.where(np.abs(xs - cx) <= HW_T + vxy)[0]
            rows = np.where(dproj[:, cols].any(axis=1))[0]
            cy_min = (float(ys[rows.max()]) if len(rows) else -half_y) + HT + vxy / 2.0
            if cy_min >= cy_max:
                continue
            cy = float(rng.uniform(cy_min, cy_max))
            mask = shell_mask(nx, ny, vxy, cx, cy)
            m = mask > 0
            if (m & dproj).any():
                continue
            # voxel-exact guarantee: every voxel the shell will occupy is air
            if vol[:, m].any():
                continue
            chosen = (cx, cy, cy_min, attempt, mask, m)
            break
        if chosen is None:
            flag(man, case, f"variant {letter}: no legal position in {MAX_TRIES} tries")
            continue
        cx, cy, cy_min, attempt, mask, m = chosen
        n_c = int((mask == 10).sum()) * nz
        n_f = int((mask == 11).sum()) * nz
        print(f"    {letter}: cx={cx:+8.2f} mm  cy={cy:7.2f} mm  "
              f"(cy range [{cy_min:.2f}, {cy_max:.2f}], try {attempt})  "
              f"carbon {n_c} + foam {n_f} vox")
        rec["variants"][letter] = {
            "cx_mm": round(cx, 3), "cy_mm": round(cy, 3),
            "cy_min_mm": round(cy_min, 3), "cy_max_mm": round(cy_max, 3),
            "tries": attempt, "carbon_vox": n_c, "foam_vox": n_f,
        }
        if dry_run:
            continue

        vp = variant_paths(case, letter, dims)
        # stamp -> write raw to local tmp -> unstamp
        vol[:, mask == 10] = 10
        vol[:, mask == 11] = 11
        tmp_raw = tmp_dir / vp["raw_name"]
        vol.tofile(tmp_raw)
        variant_md5 = md5_of_array(vol)
        vol[:, m] = 0

        # .txt companion + per-template .in files (base copies, name swapped)
        vp["txt"].write_text(
            base_txt_text.replace(base_stem + ".raw", vp["stem"] + ".raw"))
        first_in = None
        for bin_path in base_ins:
            out_in = IN_DIR / bin_path.name.replace(
                f"_{BASE_LETTER}_CBCT_", f"_{letter}_CBCT_")
            out_in.write_text(
                bin_path.read_text().replace(base_stem + ".raw",
                                             vp["stem"] + ".raw"))
            if first_in is None:
                first_in = out_in

        # positioning check PNG
        PNG_DIR.mkdir(parents=True, exist_ok=True)
        render_png(first_in, tmp_raw, vp["png"])
        print(f"      png  : {vp['png'].name}")
        pending_compress.append((letter, vp, tmp_raw, variant_md5))

    # release the 3.6 GB volume BEFORE xz runs: 'xz -T 32 -9e' alone wants
    # ~21 GB and this machine has 27 GB total — they must not coexist.
    del vol

    # ── compress (tested command) -> round-trip verify -> move -> delete raw ─
    for letter, vp, tmp_raw, variant_md5 in pending_compress:
        tmp_archive = tmp_dir / (vp["stem"] + ".tar.xz")
        compress_raw(tmp_raw, tmp_archive)
        rt = roundtrip_md5(tmp_archive, vp["raw_name"])
        if rt != variant_md5:
            flag(man, case,
                 f"variant {letter}: round-trip md5 MISMATCH — raw kept in tmp",
                 vp["png"])
            save_manifest(man)
            continue
        shutil.move(str(tmp_archive), vp["archive"])
        tmp_raw.unlink()
        rec["variants"][letter]["archive"] = vp["archive"].name
        rec["variants"][letter]["raw_md5"] = variant_md5
        append_exported(f"{case} [{letter}]", vp["archive"])
        print(f"    {letter}: archive {vp['archive'].name} "
              f"({vp['archive'].stat().st_size // 1024} KiB), raw deleted")
        save_manifest(man)

    # ── reclaim the base "A" raw (gated on round-trip proof) ─────────────────
    if dry_run or not base_raw.exists():
        if restored:
            restored.unlink(missing_ok=True)
        return
    remaining = [L for L in VARIANT_LETTERS if not variant_done(case, L, dims)]
    if remaining:
        flag(man, case, f"variants {remaining} incomplete — base raw kept")
        return
    if base_archive.exists():
        rt = roundtrip_md5(base_archive, base_stem + ".raw")
        if rt != base_md5:
            stale = base_archive.with_suffix(".xz.stale")
            shutil.move(str(base_archive), stale)
            flag(man, case,
                 f"base archive was STALE (kept as {stale.name}); rebuilt from "
                 f"current raw — bucket copy outdated until next upload")
            compress_raw(base_raw, base_archive)
            rt = roundtrip_md5(base_archive, base_stem + ".raw")
    else:
        compress_raw(base_raw, base_archive)
        rt = roundtrip_md5(base_archive, base_stem + ".raw")
    if rt == base_md5:
        base_raw.unlink()
        rec["base_raw_deleted"] = True
        print(f"    base raw verified recoverable -> deleted "
              f"({base_archive.name})")
    else:
        flag(man, case, "base round-trip md5 mismatch — base raw KEPT")
    save_manifest(man)


# ── polygon-port validation against a real base volume ──────────────────────

def validate_port(tmp_dir):
    """Fit (cx, cy) of the base shell in one slice of a base volume and report
    voxel agreement between the C++-stamped shell and this port's mask."""
    bases = find_bases()
    case, dims = next(iter(bases.items()))
    nx, ny, nz = dims
    base_stem = f"{case}_{BASE_LETTER}_{nx}x{ny}x{nz}byte"
    raw = EXPORT_DIR / (base_stem + ".raw")
    if not raw.exists():
        print("no base raw on disk to validate against")
        return
    k = nz // 2
    with open(raw, "rb") as f:
        f.seek(k * ny * nx)
        sl = np.frombuffer(f.read(ny * nx), dtype=np.uint8).reshape(ny, nx)
    ref = np.zeros_like(sl)
    ref[sl == 10] = 10
    ref[sl == 11] = 11
    js, iis = np.nonzero(ref > 0)
    vxy = 0.3
    cy0 = ((js.min() + js.max()) / 2.0 + 0.5) * vxy - ny * vxy / 2.0
    cx0 = ((iis.min() + iis.max()) / 2.0 + 0.5) * vxy - nx * vxy / 2.0
    best = (0.0, None, None)
    for dcy in np.arange(-0.6, 0.6001, 0.05):
        for dcx in np.arange(-0.6, 0.6001, 0.05):
            mk = shell_mask(nx, ny, vxy, cx0 + dcx, cy0 + dcy)
            inter = int(((mk > 0) & (ref > 0)).sum())
            union = int(((mk > 0) | (ref > 0)).sum())
            iou = inter / union
            if iou > best[0]:
                best = (iou, cx0 + dcx, cy0 + dcy, mk)
    iou, cx, cy, mk = best
    exact = int((mk == ref).sum()) / ref.size
    diff = int((mk != ref).sum())
    print(f"{case} slice k={k}: best fit cx={cx:.2f} cy={cy:.2f}  "
          f"shell-IoU={iou:.4f}  differing voxels={diff} "
          f"(labels 10/11 vs port, {100*exact:.4f}% of slice equal)")
    print("PASS" if iou > 0.99 else "FAIL — polygon port does not match")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--only", action="append", default=[],
                    help="substring filter on phantom name (repeatable)")
    ap.add_argument("--dry-run", action="store_true",
                    help="sample and report positions; write nothing")
    ap.add_argument("--validate-port", action="store_true",
                    help="check the polygon port against a real base volume")
    ap.add_argument("--tmp-dir", default="/tmp/mcrp_inpaint_tmp",
                    help="local fast dir for temporary raws (needs ~8 GB)")
    args = ap.parse_args()

    tmp_dir = Path(args.tmp_dir)
    tmp_dir.mkdir(parents=True, exist_ok=True)

    if args.validate_port:
        validate_port(tmp_dir)
        return

    bases = find_bases()
    if args.only:
        bases = {c: d for c, d in bases.items()
                 if any(s in c for s in args.only)}
    if not bases:
        print("no base volumes matched")
        sys.exit(1)

    PNG_DIR.mkdir(parents=True, exist_ok=True)
    PNG_PENDING_DIR.mkdir(parents=True, exist_ok=True)
    man = load_manifest()
    print(f"{len(bases)} phantom(s), variants {''.join(VARIANT_LETTERS)}, "
          f"seed {SEED}, gap {GAP_MM} mm"
          + (" [DRY RUN]" if args.dry_run else ""))
    for case, dims in bases.items():
        process_phantom(case, dims, tmp_dir, man, dry_run=args.dry_run)
        save_manifest(man)

    flagged = {c: r["flags"] for c, r in man["phantoms"].items() if r.get("flags")}
    print("\n=== done ===")
    if flagged:
        print(f"FLAGGED (see {PNG_PENDING_DIR.name}/):")
        for c, fl in flagged.items():
            for f_ in fl:
                print(f"  {c}: {f_}")
    else:
        print("no flags — pending folder is empty")


if __name__ == "__main__":
    main()
