#!/usr/bin/env python3
"""
Batch driver for dicom_to_mcgpu.

Folder layout: either one case per patient, nested --
    export/<PATIENT_ID>/<STUDY>/<NNN_series>/        <- input DICOM series
    export/<PATIENT_ID>/<STUDY>/*.nrrd               <- segmentation mask
    export/<PATIENT_ID>/<STUDY>/MCGPU*               <- previous outputs (ignored)
-- or flat, one case per top-level folder with .dcm files directly inside it
(e.g. mcrp_to_vox's tight-crop DICOM export, no study/series nesting):
    export/<CASE_NAME>/*.dcm

Two modes:

  scan   Walk the export folder (either layout, auto-detected per top-level
         folder) and emit a JSON skeleton, one case per patient: dicom_dir,
         output_prefix, mask_nrrd and three stretcher positions (pre-filled
         from the template, ready to be edited by hand after measuring in a
         DICOM viewer).  Each slot is the stretcher polygon centre in
         isocenter mm: {"cx_mm": .., "cy_mm": ..}.
         Pass --auto-placement to pre-fill an offset-based grid instead (see
         below) and set auto_head_placement for every case -- default 3x3=9
         positions/case (lateral x depth); tune with --stretcher-cx-count /
         --stretcher-cx-min / --stretcher-cx-max / --stretcher-cy-count /
         --stretcher-cy-max-offset.  --basic-placement (mutually exclusive
         with --auto-placement) pre-fills a SINGLE position instead: centred,
         auto-seated at the closest legal position behind the detected head.
         --output-dir writes MC-GPU output flat into an external folder
         instead of a subfolder next to the DICOM input.  --minimal
         suppresses .in generation (raw+txt only output); positioning checks
         still render, via a shared beam template instead of a per-run .in.

  run    Read that JSON, and for every case x every stretcher position:
         build a .cfg (template + overrides appended) and invoke the binary.
         After each successful build, renders a central-sagittal
         positioning-check PNG (phantom + stretcher with the CBCT beam
         overlaid) into positioning_checks/ -- use --no-checks to skip.
         For minimal (--minimal) jobs, the check renders from the .txt
         companion + --check-template instead of a per-run .in (see
         expand_in_kv.parse_split_geometry).

The binary loads cfg with "last value wins" per key, so overrides are simply
appended after the template body.  Paths may be Windows (F:\\...) or Linux
(/mnt/f/...) form - the binary converts them itself.

Stretcher slots support two forms (PLAN_dicom_to_mcgpu_auto_placement.md):
  - absolute (real-patient default):  {"cx_mm": 0.0, "cy_mm": 133.5}
  - auto (head-relative augmentation): {"cx_mm": 0.0, "auto": true,
    "cy_offset_mm": 0.0}  -- the binary's own HU-threshold head detection sets
    the Y limit (must sit behind the detected head), cy_offset_mm is the
    augmentation knob added on top.  Requires auto_head_placement (case-level
    or top-level "auto_head_placement": true in the jobs JSON) so the same
    head-detection pass is available.

Examples
--------
  # 1. generate the skeleton from the export folder
  python3 batch_dicom_to_mcgpu.py scan \\
      "F:\\Michele_diskF\\GradientHealth\\download\\testRAR-13MAY2026\\dicomweb\\export" \\
      -o batch_jobs.json

  # 2. edit batch_jobs.json: fill the 3 stretcher cx_mm/cy_mm per case

  # 3. run everything (or --dry-run to only write the .cfg files)
  python3 batch_dicom_to_mcgpu.py run batch_jobs.json
  python3 batch_dicom_to_mcgpu.py run batch_jobs.json --dry-run
  python3 batch_dicom_to_mcgpu.py run batch_jobs.json --only GRDN17YG2OQIYVPO
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent
DEFAULT_TEMPLATE = "params/dicom_to_mcgpu_template.cfg"
DEFAULT_BINARY = "build/dicom_to_mcgpu"
DEFAULT_CFG_OUT_DIR = "params/generated"
DEFAULT_PNG_DIR = "positioning_checks"
# Beam template used for positioning checks on minimal (raw+txt only, no
# per-run .in) output -- any shipped template works, they share identical
# beam geometry (see HANDOUT_mcrp_dicom_stretcher_augmentation.md).
DEFAULT_CHECK_TEMPLATE = "params/cbct_head_bar_template.in"

# Keys overridden per (case, stretcher); everything else comes from template.
# auto_head_placement/stretcher_auto/stretcher_cy_offset_mm/output_dir are
# only emitted when present in a given job's overrides dict (see cmd_run) --
# absolute stretcher slots and cases with no "auto_head_placement"/
# "output_dir" never set them, so existing real-patient JSONs behave exactly
# as before.
OVERRIDE_KEYS = (
    "dicom_dir",
    "series_uid",
    "acquisition_number",
    "output_prefix",
    "output_dir",
    "mcgpu_in_dir",
    "mask_nrrd",
    "phantom_name",
    "auto_head_placement",
    "mcgpu_in_template",
    "fov_template",
    "stretcher_enable",
    "stretcher_cx_mm",
    "stretcher_cy_mm",
    "stretcher_auto",
    "stretcher_cy_offset_mm",
)
PATH_KEYS = ("dicom_dir", "output_prefix", "output_dir", "mcgpu_in_dir", "mask_nrrd")

# Prefix for phantom_name (-> .raw named "<prefix><id>_<label>_<dims>byte.raw")
PHANTOM_NAME_PREFIX = ""
# The binary prints these lines for every .raw / .in it writes.
RAW_WRITTEN_MARKER = "Label phantom written:"
IN_WRITTEN_MARKER = "MC-GPU .in file written:"


def render_check(in_path, png_dir: Path, tag: str, cache: dict, raw_path=None):
    """Render one central-sagittal positioning-check PNG for a generated .in,
    reusing expand_in_kv.render_positioning_check (single source of truth,
    same pattern as batch_mcrp_to_vox.py's render_check).  raw_path: explicit
    path to the linked .raw -- pass this when mcgpu_in_dir separates the .in
    from its .raw; omit only when they're known to share a folder (derives
    it from geom["raw_name"] next to the .in, the previous behaviour).
    Returns the PNG path, or None."""
    from expand_in_kv import parse_in_geometry, render_positioning_check
    in_path = Path(in_path)
    geom = parse_in_geometry(in_path)
    raw = Path(raw_path) if raw_path else (in_path.parent / geom["raw_name"])
    if not raw.is_file():
        print(f"    [warn] positioning check: .raw not found: {raw}")
        return None
    png_dir.mkdir(parents=True, exist_ok=True)
    out_png = png_dir / f"{slug(tag)}_{in_path.stem}.png"
    render_positioning_check(in_path, raw, out_png, geom, cache)
    return out_png


def make_kv_variants(in_path, kv_variants=None):
    """Rewrites one dicom_to_mcgpu-generated .in into per-kV variants (only
    the active SECTION SOURCE spectrum line changes; OUTPUT IMAGE FILE NAME
    is re-suffixed to stay unique), kV embedded in the filename -- reuses
    expand_in_kv.py's set_spectrum/set_output_name (already proven, same
    logic that already does this for the real-patient kV-expansion tool).
    Deletes the original (un-suffixed) .in.  Returns the list of new .in
    paths, in kv_variants order."""
    from expand_in_kv import set_spectrum, set_output_name, KV_VARIANTS
    if kv_variants is None:
        kv_variants = KV_VARIANTS
    in_path = Path(in_path)
    lines = in_path.read_text().splitlines(keepends=True)
    stem = in_path.stem
    made = []
    for kv_name, kv_key in kv_variants:
        dst = in_path.with_name(f"{stem}_{kv_name}.in")
        new_stem = f"{stem}_{kv_name}"
        dst.write_text("".join(set_output_name(set_spectrum(lines, kv_key), new_stem)))
        made.append(dst)
    in_path.unlink()
    return made


def render_check_minimal(raw_path, check_template, png_dir: Path, tag: str, cache: dict):
    """Positioning check for a minimal (raw+txt only, no .in) run: voxel
    geometry comes from the .txt companion beside raw_path, beam geometry
    from a separate SHARED beam template (see expand_in_kv.parse_split_geometry
    for why this is safe -- identical beam geometry across positions/
    templates).  Returns the PNG path, or None."""
    from expand_in_kv import parse_split_geometry, render_positioning_check
    raw_path = Path(raw_path)
    txt_path = raw_path.with_suffix(".txt")
    if not txt_path.is_file():
        print(f"    [warn] positioning check: .txt not found: {txt_path}")
        return None
    geom = parse_split_geometry(txt_path, REPO_ROOT / check_template)
    png_dir.mkdir(parents=True, exist_ok=True)
    out_png = png_dir / f"{slug(tag)}_{raw_path.stem}.png"
    render_positioning_check(raw_path, raw_path, out_png, geom, cache)
    return out_png


# --------------------------------------------------------------------------- #
# Path helpers
# --------------------------------------------------------------------------- #
def to_linux_path(p: str) -> str:
    """Mirror the binary's toLinuxPath: X:\\... -> /mnt/x/..., \\ -> /."""
    p = p.replace("\\", "/")
    if len(p) >= 2 and p[0].isalpha() and p[1] == ":":
        p = "/mnt/" + p[0].lower() + p[2:]
    return p


def quote(v: str) -> str:
    return '"' + v + '"'


def slug(s: str) -> str:
    return re.sub(r"[^A-Za-z0-9._-]+", "_", s).strip("_")


# --------------------------------------------------------------------------- #
# Default stretcher values from the template (used to pre-fill the skeleton)
# --------------------------------------------------------------------------- #
def read_template_stretcher(template_path: Path):
    tx = ty = 0.0
    if template_path.is_file():
        for line in template_path.read_text().splitlines():
            line = line.split("#", 1)[0]
            if "=" not in line:
                continue
            k, _, v = line.partition("=")
            k, v = k.strip(), v.strip()
            try:
                if k == "stretcher_cx_mm":
                    tx = float(v)
                elif k == "stretcher_cy_mm":
                    ty = float(v)
            except ValueError:
                pass
    return tx, ty


# --------------------------------------------------------------------------- #
# scan
# --------------------------------------------------------------------------- #
def _linspace(lo: float, hi: float, n: int):
    if n <= 1:
        return [lo]
    step = (hi - lo) / (n - 1)
    return [lo + i * step for i in range(n)]


def _grid_labels(n: int):
    import string
    if n <= len(string.ascii_uppercase):
        return list(string.ascii_uppercase[:n])
    return [f"P{i}" for i in range(n)]


def auto_stretcher_grid(cx_count, cx_min, cx_max, cy_count, cy_max_offset):
    """Cartesian product of lateral (cx_mm, manually-bounded) x depth
    (cy_offset_mm, 0 = the auto-detected head-clearance limit itself, up to
    cy_max_offset further back) auto-seat positions."""
    cx_values = _linspace(cx_min, cx_max, cx_count)
    cy_values = _linspace(0.0, cy_max_offset, cy_count)
    return [{"cx_mm": cx, "auto": True, "cy_offset_mm": cy}
            for cx in cx_values for cy in cy_values]


def cmd_scan(args):
    export = Path(to_linux_path(args.export_dir))
    if not export.is_dir():
        sys.exit(f"Export folder not found: {export}")
    if args.minimal and args.in_dir:
        print("  [warn] --minimal (no .in at all) and --in-dir (a separate "
              "folder FOR .in) together: --in-dir has no effect, no .in is "
              "written either way.")

    if args.basic_placement:
        # Single canonical position: centred (cx=0), auto-seated at the
        # closest legal position directly behind the detected head
        # (cy_offset=0) -- no grid, just one clean placement.
        default_stretchers = [{"cx_mm": 0.0, "auto": True, "cy_offset_mm": 0.0}]
        stretcher_labels = ["A"]
    elif args.auto_placement:
        # Head-relative augmentation grid: lateral (cx_mm) bounds are manual
        # (--stretcher-cx-min/-max); depth (cy_offset_mm) starts at 0 -- the
        # auto-detected head-clearance limit itself -- up to
        # --stretcher-cy-max-offset further back.
        default_stretchers = auto_stretcher_grid(
            args.stretcher_cx_count, args.stretcher_cx_min, args.stretcher_cx_max,
            args.stretcher_cy_count, args.stretcher_cy_max_offset)
        stretcher_labels = _grid_labels(len(default_stretchers))
    else:
        def_cx, def_cy = read_template_stretcher(REPO_ROOT / args.template)
        default_stretchers = [{"cx_mm": def_cx, "cy_mm": def_cy} for _ in range(3)]
        stretcher_labels = ["A", "B", "C"]

    uses_auto_head = args.auto_placement or args.basic_placement

    cases = []
    for patient in sorted(p for p in export.iterdir() if p.is_dir()):
        studies = sorted(s for s in patient.iterdir() if s.is_dir())
        if studies:
            # Nested real-patient layout: export/<PATIENT>/<STUDY>/<series>/*.dcm
            if len(studies) > 1:
                print(f"  [warn] {patient.name}: {len(studies)} studies, "
                      f"using '{studies[0].name}'")
            study = studies[0]

            # DICOM series = sub-dir that is not a previous MCGPU output folder
            series = [d for d in sorted(study.iterdir())
                      if d.is_dir() and not d.name.upper().startswith("MCGPU")]
            if not series:
                print(f"  [skip] {patient.name}: no input series in {study.name}")
                continue
            if len(series) > 1:
                print(f"  [warn] {patient.name}: {len(series)} series, "
                      f"using '{series[0].name}'")
            dicom_dir = series[0]
            nrrd = sorted(study.glob("*.nrrd"))
            output_prefix = study / "MCGPU"
        elif any(patient.glob("*.dcm")):
            # Flat layout: the "patient" folder IS the DICOM series itself,
            # no study/series nesting (e.g. mcrp_to_vox's tight-crop DICOM
            # export, or any other single-series-per-folder source).
            dicom_dir = patient
            nrrd = sorted(patient.glob("*.nrrd"))
            output_prefix = patient.parent / f"{patient.name}_MCGPU"
        else:
            print(f"  [skip] no study folder or .dcm files for {patient.name}")
            continue

        if not nrrd:
            print(f"  [warn] {patient.name}: no .nrrd mask found")

        case = {
            "name": patient.name,
            "dicom_dir": str(dicom_dir),
            "output_prefix": str(output_prefix),
            "mask_nrrd": str(nrrd[0]) if nrrd else "",
            "stretchers": [dict(s) for s in default_stretchers],
        }
        if uses_auto_head:
            case["auto_head_placement"] = True
        cases.append(case)

    doc = {
        "binary": DEFAULT_BINARY,
        "template": args.template,
        "cwd": ".",
        "cfg_out_dir": DEFAULT_CFG_OUT_DIR,
        "stretcher_labels": stretcher_labels,
        "auto_head_placement": bool(uses_auto_head),
        "cases": cases,
    }
    if args.output_dir:
        # Flat external output folder for .raw/.txt (keeps DICOM input and
        # MC-GPU volumes from mixing); applies to every case unless
        # overridden per-case.
        doc["output_dir"] = to_linux_path(args.output_dir)
    if args.in_dir:
        # Flat external folder for .in file(s) ONLY, separate from
        # output_dir -- keeps a growing batch's small .in files apart from
        # its much larger .raw/.txt volumes.  NOTE: the .in's own VOXEL
        # GEOMETRY FILE reference stays "phantom/<raw-name>.raw" (relative) --
        # to actually run MC-GPU from here, place/symlink the matching .raw
        # under "<in_dir>/phantom/" first.
        doc["mcgpu_in_dir"] = to_linux_path(args.in_dir)
    if args.minimal:
        # Suppress .in generation -- output is just .raw + .txt.  Positioning
        # checks still render (via --check-template / "check_template" +
        # expand_in_kv.parse_split_geometry, see render_check_minimal).
        # Mutually pointless with --in-dir (which implies you DO want .in).
        doc["mcgpu_in_template"] = ""
    elif args.in_template:
        doc["mcgpu_in_template"] = args.in_template
    out = Path(args.output)
    out.write_text(json.dumps(doc, indent=2))
    print(f"\nWrote {len(cases)} case(s), "
          f"{len(default_stretchers)} stretcher position(s) each -> {out}")
    if args.basic_placement:
        print("Basic placement: single position, centred, auto-seated "
              "directly behind the detected head (closest legal position).")
    elif args.auto_placement:
        print("Auto-placement grid: cx in "
              f"[{args.stretcher_cx_min}, {args.stretcher_cx_max}] mm "
              f"x {args.stretcher_cx_count}, cy_offset in "
              f"[0, {args.stretcher_cy_max_offset}] mm x {args.stretcher_cy_count} "
              "(edit per-case \"stretchers\" to change).")
    else:
        print("Edit the stretcher cx_mm/cy_mm per case, then run:")
    if args.in_dir and not args.minimal:
        print(f"MC-GPU .in file(s) -> {to_linux_path(args.in_dir)} "
              "(separate from .raw/.txt; filenames prefixed for uniqueness).")
    if args.minimal:
        print("Minimal output: .raw + .txt only, no .in (positioning checks "
              "still render from the .txt + check_template).")
    print(f"  python3 {Path(__file__).name} run {out}")


# --------------------------------------------------------------------------- #
# run
# --------------------------------------------------------------------------- #
def build_cfg(template_text: str, overrides: dict) -> str:
    lines = [template_text.rstrip(), "",
             "# ===== batch overrides (appended; last value wins) ====="]
    for k in OVERRIDE_KEYS:
        if k not in overrides:
            continue
        v = overrides[k]
        if k in PATH_KEYS:
            v = quote(str(v))
        lines.append(f"{k} = {v}")
    return "\n".join(lines) + "\n"


def cmd_run(args):
    doc = json.loads(Path(args.jobs).read_text())
    binary = (REPO_ROOT / doc.get("binary", DEFAULT_BINARY)).resolve()
    template_path = REPO_ROOT / doc.get("template", DEFAULT_TEMPLATE)
    cwd = (REPO_ROOT / doc.get("cwd", ".")).resolve()
    cfg_dir = REPO_ROOT / doc.get("cfg_out_dir", DEFAULT_CFG_OUT_DIR)
    labels = doc.get("stretcher_labels", ["A", "B", "C"])
    cases = doc.get("cases", [])

    if not template_path.is_file():
        sys.exit(f"Template not found: {template_path}")
    if not args.dry_run and not binary.is_file():
        sys.exit(f"Binary not found: {binary}  (build it first)")
    template_text = template_path.read_text()
    cfg_dir.mkdir(parents=True, exist_ok=True)

    if args.only:
        wanted = set(args.only)
        cases = [c for i, c in enumerate(cases)
                 if c.get("name", str(i)) in wanted or str(i) in wanted]

    # Global defaults; a per-case key overrides it.  Both absent -> key is
    # never emitted, so existing real-patient JSONs (manual dicom_corner_z_mm,
    # absolute stretcher cx/cy, default nested output folder, template's own
    # mcgpu_in_template) behave exactly as before.
    auto_default = bool(doc.get("auto_head_placement", False))
    output_dir_default = doc.get("output_dir", "")
    in_dir_default = doc.get("mcgpu_in_dir", "")
    # None (key absent everywhere) -> don't override; "" (explicitly set, e.g.
    # via --minimal) -> override to empty, suppressing .in generation.
    mcgpu_tmpl_default = doc.get("mcgpu_in_template")
    fov_tmpl_default = doc.get("fov_template")
    # Beam template for minimal (raw+txt only, no .in) positioning checks --
    # CLI overrides a "check_template" key in the jobs JSON, which overrides
    # the built-in default.  Resolved here (before cfg-writing) because it
    # doubles as the fov_template fallback below.
    check_template = args.check_template or doc.get("check_template", DEFAULT_CHECK_TEMPLATE)

    jobs = []  # (cfg_path, tag)
    for ci, case in enumerate(cases):
        name = case.get("name", f"case{ci}")
        # base for the .in phantom reference; per-case override wins.
        ph_base = case.get("phantom_name", f"{PHANTOM_NAME_PREFIX}{name}")
        case_auto_head = bool(case.get("auto_head_placement", auto_default))
        case_output_dir = case.get("output_dir", output_dir_default)
        case_in_dir = case.get("mcgpu_in_dir", in_dir_default)
        case_mcgpu_tmpl = case.get("mcgpu_in_template", mcgpu_tmpl_default)
        case_fov_tmpl = case.get("fov_template", fov_tmpl_default)
        if case_mcgpu_tmpl == "" and not case_fov_tmpl:
            # Minimal run (no .in) with no explicit fov_template: fall back to
            # check_template so FOV-anchored placement doesn't silently
            # degrade to "volume's own top face" (dicom_to_mcgpu.cpp uses
            # fov_template, decoupled from mcgpu_in_template, for exactly
            # this reason).
            case_fov_tmpl = check_template
        for si, st in enumerate(case.get("stretchers", [])):
            label = labels[si] if si < len(labels) else f"s{si}"
            overrides = {
                "dicom_dir": case["dicom_dir"],
                "output_prefix": f'{case["output_prefix"]}_{label}',
                "mask_nrrd": case.get("mask_nrrd", ""),
                # label keeps the stretcher volumes distinct in phantom/
                "phantom_name": f"{ph_base}_{label}",
                "stretcher_enable": "true",
                "stretcher_cx_mm": st.get("cx_mm", 0.0),
            }
            # Input-series disambiguation, only when the case asks for it: some
            # folders bundle more than one series (or acquisition) and the
            # binary's "first series found" / auto acquisition pick is not
            # necessarily the one the curated mask belongs to.
            for k in ("series_uid", "acquisition_number"):
                if case.get(k) not in (None, ""):
                    overrides[k] = case[k]
            if case_auto_head:
                overrides["auto_head_placement"] = "true"
            if case_output_dir:
                overrides["output_dir"] = to_linux_path(case_output_dir)
            if case_in_dir:
                overrides["mcgpu_in_dir"] = to_linux_path(case_in_dir)
            if case_mcgpu_tmpl is not None:
                overrides["mcgpu_in_template"] = case_mcgpu_tmpl
            if case_fov_tmpl:
                overrides["fov_template"] = case_fov_tmpl
            if st.get("auto"):
                # Head-relative augmentation: binary computes the Y limit from
                # its own head detection; cy_offset_mm is the augmentation knob.
                overrides["stretcher_auto"] = "true"
                overrides["stretcher_cy_offset_mm"] = st.get("cy_offset_mm", 0.0)
            else:
                # Absolute (real-patient default / manually measured).
                overrides["stretcher_cy_mm"] = st["cy_mm"]
            cfg_path = cfg_dir / f"{slug(name)}_{label}.cfg"
            cfg_path.write_text(build_cfg(template_text, overrides))
            jobs.append((cfg_path, f"{name} [{label}]"))

    print(f"Prepared {len(jobs)} job(s) "
          f"({len(cases)} case(s) x stretcher positions).")
    print(f"cfg files -> {cfg_dir}")
    if args.dry_run:
        for cfg_path, tag in jobs:
            print(f"  [dry-run] {tag}: {cfg_path}")
        return

    raw_log_path = Path(args.raw_log) if args.raw_log \
        else Path(args.jobs).with_name(Path(args.jobs).stem + "_exported_raw.txt")

    # Verify the positioning-check renderer is importable once, up front.
    do_checks = not args.no_checks
    if do_checks:
        try:
            import matplotlib  # noqa: F401
            from expand_in_kv import render_positioning_check  # noqa: F401
        except Exception as e:                                  # noqa: BLE001
            print(f"  [warn] positioning checks disabled (missing dependency): {e}")
            do_checks = False
    png_dir = REPO_ROOT / args.png_dir
    slice_cache = {}

    failures = []
    raw_entries = []   # (tag, raw_path)
    made_pngs = []
    for n, (cfg_path, tag) in enumerate(jobs, 1):
        print(f"\n=== [{n}/{len(jobs)}] {tag} ===")
        print(f"    {binary} {cfg_path}")
        # stream output live while scanning for the .raw / .in paths it writes
        proc = subprocess.Popen([str(binary), str(cfg_path)], cwd=str(cwd),
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, bufsize=1)
        in_paths = []
        raw_path_this_job = None
        for line in proc.stdout:
            sys.stdout.write(line)
            if RAW_WRITTEN_MARKER in line:
                raw_path_this_job = line.split(RAW_WRITTEN_MARKER, 1)[1].strip()
                raw_entries.append((tag, raw_path_this_job))
            elif IN_WRITTEN_MARKER in line:
                s = line.split(IN_WRITTEN_MARKER, 1)[1].strip()
                in_paths.append(s.rsplit("(from", 1)[0].strip() if "(from" in s else s)
        rc = proc.wait()
        if rc != 0:
            print(f"    !! exit code {rc}")
            failures.append((tag, rc))
            if args.stop_on_error:
                break
            continue
        if args.kv_variants and in_paths:
            expanded = []
            for ip in in_paths:
                try:
                    expanded.extend(str(p) for p in make_kv_variants(ip))
                except Exception as e:                          # noqa: BLE001
                    print(f"    [warn] kV variant expansion failed for {ip}: {e}")
                    expanded.append(ip)
            in_paths = expanded
            for p in in_paths:
                print(f"    [kv] {p}")
        if do_checks:
            if in_paths:
                for ip in in_paths:
                    try:
                        png = render_check(ip, png_dir, tag, slice_cache,
                                           raw_path=raw_path_this_job)
                        if png:
                            made_pngs.append(png)
                            print(f"    [check] {png}")
                    except Exception as e:                      # noqa: BLE001
                        print(f"    [warn] positioning check failed for {ip}: {e}")
            elif raw_path_this_job:
                # Minimal run (mcgpu_in_template unset -> no .in written):
                # derive the check from the .txt companion + the shared
                # check_template instead (expand_in_kv.parse_split_geometry).
                try:
                    png = render_check_minimal(raw_path_this_job, check_template,
                                               png_dir, tag, slice_cache)
                    if png:
                        made_pngs.append(png)
                        print(f"    [check] {png}")
                except Exception as e:                          # noqa: BLE001
                    print(f"    [warn] positioning check failed for "
                          f"{raw_path_this_job}: {e}")

    # write the list of exported .raw files
    lines = ["# exported .raw files (one per job)\t<tag>\t<raw_path>"]
    lines += [f"{tag}\t{raw_path}" for tag, raw_path in raw_entries]
    raw_log_path.write_text("\n".join(lines) + "\n")
    print(f"\nExported {len(raw_entries)} .raw file(s); list -> {raw_log_path}")
    if do_checks:
        print(f"Positioning checks: {len(made_pngs)} PNG(s) -> {png_dir}")

    print(f"Done: {len(jobs) - len(failures)}/{len(jobs)} succeeded.")
    if failures:
        for tag, rc in failures:
            print(f"  FAILED ({rc}): {tag}")
        sys.exit(1)


# --------------------------------------------------------------------------- #
def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("scan", help="generate batch_jobs.json from an export folder")
    s.add_argument("export_dir", help="path to .../dicomweb/export (Windows or Linux)")
    s.add_argument("-o", "--output", default="batch_jobs.json")
    s.add_argument("--template", default=DEFAULT_TEMPLATE)
    s.add_argument("--output-dir", metavar="DIR",
                   help="write .raw/.txt output directly into this folder, "
                        "flat (no per-case subfolder) -- keeps it separate "
                        "from the DICOM input tree, e.g. an external drive")
    s.add_argument("--in-dir", metavar="DIR",
                   help="write .in file(s) into this SEPARATE folder, flat "
                        "(filenames prefixed with the case name for "
                        "uniqueness) -- keeps small .in files apart from the "
                        "much larger .raw/.txt volumes. NOTE: the .in still "
                        "references its .raw as \"phantom/<name>.raw\" "
                        "(relative) -- to run MC-GPU from here, place/symlink "
                        "the matching .raw under \"<in-dir>/phantom/\" first")
    s.add_argument("--minimal", action="store_true",
                   help="suppress .in generation entirely (output is just "
                        ".raw + .txt, no separate .in either); positioning "
                        "checks still render via --check-template. Usually "
                        "you want --in-dir instead, not this")
    s.add_argument("--in-template", metavar="PATH[,PATH...]",
                   help="override mcgpu_in_template for every case (comma/"
                        "semicolon-separated for several) instead of the "
                        "shared cfg template's own value -- e.g. a single "
                        "path to generate just one .in per case instead of "
                        "the template's default 3")
    placement_group = s.add_mutually_exclusive_group()
    placement_group.add_argument(
        "--auto-placement", action="store_true",
        help="pre-fill an offset-based auto stretcher GRID "
             "({\"auto\": true, \"cy_offset_mm\": ..}) instead of absolute "
             "cx_mm/cy_mm, and set auto_head_placement=true for every case")
    placement_group.add_argument(
        "--basic-placement", action="store_true",
        help="pre-fill a SINGLE auto stretcher position: centred (cx=0), "
             "seated at the closest legal position directly behind the "
             "detected head (cy_offset=0) -- no grid, just one placement")
    s.add_argument("--stretcher-cx-count", type=int, default=3, metavar="N",
                   help="[--auto-placement] number of lateral positions "
                        "(default: %(default)s)")
    s.add_argument("--stretcher-cx-min", type=float, default=-30.0, metavar="MM",
                   help="[--auto-placement] lateral min, mm (default: %(default)s)")
    s.add_argument("--stretcher-cx-max", type=float, default=30.0, metavar="MM",
                   help="[--auto-placement] lateral max, mm (default: %(default)s)")
    s.add_argument("--stretcher-cy-count", type=int, default=3, metavar="N",
                   help="[--auto-placement] number of depth positions, spaced "
                        "from 0 (the auto-detected head-clearance limit) to "
                        "--stretcher-cy-max-offset (default: %(default)s)")
    s.add_argument("--stretcher-cy-max-offset", type=float, default=60.0, metavar="MM",
                   help="[--auto-placement] max cy_offset_mm beyond the "
                        "auto-detected limit (default: %(default)s)")
    s.set_defaults(func=cmd_scan)

    r = sub.add_parser("run", help="run the binary for every case x stretcher")
    r.add_argument("jobs", help="batch_jobs.json")
    r.add_argument("--dry-run", action="store_true",
                   help="write .cfg files but do not launch the binary")
    r.add_argument("--only", nargs="+", metavar="NAME",
                   help="run only these case names (or indices)")
    r.add_argument("--raw-log", metavar="PATH",
                   help="where to write the list of exported .raw paths "
                        "(default: <jobs>_exported_raw.txt)")
    r.add_argument("--stop-on-error", action="store_true")
    r.add_argument("--png-dir", default=DEFAULT_PNG_DIR,
                   help="output folder for positioning-check PNGs "
                        "(default: %(default)s)")
    r.add_argument("--no-checks", action="store_true",
                   help="skip rendering positioning-check PNGs after each build")
    r.add_argument("--check-template", metavar="PATH",
                   help="beam template used for positioning checks on minimal "
                        "(raw+txt only, no .in) runs -- overrides a "
                        "\"check_template\" key in the jobs JSON, which "
                        f"overrides the default ({DEFAULT_CHECK_TEMPLATE})")
    r.add_argument("--kv-variants", action="store_true",
                   help="expand each generated .in into 80kV/120kV variants "
                        "(kV embedded in the filename, e.g. \"..._120kV.in\") "
                        "instead of leaving it at the template's own active "
                        "spectrum; the un-suffixed .in is removed")
    r.set_defaults(func=cmd_run)

    args = ap.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
