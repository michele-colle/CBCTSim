#!/usr/bin/env python3
"""
Build a batch_dicom_to_mcgpu.py jobs JSON from the CQ500/qureai-headct
dataset manifest (dataset_manifest.json).

Why not `batch_dicom_to_mcgpu.py scan`: that walks an export TREE
(export/<PATIENT>/<STUDY>/<series>/ with the mask .nrrd sitting next to the
series).  The qureai dataset is organised differently -- series live under
qctNN/<PATIENT> <PATIENT>/<STUDY>/<series>, masks all together in masks/,
and two "stitched" cases ship a pre-built .nrrd volume instead of a DICOM
series.  The manifest already lists image + mask per patient, so this reads
it directly and emits the same JSON `scan --basic-placement` would (single
centred auto-seated stretcher position, auto_head_placement on).

Prerequisite for the stitched cases: convert their .nrrd volumes to DICOM
first (dicom_to_mcgpu only reads DICOM series):

    python3 nrrd_to_dicom_series.py --manifest <manifest> \\
        --out-root <dataset>/stitched_dicom

Usage
-----
  python3 make_qureai_jobs.py \\
      /mnt/f/Michele_diskF/kaggle/qureai-headct/dataset_manifest.json \\
      -o batch_jobs_qureai.json \\
      --output-dir "H:\\MICHELE_MCGPU\\QUREAI_VOLUME_EXPORT" \\
      --in-dir     "H:\\MICHELE_MCGPU\\QUREAI_IN_FILES" \\
      --in-template params/cbct_head_bar_template.in
"""

import argparse
import collections
import json
import sys
from pathlib import Path

from batch_dicom_to_mcgpu import to_linux_path

DEFAULT_IN_TEMPLATE = "params/cbct_head_bar_template.in"


def series_uids(dicom_dir: Path):
    """{SeriesInstanceUID: (n_slices, min_ipp_z, rows, row_spacing)} for one folder."""
    import pydicom
    out = collections.defaultdict(lambda: [0, None, None, None])
    for f in sorted(dicom_dir.iterdir()):
        if not f.is_file():
            continue
        try:
            ds = pydicom.dcmread(str(f), stop_before_pixels=True)
        except Exception:                                        # noqa: BLE001
            continue
        e = out[ds.SeriesInstanceUID]
        z = float(ds.ImagePositionPatient[2])
        e[0] += 1
        e[1] = z if e[1] is None else min(e[1], z)
        e[2] = int(ds.Rows)
        e[3] = float(ds.PixelSpacing[0])
    return {k: tuple(v) for k, v in out.items()}


def mask_geometry(mask_path: Path):
    """(nz, origin_z, rows, row_spacing) of a mask NRRD -- used to pick the
    matching series when a folder bundles more than one."""
    import SimpleITK as sitk
    im = sitk.ReadImage(str(mask_path))
    return (im.GetSize()[2], im.GetOrigin()[2], im.GetSize()[1], im.GetSpacing()[1])


def pick_series(dicom_dir: Path, mask_path: Path):
    """Returns the SeriesInstanceUID to force, or "" when the folder holds a
    single series (nothing to disambiguate -> let the binary take the first).
    With several series, picks the one matching the mask's own geometry --
    the mask defines which series the curated segmentation belongs to."""
    uids = series_uids(dicom_dir)
    if len(uids) <= 1:
        return ""
    mnz, moz, mrows, msp = mask_geometry(mask_path)
    hits = [u for u, (n, z, rows, sp) in uids.items()
            if n == mnz and abs(z - moz) < 1e-2 and rows == mrows and abs(sp - msp) < 1e-4]
    print(f"  [note] {dicom_dir.name}: {len(uids)} series in one folder "
          f"({', '.join(f'{u[-10:]}={v[0]} sl' for u, v in uids.items())})")
    if len(hits) != 1:
        sys.exit(f"  !! cannot disambiguate: {len(hits)} series match the mask "
                 f"({mnz} slices, origin z={moz}) in {dicom_dir}")
    print(f"         -> forcing series_uid ...{hits[0][-10:]} "
          f"({mnz} slices, matches the mask)")
    return hits[0]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("manifest")
    ap.add_argument("-o", "--output", default="batch_jobs_qureai.json")
    ap.add_argument("--output-dir", required=True,
                    help="flat external folder for .raw/.txt (Windows or Linux path)")
    ap.add_argument("--in-dir", required=True,
                    help="flat external folder for the .in file(s)")
    ap.add_argument("--in-template", default=DEFAULT_IN_TEMPLATE,
                    help="mcgpu_in_template for every case (default: %(default)s)")
    ap.add_argument("--stitched-dicom-root",
                    help="folder holding the converted <PATIENT>/ DICOM series "
                         "for manifest entries that are not dicom_series "
                         "(default: <manifest dir>/stitched_dicom)")
    ap.add_argument("--prefix-root",
                    help="base folder for output_prefix (default: <manifest dir>); "
                         "unused for the .raw/.txt themselves, which go to "
                         "--output-dir")
    ap.add_argument("--kind", metavar="KIND",
                    help="only include manifest entries with this 'kind' "
                         "(e.g. teeth, single_series, stitched); default: all. "
                         "Use it to build one batch per curated group, so each "
                         "batch gets its own output/check folders")
    ap.add_argument("--cfg-dir", default="params/generated_qureai",
                    help="where the generated .cfg files go (default: %(default)s)")
    args = ap.parse_args()

    manifest = Path(args.manifest)
    doc_in = json.loads(manifest.read_text())
    root = manifest.parent
    stitched_root = Path(args.stitched_dicom_root) if args.stitched_dicom_root \
        else root / "stitched_dicom"
    prefix_root = Path(args.prefix_root) if args.prefix_root else root

    cases = []
    skipped = 0
    flagged = []
    for p in doc_in.get("patients", []):
        if args.kind and p.get("kind") != args.kind:
            skipped += 1
            continue
        name = p["patient"]
        img, mask = p["image"], p["mask"]
        mask_path = Path(mask["path"])
        print(f"=== {name} ({p['kind']})")
        if p.get("flags"):
            flagged.append((name, p["flags"]))
            print(f"  [flag] {', '.join(p['flags'])}")

        if img.get("format") == "dicom_series":
            dicom_dir = Path(img["path"])
        else:
            dicom_dir = stitched_root / name
            if not dicom_dir.is_dir() or not any(dicom_dir.glob("*.dcm")):
                sys.exit(f"  !! {name}: no converted DICOM series at {dicom_dir}\n"
                         f"     run: python3 nrrd_to_dicom_series.py --manifest "
                         f"{manifest} --out-root {stitched_root}")
            print(f"  using converted DICOM series: {dicom_dir}")
        if not dicom_dir.is_dir():
            sys.exit(f"  !! {name}: DICOM folder not found: {dicom_dir}")
        if not mask_path.is_file():
            sys.exit(f"  !! {name}: mask not found: {mask_path}")

        case = {
            "name": name,
            "dicom_dir": str(dicom_dir),
            "output_prefix": str(prefix_root / f"MCGPU_{name}"),
            "mask_nrrd": str(mask_path),
            "stretchers": [{"cx_mm": 0.0, "auto": True, "cy_offset_mm": 0.0}],
            "auto_head_placement": True,
        }
        uid = pick_series(dicom_dir, mask_path)
        if uid:
            case["series_uid"] = uid
        cases.append(case)

    doc = {
        "binary": "build/dicom_to_mcgpu",
        "template": "params/dicom_to_mcgpu_template.cfg",
        "cwd": ".",
        "cfg_out_dir": args.cfg_dir,
        "stretcher_labels": ["A"],
        "auto_head_placement": True,
        "output_dir": to_linux_path(args.output_dir),
        "mcgpu_in_dir": to_linux_path(args.in_dir),
        "mcgpu_in_template": args.in_template,
        "cases": cases,
    }
    Path(args.output).write_text(json.dumps(doc, indent=2) + "\n")
    print(f"\nWrote {len(cases)} case(s) x 1 stretcher position -> {args.output}")
    if skipped:
        print(f"  (skipped {skipped} entry/entries not of kind '{args.kind}')")
    if flagged:
        print(f"  {len(flagged)} case(s) carry manifest QC flags -- inspect their "
              f"positioning checks with extra care:")
        for name, fl in flagged:
            print(f"    {name}: {', '.join(fl)}")
    print(f"  .raw/.txt -> {doc['output_dir']}")
    print(f"  .in       -> {doc['mcgpu_in_dir']}")
    print(f"\nNext: python3 batch_dicom_to_mcgpu.py run {args.output} --kv-variants")


if __name__ == "__main__":
    main()
