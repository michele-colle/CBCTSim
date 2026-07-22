#!/usr/bin/env python3
"""Register a (moving) DICOM CT onto a reference (fixed) DICOM, then write the
registered result back out as a DICOM series that ``dicom_to_mcgpu`` can convert
to material labels unchanged.

Pipeline (all in physical/mm space, float HU internally):
  1. Read the fixed (reference) and moving (convert-target) DICOM series.
  2. Apply a segmentation NRRD on the MOVING grid: voxels outside the mask are
     set to air (-1000) so only the segmented anatomy drives the registration.
  3. Downsample BOTH images to a coarse grid (--reg-spacing) for registration
     only. A rigid transform does not need full resolution, and this is what
     keeps Elastix within RAM (full-res fixed+moving would blow up to tens of GB).
  4. Rigid registration (ITKElastix) on the downsampled copies -> transform.
  5. Apply that transform to the FULL-RES moving with transformix, resampling
     directly onto a grid centred on the REFERENCE volume's geometric centre and
     covering its extent + margin. The output thus overlays the reference and the
     anatomy is centred in the stack (the downstream converter then centres this
     volume at the isocenter). Avoids a second interpolation of the result.
  6. Write a registered DICOM series (int16 HU, CT, RescaleSlope=1/Intercept=0).

Then convert as usual, e.g.:
  build/dicom_to_mcgpu <cfg-with dicom_dir=<--out folder>>

This reuses the repo's existing itk/ITKElastix toolchain (see registration.py,
apply_transformation.py) and leaves dicom_to_mcgpu.cpp untouched.
"""

import argparse
import math
import os
import shutil
import sys
import time

import itk
import numpy as np

FloatImage = itk.Image[itk.F, 3]
ShortImage = itk.Image[itk.SS, 3]
AIR_HU = -1000.0

# CT Image Storage
CT_SOP_CLASS_UID = "1.2.840.10008.5.1.4.1.1.2"
# ITK/DCMTK-style UID root reserved for locally generated UIDs.
UID_ROOT = "1.2.826.0.1.3680043.2.1125"


# ---------------------------------------------------------------------------
# DICOM series reading (mirrors the discovery in dicom_to_mcgpu.cpp:483-536)
# ---------------------------------------------------------------------------
def _series_in_dir(directory):
    """Return (GDCMSeriesFileNames, first_uid) if a series is found, else None."""
    for details in (True, False):
        gen = itk.GDCMSeriesFileNames.New()
        gen.SetUseSeriesDetails(details)
        gen.SetRecursive(False)
        gen.SetDirectory(str(directory))
        uids = list(gen.GetSeriesUIDs())
        if uids:
            return gen, uids[0]
    return None


def read_dicom_series(directory):
    """Read a DICOM series as a float (HU) image. Falls back to scanning
    subdirectories, like the C++ tool does on WSL/NTFS mounts.

    Returns (image, gdcm_io) — the io holds the last-slice metadata dictionary,
    used to carry patient/study tags into the output series.
    """
    found = _series_in_dir(directory)
    if found is None:
        for root, _dirs, _files in os.walk(str(directory)):
            found = _series_in_dir(root)
            if found is not None:
                break
    if found is None:
        raise RuntimeError(f"No DICOM series found in or under: {directory}")

    gen, uid = found
    file_names = gen.GetFileNames(uid)
    if not file_names:
        raise RuntimeError(f"Series {uid} has no files under: {directory}")

    dicom_io = itk.GDCMImageIO.New()
    reader = itk.ImageSeriesReader[FloatImage].New()
    reader.SetImageIO(dicom_io)
    reader.SetFileNames(file_names)
    reader.ForceOrthogonalDirectionOff()
    reader.Update()
    image = reader.GetOutput()
    image.DisconnectPipeline()
    print(f"  read {len(file_names)} slices from {directory}")
    return image, dicom_io


# ---------------------------------------------------------------------------
# Resampling helpers
# ---------------------------------------------------------------------------
def _interpolator(image, linear):
    if linear:
        return itk.LinearInterpolateImageFunction.New(image)
    return itk.NearestNeighborInterpolateImageFunction.New(image)


def resample_like(image, reference, default_value, linear):
    """Resample ``image`` onto the grid of ``reference`` (physical-space)."""
    return itk.resample_image_filter(
        image,
        use_reference_image=True,
        reference_image=reference,
        interpolator=_interpolator(image, linear),
        default_pixel_value=default_value,
    )


def resample_to_spacing(image, new_spacing, default_value, linear):
    """Resample to ``new_spacing`` while preserving the physical extent, origin
    and orientation (pure re-scaling, no cropping)."""
    in_size = [int(x) for x in itk.size(image)]
    in_spacing = [float(x) for x in itk.spacing(image)]
    out_spacing = [float(s) for s in new_spacing]
    out_size = [max(1, int(round(in_size[i] * in_spacing[i] / out_spacing[i])))
                for i in range(3)]
    return itk.resample_image_filter(
        image,
        size=out_size,
        output_spacing=out_spacing,
        output_origin=[float(x) for x in itk.origin(image)],
        output_direction=image.GetDirection(),
        interpolator=_interpolator(image, linear),
        default_pixel_value=default_value,
    )


# ---------------------------------------------------------------------------
# Step 2: apply the moving segmentation mask (mask==0 -> air)
# ---------------------------------------------------------------------------
def apply_moving_mask(moving, mask_path):
    mask = itk.imread(mask_path, itk.F)
    mask_on_moving = resample_like(mask, moving, default_value=0.0, linear=False)
    m = itk.array_view_from_image(mask_on_moving)
    v = itk.array_view_from_image(moving)   # shares moving's buffer
    outside = m < 0.5
    v[outside] = AIR_HU
    kept = int((~outside).sum())
    print(f"  segmentation applied: {kept} voxels kept, "
          f"{int(outside.sum())} set to air")


# ---------------------------------------------------------------------------
# Step 4: rigid (or affine/bspline) registration via ITKElastix.
# Runs on DOWNSAMPLED copies (see main) so Elastix stays within RAM; returns the
# transform parameter object, which is applied to the full-res moving later.
# ---------------------------------------------------------------------------
def register(fixed, moving, reg_type, iterations, resolutions,
             transform_out, verbose):
    par_obj = itk.ParameterObject.New()
    par_map = par_obj.GetDefaultParameterMap(reg_type)
    if reg_type in ("rigid", "affine"):
        par_map["Registration"] = ["MultiResolutionRegistration"]
    par_map["NumberOfResolutions"] = [str(resolutions)]
    par_map["MaximumNumberOfIterations"] = [str(iterations)]
    # Snap the volume centres together before optimising (robust init).
    par_map["AutomaticTransformInitialization"] = ["true"]
    par_map["AutomaticTransformInitializationMethod"] = ["GeometricalCenter"]
    par_map["DefaultPixelValue"] = [str(int(AIR_HU))]
    par_map["ResultImagePixelType"] = ["float"]
    par_obj.AddParameterMap(par_map)

    reg = itk.ElastixRegistrationMethod.New(fixed, moving)
    reg.SetParameterObject(par_obj)
    if verbose:
        reg.LogToConsoleOn()
    else:
        reg.LogToConsoleOff()
    reg.Update()

    tp = reg.GetTransformParameterObject()
    if transform_out:
        tp.WriteParameterFile(tp.GetParameterMap(0), str(transform_out))
        print(f"  transform written: {transform_out}")
    return tp


def _physical_bbox(image):
    """Physical bounding box (min/max over the 8 corner voxel centres)."""
    size = [int(x) for x in itk.size(image)]
    mins = [math.inf] * 3
    maxs = [-math.inf] * 3
    for cx in (0, size[0] - 1):
        for cy in (0, size[1] - 1):
            for cz in (0, size[2] - 1):
                idx = itk.Index[3]()
                idx[0], idx[1], idx[2] = cx, cy, cz
                p = image.TransformIndexToPhysicalPoint(idx)
                for i in range(3):
                    mins[i] = min(mins[i], p[i])
                    maxs[i] = max(maxs[i], p[i])
    return mins, maxs


def _grid_from_bounds(center, mins, maxs, out_spacing, margin_mm):
    """Grid centred on `center`, symmetric, large enough to cover [mins,maxs]
    (plus margin).  Keeps the centre fixed while growing the extent."""
    out_spacing = [float(s) for s in out_spacing]
    out_size = []
    for i in range(3):
        half = max(abs(mins[i] - center[i]), abs(maxs[i] - center[i])) + margin_mm
        n = int(math.ceil(2.0 * half / out_spacing[i]))
        if n % 2:
            n += 1
        out_size.append(max(n, 2))
    out_origin = [center[i] - (out_size[i] - 1) / 2.0 * out_spacing[i]
                  for i in range(3)]
    return out_size, out_spacing, out_origin


def _resample_with_transform(moving, tp, out_size, out_spacing, out_origin):
    """Run transformix, resampling `moving` onto an explicit output grid
    (identity direction, linear interp -> no HU overshoot)."""
    pm = tp.GetParameterMap(0)
    pm["Size"] = [str(int(s)) for s in out_size]
    pm["Index"] = ["0", "0", "0"]
    pm["Spacing"] = [str(float(s)) for s in out_spacing]
    pm["Origin"] = [str(float(o)) for o in out_origin]
    pm["Direction"] = ["1", "0", "0", "0", "1", "0", "0", "0", "1"]
    pm["FinalBSplineInterpolationOrder"] = ["1"]   # linear
    pm["DefaultPixelValue"] = [str(int(AIR_HU))]
    pm["ResultImagePixelType"] = ["float"]
    tp.SetParameterMap(0, pm)
    result = itk.transformix_filter(moving, transform_parameter_object=tp)
    result.DisconnectPipeline()
    return result


# ---------------------------------------------------------------------------
# Step 5: compute an output grid centred on the REFERENCE volume's geometric
# centre, but sized to cover BOTH the reference FOV and the registered moving
# anatomy (so a phantom longer than the reference in any axis is not clipped).
# A cheap coarse transformix "probe" pass measures where the moving actually
# lands in the reference frame.  The centre stays on the reference so the output
# still overlays it and the downstream converter centres the feature correctly.
# ---------------------------------------------------------------------------
def compute_output_grid(reference, moving, tp, out_spacing, margin_mm,
                        probe_spacing=3.0):
    out_spacing = [float(s) for s in out_spacing]
    fmins, fmaxs = _physical_bbox(reference)
    center = [(fmins[i] + fmaxs[i]) / 2.0 for i in range(3)]
    print(f"  reference centre: {[round(c, 2) for c in center]} mm")

    # Generous coarse probe grid centred on the reference centre, guaranteed to
    # contain the registered moving wherever it lands near the reference.
    msize = [itk.size(moving)[i] * itk.spacing(moving)[i] for i in range(3)]
    mmax = max(msize)
    pmins = [center[i] - (abs(fmaxs[i] - center[i]) + mmax + margin_mm)
             for i in range(3)]
    pmaxs = [center[i] + (abs(fmaxs[i] - center[i]) + mmax + margin_mm)
             for i in range(3)]
    p_size, p_sp, p_org = _grid_from_bounds(
        center, pmins, pmaxs, [probe_spacing] * 3, 0.0)
    probe = _resample_with_transform(moving, tp, p_size, p_sp, p_org)

    # Actual moving-content bbox in the reference frame (non-air voxels).
    arr = itk.array_view_from_image(probe)          # (z, y, x)
    mask = arr > (AIR_HU + 100.0)
    if mask.any():
        zz, yy, xx = np.where(mask)
        lo = [int(xx.min()), int(yy.min()), int(zz.min())]
        hi = [int(xx.max()), int(yy.max()), int(zz.max())]
        cmins = [math.inf] * 3
        cmaxs = [-math.inf] * 3
        for cx in (lo[0], hi[0]):
            for cy in (lo[1], hi[1]):
                for cz in (lo[2], hi[2]):
                    idx = itk.Index[3]()
                    idx[0], idx[1], idx[2] = cx, cy, cz
                    p = probe.TransformIndexToPhysicalPoint(idx)
                    for i in range(3):
                        cmins[i] = min(cmins[i], p[i])
                        cmaxs[i] = max(cmaxs[i], p[i])
        touched = any(lo[i] == 0 or hi[i] == p_size[i] - 1 for i in range(3))
        if touched:
            print("  WARNING: moving content reached the probe boundary; "
                  "output extent may still clip. Increase --margin-mm.")
        print(f"  moving content bbox: "
              f"{[round(v, 1) for v in cmins]} .. {[round(v, 1) for v in cmaxs]} mm")
    else:
        print("  WARNING: no moving content found in probe; using reference FOV.")
        cmins, cmaxs = fmins, fmaxs

    # Final grid: centred on the reference, covering the union of the reference
    # FOV and the moving content, plus margin.
    umins = [min(fmins[i], cmins[i]) for i in range(3)]
    umaxs = [max(fmaxs[i], cmaxs[i]) for i in range(3)]
    return _grid_from_bounds(center, umins, umaxs, out_spacing, margin_mm)


# ---------------------------------------------------------------------------
# Step 5b: apply the transform to the full-res moving on the final output grid.
# ---------------------------------------------------------------------------
def apply_transformix(moving, tp, out_size, out_spacing, out_origin):
    result = _resample_with_transform(moving, tp, out_size, out_spacing, out_origin)
    gc = [round(out_origin[i] + (out_size[i] - 1) / 2.0 * out_spacing[i], 2)
          for i in range(3)]
    print(f"  output grid: {out_size} @ {out_spacing} mm, "
          f"origin {[round(o, 2) for o in out_origin]}, centre {gc} mm")
    return result


# ---------------------------------------------------------------------------
# Cast float HU -> int16, preserving geometry
# ---------------------------------------------------------------------------
def to_int16(image):
    arr = itk.array_from_image(image)
    arr = np.clip(np.rint(arr), -32768, 32767).astype(np.int16)
    out = itk.image_from_array(np.ascontiguousarray(arr))
    out.SetSpacing(image.GetSpacing())
    out.SetOrigin(image.GetOrigin())
    out.SetDirection(image.GetDirection())
    return out


# ---------------------------------------------------------------------------
# Step 6: write a DICOM series dicom_to_mcgpu can read
# ---------------------------------------------------------------------------
def _collect_patient_tags(dicom_io):
    """Carry a whitelist of patient/study identity tags from the moving series.
    UIDs are deliberately NOT copied — the output gets fresh ones."""
    tags = {}
    if dicom_io is None:
        return tags
    md = dicom_io.GetMetaDataDictionary()
    wanted = ["0010|0010", "0010|0020", "0010|0030", "0010|0040",
              "0008|0020", "0008|0030", "0008|1030", "0008|0050"]
    for key in wanted:
        try:
            val = md[key]
        except Exception:
            continue
        if isinstance(val, str) and val.strip():
            tags[key] = val
    return tags


def clear_output_dir(path, protected):
    """Wipe the output folder before writing (no prompt, just a warning).
    Refuses to touch the input directories as a safety guard."""
    abspath = os.path.abspath(path)
    if abspath == os.sep or len(abspath) < 4:
        sys.exit(f"ERROR: refusing to clear suspicious output path: {abspath}")
    for prot in protected:
        if prot and os.path.abspath(prot) == abspath:
            sys.exit(f"ERROR: output folder equals an input path: {abspath}")

    if os.path.isdir(path):
        entries = os.listdir(path)
        if entries:
            print(f"  WARNING: output folder is not empty - clearing "
                  f"{len(entries)} item(s) in {path}")
            for name in entries:
                p = os.path.join(path, name)
                if os.path.isdir(p) and not os.path.islink(p):
                    shutil.rmtree(p)
                else:
                    os.remove(p)
    os.makedirs(path, exist_ok=True)


def write_dicom_series(image16, out_dir, patient_tags=None):
    os.makedirs(out_dir, exist_ok=True)
    patient_tags = patient_tags or {}

    size = [int(x) for x in itk.size(image16)]
    spacing = [float(x) for x in itk.spacing(image16)]
    nx, ny, nz = size

    stamp = int(time.time())
    study_uid = f"{UID_ROOT}.{stamp}.1"
    series_uid = f"{UID_ROOT}.{stamp}.2"
    frame_uid = f"{UID_ROOT}.{stamp}.3"

    arr = itk.array_from_image(image16)  # shape (nz, ny, nx)
    for k in range(nz):
        slice2d = itk.image_from_array(np.ascontiguousarray(arr[k]))
        slice2d.SetSpacing([spacing[0], spacing[1]])
        idx = itk.Index[3]()
        idx[0], idx[1], idx[2] = 0, 0, k
        ipp = image16.TransformIndexToPhysicalPoint(idx)
        slice2d.SetOrigin([ipp[0], ipp[1]])

        md = slice2d.GetMetaDataDictionary()
        for key, val in patient_tags.items():
            md[key] = val
        md["0008|0060"] = "CT"                     # Modality
        md["0028|1052"] = "0"                       # Rescale Intercept
        md["0028|1053"] = "1"                       # Rescale Slope
        md["0028|1054"] = "HU"                      # Rescale Type
        md["0020|000d"] = study_uid                 # Study Instance UID
        md["0020|000e"] = series_uid                # Series Instance UID
        md["0020|0052"] = frame_uid                 # Frame of Reference UID
        md["0008|0016"] = CT_SOP_CLASS_UID          # SOP Class UID
        md["0008|0018"] = f"{series_uid}.{k + 1}"   # SOP Instance UID (unique)
        md["0020|0037"] = "1\\0\\0\\0\\1\\0"        # Image Orientation (Patient)
        md["0020|0032"] = f"{ipp[0]}\\{ipp[1]}\\{ipp[2]}"  # Image Position
        md["0028|0030"] = f"{spacing[0]}\\{spacing[1]}"    # Pixel Spacing
        md["0018|0050"] = f"{spacing[2]}"           # Slice Thickness
        md["0020|1041"] = f"{ipp[2]}"               # Slice Location
        md["0020|0013"] = str(k + 1)                # Instance Number
        md["0008|0008"] = "DERIVED\\SECONDARY\\AXIAL"

        dio = itk.GDCMImageIO.New()
        dio.SetMetaDataDictionary(md)
        dio.KeepOriginalUIDOn()   # honour the UIDs we set instead of regenerating
        fname = os.path.join(out_dir, f"CT.{k + 1:04d}.dcm")
        itk.imwrite(slice2d, fname, imageio=dio)

    print(f"  wrote {nz} DICOM slices to {out_dir}")
    print(f"    Series Instance UID: {series_uid}")


# ---------------------------------------------------------------------------
def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Register a moving DICOM onto a reference DICOM and write "
                    "the registered result as a DICOM series for dicom_to_mcgpu.")
    ap.add_argument("--fixed", required=True,
                    help="Reference (fixed) DICOM series directory")
    ap.add_argument("--moving", required=True,
                    help="Convert-target (moving) DICOM series directory")
    ap.add_argument("--out", required=True,
                    help="Output directory for the registered DICOM series")
    ap.add_argument("--mask", default=None,
                    help="Segmentation NRRD on the MOVING grid; voxels==0 -> air")
    ap.add_argument("--reg-type", default="rigid",
                    choices=["rigid", "affine", "bspline"],
                    help="Elastix registration type (default: rigid)")
    ap.add_argument("--margin-mm", type=float, default=20.0,
                    help="Air margin around the anatomy in the output (mm)")
    ap.add_argument("--out-spacing", type=float, nargs=3, default=None,
                    metavar=("SX", "SY", "SZ"),
                    help="Output voxel spacing X Y Z (mm), per-axis. "
                         "Default: the fixed series spacing.")
    ap.add_argument("--reg-spacing", type=float, default=1.5,
                    help="Isotropic voxel spacing (mm) the images are "
                         "downsampled to FOR REGISTRATION ONLY, to bound RAM "
                         "(default: 1.5). A rigid transform does not need tiny "
                         "voxels. The transform is then applied to the full-res "
                         "moving. Increase if Elastix still uses too much RAM.")
    ap.add_argument("--iterations", type=int, default=500)
    ap.add_argument("--resolutions", type=int, default=3)
    ap.add_argument("--transform-out", default=None,
                    help="Where to write the Elastix transform parameters "
                         "(default: <out>/registration_transform.txt)")
    ap.add_argument("--save-nrrd", action="store_true",
                    help="Also write <out>/registered_hu.nrrd for QC in Slicer")
    ap.add_argument("--verbose", action="store_true",
                    help="Stream Elastix optimisation log to the console")
    args = ap.parse_args(argv)

    transform_out = args.transform_out or os.path.join(
        args.out, "registration_transform.txt")
    clear_output_dir(args.out, protected=[args.fixed, args.moving, args.mask])

    print("[1/6] Reading DICOM series...")
    fixed, _fixed_io = read_dicom_series(args.fixed)
    moving, moving_io = read_dicom_series(args.moving)

    if args.mask:
        print("[2/6] Applying moving segmentation...")
        apply_moving_mask(moving, args.mask)
    else:
        print("[2/6] No --mask given; skipping segmentation step.")

    fixed_spacing = [float(x) for x in itk.spacing(fixed)]
    # Output at the fixed (reference) resolution by default — the reference is
    # the target, so there is no point keeping the finer moving resolution.
    if args.out_spacing:
        out_spacing = [float(s) for s in args.out_spacing]
        print(f"  output resolution: {out_spacing} mm (from --out-spacing)")
    else:
        out_spacing = fixed_spacing
        print(f"  output resolution: {out_spacing} mm (matching fixed volume)")

    # Downsample BOTH images to a coarse isotropic grid for registration only.
    # This is what keeps Elastix within RAM: a rigid transform does not need
    # full resolution, and we apply the result to the full-res moving afterwards.
    reg_spacing = [args.reg_spacing] * 3
    print(f"[3/6] Downsampling to {args.reg_spacing} mm for registration...")
    fixed_reg = resample_to_spacing(fixed, reg_spacing, AIR_HU, linear=True)
    moving_reg = resample_to_spacing(moving, reg_spacing, AIR_HU, linear=True)
    print(f"  registration grids: fixed {list(itk.size(fixed_reg))}, "
          f"moving {list(itk.size(moving_reg))}")

    print(f"[4/6] {args.reg_type} registration (ITKElastix)...")
    tp = register(fixed_reg, moving_reg, args.reg_type,
                  args.iterations, args.resolutions,
                  transform_out, args.verbose)

    print(f"[5/6] Applying transform to full-res moving + centering on "
          f"reference volume centre (margin {args.margin_mm} mm)...")
    out_size, out_spacing, out_origin = compute_output_grid(
        fixed, moving, tp, out_spacing, args.margin_mm)
    n_vox = out_size[0] * out_size[1] * out_size[2]
    if n_vox > 1_500_000_000:
        sys.exit(f"ERROR: output grid is {out_size} = {n_vox/1e9:.1f} G voxels "
                 f"(~{2*n_vox/1e9:.0f} GB int16). Increase --out-spacing to "
                 f"reduce it (current output spacing {out_spacing} mm).")
    registered = apply_transformix(moving, tp, out_size, out_spacing, out_origin)
    enlarged16 = to_int16(registered)

    if args.save_nrrd:
        nrrd_path = os.path.join(args.out, "registered_hu.nrrd")
        itk.imwrite(enlarged16, nrrd_path)
        print(f"  QC volume written: {nrrd_path}")

    print("[6/6] Writing registered DICOM series...")
    write_dicom_series(enlarged16, args.out,
                       _collect_patient_tags(moving_io))

    print("\nDone. Convert with a cfg whose dicom_dir points at:")
    print(f"  {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
