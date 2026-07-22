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
import os

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from scipy.ndimage import uniform_filter1d

from cbct_utils import load_params, list_images, load_study

# ── Default file paths (WSL mount of Windows downloads) ───────────────────────
#_BASE   = "/mnt/c/Users/colle/Downloads/BARELLA7G"
#_PARAMS = os.path.join(_BASE, "19.1220805103146695.81_Params.json")
#_BASE   = "/mnt/f/Michele_diskF/Test Fantoccio Zurigo-CBCTvsCT/CBCT_DE/Fantoccio_Head_17x17DE_Reg_Zurigo/Fantoccio_Head_17x17_Zurigo"
#_PARAMS = os.path.join(_BASE, "19.1240516105915049.1331_Params.json")
_BASE   = "/mnt/f/Michele_diskF/TestRAR__25_05_2026/04_ScanTable2"
_PARAMS = os.path.join(_BASE, "19.1220805103146695.379_Params.json")
_I0FILE  = os.path.join(_BASE, "I0.txt")

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
MIN_SINO_VAL  = 0.05  # minimum mean log-projection at detected peak (rejects noise/full-shadow)

# ── Vertical-edge emphasis (stretcher tracking with a phantom in the field) ───
#   The stretcher panel edges are STRAIGHT, FULL-HEIGHT vertical lines, whereas
#   an intervening phantom is a CURVED silhouette.  Smoothing each column over a
#   tall vertical window keeps the vertical edges coherent while the phantom's
#   oblique boundary — which crosses any given column over only a few rows — is
#   diluted.  A horizontal gradient then turns the surviving vertical edges into
#   sharp peaks, and the row average gives the 1-D detection profile.
USE_VERTICAL_EDGE = False   # build the detection sinogram from the vertical-edge filter
VEDGE_ROWS        = 401     # vertical smoothing window (rows) enforcing vertical coherence
VEDGE_DX          = 3       # horizontal-gradient half-span (columns)
VEDGE_MIN_COV     = 0.5     # min fraction of valid (non-padded) rows for a usable column
VEDGE_DROP_FRAC   = 0.35    # detect_edge threshold fraction for the edge profile


# ═════════════════════════════════════════════════════════════════════════════
#  I/O helpers
# ═════════════════════════════════════════════════════════════════════════════

def load_i0(i0_path, scan_key="imgScan_0"):
    with open(i0_path) as f:
        for line in f:
            k, v = line.strip().split(":")
            if k.strip() == scan_key:
                return float(v.strip())
    raise KeyError(scan_key)


def load_i0_list(path):
    """Load sparse (1-based proj_idx, I0) pairs from a whitespace-separated file."""
    data = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split()
            data[int(parts[0])] = float(parts[1])
    return data


def fit_i0_linear(i0_dict, n_proj):
    """
    Fit I0(k) = a·k + b to sparse {1-based_idx: I0} pairs and return a
    length-n_proj float array (index 0 corresponds to projection 1).
    """
    idxs = np.array(sorted(i0_dict.keys()), dtype=np.float64)
    vals = np.array([i0_dict[int(k)] for k in idxs], dtype=np.float64)
    a, b = np.polyfit(idxs, vals, 1)
    k_all  = np.arange(1, n_proj + 1, dtype=np.float64)
    i0_arr = a * k_all + b
    print(f"I0 linear fit:  I0(k) = {a:+.4f}·k + {b:.2f}")
    print(f"  sparse points: {len(idxs)}  |  fitted range: "
          f"{i0_arr.min():.1f} … {i0_arr.max():.1f}")
    return i0_arr, (idxs, vals, a, b)



# ═════════════════════════════════════════════════════════════════════════════
#  Sinogram builder  (matrix-based histogram)
# ═════════════════════════════════════════════════════════════════════════════

def _compute_t_table(M_flat, rows, cols, pitch):
    """
    Physical x coordinate (mm from detector centre) for every detector pixel,
    using the raw projection matrix (before gantry rotation is applied).

    t_mm[r, c] = (c - M[0,3] - M[0,2]/M[1,2] * (r - M[1,3])) * pitch

    The M[0,2]/M[1,2] term removes the horizontal column shift induced by
    the vertical position of a pixel, correcting for detector tilt.
    """
    M        = np.array(M_flat, dtype=np.float64).reshape(3, 4)
    c_arr    = np.arange(cols, dtype=np.float64)
    r_arr    = np.arange(rows, dtype=np.float64)
    row_corr = M[0, 2] / M[1, 2] * (r_arr - M[1, 3])          # shape (rows,)
    t_mm     = (c_arr[np.newaxis, :] - M[0, 3] - row_corr[:, np.newaxis]) * pitch
    return t_mm.astype(np.float32)


def _apply_matrix_correction_2d(log_proj, M_flat, rows, cols):
    """
    Remap a log-projection image using bilinear interpolation (inverse/pull mapping).

    For each output pixel (r_out, c_out) the corresponding input position is:
        r_in  = r_out − (rows/2 − M[1,3])
        c_in  = c_out − (cols/2 − M[0,3]) + M[0,2]/M[1,2] · (r_out − rows/2)

    Both shifts are referenced to the image centre so the isocenter lands at
    (cols/2, rows/2) in the output.  Pixels whose source falls outside the
    input boundary are set to 0.
    """
    M = np.array(M_flat, dtype=np.float64).reshape(3, 4)

    r_out = np.arange(rows, dtype=np.float64)              # (rows,)
    c_out = np.arange(cols, dtype=np.float64)              # (cols,)

    # ── inverse mapping ───────────────────────────────────────────────────────
    r_in = (r_out - (rows / 2.0 - M[1, 3]))[:, np.newaxis]          # (rows, 1)

    dc_inv = -(cols / 2.0 - M[0, 3]) + M[0, 2] / M[1, 2] * (r_out - rows / 2.0)
    c_in = c_out[np.newaxis, :] + dc_inv[:, np.newaxis]              # (rows, cols)

    r_in = np.broadcast_to(r_in, (rows, cols))                       # (rows, cols)

    # ── bilinear interpolation ────────────────────────────────────────────────
    r0 = np.floor(r_in).astype(np.int32)
    c0 = np.floor(c_in).astype(np.int32)
    wr = (r_in - r0).astype(np.float32)
    wc = (c_in - c0).astype(np.float32)

    valid = (r_in >= 0) & (r_in < rows) & (c_in >= 0) & (c_in < cols)

    r0 = np.clip(r0,     0, rows - 1);  r1 = np.clip(r0 + 1, 0, rows - 1)
    c0 = np.clip(c0,     0, cols - 1);  c1 = np.clip(c0 + 1, 0, cols - 1)

    out = ((1 - wr) * (1 - wc) * log_proj[r0, c0] +
           (1 - wr) *      wc  * log_proj[r0, c1] +
                wr  * (1 - wc) * log_proj[r1, c0] +
                wr  *      wc  * log_proj[r1, c1])
    out[~valid] = 0.0
    return out.astype(np.float32)


def vertical_edge_profile(img, rows_win=VEDGE_ROWS, dx=VEDGE_DX,
                          min_cov=VEDGE_MIN_COV):
    """
    Collapse a corrected 2-D log-projection to a 1-D profile that preserves
    straight VERTICAL edges (stretcher panel sides) and damps everything else
    (the curved phantom silhouette, smooth soft-tissue gradients).

    Steps
    -----
    1. Build a validity mask excluding geometric-correction padding (img≈0) so
       the zero border does not create fake gradients.
    2. Smooth each column over a tall vertical window (masked average).  A
       vertical edge is coherent over all rows and survives; a curved/oblique
       edge crosses a given column over only a few rows and is diluted by
       ~(rows_crossed / rows_win).
    3. Horizontal gradient (central difference over ±dx columns) → each vertical
       edge becomes a sharp signed step.
    4. Rectify and average down the rows → 1-D edge-strength profile (both the
       left and right panel edges appear as positive peaks, matching the
       air-before-peak logic in detect_edge()).

    Columns with fewer than min_cov valid rows are zeroed to suppress the
    detector-border padding artefact.
    """
    mask = (np.abs(img) > 1e-6).astype(np.float32)
    num  = uniform_filter1d(img * mask, rows_win, axis=0, mode="nearest")
    den  = uniform_filter1d(mask,       rows_win, axis=0, mode="nearest")
    V    = num / np.maximum(den, 1e-6)

    G = np.zeros_like(V)
    G[:, dx:-dx] = V[:, 2 * dx:] - V[:, :-2 * dx]
    G *= mask                                   # ignore gradients into padding

    prof = np.abs(G).mean(axis=0)
    prof[mask.mean(axis=0) < min_cov] = 0.0     # kill low-coverage border columns
    return prof.astype(np.float32)


def build_sinogram(image_list, rows, cols, i0_val, scan, save_dir=None,
                   save_dir_uncorr=None):
    """
    Apply the per-projection matrix correction to each image, then compute the
    sinogram as the row average of the corrected log-projection.

    Returns (sino, sino_edge, bin_edges):
        sino[i, c]      = mean over all rows of corrected log-projection column c
        sino_edge[i, c] = vertical-edge-emphasised profile (see
                          vertical_edge_profile); used for stretcher detection
                          when USE_VERTICAL_EDGE is True
        bin_edges       = (cols+1,) array in mm from the isocenter,
                          matching corrected image column c → x = (c - cols/2)*pitch

    i0_val: scalar float  → single I0 for all projections (original behaviour)
            1-D array     → per-projection I0 indexed by img_num-1

    If save_dir is given, each geometrically-corrected image (rows × cols,
    float32) is saved as  NNN_corrproj{cols}x{rows}float.raw.

    If save_dir_uncorr is given, the raw log-projection (log(I0) − log(img))
    *before* the geometric correction is saved as
    NNN_logproj{cols}x{rows}float.raw.
    """
    pitch      = scan["det_column_pitch"]
    mats       = scan["matrix_proj"]
    scalar_i0  = np.isscalar(i0_val)
    n          = len(image_list)
    sino       = np.zeros((n, cols), dtype=np.float64)
    sino_edge  = np.zeros((n, cols), dtype=np.float32)

    for i, (img_num, path) in enumerate(image_list):
        M_flat   = mats[img_num - 1]
        this_i0  = float(i0_val) if scalar_i0 else float(i0_val[img_num - 1])
        img      = np.fromfile(path, dtype=np.uint16).reshape(rows, cols).astype(np.float32)
        np.clip(img, 1, None, out=img)
        log_proj = np.log(this_i0) - np.log(img)

        if save_dir_uncorr is not None:
            fname = f"{img_num:03d}_logproj{cols}x{rows}float.raw"
            log_proj.astype(np.float32).tofile(os.path.join(save_dir_uncorr, fname))

        corrected   = _apply_matrix_correction_2d(log_proj, M_flat, rows, cols)
        sino[i]      = corrected.mean(axis=0)
        sino_edge[i] = vertical_edge_profile(corrected)

        if save_dir is not None:
            fname = f"{img_num:03d}_corrproj{cols}x{rows}float.raw"
            corrected.tofile(os.path.join(save_dir, fname))

    bin_edges = (np.arange(cols + 1, dtype=np.float64) - cols / 2.0) * pitch
    return sino.astype(np.float32), sino_edge, bin_edges


# ═════════════════════════════════════════════════════════════════════════════
#  Edge detection (log-projection domain: air≈0, shadow>0)
# ═════════════════════════════════════════════════════════════════════════════

def _smooth(arr, win):
    return np.convolve(arr, np.ones(win) / win, mode="same")


def detect_edge(row, direction, drop_frac=DROP_FRAC):
    """
    Find the first local maximum above threshold from the given direction.
    Returns the integer bin index as a float, or None if not found.
    No interpolation — marker lands exactly on the peak bin.

    drop_frac sets the detection threshold as air + drop_frac·(shadow − air);
    the vertical-edge profile sits on a higher pedestal than the amplitude
    profile, so it needs a larger drop_frac (see VEDGE_DROP_FRAC).
    """
    s   = _smooth(row, SMOOTH_WIN)
    n   = len(s)
    air = np.percentile(s, 5)
    shd = np.percentile(s, 95)
    if shd - air < 0.01:
        return None
    thresh = air + drop_frac * (shd - air)

    if direction == "L2R":
        for i in range(DEAD + 1, n - DEAD - 1):
            if s[i] > thresh and s[i] >= s[i - 1] and s[i] >= s[i + 1]:
                air_cols = s[DEAD : max(DEAD + 1, i - MIN_AIR_COLS)]
                if len(air_cols) == 0 or air_cols.mean() > thresh:
                    return None
                return float(i)
    else:  # R2L
        for i in range(n - DEAD - 2, DEAD, -1):
            if s[i] > thresh and s[i] >= s[i + 1] and s[i] >= s[i - 1]:
                air_cols = s[min(n - DEAD, i + MIN_AIR_COLS) : n - DEAD]
                if len(air_cols) == 0 or air_cols.mean() > thresh:
                    return None
                return float(i)
    return None


# ═════════════════════════════════════════════════════════════════════════════
#  Projection model  (identical model to find_barella.ipynb)
# ═════════════════════════════════════════════════════════════════════════════

def _project_mm(p, th_rad, sod, sid):
    """Theoretical detector x (mm) for 2-D point p at gantry angle th_rad."""
    R     = np.array([[np.cos(th_rad), -np.sin(th_rad)],
                      [np.sin(th_rad),  np.cos(th_rad)]])
    P_rot = R @ p
    return P_rot[1] * sid / (P_rot[0] + sod)


# ─────────────────────────────────────────────────────────────────────────────

def fit_position(measurements, scan):
    """
    measurements : list of (proj_0based_index, t_mm [, range_idx])
                   where t_mm is the detector x in mm from the rotation centre.
    Returns (component0, component1) in mm from the rotation centre,
    where component1 is the lateral X direction and component0 is the depth Y direction.
    """
    angles_rad = np.deg2rad(scan["angle_eff"])
    sod_arr    = np.array(scan["sod"])
    sid_arr    = np.array(scan["sid"])

    rows_A, rows_y = [], []
    for m in measurements:
        idx, t = m[0], m[1]
        th = angles_rad[idx]
        s  = sod_arr[idx]
        d  = sid_arr[idx] - sod_arr[idx]
        R  = np.array([[np.cos(th), -np.sin(th)],
                       [np.sin(th),  np.cos(th)]])
        rows_A.append(np.array([t, -(d + s)]) @ R)
        rows_y.append(-t * s)

    A = np.array(rows_A)
    y = np.array(rows_y)
    x_hat, _, _, _ = np.linalg.lstsq(A, y, rcond=None)
    return x_hat


def resolve_ranges(ranges, scan0, scan_target):
    """
    Translate projection-index ranges defined for scan 0 into ranges for scan_target.

    For each (lo, hi, direction, edge_label) the angle interval [a_min, a_max]
    covered by images lo..hi in scan 0 is computed; then all 1-based image numbers
    in scan_target whose angle_eff falls inside that interval are collected.
    """
    angles0  = np.array(scan0["angle_eff"])
    angles_t = np.array(scan_target["angle_eff"])
    resolved = []
    for lo, hi, direction, edge_label in ranges:
        seg             = angles0[lo - 1 : hi]
        a_min, a_max    = float(seg.min()), float(seg.max())
        idxs            = np.where((angles_t >= a_min) & (angles_t <= a_max))[0]
        if len(idxs) == 0:
            print(f"  WARNING: no images in target scan for "
                  f"{a_min:.1f}°–{a_max:.1f}° ({direction}, {edge_label})")
            continue
        t_lo, t_hi = int(idxs[0]) + 1, int(idxs[-1]) + 1
        print(f"  range [{lo}..{hi}] → [{t_lo}..{t_hi}]  "
              f"({a_min:.1f}°–{a_max:.1f}°, {direction}, {edge_label})")
        resolved.append((t_lo, t_hi, direction, edge_label))
    return resolved


# ═════════════════════════════════════════════════════════════════════════════
#  I0 resolution + corrected-projection export
# ═════════════════════════════════════════════════════════════════════════════

def resolve_i0_path(base, scan_index, i0_list_override=None):
    """
    Decide which I0 source a scan should use, matching the auto-detect rule in
    __main__:  explicit list override  →  <base>/imgScan_<n>_I0list.txt if it
    exists  →  None (caller falls back to the scalar I0.txt file).
    """
    if i0_list_override:
        return i0_list_override
    candidate = os.path.join(base, f"imgScan_{scan_index}_I0list.txt")
    return candidate if os.path.isfile(candidate) else None


def resolve_i0_value(base, scan_index, i0_file, images, i0_list_override=None):
    """Return the I0 to use for a scan: per-projection array if a sparse list is
    found, else the scalar value from I0.txt."""
    i0_list_path = resolve_i0_path(base, scan_index, i0_list_override)
    if i0_list_path:
        n_proj  = max(num for num, _ in images)
        i0_dict = load_i0_list(i0_list_path)
        i0_val, _ = fit_i0_linear(i0_dict, n_proj)
        return i0_val
    return load_i0(i0_file, f"imgScan_{scan_index}")


def export_corrected_projections(params_path, base, i0_file, scan_index,
                                 i0_list_override=None):
    """
    Build and save the geometrically-corrected 2-D projections for one scan,
    without running edge detection / fitting.  Used to export scans other than
    the one being analysed (e.g. imgScan_1 when analysing scan 0).
    """
    scan   = load_params(params_path, scan_index)
    imgdir = os.path.join(base, f"imgScan_{scan_index}")
    if not os.path.isdir(imgdir):
        print(f"[scan {scan_index}] {imgdir} not found — skipping export.")
        return

    images = list_images(imgdir)
    if not images:
        print(f"[scan {scan_index}] no images in {imgdir} — skipping export.")
        return

    i0_val = resolve_i0_value(base, scan_index, i0_file, images, i0_list_override)
    rows   = scan["det_rows"]
    cols   = scan["det_columns"]

    corrected_dir = os.path.join(base, f"corrected_imgScan_{scan_index}")
    uncorr_dir    = os.path.join(base, f"uncorrected_imgScan_{scan_index}")
    os.makedirs(corrected_dir, exist_ok=True)
    os.makedirs(uncorr_dir,    exist_ok=True)
    print(f"[scan {scan_index}] saving {len(images)} corrected 2D projections "
          f"→ {corrected_dir}")
    print(f"[scan {scan_index}] saving {len(images)} uncorrected log projections "
          f"→ {uncorr_dir}")
    build_sinogram(images, rows, cols, i0_val, scan,
                   save_dir=corrected_dir, save_dir_uncorr=uncorr_dir)


# ═════════════════════════════════════════════════════════════════════════════
#  Main
# ═════════════════════════════════════════════════════════════════════════════

def main(params_path, imgdir, i0_path, scan_index, vol_size_mm=None,
         i0_list_path=None):
    scan   = load_params(params_path, scan_index)
    images = list_images(imgdir)

    # ── Resolve projection ranges for this scan ───────────────────────────────
    if scan_index == 0:
        active_ranges = RANGES
    else:
        scan0 = load_params(params_path, 0)
        print(f"Resolving angle ranges from scan 0 → scan {scan_index}:")
        active_ranges = resolve_ranges(RANGES, scan0, scan)

    if i0_list_path:
        n_proj   = max(num for num, _ in images)
        i0_dict  = load_i0_list(i0_list_path)
        i0_val, (sp_idx, sp_val, _a, _b) = fit_i0_linear(i0_dict, n_proj)

        # ── diagnostic plot: sparse points + linear fit ───────────────────────
        _, ax0 = plt.subplots(figsize=(9, 4))
        ax0.scatter(sp_idx, sp_val, s=20, color="steelblue", zorder=3,
                    label="measured I0 (sparse)")
        k_full = np.arange(1, n_proj + 1)
        ax0.plot(k_full, i0_val, color="tomato", lw=1.5,
                 label=f"linear fit  I0(k)={_a:+.3f}·k+{_b:.1f}")
        ax0.set_xlabel("Projection index (1-based)")
        ax0.set_ylabel("I0")
        ax0.set_title("I0 linear fit from sparse measurements")
        ax0.legend(fontsize=9)
        plt.tight_layout()
    else:
        i0_val = load_i0(i0_path, f"imgScan_{scan_index}")
    rows = scan["det_rows"]
    cols = scan["det_columns"]

    i0_label = f"{float(np.mean(i0_val)):.1f} (mean)" if not np.isscalar(i0_val) else f"{i0_val:.1f}"
    print(f"Images: {len(images)}  det {cols}×{rows}  I0={i0_label}")

    # ── Build sinogram & save corrected + uncorrected 2D projections ──────────
    corrected_dir = os.path.join(os.path.dirname(imgdir), f"corrected_imgScan_{scan_index}")
    uncorr_dir    = os.path.join(os.path.dirname(imgdir), f"uncorrected_imgScan_{scan_index}")
    os.makedirs(corrected_dir, exist_ok=True)
    os.makedirs(uncorr_dir,    exist_ok=True)
    print("Building sinogram …")
    sino, sino_edge, bin_edges = build_sinogram(images, rows, cols, i0_val, scan,
                                                save_dir=corrected_dir,
                                                save_dir_uncorr=uncorr_dir)
    print(f"Saved {len(images)} corrected 2D projections   → {corrected_dir}")
    print(f"Saved {len(images)} uncorrected log projections → {uncorr_dir}")

    # Detection profile: vertical-edge-emphasised sinogram (robust to an
    # intervening phantom) or the legacy row-average amplitude.
    sino_det = sino_edge if USE_VERTICAL_EDGE else sino
    print(f"Detection sinogram: {'vertical-edge' if USE_VERTICAL_EDGE else 'row-average amplitude'}")
    bin_width       = bin_edges[1] - bin_edges[0]
    img_numbers = [num for num, _ in images]   # 1-based list

    angles_deg = np.array(scan["angle_eff"])

    # ── Detect edges in each range ────────────────────────────────────────────
    # detections: (sino_row, col_float, range_idx, proj_idx)
    detections = []

    for ri, (lo, hi, direction, edge_label) in enumerate(active_ranges):
        for img_num in range(lo, hi + 1):
            if img_num not in img_numbers:
                continue
            sino_row = img_numbers.index(img_num)
            proj_idx = img_num - 1

            drop = VEDGE_DROP_FRAC if USE_VERTICAL_EDGE else DROP_FRAC
            col = detect_edge(sino_det[sino_row], direction, drop_frac=drop)
            if col is None:
                continue
            # gate on the amplitude sinogram: the detected column must sit in a
            # region of real attenuation (rejects noise / clear-air peaks)
            if sino[sino_row, int(col)] < MIN_SINO_VAL:
                continue
            detections.append((sino_row, col, ri, proj_idx))

    # ── Per-range linear fit → flag outliers ──────────────────────────────────
    outlier_mask = [False] * len(detections)   # True = outlier

    for ri in range(len(active_ranges)):
        idx_in_range = [i for i, d in enumerate(detections) if d[2] == ri]
        if len(idx_in_range) < 3:
            continue
        ang = np.array([angles_deg[detections[i][3]] for i in idx_in_range])
        col = np.array([detections[i][1]             for i in idx_in_range])
        a, b   = np.polyfit(ang, col, 1)
        resid  = col - (a * ang + b)
        sigma  = resid.std()
        lo_lbl, hi_lbl, d, el = active_ranges[ri]
        n_out  = 0
        for k, i in enumerate(idx_in_range):
            if abs(resid[k]) > OUTLIER_SIGMA * sigma:
                outlier_mask[i] = True
                n_out += 1
        print(f"  [{lo_lbl}..{hi_lbl}] {d:3s} ({el:5s}): "
              f"{len(idx_in_range)} edges  σ={sigma:.1f} px  outliers={n_out}")

    # ── Build measurement lists excluding outliers ────────────────────────────
    left_meas  = []   # (proj_idx, t_mm, range_idx)  — mm from rotation centre
    right_meas = []
    left_pts   = []   # (angle_deg, t_mm, range_idx) for validation plot
    right_pts  = []
    print(f"\n{'img':>5}  {'angle_deg':>10}  {'edge':>5}  {'x_mm':>10}  {'sino_val':>8}  {'outlier'}")
    for i, (sino_row, col, ri, proj_idx) in enumerate(detections):
        x_mm_edge  = bin_edges[0] + (col + 0.5) * bin_width
        edge_label  = active_ranges[ri][3]
        flag        = "OUTLIER" if outlier_mask[i] else ""
        sino_val    = sino[sino_row, int(col)]
        print(f"{proj_idx+1:>5}  {angles_deg[proj_idx]:>10.3f}  {edge_label:>5}  {x_mm_edge:>10.3f}  {sino_val:>8.4f}  {flag}")
        if outlier_mask[i]:
            continue
        if edge_label == "left":
            left_meas.append((proj_idx, x_mm_edge, ri))
            left_pts.append((angles_deg[proj_idx], x_mm_edge, ri))
        else:
            right_meas.append((proj_idx, x_mm_edge, ri))
            right_pts.append((angles_deg[proj_idx], x_mm_edge, ri))

    # ── Sinogram plot with overlaid edges ─────────────────────────────────────
    fig, ax = plt.subplots(figsize=(14, 6))
    vmax = np.percentile(sino_det, 99)
    ax.imshow(sino_det, aspect="auto", cmap="gray", vmin=0, vmax=vmax,
              extent=[bin_edges[0], bin_edges[-1], len(images), 0], origin="upper")

    # reference marker: first non-zero bin of row 0, same placement rule as edge dots
    ref_bin = int(np.argmax(sino[0] > 0))
    ax.plot(bin_edges[0] + (ref_bin + 0.5) * bin_width, 0.5, "+", color="blue",
            markersize=10, markeredgewidth=1.5, zorder=10, label="ref [row0, first>0]")

    for i, (sino_row, col, ri, _) in enumerate(detections):
        x_plot = bin_edges[0] + (col + 0.5) * bin_width
        y_plot = sino_row + 0.5          # shift to pixel centre
        if outlier_mask[i]:
            ax.plot(x_plot, y_plot, "x", color="red", markersize=6,
                    markeredgewidth=1.2, zorder=5)
        else:
            ax.plot(x_plot, y_plot, ".", color=RANGE_COLORS[ri],
                    markersize=3, alpha=0.85)

    patches = [
        mpatches.Patch(color=RANGE_COLORS[ri],
                       label=f"[{lo}..{hi}] {d} ({el})")
        for ri, (lo, hi, d, el) in enumerate(active_ranges)
    ]
    patches.append(mpatches.Patch(color="red", label="outlier"))
    ax.legend(handles=patches, loc="upper right", fontsize=9)
    ax.set_xlabel("Physical x position (mm)")
    ax.set_ylabel("Projection index")
    ax.set_title(f"{'Vertical-edge' if USE_VERTICAL_EDGE else 'Row-averaged'} "
                 f"sinogram with detected stretcher edges")
    plt.tight_layout()

    # ── Fit & report ──────────────────────────────────────────────────────────
    if len(left_meas) >= 3 and len(right_meas) >= 3:
        p_L = fit_position(left_meas,  scan)
        p_R = fit_position(right_meas, scan)

        avg = (p_L + p_R) / 2.0
        sep = np.linalg.norm(p_L - p_R)

        print(f"\n[left  edge]  px={p_L[1]:.2f} mm  py={p_L[0]:.2f} mm  ({len(left_meas)} pts)")
        print(f"[right edge]  px={p_R[1]:.2f} mm  py={p_R[0]:.2f} mm  ({len(right_meas)} pts)")
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

        # ── Fit validation plot ───────────────────────────────────────────────
        sod_arr  = np.array(scan["sod"])
        sid_arr  = np.array(scan["sid"])
        sod_mean = sod_arr.mean()
        sid_mean = sid_arr.mean()
        th_smooth = np.deg2rad(np.linspace(0, 360, 1000))

        fig2, axes2 = plt.subplots(2, 2, figsize=(14, 8))
        for row_idx, (label, p_fit, pts, meas) in enumerate([
            ("left",  p_L, left_pts,  left_meas),
            ("right", p_R, right_pts, right_meas),
        ]):
            ax_fit, ax_res = axes2[row_idx]

            curve = np.array([_project_mm(p_fit, th, sod_mean, sid_mean)
                               for th in th_smooth])
            ax_fit.plot(np.rad2deg(th_smooth), curve, lw=1, color="k",
                        label=f"fit  (py={p_fit[0]:.1f}, px={p_fit[1]:.1f}) mm")

            # scatter measured points coloured by range
            for ri_plot in range(len(active_ranges)):
                sub = [(a, x) for a, x, ri in pts if ri == ri_plot]
                if not sub:
                    continue
                lo, hi, d, el = active_ranges[ri_plot]
                ax_fit.scatter([a for a, _ in sub], [x for _, x in sub],
                               s=14, color=RANGE_COLORS[ri_plot], zorder=3,
                               label=f"[{lo}..{hi}] {d}")

            ax_fit.set_xlabel("Gantry angle (deg)")
            ax_fit.set_ylabel("Detector x (mm)")
            ax_fit.set_title(f"{label} edge — projection curve")
            ax_fit.legend(fontsize=8)

            xmm_m  = np.array([x for _, x, _ in pts])
            t_pred = np.array([_project_mm(p_fit, np.deg2rad(angles_deg[idx]),
                                            sod_arr[idx], sid_arr[idx])
                                for idx, _, _ in meas])
            res_mm = xmm_m - t_pred
            rms    = np.sqrt(np.mean(res_mm ** 2))

            for ri_plot in range(len(active_ranges)):
                sub_idx = [k for k, (_, _, ri) in enumerate(pts) if ri == ri_plot]
                if not sub_idx:
                    continue
                lo, hi, d, el = active_ranges[ri_plot]
                ax_res.scatter([list(pts)[k][0] for k in sub_idx],
                               res_mm[sub_idx], s=14,
                               color=RANGE_COLORS[ri_plot], label=f"[{lo}..{hi}]")

            ax_res.axhline(0, color="k", lw=0.8, ls="--")
            ax_res.set_xlabel("Gantry angle (deg)")
            ax_res.set_ylabel("Residual (mm)")
            ax_res.set_title(f"{label} edge — residuals  |  RMS = {rms:.3f} mm")
            ax_res.legend(fontsize=8)

        fig2.suptitle("Fit validation: measured vs predicted detector x", fontsize=11)
        plt.tight_layout()
    else:
        print("Not enough edges for regression.")

    plt.show()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Detect stretcher position from CBCT projections.")
    parser.add_argument("--params",   default=_PARAMS,
                        help="Path to the _Params.json file.")
    parser.add_argument("--scan",     type=int, default=0,
                        help="Scan index (0 or 1). Sets imgScan_N dir and I0 source automatically.")
    parser.add_argument("--i0",       default=_I0FILE,
                        help="Path to I0.txt (scalar per scan, used when no I0 list is found).")
    parser.add_argument("--imgdir",   default=None,
                        help="Override imgScan directory (default: _BASE/imgScan_<scan>).")
    parser.add_argument("--i0-list",  default=None,
                        help="Override sparse I0 list file (default: _BASE/imgScan_<scan>_I0list.txt "
                             "if it exists, else falls back to --i0).")
    parser.add_argument("--vol-size", default="500x500",
                        help="XY volume size in mm as WxH, e.g. 500x500.")
    args = parser.parse_args()

    scan_key = f"imgScan_{args.scan}"

    imgdir = args.imgdir or os.path.join(_BASE, scan_key)

    # I0 list: use explicit override, then auto-detect, then fall back to scalar file
    if args.i0_list:
        i0_list_path = args.i0_list
    else:
        candidate = os.path.join(_BASE, f"{scan_key}_I0list.txt")
        i0_list_path = candidate if os.path.isfile(candidate) else None

    if i0_list_path:
        print(f"Using per-projection I0 list: {i0_list_path}")
    else:
        print(f"Using scalar I0 from: {args.i0}  (key: {scan_key})")

    vol = None
    if args.vol_size:
        w, h = args.vol_size.lower().split("x")
        vol  = (float(w), float(h))

    # ── Export corrected 2D projections for every other scan in the study ─────
    # main() saves the corrected projections for the analysed scan; do the same
    # for the remaining scans (e.g. imgScan_1 when analysing scan 0) so a single
    # run produces all corrected_imgScan_<n>/ directories.
    n_scans = len(load_study(args.params)["scans"])
    for s in range(n_scans):
        if s == args.scan:
            continue
        export_corrected_projections(args.params, _BASE, args.i0, s)

    main(args.params, imgdir, args.i0, args.scan, vol_size_mm=vol,
         i0_list_path=i0_list_path)
