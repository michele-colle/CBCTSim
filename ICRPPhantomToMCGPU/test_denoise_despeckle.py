#!/usr/bin/env python3
"""
test_denoise_despeckle.py

Evaluation harness for noise-robust HU -> material classification, as an
alternative to the current per-voxel hard thresholding in dicom_to_mcgpu.cpp.

It compares three pipelines on a real DICOM series:

    baseline          raw HU  -> labels                    (current behaviour)
    median            median(HU) -> labels                 (option A: denoise source)
    median+despeckle  median(HU) -> labels -> despeckle     (+ connected-component cleanup)

The classification thresholds and label scheme are kept IDENTICAL to
dicom_to_mcgpu.cpp so results transfer directly:

    label 0=air, 1=fat, 2=soft, 3=spongiosa, 4=cortical
    if      hu <  thr_air_fat        -> 0
    else if hu <  thr_fat_soft       -> 1
    else if hu <  thr_soft_spongiosa -> 2
    else if hu <  thr_spongiosa_cort -> 3
    else                             -> 4
    (== numpy.digitize with those 4 thresholds as bins)

"Speckle" = small connected components of a material label. CT noise turns
single voxels into spurious cortical/air islands scattered through tissue; the
number and volume of sub-min_size components is our noise metric. Median
filtering should shrink it; despeckle should drive it to zero for the treated
labels while changing only a tiny fraction of voxels (i.e. not eating anatomy).

Usage:
    python3 test_denoise_despeckle.py                       # defaults, centred 64-slice sub-volume
    python3 test_denoise_despeckle.py --dicom_dir DIR
    python3 test_denoise_despeckle.py --median-radius 1 --min-size 20 --connectivity 1
    python3 test_denoise_despeckle.py --full               # whole volume (slow, RAM-heavy)
"""

import argparse
import os
import sys
import time

import numpy as np
import SimpleITK as sitk
from scipy import ndimage as ndi

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.colors import ListedColormap, BoundaryNorm

# --- Label scheme (mirror of dicom_to_mcgpu.cpp) ----------------------------
LABEL_NAMES = {0: "air", 1: "fat", 2: "soft", 3: "spongiosa", 4: "cortical", 5: "implant"}
# Discrete colormap for labels 0..5 (only 0..4 occur from source HU)
LABEL_COLORS = ["#000000", "#d9b26f", "#c98b8b", "#7fb3d5", "#f5f5f5", "#e74c3c"]

DEFAULT_DICOM_DIR = "/mnt/f/Michele_diskF/Test Fantoccio Zurigo-CBCTvsCT/patient_ct_registered"


def log(msg):
    print(msg, flush=True)


# ---------------------------------------------------------------------------
def load_dicom_hu(dicom_dir):
    """Load a DICOM series as an int16 HU numpy array (z, y, x). Picks the
    first series found, matching dicom_to_mcgpu.cpp's default behaviour."""
    reader = sitk.ImageSeriesReader()
    ids = reader.GetGDCMSeriesIDs(dicom_dir)
    if not ids:
        # fall back: recursive search for the first dir that has a series
        for root, _dirs, _files in os.walk(dicom_dir):
            ids = reader.GetGDCMSeriesIDs(root)
            if ids:
                dicom_dir = root
                break
    if not ids:
        raise RuntimeError(f"No DICOM series found in or under: {dicom_dir}")
    if len(ids) > 1:
        log(f"Multiple series in {dicom_dir}; using first: {ids[0]}")
    files = reader.GetGDCMSeriesFileNames(dicom_dir, ids[0])
    reader.SetFileNames(files)
    t0 = time.time()
    img = reader.Execute()
    log(f"[timing] DICOM read      : {time.time() - t0:.2f} s")

    # HU = pixel * slope + intercept; SimpleITk applies rescale on read for CT.
    hu = sitk.GetArrayFromImage(img).astype(np.int16)  # (z, y, x)
    sx, sy, sz = img.GetSpacing()                       # mm (x, y, z)
    log(f"DICOM size      : {hu.shape[2]} x {hu.shape[1]} x {hu.shape[0]}  (x,y,z)")
    log(f"DICOM spacing   : {sx:.3f} x {sy:.3f} x {sz:.3f} mm")
    log(f"HU range        : [{hu.min()}, {hu.max()}]")
    return hu, (sx, sy, sz)


# ---------------------------------------------------------------------------
def classify(hu, thr):
    """Vectorised equivalent of the cpp threshold ladder -> labels 0..4."""
    # np.digitize(hu, [t0,t1,t2,t3]) gives 0 for hu<t0, 1 for t0<=hu<t1, ...
    # which matches the cpp strict-`<` ladder exactly.
    return np.digitize(hu, thr).astype(np.uint8)


def component_stats(labels, min_size, structure, labels_of_interest):
    """For each label, return (#components, #components<min_size, voxels in
    those small components). The largest component per label is never counted
    as 'small' regardless of size (protects the exterior air background)."""
    stats = {}
    for L in labels_of_interest:
        mask = labels == L
        n_vox = int(mask.sum())
        if n_vox == 0:
            stats[L] = (0, 0, 0)
            continue
        cc, n = ndi.label(mask, structure=structure)
        if n == 0:
            stats[L] = (0, 0, 0)
            continue
        sizes = np.bincount(cc.ravel())
        sizes[0] = 0  # ignore background of this binary mask
        largest = int(sizes.argmax())
        small_ids = np.where((sizes > 0) & (sizes < min_size))[0]
        small_ids = small_ids[small_ids != largest]
        n_small = int(small_ids.size)
        vox_small = int(sizes[small_ids].sum())
        stats[L] = (n, n_small, vox_small)
    return stats


def despeckle(labels, min_size, structure, labels_to_process):
    """Remove sub-min_size connected components of each treated label and
    refill each removed voxel with the label of its nearest surviving voxel
    (Euclidean nearest-neighbour). Returns (new_labels, removed_mask)."""
    remove = np.zeros(labels.shape, dtype=bool)
    for L in labels_to_process:
        mask = labels == L
        if not mask.any():
            continue
        cc, n = ndi.label(mask, structure=structure)
        if n == 0:
            continue
        sizes = np.bincount(cc.ravel())
        sizes[0] = 0
        largest = int(sizes.argmax())
        kill_ids = np.where((sizes > 0) & (sizes < min_size))[0]
        kill_ids = kill_ids[kill_ids != largest]
        if kill_ids.size:
            remove |= np.isin(cc, kill_ids)
    if not remove.any():
        return labels.copy(), remove
    # For each removed voxel, index of nearest voxel where remove==False
    # (i.e. nearest surviving voxel). Kept voxels map to themselves.
    idx = ndi.distance_transform_edt(remove, return_distances=False,
                                     return_indices=True)
    filled = labels[tuple(idx)]
    return filled, remove


# ---------------------------------------------------------------------------
def dist_table(title, labels, total):
    log(f"\n{title}")
    log(f"  {'label':<12}{'voxels':>14}{'percent':>10}")
    for L in sorted(np.unique(labels)):
        c = int((labels == L).sum())
        name = LABEL_NAMES.get(int(L), f"#{int(L)}")
        log(f"  {L:>2} {name:<9}{c:>14}{100.0 * c / total:>9.3f}%")


def speckle_table(title, stats):
    log(f"\n{title}")
    log(f"  {'label':<12}{'components':>12}{'small(<min)':>14}{'small voxels':>14}")
    for L in sorted(stats):
        n, ns, vs = stats[L]
        name = LABEL_NAMES.get(int(L), f"#{int(L)}")
        log(f"  {L:>2} {name:<9}{n:>12}{ns:>14}{vs:>14}")


# ---------------------------------------------------------------------------
def save_slice_figure(path, hu, base, med, desp, z, spacing):
    """One row of panels for axial slice z: HU | baseline | median |
    median+despeckle | changed(base vs despeckle)."""
    cmap = ListedColormap(LABEL_COLORS)
    norm = BoundaryNorm(np.arange(-0.5, len(LABEL_COLORS)), cmap.N)
    changed = (base[z] != desp[z])

    fig, ax = plt.subplots(1, 5, figsize=(22, 5))
    ax[0].imshow(hu[z], cmap="gray", vmin=-500, vmax=1000)
    ax[0].set_title(f"HU  (z={z})")
    for a, img, t in ((ax[1], base[z], "baseline labels"),
                      (ax[2], med[z], "median labels"),
                      (ax[3], desp[z], "median+despeckle")):
        a.imshow(img, cmap=cmap, norm=norm, interpolation="nearest")
        a.set_title(t)
    ax[4].imshow(base[z], cmap="gray", alpha=0.3, interpolation="nearest")
    ax[4].imshow(np.ma.masked_where(~changed, changed), cmap="autumn",
                 interpolation="nearest")
    ax[4].set_title(f"changed: {int(changed.sum())} px")
    for a in ax:
        a.set_xticks([]); a.set_yticks([])
    fig.suptitle(f"slice z={z}  spacing={spacing[0]:.2f}x{spacing[1]:.2f} mm")
    fig.tight_layout()
    fig.savefig(path, dpi=90, bbox_inches="tight")
    plt.close(fig)
    log(f"  wrote {path}")


# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dicom_dir", default=DEFAULT_DICOM_DIR)
    ap.add_argument("--out-dir", default="denoise_checks")
    ap.add_argument("--thr", type=int, nargs=4, default=[-500, -50, 200, 800],
                    metavar=("AIR_FAT", "FAT_SOFT", "SOFT_SPONG", "SPONG_CORT"),
                    help="HU thresholds (match dicom_to_mcgpu.cpp / cfg)")
    ap.add_argument("--median-radius", type=int, default=1,
                    help="median filter radius in voxels (1 => 3x3x3)")
    ap.add_argument("--min-size", type=int, default=20,
                    help="connected components smaller than this are despeckled")
    ap.add_argument("--connectivity", type=int, default=1, choices=[1, 2, 3],
                    help="3D connectivity for components (1=6, 2=18, 3=26 neigh.)")
    ap.add_argument("--despeckle-labels", type=int, nargs="*", default=[0, 3, 4],
                    help="labels to despeckle (default: air, spongiosa, cortical)")
    ap.add_argument("--z-thickness", type=int, default=64,
                    help="centred sub-volume thickness in slices (0 or --full = all)")
    ap.add_argument("--full", action="store_true", help="process the whole volume")
    ap.add_argument("--n-figs", type=int, default=3,
                    help="number of axial comparison PNGs to render")
    ap.add_argument("--save-raw", action="store_true",
                    help="also dump baseline & despeckled label volumes as uint8 .raw")
    args = ap.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)
    thr = np.array(args.thr, dtype=np.int16)
    log(f"Thresholds      : {list(thr)}  (air|fat|soft|spongiosa|cortical)")
    log(f"Median radius   : {args.median_radius}  ({2*args.median_radius+1}^3 kernel)")
    log(f"Despeckle       : min_size={args.min_size} conn={args.connectivity} "
        f"labels={args.despeckle_labels}")

    # --- load ---------------------------------------------------------------
    hu_full, spacing = load_dicom_hu(args.dicom_dir)

    # --- crop to a centred sub-volume unless --full -------------------------
    nz = hu_full.shape[0]
    if args.full or args.z_thickness <= 0 or args.z_thickness >= nz:
        hu = hu_full
        z_off = 0
        log(f"Sub-volume      : full ({nz} slices)")
    else:
        z0 = max(0, nz // 2 - args.z_thickness // 2)
        z1 = min(nz, z0 + args.z_thickness)
        hu = hu_full[z0:z1].copy()
        z_off = z0
        log(f"Sub-volume      : slices [{z0}, {z1}) of {nz}")
    total = hu.size

    structure = ndi.generate_binary_structure(3, args.connectivity)
    labels_of_interest = sorted(set(range(5)) | set(args.despeckle_labels))

    # --- pipeline 1: baseline ----------------------------------------------
    t0 = time.time()
    base = classify(hu, thr)
    log(f"\n[timing] classify        : {time.time() - t0:.2f} s")

    # --- pipeline 2: median then classify -----------------------------------
    t0 = time.time()
    hu_img = sitk.GetImageFromArray(hu)
    hu_med = sitk.GetArrayFromImage(
        sitk.Median(hu_img, [args.median_radius] * 3)).astype(np.int16)
    log(f"[timing] median filter    : {time.time() - t0:.2f} s")
    med = classify(hu_med, thr)

    # --- pipeline 3: median + connected-component despeckle -----------------
    t0 = time.time()
    desp, removed = despeckle(med, args.min_size, structure, args.despeckle_labels)
    log(f"[timing] despeckle        : {time.time() - t0:.2f} s")

    # --- reports ------------------------------------------------------------
    dist_table("Label distribution — baseline", base, total)
    dist_table("Label distribution — median", med, total)
    dist_table("Label distribution — median+despeckle", desp, total)

    s_base = component_stats(base, args.min_size, structure, labels_of_interest)
    s_med = component_stats(med, args.min_size, structure, labels_of_interest)
    s_desp = component_stats(desp, args.min_size, structure, labels_of_interest)
    speckle_table("Connected components — baseline", s_base)
    speckle_table("Connected components — median", s_med)
    speckle_table("Connected components — median+despeckle", s_desp)

    # --- headline metrics ---------------------------------------------------
    def small_totals(stats):
        return (sum(v[1] for v in stats.values()),
                sum(v[2] for v in stats.values()))
    nb, vb = small_totals(s_base)
    nm, vm = small_totals(s_med)
    nd, vd = small_totals(s_desp)
    ch_med = int((base != med).sum())
    ch_desp = int((base != desp).sum())

    log("\n=================== SUMMARY ===================")
    log(f"Speckle components (<{args.min_size} vox), sum over treated labels:")
    log(f"  baseline          : {nb:>8}   ({vb} voxels)")
    log(f"  median            : {nm:>8}   ({vm} voxels)"
        f"   [{100.0*(nb-nm)/max(nb,1):.1f}% fewer]")
    log(f"  median+despeckle  : {nd:>8}   ({vd} voxels)"
        f"   [{100.0*(nb-nd)/max(nb,1):.1f}% fewer]")
    log(f"Voxels reclassified vs baseline:")
    log(f"  median            : {ch_med:>10}  ({100.0*ch_med/total:.3f}% of volume)")
    log(f"  median+despeckle  : {ch_desp:>10}  ({100.0*ch_desp/total:.3f}% of volume)")
    log(f"  despeckle refilled: {int(removed.sum()):>10} voxels")
    log("===============================================")

    # --- figures ------------------------------------------------------------
    if args.n_figs > 0:
        zc = hu.shape[0] // 2
        span = min(hu.shape[0] // 2, hu.shape[0])
        zs = np.unique(np.linspace(max(0, zc - span // 2),
                                   min(hu.shape[0] - 1, zc + span // 2),
                                   args.n_figs).astype(int))
        log("")
        for z in zs:
            p = os.path.join(args.out_dir, f"compare_z{z_off + z:04d}.png")
            save_slice_figure(p, hu, base, med, desp, int(z), spacing)

    # --- optional raw dumps -------------------------------------------------
    if args.save_raw:
        nzc, nyc, nxc = base.shape
        dim = f"{nxc}x{nyc}x{nzc}"
        for name, vol in (("baseline", base), ("despeckle", desp)):
            p = os.path.join(args.out_dir, f"labels_{name}_{dim}uint8.raw")
            vol.astype(np.uint8).tofile(p)
            log(f"  wrote {p}")

    log("\nDone.")


if __name__ == "__main__":
    sys.exit(main())
