#!/usr/bin/env python3
"""
Batch driver for dicom_to_mcgpu.

Folder layout (one case per patient):
    export/<PATIENT_ID>/<STUDY>/<NNN_series>/        <- input DICOM series
    export/<PATIENT_ID>/<STUDY>/*.nrrd               <- segmentation mask
    export/<PATIENT_ID>/<STUDY>/MCGPU*               <- previous outputs (ignored)

Two modes:

  scan   Walk the export folder and emit a JSON skeleton, one case per
         patient: dicom_dir, output_prefix, mask_nrrd and three stretcher
         positions (pre-filled from the template, ready to be edited by hand
         after measuring in a DICOM viewer).

  run    Read that JSON, and for every case x every stretcher position:
         build a .cfg (template + overrides appended) and invoke the binary.

The binary loads cfg with "last value wins" per key, so overrides are simply
appended after the template body.  Paths may be Windows (F:\\...) or Linux
(/mnt/f/...) form - the binary converts them itself.

Examples
--------
  # 1. generate the skeleton from the export folder
  python3 batch_dicom_to_mcgpu.py scan \\
      "F:\\Michele_diskF\\GradientHealth\\download\\testRAR-13MAY2026\\dicomweb\\export" \\
      -o batch_jobs.json

  # 2. edit batch_jobs.json: fill the 3 stretcher top_x_mm/top_y_mm per case

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

# Keys overridden per (case, stretcher); everything else comes from template.
OVERRIDE_KEYS = (
    "dicom_dir",
    "output_prefix",
    "mask_nrrd",
    "phantom_name",
    "stretcher_enable",
    "stretcher_top_x_mm",
    "stretcher_top_y_mm",
)
PATH_KEYS = ("dicom_dir", "output_prefix", "mask_nrrd")

# Prefix for the phantom_name written into the .in (-> phantom/GH_<id>_<dims>.raw)
PHANTOM_NAME_PREFIX = "GH_"
# The binary prints this line for every .raw it writes.
RAW_WRITTEN_MARKER = "Label phantom written:"


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
                if k == "stretcher_top_x_mm":
                    tx = float(v)
                elif k == "stretcher_top_y_mm":
                    ty = float(v)
            except ValueError:
                pass
    return tx, ty


# --------------------------------------------------------------------------- #
# scan
# --------------------------------------------------------------------------- #
def cmd_scan(args):
    export = Path(to_linux_path(args.export_dir))
    if not export.is_dir():
        sys.exit(f"Export folder not found: {export}")

    def_tx, def_ty = read_template_stretcher(REPO_ROOT / args.template)
    default_stretchers = [{"top_x_mm": def_tx, "top_y_mm": def_ty} for _ in range(3)]

    cases = []
    for patient in sorted(p for p in export.iterdir() if p.is_dir()):
        studies = sorted(s for s in patient.iterdir() if s.is_dir())
        if not studies:
            print(f"  [skip] no study folder for {patient.name}")
            continue
        if len(studies) > 1:
            print(f"  [warn] {patient.name}: {len(studies)} studies, "
                  f"using '{studies[0].name}'")
        study = studies[0]

        # DICOM series = sub-dir that is not a previous MCGPU output folder
        series = [d for d in sorted(study.iterdir())
                  if d.is_dir() and not d.name.upper().startswith("MCGPU")]
        nrrd = sorted(study.glob("*.nrrd"))
        if not series:
            print(f"  [skip] {patient.name}: no input series in {study.name}")
            continue
        if len(series) > 1:
            print(f"  [warn] {patient.name}: {len(series)} series, "
                  f"using '{series[0].name}'")
        if not nrrd:
            print(f"  [warn] {patient.name}: no .nrrd mask in {study.name}")

        cases.append({
            "name": patient.name,
            "dicom_dir": str(series[0]),
            "output_prefix": str(study / "MCGPU"),
            "mask_nrrd": str(nrrd[0]) if nrrd else "",
            "stretchers": [dict(s) for s in default_stretchers],
        })

    doc = {
        "binary": DEFAULT_BINARY,
        "template": args.template,
        "cwd": ".",
        "cfg_out_dir": DEFAULT_CFG_OUT_DIR,
        "stretcher_labels": ["A", "B", "C"],
        "cases": cases,
    }
    out = Path(args.output)
    out.write_text(json.dumps(doc, indent=2))
    print(f"\nWrote {len(cases)} case(s) -> {out}")
    print("Edit the 3 stretcher top_x_mm/top_y_mm per case, then run:")
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

    jobs = []  # (cfg_path, tag)
    for ci, case in enumerate(cases):
        name = case.get("name", f"case{ci}")
        # base for the .in phantom reference; per-case override wins.
        ph_base = case.get("phantom_name", f"{PHANTOM_NAME_PREFIX}{name}")
        for si, st in enumerate(case.get("stretchers", [])):
            label = labels[si] if si < len(labels) else f"s{si}"
            overrides = {
                "dicom_dir": case["dicom_dir"],
                "output_prefix": f'{case["output_prefix"]}_{label}',
                "mask_nrrd": case.get("mask_nrrd", ""),
                # label keeps the 3 stretcher volumes distinct in phantom/
                "phantom_name": f"{ph_base}_{label}",
                "stretcher_enable": "true",
                "stretcher_top_x_mm": st["top_x_mm"],
                "stretcher_top_y_mm": st["top_y_mm"],
            }
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

    failures = []
    raw_entries = []  # (tag, raw_path)
    for n, (cfg_path, tag) in enumerate(jobs, 1):
        print(f"\n=== [{n}/{len(jobs)}] {tag} ===")
        print(f"    {binary} {cfg_path}")
        # stream output live while scanning for the .raw paths it writes
        proc = subprocess.Popen([str(binary), str(cfg_path)], cwd=str(cwd),
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, bufsize=1)
        for line in proc.stdout:
            sys.stdout.write(line)
            if RAW_WRITTEN_MARKER in line:
                raw_path = line.split(RAW_WRITTEN_MARKER, 1)[1].strip()
                raw_entries.append((tag, raw_path))
        rc = proc.wait()
        if rc != 0:
            print(f"    !! exit code {rc}")
            failures.append((tag, rc))
            if args.stop_on_error:
                break

    # write the list of exported .raw files
    lines = ["# exported .raw files (one per job)\t<tag>\t<raw_path>"]
    lines += [f"{tag}\t{raw_path}" for tag, raw_path in raw_entries]
    raw_log_path.write_text("\n".join(lines) + "\n")
    print(f"\nExported {len(raw_entries)} .raw file(s); list -> {raw_log_path}")

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
    r.set_defaults(func=cmd_run)

    args = ap.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
