#!/usr/bin/env python3
"""
Extract a single leg (mid-thigh to toe tip) from a VSD Full Body
(Zenodo 8302449) case and write it out as a CT DICOM series, with the scan
table/stretcher removed.

Why: VSD ships each case as a Pelvis-Thighs crop (has the femurs) and a
Shanks-Feet crop (has the foot bones) -- never a single volume spanning
mid-thigh to toe tip. The two crops overlap around the knee, and for the
z* cases both are stored top/bottom-inverted relative to everything else
(see VSD_FULLBODY_INVENTORY.md), so there is no fixed sign convention to
hardcode. A few cases (010/015/016/017) ship no crops at all -- just two
full raw series, one covering each region.

Approach, per case + leg side:
  1. Locate the "upper" (Pelvis-Thighs-like) and "lower" (Shanks-Feet-like)
     volume+segmentation pair, whichever files they happen to be.
  2. Read the segment name -> label value map straight out of each .seg.nrrd
     text header (label numbering is not consistent across cases).
  3. Mid-thigh Z = midpoint of Femur_<side>'s bounding box (upper volume).
     Ankle Z = Talus_<side> bbox center (lower volume) -- used only to learn
     which direction is "toward the foot" for this case's Z convention.
     Toe tip = the Phalanges_<side> bbox extreme farthest from the ankle in
     that direction, pushed 10 mm further out.
  4. X/Y bbox = union of this leg's bone labels in both volumes, padded by a
     margin, clamped short of the other leg's nearest bone bbox.
  5. Resample both source volumes onto one output grid spanning that box
     (nearest-neighbour validity masks track where each source actually has
     data), splitting the overlap at its midpoint between thigh and toe.
  6. Threshold at -300 HU, keep only the largest connected component (the
     leg -- the table shows up as a separate, thinner, full-width/length
     component) and blank everything else to air.
  7. Write the result as a DICOM series via nrrd_to_dicom_series.convert().

Usage
-----
  python3 extract_leg_dicom.py --root <zenodo_8302449_dir> --out-root <out_dir>
  python3 extract_leg_dicom.py --root ... --out-root ... --only 006 z001
  python3 extract_leg_dicom.py --root ... --out-root ... --dry-run
"""

import argparse
import re
import sys
from pathlib import Path

import numpy as np
import SimpleITK as sitk
from scipy import ndimage

sys.path.insert(0, str(Path(__file__).resolve().parent))
from nrrd_to_dicom_series import convert as write_dicom_series, verify as verify_dicom_series

AIR = -1024
THRESHOLD_HU = -300
XY_MARGIN_MM = 40.0
XY_LEG_GAP_MM = 15.0
Z_TOE_MARGIN_MM = 10.0
EXCLUDE_CASES = {
    "Phantom001",  # calibration object, not a human leg
    "z036",        # Femur_L/R segmentation doesn't separate the femur from the
                    # rest of the pelvis (spans ~500mm, overlapping X ranges) --
                    # breaks the mid-thigh landmark; see VSD_LEG_EXTRACTION.md
}


def parse_segment_labels(seg_path: Path) -> dict:
    """Segment name -> (label value, layer), straight from the .seg.nrrd text
    header. A case with spatially overlapping segments (z050) gets exported
    as a 4-D labelmap (an extra "list" axis of stacked layers) since two
    segments can't share a voxel in a single 3-D layer -- Layer says which
    slab of that stack a given segment's label value lives in."""
    with open(seg_path, "rb") as f:
        header = f.read(60_000).decode("latin-1")
    end = header.find("\n\n")
    if end != -1:
        header = header[:end]
    names = dict(re.findall(r"Segment(\d+)_Name:=(.+)", header))
    values = dict(re.findall(r"Segment(\d+)_LabelValue:=(\d+)", header))
    layers = dict(re.findall(r"Segment(\d+)_Layer:=(\d+)", header))
    return {names[k].strip(): (int(values[k]), int(layers.get(k, 0)))
            for k in names if k in values}


def bbox_phys(img: sitk.Image, arr: np.ndarray, label_layer):
    """Physical-space (min, max) corner of a label's voxels, or None."""
    if label_layer is None:
        return None
    label, layer = label_layer
    if arr.ndim == 4:
        arr = arr[..., layer]
    kk, jj, ii = np.where(arr == label)
    if len(ii) == 0:
        return None
    corners = [img.TransformIndexToPhysicalPoint((int(i), int(j), int(k)))
               for i in (int(ii.min()), int(ii.max()))
               for j in (int(jj.min()), int(jj.max()))
               for k in (int(kk.min()), int(kk.max()))]
    corners = np.array(corners)
    return corners.min(axis=0), corners.max(axis=0)


def normalize_mirror_direction(img: sitk.Image) -> sitk.Image:
    """Undo a pure point-reflection header (diagonal +-1 direction, origin
    negated on the same axes) if present, else return img unchanged.

    Discovered on z063's *raw* Pelvis-Thighs volume (not just a segmentation
    -- direction (-1,0,0, 0,1,0, 0,0,-1), origin also negated on X/Z). Its
    paired Shanks-Feet volume is normal, so the two ended up in inconsistent
    frames: the anchor computed from the (mirrored, uncorrected) upper volume
    didn't line up with the lower volume's own bone positions, and
    resolve_bbox silently picked the wrong-side foot/shank bones for one leg
    (same Tibia_R bbox came back for both the L and R query on z063) --
    producing a ~24mm-wide, badly mispositioned crop with no error raised.
    Every other raw volume seen in this dataset already has identity
    direction, so this is a no-op for them."""
    d = img.GetDirection()
    identity = (1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0)
    if d == identity:
        return img
    diag = (d[0], d[4], d[8])
    off_diag_zero = all(abs(d[i]) < 1e-9 for i in (1, 2, 3, 5, 6, 7))
    if not off_diag_zero or any(abs(abs(v) - 1.0) > 1e-6 for v in diag):
        raise RuntimeError(f"unexpected non-diagonal direction matrix {d}")
    origin = list(img.GetOrigin())
    for ax in range(3):
        if diag[ax] < 0:
            origin[ax] = -origin[ax]
    out = sitk.Image(img)
    out.SetOrigin(tuple(origin))
    out.SetDirection(identity)
    return out


def find_case_volumes(case_dir: Path):
    """-> {'upper': (raw_path, seg_path), 'lower': (raw_path, seg_path)}"""
    found = {}
    for seg in case_dir.rglob("*_Segmentation.seg.nrrd"):
        if "Pelvis-Thighs" in seg.name:
            kind = "upper"
        elif "Shanks-Feet" in seg.name:
            kind = "lower"
        else:
            continue
        raw_candidates = [f for f in seg.parent.glob("*.nrrd")
                           if "_Segmentation" not in f.name and "_Reconstruction" not in f.name]
        marker = "Pelvis-Thighs" if kind == "upper" else "Shanks-Feet"
        preferred = [f for f in raw_candidates if marker in f.name]
        raw = preferred[0] if preferred else (raw_candidates[0] if len(raw_candidates) == 1 else None)
        if raw is not None:
            found[kind] = (raw, seg)
    return found


def resolve_bbox(seg_img, seg_arr, labels_map: dict, base: str, side: str, anchor: dict):
    """Bbox for <base>_<side>, chosen by spatial proximity to anchor[side]
    rather than trusting the label name. At least one case (z001) ships
    Patella_L/Patella_R swapped relative to Femur_L/Femur_R, so name alone
    is not trustworthy for the secondary bones."""
    other = "R" if side == "L" else "L"
    candidates = []
    for nm in (f"{base}_{side}", f"{base}_{other}"):
        lab = labels_map.get(nm)
        if lab is None:
            continue
        bb = bbox_phys(seg_img, seg_arr, lab)
        if bb is None:
            continue
        cx = (bb[0][0] + bb[1][0]) / 2.0
        candidates.append((abs(cx - anchor[side]), bb))
    if not candidates:
        return None
    candidates.sort(key=lambda c: c[0])
    return candidates[0][1]


UPPER_XY_BASES = ["Patella"]
LOWER_XY_BASES = ["Tibia", "Fibula", "Talus", "Calcaneus", "Tarsals", "Metatarsals", "Phalanges"]


def make_validity_mask(src_img: sitk.Image, ref: sitk.Image) -> sitk.Image:
    ones = sitk.Image(src_img.GetSize(), sitk.sitkUInt8)
    ones.CopyInformation(src_img)
    ones += 1
    return sitk.Resample(ones, ref, sitk.Transform(), sitk.sitkNearestNeighbor, 0)


def extract_leg(case: str, case_dir: Path, side: str, out_root: Path, force: bool):
    L = "L" if side == "L" else "R"
    O = "R" if side == "L" else "L"  # opposite side, for the inter-leg clamp

    vols = find_case_volumes(case_dir)
    if "upper" not in vols or "lower" not in vols:
        raise RuntimeError(f"missing upper/lower volume(s): found {list(vols)}")

    upper_path, upper_seg_path = vols["upper"]
    lower_path, lower_seg_path = vols["lower"]

    upper_img = normalize_mirror_direction(sitk.ReadImage(str(upper_path)))
    lower_img = normalize_mirror_direction(sitk.ReadImage(str(lower_path)))
    upper_seg = sitk.ReadImage(str(upper_seg_path))
    lower_seg = sitk.ReadImage(str(lower_seg_path))

    # Some cases (the z* ones) ship a segmentation whose own header claims a
    # mirrored frame relative to its raw volume (direction/origin flipped on
    # X and Z) even though the arrays are voxel-for-voxel the same size --
    # a metadata bug in how they were exported. Trust the raw volume's
    # geometry whenever the grids match voxel-for-voxel.
    for seg, raw, tag in ((upper_seg, upper_img, "upper"), (lower_seg, lower_img, "lower")):
        if seg.GetSize() != raw.GetSize():
            raise RuntimeError(f"{tag} segmentation size {seg.GetSize()} != raw {raw.GetSize()}")
        if seg.GetDirection() != raw.GetDirection() or seg.GetOrigin() != raw.GetOrigin():
            seg.CopyInformation(raw)
    upper_labels = parse_segment_labels(upper_seg_path)
    lower_labels = parse_segment_labels(lower_seg_path)
    upper_arr = sitk.GetArrayFromImage(upper_seg)
    lower_arr = sitk.GetArrayFromImage(lower_seg)

    femur_bb_side = {}
    for s in ("L", "R"):
        lab = upper_labels.get(f"Femur_{s}")
        bb = bbox_phys(upper_seg, upper_arr, lab) if lab is not None else None
        if bb is None:
            raise RuntimeError(f"Femur_{s} not present in {upper_seg_path.name}")
        femur_bb_side[s] = bb
    # Femur is the one bone unlikely to ever be mislabeled L/R -- use its
    # X-center as the ground-truth anchor every other per-leg label is
    # resolved against (see resolve_bbox).
    anchor = {s: (bb[0][0] + bb[1][0]) / 2.0 for s, bb in femur_bb_side.items()}

    femur_bb = femur_bb_side[L]
    mid_thigh_z = (femur_bb[0][2] + femur_bb[1][2]) / 2.0

    talus_bb = resolve_bbox(lower_seg, lower_arr, lower_labels, "Talus", L, anchor)
    if talus_bb is None:
        raise RuntimeError(f"Talus_{L} not resolvable in {lower_seg_path.name}")

    # Phalanges is the usual toe-tip landmark, but at least one case (z066)
    # ships it as an empty segment (declared in the header, zero voxels
    # painted -- matches its missing .ply mesh). Fall back to Metatarsals,
    # with extra margin: the real toes are still present in the raw CT and
    # will be picked up by the HU threshold once the crop reaches them.
    toe_base, toe_margin = "Phalanges", Z_TOE_MARGIN_MM
    phal_bb = resolve_bbox(lower_seg, lower_arr, lower_labels, toe_base, L, anchor)
    if phal_bb is None:
        toe_base, toe_margin = "Metatarsals", Z_TOE_MARGIN_MM + 40.0
        phal_bb = resolve_bbox(lower_seg, lower_arr, lower_labels, toe_base, L, anchor)
    if phal_bb is None:
        raise RuntimeError(f"no toe landmark (Phalanges/Metatarsals)_{L} resolvable in {lower_seg_path.name}")

    ankle_z = (talus_bb[0][2] + talus_bb[1][2]) / 2.0
    direction = 1.0 if ankle_z >= mid_thigh_z else -1.0
    toe_extreme_z = phal_bb[1][2] if direction > 0 else phal_bb[0][2]
    toe_tip_z = toe_extreme_z + direction * toe_margin

    z_lo, z_hi = sorted((mid_thigh_z, toe_tip_z))

    def side_xy_bbox(side):
        lo, hi = femur_bb_side[side][0][:2].copy(), femur_bb_side[side][1][:2].copy()
        for base in UPPER_XY_BASES:
            bb = resolve_bbox(upper_seg, upper_arr, upper_labels, base, side, anchor)
            if bb is not None:
                lo, hi = np.minimum(lo, bb[0][:2]), np.maximum(hi, bb[1][:2])
        for base in LOWER_XY_BASES:
            bb = resolve_bbox(lower_seg, lower_arr, lower_labels, base, side, anchor)
            if bb is not None:
                lo, hi = np.minimum(lo, bb[0][:2]), np.maximum(hi, bb[1][:2])
        return lo, hi

    xy_lo, xy_hi = side_xy_bbox(L)
    other_lo, other_hi = side_xy_bbox(O)

    x_lo, y_lo = xy_lo - XY_MARGIN_MM
    x_hi, y_hi = xy_hi + XY_MARGIN_MM
    if anchor[L] > anchor[O]:
        x_lo = max(x_lo, other_hi[0] + XY_LEG_GAP_MM)
    else:
        x_hi = min(x_hi, other_lo[0] - XY_LEG_GAP_MM)

    sx, sy, sz = upper_img.GetSpacing()
    sz = abs(lower_img.GetSpacing()[2])
    origin = (float(x_lo), float(y_lo), float(z_lo))
    size = (max(1, int(round((x_hi - x_lo) / sx)) + 1),
            max(1, int(round((y_hi - y_lo) / sy)) + 1),
            max(1, int(round((z_hi - z_lo) / sz)) + 1))

    ref = sitk.Image(size, sitk.sitkInt16)
    ref.SetSpacing((sx, sy, sz))
    ref.SetOrigin(origin)
    ref.SetDirection((1, 0, 0, 0, 1, 0, 0, 0, 1))

    upper_rs = sitk.Resample(upper_img, ref, sitk.Transform(), sitk.sitkLinear, AIR, sitk.sitkInt16)
    lower_rs = sitk.Resample(lower_img, ref, sitk.Transform(), sitk.sitkLinear, AIR, sitk.sitkInt16)
    upper_valid = sitk.GetArrayFromImage(make_validity_mask(upper_img, ref)).astype(bool)
    lower_valid = sitk.GetArrayFromImage(make_validity_mask(lower_img, ref)).astype(bool)
    upper_arr_rs = sitk.GetArrayFromImage(upper_rs)
    lower_arr_rs = sitk.GetArrayFromImage(lower_rs)

    nz = size[2]
    z_phys = origin[2] + np.arange(nz) * sz
    thigh_end_z, toe_end_z = (z_lo, z_hi) if mid_thigh_z <= toe_tip_z else (z_hi, z_lo)
    denom = (toe_end_z - thigh_end_z) or 1.0
    t = (z_phys - thigh_end_z) / denom
    prefer_upper = (t < 0.5)[:, None, None]

    merged = np.where(prefer_upper & upper_valid, upper_arr_rs,
              np.where(lower_valid, lower_arr_rs,
              np.where(upper_valid, upper_arr_rs, AIR))).astype(np.int16)

    mask = merged > THRESHOLD_HU
    lbl, n = ndimage.label(mask, structure=np.ones((3, 3, 3), dtype=np.int8))
    if n == 0:
        raise RuntimeError("nothing above threshold in the cropped box")
    sizes = ndimage.sum(mask, lbl, index=range(1, n + 1))
    leg_id = int(np.argmax(sizes)) + 1
    leg_frac = float(sizes[leg_id - 1] / mask.sum())
    keep = lbl == leg_id
    cleaned = np.where(keep, merged, AIR).astype(np.int16)

    out_img = sitk.GetImageFromArray(cleaned)
    out_img.SetSpacing((sx, sy, sz))
    out_img.SetOrigin(origin)
    out_img.SetDirection((1, 0, 0, 0, 1, 0, 0, 0, 1))

    side_name = "Left" if side == "L" else "Right"
    name = f"{case}_{side_name}Leg_MidThigh-ToToe"
    tmp_nrrd = out_root / f".{name}.tmp.nrrd"
    out_dir = out_root / name
    sitk.WriteImage(out_img, str(tmp_nrrd))
    try:
        nz_written = write_dicom_series(tmp_nrrd, out_dir, name, force=force)
        ok = nz_written == 0 or verify_dicom_series(tmp_nrrd, out_dir)
    finally:
        tmp_nrrd.unlink(missing_ok=True)

    return {
        "case": case, "side": side, "size": size, "leg_frac": leg_frac,
        "n_components": n, "verify_ok": ok, "out_dir": out_dir,
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", required=True, help="zenodo_8302449 directory")
    ap.add_argument("--out-root", required=True, help="parent folder for per-leg DICOM output")
    ap.add_argument("--only", nargs="*", help="restrict to these case IDs")
    ap.add_argument("--sides", default="LR", help="which side(s) to extract (default LR)")
    ap.add_argument("--force", action="store_true")
    args = ap.parse_args()

    root = Path(args.root)
    out_root = Path(args.out_root)
    out_root.mkdir(parents=True, exist_ok=True)

    cases = sorted(p.name for p in root.iterdir() if p.is_dir() and p.name not in EXCLUDE_CASES)
    if args.only:
        cases = [c for c in cases if c in args.only]

    results, failures = [], []
    for case in cases:
        case_dir = root / case / case
        for side in args.sides:
            label = f"{case}/{side}"
            try:
                r = extract_leg(case, case_dir, side, out_root, args.force)
                flag = "" if r["verify_ok"] and r["leg_frac"] > 0.5 else "  [CHECK]"
                print(f"{label}: {r['size']} leg={r['leg_frac']*100:.1f}% of thresholded voxels, "
                      f"{r['n_components']} components, verify={'OK' if r['verify_ok'] else 'FAIL'}{flag}")
                results.append(r)
            except Exception as exc:
                print(f"{label}: FAILED -- {exc}")
                failures.append((label, str(exc)))

    print(f"\n{len(results)} leg(s) extracted, {len(failures)} failure(s).")
    if failures:
        print("Failures:")
        for label, msg in failures:
            print(f"  {label}: {msg}")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
