#!/usr/bin/env python3
"""
Render one central-sagittal-slice PNG per DICOM series folder, for a quick
visual pass over a batch of extractions (e.g. VSD_dicom, MCRP_dicom) without
opening each series in a viewer.

Takes the real slice at the volume's mid X-index (not a projection/MIP) --
what you'd see scrubbing to the middle of a sagittal reformat.

Usage
-----
  python3 make_sagittal_check_pngs.py /mnt/h/MICHELE_MCGPU/VSD_dicom
  python3 make_sagittal_check_pngs.py /mnt/h/MICHELE_MCGPU/VSD_dicom \\
      --out sagittal_checks_vsd --vmin -200 --vmax 1000
"""

import argparse
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import SimpleITK as sitk


def render_one(series_dir: Path, out_path: Path, vmin: float, vmax: float) -> bool:
    reader = sitk.ImageSeriesReader()
    files = reader.GetGDCMSeriesFileNames(str(series_dir))
    if not files:
        return False
    reader.SetFileNames(files)
    img = reader.Execute()
    arr = sitk.GetArrayFromImage(img)          # [z, y, x]
    sx, sy, sz = img.GetSpacing()

    mid_x = arr.shape[2] // 2
    sagittal = arr[:, :, mid_x]                # [z, y] -- superior/inferior x anterior/posterior

    extent = [0, sagittal.shape[1] * sy, 0, sagittal.shape[0] * sz]
    fig_h = max(4.0, min(16.0, extent[3] / 60.0))
    fig, ax = plt.subplots(figsize=(fig_h * extent[1] / extent[3] + 1.5, fig_h))
    ax.imshow(sagittal, cmap="gray", vmin=vmin, vmax=vmax, origin="lower",
              extent=extent, aspect="equal")
    ax.set_title(f"{series_dir.name}  (x={mid_x}/{arr.shape[2]})", fontsize=8)
    ax.set_xlabel("Y (mm)", fontsize=7)
    ax.set_ylabel("Z (mm)", fontsize=7)
    ax.tick_params(labelsize=6)
    plt.tight_layout()
    plt.savefig(out_path, dpi=110)
    plt.close(fig)
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("root", help="folder containing one subfolder per DICOM series")
    ap.add_argument("--out", default=None,
                     help="output folder for PNGs (default: <root>_sagittal_checks, sibling of root)")
    ap.add_argument("--vmin", type=float, default=-200.0, help="window lower bound [HU]")
    ap.add_argument("--vmax", type=float, default=1000.0, help="window upper bound [HU]")
    args = ap.parse_args()

    root = Path(args.root)
    out_dir = Path(args.out) if args.out else root.parent / f"{root.name}_sagittal_checks"
    out_dir.mkdir(parents=True, exist_ok=True)

    series_dirs = sorted(p for p in root.iterdir() if p.is_dir())
    ok, failed = 0, []
    for d in series_dirs:
        out_path = out_dir / f"{d.name}.png"
        try:
            if render_one(d, out_path, args.vmin, args.vmax):
                ok += 1
                print(f"{d.name}: OK")
            else:
                failed.append((d.name, "no DICOM files found"))
                print(f"{d.name}: no DICOM files found")
        except Exception as exc:
            failed.append((d.name, str(exc)))
            print(f"{d.name}: FAILED -- {exc}")

    print(f"\n{ok} rendered -> {out_dir}, {len(failed)} failed.")
    for name, msg in failed:
        print(f"  {name}: {msg}")


if __name__ == "__main__":
    main()
