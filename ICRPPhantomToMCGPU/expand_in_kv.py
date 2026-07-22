#!/usr/bin/env python3
"""
expand_in_kv.py -- collect the .in files produced by the batcher and emit a
flat, numbered series of MC-GPU .in files, doubled into 80 kV and 120 kV
variants.

Naming convention (one number per patient+stretcher, a letter per template):

  <N><letter>_<patient>_<label>_<kind>_<kV>.in

    N       running number, one per (patient, stretcher position), from --start
    letter  template kind:  head_bar -> a, head_only -> b, bar_only -> c
    label   stretcher position (A/B/C)
    kV      80kV then 120kV (both share the same N+letter)

e.g. for one patient+stretcher:
    11a_<name>_head_bar_80kV.in     11a_<name>_head_bar_120kV.in
    11b_<name>_head_only_80kV.in    11b_<name>_head_only_120kV.in
    11c_<name>_bar_only_80kV.in     11c_<name>_bar_only_120kV.in

Only the SECTION SOURCE spectrum lines change (which of the three .spc lines is
uncommented); everything else is copied verbatim, except OUTPUT IMAGE FILE NAME
which is set to "results/<output-stem>" so every run writes a distinct image.

It can also render, for each .in file, a central-sagittal positioning-check
PNG: the label geometry (phantom + stretcher) with the CBCT beam at projection
angle 0 overlaid (source point + the two rays to the top/bottom detector edges).
One PNG per .in, named after the .in, dropped in a folder to scroll in a viewer.

Usage
-----
  # expand only
  python3 expand_in_kv.py [SCAN_ROOT] [-o OUT_DIR] [--start N] [--dry-run]

  # expand, then render a positioning-check PNG per generated .in
  python3 expand_in_kv.py --render-checks --raw-root /path/to/raws

  # only render checks from an existing folder of .in files
  python3 expand_in_kv.py --checks-only --in-dir mcgpu_in_batch \
      --raw-root /path/to/raws --png-dir positioning_checks

Defaults: SCAN_ROOT=/home/colle/GradientHealthExport/export
          OUT_DIR=mcgpu_in_batch   START=11   PNG_DIR=positioning_checks
"""

import argparse
import re
from collections import defaultdict
from pathlib import Path

DEFAULT_SCAN_ROOT = "/home/colle/GradientHealthExport/export"
DEFAULT_OUT_DIR = "mcgpu_in_batch"
DEFAULT_START = 11
DEFAULT_PNG_DIR = "positioning_checks"

# tokens stripped from the template name when building <kind>
REDUNDANT_TOKENS = {"cbct", "template"}

# template kind -> letter (also defines the a/b/c emission order)
KIND_LETTER = {"head_bar": "a", "head_only": "b", "bar_only": "c"}

# kV variants in emission order: (name, substring that stays ACTIVE)
KV_VARIANTS = [("80kV", "80kV"), ("120kV", "120kV")]


def is_spectrum_line(line: str) -> bool:
    s = line.lstrip().lstrip("#").lstrip()
    return s.startswith("spectrum/") and ".spc" in s


def bare(line: str) -> str:
    """Strip leading '#'(s)/whitespace and the trailing newline."""
    s = line.rstrip("\n").lstrip()
    while s.startswith("#"):
        s = s[1:].lstrip()
    return s


def set_spectrum(lines, active_key: str):
    """Comment every spectrum line except the one containing active_key."""
    out = []
    for ln in lines:
        if is_spectrum_line(ln):
            content = bare(ln)
            out.append(("" if active_key in content else "#") + content + "\n")
        else:
            out.append(ln)
    return out


def set_output_name(lines, stem: str):
    out = []
    for ln in lines:
        if "OUTPUT IMAGE FILE NAME" in ln:
            out.append(f"results/{stem}   # OUTPUT IMAGE FILE NAME\n")
        else:
            out.append(ln)
    return out


def parse_meta(in_path: Path, scan_root: Path):
    """Extract (patient, label, kind) from the .in file path."""
    rel = in_path.relative_to(scan_root)
    patient = rel.parts[0]

    folder = in_path.parent.name           # e.g. MCGPU_A_vox_2134x2134x834
    m = re.search(r"_([A-Za-z0-9]+)_vox_", folder)
    label = m.group(1) if m else folder

    stem = in_path.stem                    # e.g. CBCT_cbct_head_only_template
    if stem.upper().startswith("CBCT_"):
        stem = stem[5:]
    toks = [t for t in stem.split("_") if t.lower() not in REDUNDANT_TOKENS]
    kind = "_".join(toks) if toks else stem
    return patient, label, kind


# ── Positioning-check rendering ─────────────────────────────────────────────
# For each .in we draw the central SAGITTAL slice (X = Nx/2 plane) of the linked
# label .raw and overlay the CBCT beam at projection angle 0 (assumed aligned
# with this sagittal plane, per the task spec): the source point and the two
# rays that reach the TOP and BOTTOM edges of the detector.  Purpose: a manual
# check that the phantom + stretcher sit correctly inside the beam / detector.
#
# Coordinate frame (same as dicom_to_mcgpu.cpp / MC-GPU): origin = isocenter =
# volume centre; the .in VOXEL GEOMETRY offset is the low corner of the grid, so
# voxel [i,j,k] centre = offset + (idx+0.5)*voxelsize.  Raw byte order is
# [k=Z][j=Y][i=X] (X fastest), so the central sagittal slice is raw[:, :, Nx//2]
# with shape (Nz, Ny) in [Z, Y].  The beam travels along +Y; its height fan is
# in Z, which lies in this sagittal plane.

# label -> RGB (0..1); matches the dicom_to_mcgpu.cpp label scheme.
_LABEL_RGB = {
    0:  (0.06, 0.06, 0.09),   # air (background)
    1:  (0.45, 0.33, 0.22),   # fat
    2:  (0.83, 0.55, 0.52),   # soft tissue
    3:  (0.80, 0.68, 0.38),   # bone spongiosa
    4:  (0.97, 0.95, 0.88),   # bone cortical
    5:  (0.95, 0.12, 0.12),   # implant
    10: (0.20, 0.82, 0.85),   # stretcher carbon
    11: (0.16, 0.42, 0.58),   # stretcher foam
}
_LABEL_NAME = {0: "air", 1: "fat", 2: "soft", 3: "spongiosa", 4: "cortical",
               5: "implant", 10: "carbon", 11: "foam"}
_UNKNOWN_RGB = (0.85, 0.0, 0.85)   # magenta = label with no assigned colour


def _in_section_values(lines, marker, count):
    """First `count` non-comment value strings after the line containing `marker`.
    A value string is the line content with any trailing '# comment' stripped;
    blank / fully-commented lines are skipped so commented-out spectrum lines
    never shift the positional indexing."""
    vals, grabbing = [], False
    for ln in lines:
        if not grabbing:
            if marker in ln:
                grabbing = True
            continue
        body = ln.split("#", 1)[0].strip()
        if body:
            vals.append(body)
            if len(vals) >= count:
                break
    return vals


def parse_in_geometry(in_path):
    """Parse the geometry an MC-GPU .in needs for the positioning plot.
    Returns a dict; raises ValueError if a required section is missing."""
    lines = Path(in_path).read_text().splitlines()
    src = _in_section_values(lines, "SECTION SOURCE", 3)
    det = _in_section_values(lines, "SECTION IMAGE DETECTOR", 4)
    vox = _in_section_values(lines, "SECTION VOXELIZED GEOMETRY", 4)
    if len(src) < 3 or len(det) < 4 or len(vox) < 4:
        raise ValueError("could not locate SOURCE / DETECTOR / VOXEL sections")
    return dict(
        src_pos=[float(v) for v in src[1].split()[:3]],   # X Y Z [cm]
        src_dir=[float(v) for v in src[2].split()[:3]],    # U V W
        det_size=[float(v) for v in det[2].split()[:2]],   # Dx Dz [cm]
        sdd=float(det[3].split()[0]),                      # source-to-detector [cm]
        raw_name=Path(vox[0].split()[0]).name,             # phantom/<name>.raw
        offset=[float(v) for v in vox[1].split()[:3]],     # grid low corner [cm]
        nvox=[int(float(v)) for v in vox[2].split()[:3]],  # Nx Ny Nz
        vsize=[float(v) for v in vox[3].split()[:3]],      # [cm]
    )


def _load_central_sagittal(raw_path, nvox, cache=None):
    """Memmap the label .raw and return its central sagittal slice (X=Nx//2)
    as a (Nz, Ny) array in [Z, Y] order.  Cached by raw path, since every
    kind/kV of one patient+stretcher shares the same volume."""
    import numpy as np
    key = str(raw_path)
    if cache is not None and key in cache:
        return cache[key]
    nx, ny, nz = nvox
    expect = nx * ny * nz
    size = Path(raw_path).stat().st_size
    if size == expect:
        dtype = np.uint8
    elif size == expect * 2:
        dtype = np.int16
    else:
        raise ValueError(f"{Path(raw_path).name}: size {size} B != {nx}x{ny}x{nz} "
                         f"(uint8={expect}, int16={expect * 2})")
    mm = np.memmap(raw_path, dtype=dtype, mode="r", shape=(nz, ny, nx))
    sl = np.ascontiguousarray(mm[:, :, nx // 2])   # (Nz, Ny) = [Z, Y]
    del mm
    if cache is not None:
        cache[key] = sl
    return sl


def render_positioning_check(in_path, raw_path, out_png, geom=None, slice_cache=None):
    """Render one central-sagittal positioning-check PNG for `in_path`."""
    import numpy as np
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.lines import Line2D
    from matplotlib.patches import Patch, Rectangle

    in_path = Path(in_path)
    if geom is None:
        geom = parse_in_geometry(in_path)
    nx, ny, nz = geom["nvox"]
    vx, vy, vz = geom["vsize"]
    ox, oy, oz = geom["offset"]

    sl = _load_central_sagittal(raw_path, geom["nvox"], slice_cache)  # [Z, Y]

    # label field -> RGB image
    rgb = np.empty((nz, ny, 3), dtype=float)
    labels_present = []
    for lab in np.unique(sl):
        lab = int(lab)
        rgb[sl == lab] = _LABEL_RGB.get(lab, _UNKNOWN_RGB)
        labels_present.append(lab)

    # physical extents [cm]: Y = columns (j), Z = rows (k)
    y_min, y_max = oy, oy + ny * vy
    z_min, z_max = oz, oz + nz * vz

    # beam at projection angle 0, in the sagittal (Y, Z) plane
    sy, sz = geom["src_pos"][1], geom["src_pos"][2]      # source Y, Z
    det_y = sy + geom["sdd"] * geom["src_dir"][1]        # detector plane Y
    dz_half = geom["det_size"][1] / 2.0                  # half detector height (Z)
    z_top, z_bot = sz + dz_half, sz - dz_half

    x_lo, x_hi = min(sy, y_min), max(det_y, y_max)
    z_lo, z_hi = min(z_min, z_bot), max(z_max, z_top)
    xr, zr = (x_hi - x_lo) or 1.0, (z_hi - z_lo) or 1.0

    fw = 13.0
    fh = max(3.6, fw * (zr / xr) + 1.7)
    fig, ax = plt.subplots(figsize=(fw, fh))
    ax.set_facecolor("#0a0a12")

    ax.imshow(rgb, origin="lower", extent=[y_min, y_max, z_min, z_max],
              aspect="equal", interpolation="nearest", zorder=1)
    ax.add_patch(Rectangle((y_min, z_min), y_max - y_min, z_max - z_min,
                 fill=False, ec="white", lw=0.6, ls=":", alpha=0.5, zorder=2))

    # beam edges (source -> top/bottom detector edge), detector, source, isocenter
    ax.plot([sy, det_y], [sz, z_top], color="#ffcc00", lw=1.4, zorder=4)
    ax.plot([sy, det_y], [sz, z_bot], color="#ffcc00", lw=1.4, zorder=4)
    ax.plot([det_y, det_y], [z_bot, z_top], color="#33e0ff", lw=2.4, zorder=4)
    ax.plot([sy], [sz], marker="*", color="red", ms=15, zorder=5)
    ax.axhline(0, color="white", lw=0.5, alpha=0.25, zorder=3)
    ax.axvline(0, color="white", lw=0.5, alpha=0.25, zorder=3)

    ax.set_xlim(x_lo - 0.03 * xr, x_hi + 0.03 * xr)
    ax.set_ylim(z_lo - 0.06 * zr, z_hi + 0.06 * zr)
    ax.set_xlabel("Y — beam axis  [cm]   (source ✱  →  detector)")
    ax.set_ylabel("Z — cranial–caudal  [cm]")
    ax.set_title(f"{in_path.stem}\ncentral sagittal (X=0)  ·  CBCT projection angle 0",
                 fontsize=10)

    handles = [Patch(fc=_LABEL_RGB.get(l, _UNKNOWN_RGB),
                     label=_LABEL_NAME.get(l, f"label {l}"))
               for l in labels_present if l != 0]
    handles += [Line2D([0], [0], color="#ffcc00", lw=1.4, label="beam edge"),
                Line2D([0], [0], color="#33e0ff", lw=2.4, label="detector"),
                Line2D([0], [0], color="red", marker="*", ls="", ms=11, label="source")]
    ax.legend(handles=handles, loc="upper left", fontsize=7, ncol=2,
              framealpha=0.6, facecolor="#202028", labelcolor="white")

    fig.savefig(out_png, dpi=140, bbox_inches="tight", facecolor="#0a0a12")
    plt.close(fig)


def render_all_checks(in_dir, raw_roots, png_dir):
    """Render a positioning-check PNG for every .in in `in_dir` whose linked
    .raw is found under any of `raw_roots`.  Returns (made, missing, errors)."""
    in_dir = Path(in_dir)
    ins = sorted(in_dir.glob("*.in"))
    if not ins:
        raise SystemExit(f"No .in files in {in_dir}")

    raw_index = {}   # basename -> path (first match wins)
    for root in raw_roots:
        root = Path(root)
        if not root.is_dir():
            print(f"  [warn] raw-root not found, skipped: {root}")
            continue
        for r in root.rglob("*.raw"):
            raw_index.setdefault(r.name, r)
    print(f"Indexed {len(raw_index)} .raw file(s) from {len(raw_roots)} root(s).")

    png_dir = Path(png_dir)
    png_dir.mkdir(parents=True, exist_ok=True)

    slice_cache = {}
    made, missing, errors = [], [], []
    for in_path in ins:
        try:
            geom = parse_in_geometry(in_path)
        except Exception as e:                       # noqa: BLE001
            errors.append((in_path.name, f"parse: {e}"))
            continue
        raw = raw_index.get(geom["raw_name"])
        if raw is None:
            missing.append((in_path.name, geom["raw_name"]))
            continue
        out_png = png_dir / (in_path.stem + ".png")
        try:
            render_positioning_check(in_path, raw, out_png, geom, slice_cache)
            made.append(out_png)
            print(f"  ok {out_png.name}")
        except Exception as e:                       # noqa: BLE001
            errors.append((in_path.name, str(e)))
            print(f"  !! {in_path.name}: {e}")

    print(f"\nRendered {len(made)} PNG(s) -> {png_dir}")
    if missing:
        print(f"{len(missing)} .in skipped (linked .raw not found under raw-root):")
        for n, b in missing[:20]:
            print(f"   {n}: {b}")
        if len(missing) > 20:
            print(f"   ... and {len(missing) - 20} more")
    if errors:
        print(f"{len(errors)} error(s):")
        for n, e in errors:
            print(f"   {n}: {e}")
    return made, missing, errors


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("scan_root", nargs="?", default=DEFAULT_SCAN_ROOT,
                    help="folder to scan for CBCT_*.in (default: %(default)s)")
    ap.add_argument("-o", "--out-dir", default=DEFAULT_OUT_DIR,
                    help="output folder for the numbered .in files (default: %(default)s)")
    ap.add_argument("--start", type=int, default=DEFAULT_START,
                    help="first index number (default: %(default)s)")
    ap.add_argument("--dry-run", action="store_true",
                    help="list what would be written without writing")
    ap.add_argument("--render-checks", action="store_true",
                    help="after expanding, render one sagittal positioning-check "
                         "PNG per generated .in (needs the linked .raw on disk)")
    ap.add_argument("--checks-only", action="store_true",
                    help="skip kV expansion; only render positioning-check PNGs "
                         "from --in-dir")
    ap.add_argument("--in-dir", default=None,
                    help="folder of .in files to render (default: --out-dir)")
    ap.add_argument("--raw-root", nargs="+", default=[DEFAULT_SCAN_ROOT],
                    help="folder(s) to search (recursively) for the linked .raw "
                         "files (default: %(default)s)")
    ap.add_argument("--png-dir", default=DEFAULT_PNG_DIR,
                    help="output folder for the check PNGs (default: %(default)s)")
    args = ap.parse_args()

    out_dir = Path(args.out_dir)

    # Render-only mode: no scanning/expansion, just draw the checks.
    if args.checks_only:
        render_all_checks(Path(args.in_dir) if args.in_dir else out_dir,
                          args.raw_root, args.png_dir)
        return

    scan_root = Path(args.scan_root)
    if not scan_root.is_dir():
        raise SystemExit(f"Scan root not found: {scan_root}")

    sources = sorted(scan_root.rglob("CBCT_*.in"))
    if not sources:
        raise SystemExit(f"No CBCT_*.in files found under {scan_root}")

    # group by (patient, stretcher label): {(patient,label): {kind: src}}
    groups = defaultdict(dict)
    unknown = set()
    for src in sources:
        patient, label, kind = parse_meta(src, scan_root)
        if kind not in KIND_LETTER:
            unknown.add(kind)
        groups[(patient, label)][kind] = src
    if unknown:
        print(f"  [warn] template kinds with no a/b/c letter, skipped: {sorted(unknown)}")

    if not args.dry_run:
        out_dir.mkdir(parents=True, exist_ok=True)
        removed = sum(1 for p in out_dir.glob("*.in") if p.unlink() is None)
        if removed:
            print(f"  cleared {removed} existing .in from {out_dir}")

    written = 0
    num = args.start
    for (patient, label) in sorted(groups):
        g = groups[(patient, label)]
        for kind, letter in KIND_LETTER.items():        # a, b, c order
            src = g.get(kind)
            if src is None:
                continue
            lines = src.read_text().splitlines(keepends=True)
            for kv_name, kv_key in KV_VARIANTS:          # 80kV, 120kV
                stem = f"{num}{letter}_{patient}_{label}_{kind}_{kv_name}"
                dst = out_dir / f"{stem}.in"
                if args.dry_run:
                    print(f"  {dst.name}   <- {src.relative_to(scan_root)}")
                else:
                    dst.write_text("".join(
                        set_output_name(set_spectrum(lines, kv_key), stem)))
                    written += 1
        num += 1

    n_groups = len(groups)
    print(f"\n{'Would write' if args.dry_run else 'Wrote'} {written or n_groups * len(KIND_LETTER) * 2} "
          f".in files from {n_groups} patient+stretcher group(s), "
          f"numbered {args.start}..{num - 1} -> {out_dir}")

    if args.render_checks and not args.dry_run:
        print()
        render_all_checks(out_dir, args.raw_root, args.png_dir)


if __name__ == "__main__":
    main()
