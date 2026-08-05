#!/usr/bin/env python3
"""
Convert an ITK-readable 3-D volume (.nrrd, .nii, .nii.gz, .mha, ...) into a CT
DICOM series that dicom_to_mcgpu can consume as if it were a real patient
series.

Why: dicom_to_mcgpu reads its input through GDCMSeriesFileNames, i.e. a
DIRECTORY OF DICOM SLICES -- it cannot open a single .nrrd.  Some curated
datasets ship pre-built volumes instead of a series (e.g. the CQ500
"stitched" cases in qureai-headct/dataset_manifest.json, joined from two
partial acquisitions).  This writes those out slice-by-slice with the
geometry PRESERVED EXACTLY (origin / spacing / direction cosines), so a
companion segmentation mask in the same physical space still aligns after the
conversion.  Oblique volumes are kept oblique: the true direction goes into
ImageOrientationPatient rather than being resampled away.

Pixel data is written as signed int16 with RescaleIntercept=0 / Slope=1, so
HU values round-trip bit-exactly (no -1024 offset clipping of the -3024
out-of-FOV padding CT scanners write).

Usage
-----
  python3 nrrd_to_dicom_series.py <input_volume> <output_dir> [--name ID]

  # from a dataset manifest (all "kind": "stitched" entries at once)
  python3 nrrd_to_dicom_series.py --manifest dataset_manifest.json \\
      --out-root /mnt/f/.../stitched_dicom

Existing output folders are skipped unless --force is given.
"""

import argparse
import json
import sys
from pathlib import Path

import numpy as np
import SimpleITK as sitk
import pydicom
from pydicom.dataset import Dataset, FileMetaDataset
from pydicom.uid import CTImageStorage, ExplicitVRLittleEndian, generate_uid

def load_volume(path: Path) -> sitk.Image:
    """Read any ITK-supported volume, with a nibabel fallback for NIfTI files
    whose sform is stored too coarsely to pass ITK's orthonormality test.

    TotalSegmentator's ct.nii.gz keeps the scanner's oblique geometry in a
    float32 sform, and that rounding leaves the direction cosines a few 1e-4
    off orthogonal -- ITK refuses to open those at all.  Replacing the matrix
    with its nearest rotation (polar decomposition via SVD) recovers the
    geometry the scanner meant, to well under a voxel across the volume.
    """
    try:
        return sitk.ReadImage(str(path))
    except RuntimeError as exc:
        if "orthonormal" not in str(exc):
            raise

    import nibabel as nib

    nii = nib.load(str(path))
    arr = np.asanyarray(nii.dataobj)                       # [i, j, k]
    # NIfTI world space is RAS+, DICOM/ITK is LPS: negate the first two axes.
    aff = np.diag([-1.0, -1.0, 1.0, 1.0]) @ nii.affine
    mat = aff[:3, :3]
    spacing = np.linalg.norm(mat, axis=0)
    raw_dir = mat / spacing
    u, _, vt = np.linalg.svd(raw_dir)
    direction = u @ vt                                     # nearest rotation

    # How far the fix moves the far corner of the volume -- the honest measure
    # of what was given up, since the per-entry residual alone means little.
    corner = np.array(arr.shape) * spacing
    shift = float(np.linalg.norm((direction - raw_dir) @ corner))
    print(f"  [nibabel] sform not orthonormal; re-orthonormalised "
          f"(corner moves {shift * 1000:.3f} um)")

    img = sitk.GetImageFromArray(np.ascontiguousarray(arr.transpose(2, 1, 0)))
    img.SetSpacing([float(s) for s in spacing])
    img.SetOrigin([float(o) for o in aff[:3, 3]])
    img.SetDirection([float(d) for d in direction.ravel()])
    return img


def convert(volume_path: Path, out_dir: Path, name: str, force: bool = False) -> int:
    """Write volume_path as a CT DICOM series in out_dir.  Returns #slices."""
    img = load_volume(volume_path)

    # Columns of the direction matrix = world direction of each voxel axis.
    d = img.GetDirection()
    axis_i = np.array([d[0], d[3], d[6]])                  # along a row
    axis_j = np.array([d[1], d[4], d[7]])                  # down a column
    axis_k = np.array([d[2], d[5], d[8]])                  # slice normal

    arr = sitk.GetArrayFromImage(img)                      # [k, j, i]
    if arr.dtype != np.int16:
        # HU must survive as int16; refuse silent precision loss.
        lo, hi = float(arr.min()), float(arr.max())
        if lo < -32768 or hi > 32767:
            sys.exit(f"{volume_path.name}: values [{lo}, {hi}] do not fit int16")
        arr = arr.astype(np.int16)

    sx, sy, sz = img.GetSpacing()
    origin = np.array(img.GetOrigin())
    ox, oy, oz = origin
    nz, ny, nx = arr.shape

    if out_dir.exists() and any(out_dir.glob("*.dcm")) and not force:
        print(f"  [skip] {out_dir} already has .dcm files (use --force)")
        return 0
    out_dir.mkdir(parents=True, exist_ok=True)

    # Deterministic UIDs: re-running the conversion reproduces the same series
    # (so a re-converted case is not seen as a different study downstream).
    study_uid  = generate_uid(entropy_srcs=[name, "study"])
    series_uid = generate_uid(entropy_srcs=[name, "series"])
    for_uid    = generate_uid(entropy_srcs=[name, "frame_of_reference"])

    for k in range(nz):
        ds = Dataset()
        ds.file_meta = FileMetaDataset()
        ds.file_meta.MediaStorageSOPClassUID = CTImageStorage
        ds.file_meta.MediaStorageSOPInstanceUID = generate_uid(
            entropy_srcs=[name, "instance", str(k)])
        ds.file_meta.TransferSyntaxUID = ExplicitVRLittleEndian
        ds.file_meta.ImplementationClassUID = generate_uid(entropy_srcs=["nrrd_to_dicom_series"])

        ds.SOPClassUID = CTImageStorage
        ds.SOPInstanceUID = ds.file_meta.MediaStorageSOPInstanceUID
        ds.Modality = "CT"
        ds.PatientName = name
        ds.PatientID = name
        ds.StudyInstanceUID = study_uid
        ds.SeriesInstanceUID = series_uid
        ds.FrameOfReferenceUID = for_uid
        ds.StudyID = "1"
        ds.SeriesNumber = 1
        ds.InstanceNumber = k + 1
        ds.SeriesDescription = f"{volume_path.stem} (converted)"
        ds.StudyDescription = "converted volume"

        # Type 2 attributes of the CT Image IOD: must be PRESENT, may be empty.
        # Omitting them altogether makes strict viewers and PACS reject the
        # series.  Left blank rather than invented -- the source has no dates,
        # and PatientPosition is deliberately absent so no viewer infers L/R
        # from a guess instead of from ImageOrientationPatient.
        ds.StudyDate = ""
        ds.StudyTime = ""
        ds.AccessionNumber = ""
        ds.ReferringPhysicianName = ""
        ds.PatientBirthDate = ""
        ds.PatientSex = ""

        # Geometry -- preserved exactly, oblique directions included.
        pos = origin + k * sz * axis_k
        ds.ImagePositionPatient = [f"{v:.6f}" for v in pos]
        ds.ImageOrientationPatient = [f"{v:.9f}" for v in (*axis_i, *axis_j)]
        ds.PixelSpacing = [f"{sy:.9f}", f"{sx:.9f}"]        # [row, column]
        ds.SliceThickness = f"{sz:.6f}"
        ds.SpacingBetweenSlices = f"{sz:.6f}"
        # For an oblique series slice location is measured along the normal.
        ds.SliceLocation = f"{float(pos @ axis_k):.6f}"

        # Pixel data -- signed HU, no rescale, so values round-trip exactly.
        ds.SamplesPerPixel = 1
        ds.PhotometricInterpretation = "MONOCHROME2"
        ds.Rows = ny
        ds.Columns = nx
        ds.BitsAllocated = 16
        ds.BitsStored = 16
        ds.HighBit = 15
        ds.PixelRepresentation = 1                          # signed
        ds.RescaleIntercept = "0"
        ds.RescaleSlope = "1"
        ds.RescaleType = "HU"
        ds.WindowCenter = "40"
        ds.WindowWidth = "400"
        ds.PixelData = np.ascontiguousarray(arr[k]).tobytes()

        ds.save_as(out_dir / f"IMG{k + 1:05d}.dcm", enforce_file_format=True)

    print(f"  wrote {nz} slice(s) -> {out_dir}  "
          f"({nx}x{ny}x{nz} @ {sx:.4f}x{sy:.4f}x{sz:.4f} mm, "
          f"origin ({ox:.2f}, {oy:.2f}, {oz:.2f}))")

    # Measure each voxel axis against the CLOSEST patient axis, otherwise the
    # RAS->LPS sign flip alone reads as a 180 deg "tilt".
    tilt = max(np.degrees(np.arccos(np.clip(np.abs(a).max(), -1, 1)))
               for a in (axis_i, axis_j, axis_k))
    if tilt > 0.1:
        # Since the direction-cosine fix, dicom_to_mcgpu resolves this into
        # true patient space, so the phantom comes out correctly oriented.
        print(f"  [note] oblique volume: voxel axes are up to {tilt:.2f} deg off "
              f"the patient axes (written to ImageOrientationPatient; "
              f"dicom_to_mcgpu honours it)")
    return nz


def verify(volume_path: Path, out_dir: Path) -> bool:
    """Read the written series back and compare voxels + geometry to the source."""
    src = load_volume(volume_path)
    rdr = sitk.ImageSeriesReader()
    rdr.SetFileNames(rdr.GetGDCMSeriesFileNames(str(out_dir)))
    got = rdr.Execute()

    ok = True
    if got.GetSize() != src.GetSize():
        print(f"  [FAIL] size {got.GetSize()} != {src.GetSize()}"); ok = False
    for tag, a, b in (("spacing", got.GetSpacing(), src.GetSpacing()),
                      ("origin", got.GetOrigin(), src.GetOrigin()),
                      ("direction", got.GetDirection(), src.GetDirection())):
        if max(abs(x - y) for x, y in zip(a, b)) > 1e-3:
            print(f"  [FAIL] {tag} {a} != {b}"); ok = False
    d = sitk.GetArrayFromImage(got).astype(np.int32) - \
        sitk.GetArrayFromImage(src).astype(np.int32)
    if np.any(d):
        print(f"  [FAIL] {int(np.count_nonzero(d))} voxel(s) differ "
              f"(max |diff| = {int(np.abs(d).max())})"); ok = False
    print(f"  verify: {'OK -- voxels and geometry match the source' if ok else 'MISMATCH'}")
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("volume", nargs="?", help="input .nrrd/.nii/.mha volume")
    ap.add_argument("out_dir", nargs="?", help="output DICOM folder")
    ap.add_argument("--name", help="PatientID/PatientName (default: file stem)")
    ap.add_argument("--manifest", help="dataset_manifest.json: convert every "
                                      "entry whose image format is not dicom_series")
    ap.add_argument("--out-root", help="[--manifest] parent folder for the "
                                      "per-patient DICOM folders")
    ap.add_argument("--force", action="store_true", help="overwrite existing output")
    ap.add_argument("--no-verify", action="store_true",
                    help="skip the read-back voxel/geometry comparison")
    args = ap.parse_args()

    jobs = []
    if args.manifest:
        if not args.out_root:
            sys.exit("--manifest requires --out-root")
        doc = json.loads(Path(args.manifest).read_text())
        for p in doc.get("patients", []):
            if p["image"].get("format") == "dicom_series":
                continue
            jobs.append((Path(p["image"]["path"]),
                         Path(args.out_root) / p["patient"], p["patient"]))
    elif args.volume and args.out_dir:
        vol = Path(args.volume)
        jobs.append((vol, Path(args.out_dir), args.name or vol.stem))
    else:
        ap.error("give <volume> <out_dir>, or --manifest with --out-root")

    failures = 0
    for vol, out_dir, name in jobs:
        print(f"=== {name}: {vol}")
        n = convert(vol, out_dir, name, args.force)
        if n and not args.no_verify and not verify(vol, out_dir):
            failures += 1
    print(f"\nConverted {len(jobs)} volume(s), {failures} verification failure(s).")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
