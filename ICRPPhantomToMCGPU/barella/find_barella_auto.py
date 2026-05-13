#!/usr/bin/env python3
"""
find_barella_auto.py

Automatically determine the stretcher (barella) top-panel centre position
(cx, cy in mm relative to the gantry isocenter) and the scan-frame rotation
offset by fitting detected shadow edges to the geometric projection model.

Workflow
--------
1. Load all raw uint16 projections from imgScan_0 (skip BlankImg).
2. Compute the row-average log-projection for each image:
       p_i[col] = log(I0) − mean_rows( log(img[row, col]) )
   → builds a sinogram of shape (n_proj, det_columns).
3. For each projection in the four user-defined ranges detect the stretcher
   edge as the first RISING feature in the row-average log-projection:
       L2R  →  first col from the left  where p rises above threshold
       R2L  →  first col from the right where p rises above threshold
4. Show the sinogram with overlaid detected edge positions.
5. Fit two 2-D points (left and right outer edge of the top panel) by linear
   regression (same model as find_barella.ipynb).
6. Report centre position and rotation angle θ.

Edge ranges (1-based image numbers)
-------------------------------------
   [20..121]  L2R  →  left outer edge (source in first quadrant)
   [137..154] L2R  →  right outer edge (source crossing top)
   [283..341] R2L  →  left outer edge (source in third quadrant, mirror of range 1)
   [354..417] R2L  →  right outer edge (source in fourth quadrant, mirror of range 2)

Usage
-----
    python find_barella_auto.py \\
        --params  /path/to/19..._Params.json \\
        --imgdir  /path/to/imgScan_0 \\
        --i0      /path/to/I0.txt \\
        [--scan   0]
"""

import argparse
import json
import os
import re

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches

# ── Default file paths (WSL mount of Windows downloads) ───────────────────────
_BASE   = "/mnt/c/Users/colle/Downloads/BARELLA7G"
_PARAMS = os.path.join(_BASE, "19.1220805103146695.81_Params.json")
_IMGDIR = os.path.join(_BASE, "imgScan_0")
_I0FILE = os.path.join(_BASE, "I0.txt")

# ── Projection ranges: (first_img, last_img, direction, edge_label) ───────────
#   direction   "L2R" → leftmost rising feature  (left  outer panel edge)
#               "R2L" → rightmost rising feature (right outer panel edge)
RANGES = [
    (20,  121, "L2R", "left"),
    (137, 154, "L2R", "right"),
    (283, 341, "R2L", "left"),
    (354, 417, "R2L", "right"),
]
RANGE_COLORS = ["tomato", "orange", "deepskyblue", "limegreen"]

# ── Edge-detection parameters ─────────────────────────────────────────────────
SMOOTH_WIN    = 15    # uniform moving-average window (columns)
DROP_FRAC     = 0.10  # threshold = air + DROP_FRAC * (shadow_level − air_level)
DEAD          = 10    # columns to ignore at each detector edge
MIN_AIR_COLS  = 20    # minimum clear-air columns required for a valid detection
OUTLIER_SIGMA = 2.5   # residual threshold (×std) for linear-fit outlier rejection


# ═════════════════════════════════════════════════════════════════════════════
#  I/O helpers
# ═════════════════════════════════════════════════════════════════════════════

def load_params(params_path, scan_index=0):
    with open(params_path) as f:
        return json.load(f)["scans"][scan_index]


def load_i0(i0_path, scan_key="imgScan_0"):
    with open(i0_path) as f:
        for line in f:
            k, v = line.strip().split(":")
            if k.strip() == scan_key:
                return float(v.strip())
    raise KeyError(scan_key)


def list_images(imgdir):
    """Sorted list of (1-based index, full path) excluding BlankImg files."""
    pat = re.compile(r"_Img(\d+)\.raw$", re.IGNORECASE)
    entries = []
    for fname in os.listdir(imgdir):
        m = pat.search(fname)
        if m:
            entries.append((int(m.group(1)), os.path.join(imgdir, fname)))
    entries.sort(key=lambda x: x[0])
    return entries


# ═════════════════════════════════════════════════════════════════════════════
#  Sinogram builder
# ═════════════════════════════════════════════════════════════════════════════

def build_sinogram(image_list, rows, cols, i0_val):
    """
    Return float32 array of shape (n, cols):
        sino[i] = log(I0) − mean_rows( log( img ) )   for image i
    """
    log_I0 = np.float32(np.log(i0_val))
    n      = len(image_list)
    sino   = np.empty((n, cols), dtype=np.float32)
    for i, (_, path) in enumerate(image_list):
        img = np.fromfile(path, dtype=np.uint16).reshape(rows, cols).astype(np.float32)
        np.clip(img, 1, None, out=img)
        sino[i] = log_I0 - np.log(img).mean(axis=0)
    return sino


# ═════════════════════════════════════════════════════════════════════════════
#  Edge detection (log-projection domain: air≈0, shadow>0)
# ═════════════════════════════════════════════════════════════════════════════

def _smooth(arr, win):
    return np.convolve(arr, np.ones(win) / win, mode="same")


def detect_edge(row, direction):
    """
    Detect the first RISING edge (air→shadow) in a 1-D log-projection row.
    Returns sub-pixel column float, or None if no valid edge found.
    """
    s   = _smooth(row, SMOOTH_WIN)
    n   = len(s)
    air = np.percentile(s, 5)
    shd = np.percentile(s, 95)
    if shd - air < 0.01:
        return None
    thresh = air + DROP_FRAC * (shd - air)

    if direction == "L2R":
        for i in range(DEAD, n - DEAD):
            if s[i] > thresh:
                air_cols = s[DEAD : max(DEAD + 1, i - MIN_AIR_COLS)]
                if len(air_cols) == 0 or air_cols.mean() > thresh:
                    return None
                i0 = max(0, i - 1)
                if abs(s[i] - s[i0]) < 1e-9:
                    return float(i)
                return i0 + (thresh - s[i0]) / (s[i] - s[i0])
    else:  # R2L
        for i in range(n - DEAD - 1, DEAD - 1, -1):
            if s[i] > thresh:
                air_cols = s[min(n - DEAD, i + MIN_AIR_COLS) : n - DEAD]
                if len(air_cols) == 0 or air_cols.mean() > thresh:
                    return None
                i1 = min(n - 1, i + 1)
                if abs(s[i1] - s[i]) < 1e-9:
                    return float(i)
                return i + (thresh - s[i]) / (s[i1] - s[i])
    return None


# ═════════════════════════════════════════════════════════════════════════════
#  Linear regression  (identical model to find_barella.ipynb)
# ═════════════════════════════════════════════════════════════════════════════

def fit_position(measurements, scan):
    """
    measurements : list of (proj_0based_index, x_pixel_offset_from_isocenter)
    Returns (px, py) in mm in the gantry frame.
    """
    angles_rad    = np.deg2rad(scan["angle_eff"])
    sod_arr       = np.array(scan["sod"])
    sid_arr       = np.array(scan["sid"])
    det_col_pitch = scan["det_column_pitch"]

    rows_A, rows_y = [], []
    for idx, x_pix in measurements:
        th  = angles_rad[idx]
        s   = sod_arr[idx]
        d   = sid_arr[idx] - sod_arr[idx]
        t   = x_pix * det_col_pitch
        R   = np.array([[np.cos(th), -np.sin(th)],
                        [np.sin(th),  np.cos(th)]])
        rows_A.append(np.array([t, -(d + s)]) @ R)
        rows_y.append(-t * s)

    A = np.array(rows_A)
    y = np.array(rows_y)
    x_hat, _, _, _ = np.linalg.lstsq(A, y, rcond=None)
    return x_hat


# ═════════════════════════════════════════════════════════════════════════════
#  Main
# ═════════════════════════════════════════════════════════════════════════════

def main(params_path, imgdir, i0_path, scan_index, vol_size_mm=None):
    scan   = load_params(params_path, scan_index)
    i0_val = load_i0(i0_path, f"imgScan_{scan_index}")
    images = list_images(imgdir)
    rows   = scan["det_rows"]
    cols   = scan["det_columns"]
    iso_u  = np.array(scan["iso_u"])

    print(f"Images: {len(images)}  det {cols}×{rows}  I0={i0_val:.1f}")

    # ── Build sinogram ────────────────────────────────────────────────────────
    print("Building sinogram …")
    sino = build_sinogram(images, rows, cols, i0_val)
    img_numbers = [num for num, _ in images]   # 1-based list

    angles_deg = np.array(scan["angle_eff"])

    # ── Detect edges in each range ────────────────────────────────────────────
    # detections: (sino_row, col_float, range_idx, proj_idx)
    detections = []

    for ri, (lo, hi, direction, edge_label) in enumerate(RANGES):
        for img_num in range(lo, hi + 1):
            if img_num not in img_numbers:
                continue
            sino_row = img_numbers.index(img_num)
            proj_idx = img_num - 1

            col = detect_edge(sino[sino_row], direction)
            if col is None:
                continue
            detections.append((sino_row, col, ri, proj_idx))

    # ── Per-range linear fit → flag outliers ──────────────────────────────────
    outlier_mask = [False] * len(detections)   # True = outlier

    for ri in range(len(RANGES)):
        idx_in_range = [i for i, d in enumerate(detections) if d[2] == ri]
        if len(idx_in_range) < 3:
            continue
        ang = np.array([angles_deg[detections[i][3]] for i in idx_in_range])
        col = np.array([detections[i][1]             for i in idx_in_range])
        a, b   = np.polyfit(ang, col, 1)
        resid  = col - (a * ang + b)
        sigma  = resid.std()
        lo_lbl, hi_lbl, d, el = RANGES[ri]
        n_out  = 0
        for k, i in enumerate(idx_in_range):
            if abs(resid[k]) > OUTLIER_SIGMA * sigma:
                outlier_mask[i] = True
                n_out += 1
        print(f"  [{lo_lbl}..{hi_lbl}] {d:3s} ({el:5s}): "
              f"{len(idx_in_range)} edges  σ={sigma:.1f} px  outliers={n_out}")

    # ── Build measurement lists excluding outliers ────────────────────────────
    left_meas  = []
    right_meas = []
    for i, (sino_row, col, ri, proj_idx) in enumerate(detections):
        if outlier_mask[i]:
            continue
        x_offset   = col - iso_u[proj_idx] + 0.5
        edge_label = RANGES[ri][3]
        if edge_label == "left":
            left_meas.append((proj_idx, x_offset))
        else:
            right_meas.append((proj_idx, x_offset))

    # ── Sinogram plot with overlaid edges ─────────────────────────────────────
    fig, ax = plt.subplots(figsize=(14, 6))
    vmax = np.percentile(sino, 99)
    ax.imshow(sino, aspect="auto", cmap="gray", vmin=0, vmax=vmax,
              extent=[0, cols, len(images), 0], origin="upper")

    for i, (sino_row, col, ri, _) in enumerate(detections):
        if outlier_mask[i]:
            ax.plot(col, sino_row, "x", color="red", markersize=6,
                    markeredgewidth=1.2, zorder=5)
        else:
            ax.plot(col, sino_row, ".", color=RANGE_COLORS[ri],
                    markersize=3, alpha=0.85)

    patches = [
        mpatches.Patch(color=RANGE_COLORS[ri],
                       label=f"[{lo}..{hi}] {d} ({el})")
        for ri, (lo, hi, d, el) in enumerate(RANGES)
    ]
    patches.append(mpatches.Patch(color="red", label="outlier"))
    ax.legend(handles=patches, loc="upper right", fontsize=9)
    ax.set_xlabel("Detector column")
    ax.set_ylabel("Projection index")
    ax.set_title("Row-averaged sinogram with detected stretcher edges")
    plt.tight_layout()

    # ── Fit & report ──────────────────────────────────────────────────────────
    if len(left_meas) >= 3 and len(right_meas) >= 3:
        p_L = fit_position(left_meas,  scan)
        p_R = fit_position(right_meas, scan)

        avg = (p_L + p_R) / 2.0
        sep = np.linalg.norm(p_L - p_R)

        print(f"\n[left  edge]  px={p_L[0]:.2f} mm  py={p_L[1]:.2f} mm  ({len(left_meas)} pts)")
        print(f"[right edge]  px={p_R[0]:.2f} mm  py={p_R[1]:.2f} mm  ({len(right_meas)} pts)")
        cx = avg[1]
        cy = avg[0]

        print(f"\nEdge separation: {sep:.1f} mm")
        print(f"\nCentre (from isocenter):  cx={cx:.3f} mm   cy={cy:.3f} mm")
        if vol_size_mm is not None:
            lx, ly = vol_size_mm
            print(f"Centre (from vol corner):  cx={cx + lx/2:.3f} mm   cy={cy + ly/2:.3f} mm"
                  f"   [vol {lx}×{ly} mm, corner = isocenter − ({lx/2},{ly/2}) mm]")

        # θ_align: gantry angle at which the source looks along the stretcher
        # width axis (both outer edges project to the same detector column).
        # Two solutions exist, 180° apart — same geometry, opposite source side.
        # The in-plane rotation of the stretcher equals θ_align itself.
        ax_, ay_ = p_L
        bx_, by_ = p_R
        theta_1 = np.arctan2(ax_ - bx_, ay_ - by_)
        theta_align = theta_1 % np.pi          # unique representative in [0°, 180°)
        print(f"Stretcher in-plane rotation: {np.rad2deg(theta_align):.3f}°  "
              f"(gantry angle where both edges align on the same detector column)")
    else:
        print("Not enough edges for regression.")

    plt.show()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--params",   default=_PARAMS)
    parser.add_argument("--imgdir",   default=_IMGDIR)
    parser.add_argument("--i0",       default=_I0FILE)
    parser.add_argument("--scan",     type=int, default=0)
    parser.add_argument("--vol-size", default="500x500",
                        help="XY volume size in mm as WxH, e.g. 500x500. "
                             "Used to express the centre relative to the volume corner.")
    args = parser.parse_args()

    vol = None
    if args.vol_size:
        w, h  = args.vol_size.lower().split("x")
        vol   = (float(w), float(h))

    main(args.params, args.imgdir, args.i0, args.scan, vol_size_mm=vol)
