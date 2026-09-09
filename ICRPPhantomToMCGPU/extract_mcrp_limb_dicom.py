#!/usr/bin/env python3
"""
Extract single-limb (arm or leg) DICOM series from the MRCP tetrahedral mesh
phantoms, adding them to the same kind of DICOM pool as extract_leg_dicom.py
built from VSD (see VSD_dicom).

Why this can't reuse the existing DICOM_EXPORT_MCRP: those tight-crop DICOMs
are head-only (vol_length_mm = 300mm below the head tip, see
params/mcrp_to_dicom_template.cfg) -- arms and legs were never captured.
Re-voxelizing the *whole* body at production spacing (0.3mm) would run
tens of GB per phantom (disk is the standing constraint here -- see
CLAUDE.md), so instead this computes a tight ROI box per limb directly from
the mesh (.node/.ele/.material) and drives mcrp_to_vox's existing legacy ROI
mode (x_start_mm/x_end_mm/... relative to the phantom's own bbMin corner,
write_dicom=true) once per limb -- the same Geant4 mu->HU physics as every
other MCRP export, just windowed tight around one limb.

Mesh notes worth knowing before touching this again:
  - .node coordinates are in CENTIMETRES, not mm (confirmed empirically: raw
    values give an adult male height of ~176 -- i.e. 176 cm). Multiply by 10.
  - .ele row = tet_id, 4 node ids (0-indexed, matching .node row order
    directly), material id ("voxelId" in MC-GPU_material_config.txt, same
    number as the "mNNN" tag in the phantom's own .material file).
  - Left/right is NOT distinguished in the limb materials (e.g. "Humeri_upper"
    covers both arms) -- unlike organs such as kidneys. This script clusters
    each limb's most-proximal bone group (Femora_upper for legs, Humeri_upper
    for arms) into two spatial groups by X, then classifies every other bone
    group in the chain by nearest cluster centroid (limbs don't cross sides).
  - Which X sign is anatomical "left" isn't assumed -- it's read off
    Kidney_left/Kidney_right's own centroid X per phantom (confirmed +X =
    patient's left on MRCP_AM, but this is computed fresh per phantom here).
  - Both the "flag" name (e.g. "Tibiae_fibulae_and_patellae") and separated
    names (e.g. "Tibiae"/"Fibulae"/"Patellae") show up across phantoms --
    bone groups are matched by keyword, not one hardcoded name.

Usage
-----
  python3 extract_mcrp_limb_dicom.py --phantom MRCP_AM --limb LeftLeg --dry-run
  python3 extract_mcrp_limb_dicom.py --phantom MRCP_AM --limb LeftLeg RightLeg LeftArm RightArm
  python3 extract_mcrp_limb_dicom.py --all --out-root /mnt/h/MICHELE_MCGPU/MCRP_dicom
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np
import pandas as pd
from sklearn.cluster import KMeans

ROOT = Path(__file__).resolve().parent
BINARY = ROOT / "build" / "mcrp_to_vox"
PHANTOM_DIR = ROOT / "phantoms"
MATERIAL_DIR = ROOT / "data" / "mcgpu_mcrp_materials"

ALL_PHANTOMS = ["MRCP-00F", "MRCP-00M", "MRCP-01F", "MRCP-01M", "MRCP-05F", "MRCP-05M",
                "MRCP-10F", "MRCP-10M", "MRCP-15F", "MRCP-15M", "MRCP_AF", "MRCP_AM"]

XY_MARGIN_MM = 25.0
XY_LIMB_GAP_MM = 10.0
DISTAL_MARGIN_MM = 30.0   # extra reach past the wrist/ankle bone bbox for fingers/toes
VOXEL_SIZE_MM = 1.0

# Proximal -> distal bone-group keyword chains. Matched by substring against
# whatever this phantom's material list actually calls each group.
LEG_CHAIN = ["Femora_upper", "Femora_lower", ["Tibia", "Fibula", "Patella"], "Ankles_and_foot"]
ARM_CHAIN = ["Humeri_upper", "Humeri_lower", ["Ulna", "Radi"], "Wrists_and_hand"]


def load_organ_ids(phantom: str) -> dict:
    """organ keyword group -> list of voxelIds, from MC-GPU_material_config.txt."""
    cfg_path = MATERIAL_DIR / phantom / "MC-GPU_material_config.txt"
    text = cfg_path.read_text()
    orgs: dict[str, list[int]] = {}
    for m in re.finditer(r'material/(?:.*/)?icrp_(.+?)\.mcgpu\s+density=[\d.]+\s+voxelId=(\d+)', text):
        orgs.setdefault(m.group(1), []).append(int(m.group(2)))
    return orgs


def ids_for_group(orgs: dict, group) -> list:
    """group is a keyword string, or a list of keywords (any-of)."""
    keywords = group if isinstance(group, list) else [group]
    out = []
    for name, vids in orgs.items():
        if any(name.startswith(kw) or kw in name for kw in keywords):
            out += vids
    return out


def load_mesh(phantom: str):
    ele_path = PHANTOM_DIR / f"{phantom}.ele"
    node_path = PHANTOM_DIR / f"{phantom}.node"
    ele = pd.read_csv(ele_path, sep=r"\s+", skiprows=1, header=None,
                       names=["tid", "n0", "n1", "n2", "n3", "mat"], dtype=np.int32)
    with open(node_path) as f:
        n_nodes = int(f.readline().split()[0])
    node = pd.read_csv(node_path, sep=r"\s+", skiprows=1, header=None, nrows=n_nodes,
                        names=["nid", "x", "y", "z"], dtype=np.float64)
    coords_mm = node[["x", "y", "z"]].values * 10.0   # cm -> mm
    n0 = ele["n0"].values.astype(np.int64)
    n1 = ele["n1"].values.astype(np.int64)
    n2 = ele["n2"].values.astype(np.int64)
    n3 = ele["n3"].values.astype(np.int64)
    mat = ele["mat"].values
    return coords_mm, n0, n1, n2, n3, mat


def tet_centroids_and_points(coords_mm, n0, n1, n2, n3, mat, ids):
    if not ids:
        return np.empty((0, 3)), np.empty((0, 3))
    idxs = np.where(np.isin(mat, ids))[0]
    if len(idxs) == 0:
        return np.empty((0, 3)), np.empty((0, 3))
    node_ids = np.stack([n0[idxs], n1[idxs], n2[idxs], n3[idxs]], axis=1)
    pts = coords_mm[node_ids]              # [ntet, 4, 3]
    return pts.mean(axis=1), pts.reshape(-1, 3)


def left_right_sign(coords_mm, n0, n1, n2, n3, mat, orgs) -> float:
    """+1 if +X is anatomical left (per Kidney_left/right centroid), else -1."""
    kl = ids_for_group(orgs, "Kidney_left")
    kr = ids_for_group(orgs, "Kidney_right")
    _, pts_l = tet_centroids_and_points(coords_mm, n0, n1, n2, n3, mat, kl)
    _, pts_r = tet_centroids_and_points(coords_mm, n0, n1, n2, n3, mat, kr)
    if len(pts_l) == 0 or len(pts_r) == 0:
        raise RuntimeError("Kidney_left/right not found -- can't resolve L/R convention")
    return 1.0 if pts_l[:, 0].mean() > pts_r[:, 0].mean() else -1.0


def compute_limb_roi(phantom: str, limb: str):
    """limb: 'LeftLeg' | 'RightLeg' | 'LeftArm' | 'RightArm'.
    Returns dict with x_start/end, y_start/end, z_start/end (mm, relative to
    the phantom's overall bbMin -- mcrp_to_vox's own ROI convention), plus a
    few diagnostics."""
    side = "Left" if limb.startswith("Left") else "Right"
    is_leg = limb.endswith("Leg")
    chain = LEG_CHAIN if is_leg else ARM_CHAIN

    orgs = load_organ_ids(phantom)
    coords_mm, n0, n1, n2, n3, mat = load_mesh(phantom)
    bb_min = coords_mm.min(axis=0)

    left_is_pos = left_right_sign(coords_mm, n0, n1, n2, n3, mat, orgs) > 0
    want_pos = (side == "Left") == left_is_pos

    proximal_ids = ids_for_group(orgs, chain[0])
    prox_cen, _ = tet_centroids_and_points(coords_mm, n0, n1, n2, n3, mat, proximal_ids)
    if len(prox_cen) < 10:
        raise RuntimeError(f"too few '{chain[0]}' tetrahedra ({len(prox_cen)}) in {phantom}")
    km = KMeans(n_clusters=2, n_init=4, random_state=0).fit(prox_cen[:, 0:1])
    centroid_x = [prox_cen[km.labels_ == c, 0].mean() for c in (0, 1)]
    this_cluster = int(np.argmax(centroid_x) if want_pos else np.argmin(centroid_x))
    other_cluster = 1 - this_cluster
    this_anchor_x, other_anchor_x = centroid_x[this_cluster], centroid_x[other_cluster]

    def side_points_for(ids):
        cen, pts = tet_centroids_and_points(coords_mm, n0, n1, n2, n3, mat, ids)
        if len(cen) == 0:
            return np.empty((0, 3)), np.empty((0, 3))
        d_this = np.abs(cen[:, 0] - this_anchor_x)
        d_other = np.abs(cen[:, 0] - other_anchor_x)
        keep = d_this < d_other
        pts4 = pts.reshape(-1, 4, 3)[keep] if keep.any() else np.empty((0, 4, 3))
        pts_this = pts4.reshape(-1, 3)
        pts_other_mask = ~keep
        pts4_other = pts.reshape(-1, 4, 3)[pts_other_mask] if pts_other_mask.any() else np.empty((0, 4, 3))
        return pts_this, pts4_other.reshape(-1, 3)

    all_this, all_other = [], []
    group_pts_this = []
    for group in chain:
        pts_this, pts_other = side_points_for(ids_for_group(orgs, group))
        if len(pts_this) == 0:
            raise RuntimeError(f"no tetrahedra resolved for group {group!r} ({limb}) in {phantom}")
        group_pts_this.append(pts_this)
        all_this.append(pts_this)
        all_other.append(pts_other)
    all_this = np.concatenate(all_this)
    all_other = np.concatenate(all_other) if any(len(p) for p in all_other) else np.empty((0, 3))

    # mid-thigh / mid-upper-arm landmark: midpoint of the proximal group's own Z range.
    prox_pts = group_pts_this[0]
    z_prox_mid = (prox_pts[:, 2].min() + prox_pts[:, 2].max()) / 2.0

    # distal landmark: extreme of the last group's bbox, in the direction away
    # from the proximal group (mesh Z sign convention isn't assumed fixed).
    distal_pts = group_pts_this[-1]
    z_distal_lo, z_distal_hi = distal_pts[:, 2].min(), distal_pts[:, 2].max()
    direction = 1.0 if (distal_pts[:, 2].mean() >= z_prox_mid) else -1.0
    z_distal_extreme = z_distal_hi if direction > 0 else z_distal_lo
    z_distal = z_distal_extreme + direction * DISTAL_MARGIN_MM

    z_lo, z_hi = sorted((z_prox_mid, z_distal))

    xy_lo = all_this[:, :2].min(axis=0) - XY_MARGIN_MM
    xy_hi = all_this[:, :2].max(axis=0) + XY_MARGIN_MM
    if len(all_other):
        other_lo = all_other[:, :2].min(axis=0)
        other_hi = all_other[:, :2].max(axis=0)
        if this_anchor_x > other_anchor_x:
            xy_lo[0] = max(xy_lo[0], other_hi[0] + XY_LIMB_GAP_MM)
        else:
            xy_hi[0] = min(xy_hi[0], other_lo[0] - XY_LIMB_GAP_MM)

    if not is_leg:
        # Unlike legs (mid-thigh is already well clear of the pelvis), an
        # arm's Z window sits right alongside the ribcage -- clamp the
        # medial X edge to the trunk's own lateral extent over that same Z
        # range, or the torso bleeds into the crop (seen on MRCP_AM/RightArm).
        _, rib_pts = tet_centroids_and_points(coords_mm, n0, n1, n2, n3, mat,
                                               ids_for_group(orgs, "Ribs"))
        if len(rib_pts):
            in_window = (rib_pts[:, 2] >= z_lo) & (rib_pts[:, 2] <= z_hi)
            rib_pts = rib_pts[in_window]
        if len(rib_pts):
            if this_anchor_x > 0:
                xy_lo[0] = max(xy_lo[0], rib_pts[:, 0].max() + XY_LIMB_GAP_MM)
            else:
                xy_hi[0] = min(xy_hi[0], rib_pts[:, 0].min() - XY_LIMB_GAP_MM)

    roi_world = {"x": (xy_lo[0], xy_hi[0]), "y": (xy_lo[1], xy_hi[1]), "z": (z_lo, z_hi)}
    roi_local = {ax: (lo - bb_min[i], hi - bb_min[i])
                 for i, (ax, (lo, hi)) in enumerate(roi_world.items())}
    return roi_local, roi_world, this_anchor_x, other_anchor_x


def write_cfg(phantom: str, limb: str, roi_local: dict, cfg_dir: Path, out_dir: Path) -> Path:
    cfg_dir.mkdir(parents=True, exist_ok=True)
    name = f"{phantom}_{limb}"
    cfg_path = cfg_dir / f"{name}.cfg"
    x0, x1 = roi_local["x"]
    y0, y1 = roi_local["y"]
    z0, z1 = roi_local["z"]
    cfg_path.write_text(
        f"phantom_name  = {phantom}\n"
        f"voxel_size_mm = {VOXEL_SIZE_MM}\n"
        f"x_start_mm = {x0:.3f}\n"
        f"x_end_mm   = {x1:.3f}\n"
        f"y_start_mm = {y0:.3f}\n"
        f"y_end_mm   = {y1:.3f}\n"
        f"z_start_mm = {z0:.3f}\n"
        f"z_end_mm   = {z1:.3f}\n"
        f"write_dicom = true\n"
        f"output_dir = {out_dir}\n"
    )
    return cfg_path


def run_and_collect(phantom: str, limb: str, cfg_path: Path, out_dir: Path, pool_dir: Path,
                     keep_intermediates: bool = False) -> Path:
    env = dict(os.environ)
    env["LD_LIBRARY_PATH"] = "/opt/Geant4/lib:" + env.get("LD_LIBRARY_PATH", "")
    result = subprocess.run([str(BINARY), str(cfg_path)], cwd=ROOT, env=env,
                             capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"mcrp_to_vox failed for {phantom}/{limb}:\n{result.stdout[-3000:]}\n{result.stderr[-3000:]}")

    case_dirs = sorted(out_dir.glob(f"*{phantom}*"), key=lambda p: p.stat().st_mtime, reverse=True)
    if not case_dirs:
        raise RuntimeError(f"no output folder produced for {phantom}/{limb} in {out_dir}")
    case_dir = case_dirs[0]
    dicom_dirs = list(case_dir.glob("*_dicom_*"))
    if not dicom_dirs:
        raise RuntimeError(f"no DICOM subfolder in {case_dir}")
    dicom_dir = dicom_dirs[0]

    pool_dir.mkdir(parents=True, exist_ok=True)
    dest = pool_dir / f"{phantom}_{limb}"
    if dest.exists():
        shutil.rmtree(dest)
    shutil.move(str(dicom_dir), str(dest))

    if not keep_intermediates:
        shutil.rmtree(case_dir, ignore_errors=True)
    return dest


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--phantom", nargs="*", help="phantom name(s), e.g. MRCP_AM")
    ap.add_argument("--all", action="store_true", help="all 12 phantoms")
    ap.add_argument("--limb", nargs="*", default=["LeftLeg", "RightLeg", "LeftArm", "RightArm"])
    ap.add_argument("--out-root", default="/mnt/h/MICHELE_MCGPU/MCRP_dicom")
    ap.add_argument("--work-dir", default=str(ROOT / "output" / "mcrp_limb_tmp"))
    ap.add_argument("--cfg-dir", default=str(ROOT / "params" / "generated_mcrp_limb"))
    ap.add_argument("--dry-run", action="store_true", help="only print computed ROIs")
    ap.add_argument("--keep-intermediates", action="store_true")
    args = ap.parse_args()

    phantoms = ALL_PHANTOMS if args.all else (args.phantom or [])
    if not phantoms:
        ap.error("give --phantom NAME [NAME ...] or --all")

    out_root = Path(args.out_root)
    work_dir = Path(args.work_dir)
    cfg_dir = Path(args.cfg_dir)
    work_dir.mkdir(parents=True, exist_ok=True)

    failures = []
    for phantom in phantoms:
        for limb in args.limb:
            label = f"{phantom}/{limb}"
            try:
                roi_local, roi_world, this_x, other_x = compute_limb_roi(phantom, limb)
                size_mm = tuple(round(hi - lo, 1) for lo, hi in roi_local.values())
                print(f"{label}: local ROI {roi_local}  size(mm)={size_mm}  "
                      f"anchor_x(this,other)=({this_x:.1f},{other_x:.1f})")
                if args.dry_run:
                    continue
                cfg_path = write_cfg(phantom, limb, roi_local, cfg_dir, work_dir)
                dest = run_and_collect(phantom, limb, cfg_path, work_dir, out_root,
                                        args.keep_intermediates)
                print(f"  -> {dest}")
            except Exception as exc:
                print(f"{label}: FAILED -- {exc}")
                failures.append((label, str(exc)))

    print(f"\n{len(phantoms) * len(args.limb) - len(failures)} limb(s) done, {len(failures)} failure(s).")
    for label, msg in failures:
        print(f"  {label}: {msg}")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
