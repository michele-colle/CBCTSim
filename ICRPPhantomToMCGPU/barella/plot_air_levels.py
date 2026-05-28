#!/usr/bin/env python3
"""
plot_air_levels.py

For each of the 4 air scans in TestRAR__25_05_2026, plot the mean raw pixel
value in the central-half-width column ROI for every projection.

Two figures side-by-side: 120 kV (imgScan_0) and 80 kV (imgScan_1),
each with 4 lines (pre-table air ×2, post-table air ×2).
"""

import os
import re
import functools
import numpy as np
import matplotlib.pyplot as plt

from cbct_utils import (find_params_json, load_study, load_acq_time,
                        list_images, calc_i0_mean_from_dap)

# ── Paths ──────────────────────────────────────────────────────────────────────
_BASE = "/mnt/c/Users/colle/Downloads/TestRAR__25_05_2026"

AIR_SCANS = [
    ("02_ScanBlank2",           "Air 2"),
    ("05_ScanBlank3_postTable", "Air 3"),
    ("06_ScanBlank4_postTable", "Air 4"),
]
AIR_COLORS = ["darkorange", "seagreen", "crimson"]

TABLE_SCANS = [
    ("03_ScanTable1", "Table 1"),
    ("04_ScanTable2", "Table 2"),
]
TABLE_COLORS = ["mediumpurple", "hotpink"]

KV_LABEL = {0: "120 kV  (imgScan_0)", 1: "80 kV  (imgScan_1)"}

# ── Simulated projections (already log-projected, float32) ────────────────────
SIM_DIRS = {
    0: ("/mnt/f/Michele_diskF/GradientHealth/download/testRAR-13MAY2026"
        "/dicomweb/export/GRDN1TQOUC2B1R2U/20251119_11096464/MCGPU_sim"
        "/08b_Stretcher_test_rar_25_05_26_120_kV/results/total_proj"),
    # 1: "...",  # 80 kV — add when ready
}


def _hhmm(pjson):
    """Return 'HH:MM' from a *_Params.json acquisition time."""
    return load_acq_time(pjson).split()[-1][:5]


def _sort_legend(ax, loc="lower right"):
    """Sort the current axes legend handles chronologically by the HH:MM in labels."""
    handles, labels = ax.get_legend_handles_labels()
    pairs = sorted(zip(handles, labels), key=lambda x: x[1].split(" - ")[-1])
    ax.legend(*zip(*pairs), loc=loc, fontsize=9)


@functools.lru_cache(maxsize=None)
def central_roi_mean(imgdir, rows, cols):
    """
    For every projection in imgdir return the mean raw uint16 value in the
    central half of the columns (cols//4 : 3*cols//4), averaged over all rows.
    """
    images = list_images(imgdir)
    c0, c1 = cols // 4, cols - cols // 4
    means = np.empty(len(images), dtype=np.float64)
    for i, (_, path) in enumerate(images):
        img = np.fromfile(path, dtype=np.uint16).reshape(rows, cols)
        means[i] = img[:, c0:c1].mean()
    return means


def _classify_and_fit_dap(dap):
    """
    Detect known bad DAP patterns and return a linear fit to the valid portion.

    Returns (issue, slope, intercept, good_mask):
      issue       'sparse' | 'plateau' | None
      slope/intercept  linear fit to the good portion (or None if unclassified)
      good_mask   bool array of length len(dap) marking the fitted samples
    """
    dap = np.asarray(dap, dtype=np.float64)
    n   = len(dap)
    deltas = np.diff(dap)

    pos_d = deltas[deltas > 0]
    if len(pos_d) < 5:
        return "empty", None, None, None
    expected = np.median(pos_d)

    # ── Case 1: sparse — bulk of the array is near zero ──────────────────────
    if np.mean(dap < np.max(dap) * 0.02) > 0.4:
        first_good = np.where(dap >= np.max(dap) * 0.02)[0][0]
        good = np.zeros(n, dtype=bool)
        good[first_good:] = True
        idx = np.where(good)[0]
        a, b = np.polyfit(idx, dap[idx], 1)
        return "sparse", a, b, good

    # ── Case 2: plateau — deltas drop to near zero and stay there ────────────
    active      = deltas > expected * 0.1
    last_active = np.where(active)[0][-1] if np.any(active) else 0
    if (n - 1 - last_active) > max(5, n * 0.05):
        good = np.zeros(n, dtype=bool)
        good[: last_active + 2] = True
        idx  = np.where(good)[0]
        a, b = np.polyfit(idx, dap[idx], 1)
        return "plateau", a, b, good

    return None, None, None, None


def plot_dap(all_scans, all_colors, all_ls):
    _, axes = plt.subplots(2, 2, figsize=(15, 9), sharey="row")
    axes[0, 0].set_title(KV_LABEL[0])
    axes[0, 1].set_title(KV_LABEL[1])

    for scan_idx in range(2):
        ax_cum   = axes[0, scan_idx]
        ax_delta = axes[1, scan_idx]

        for (folder, label), color, ls in zip(all_scans, all_colors, all_ls):
            scan_dir = os.path.join(_BASE, folder)
            pjson    = find_params_json(scan_dir)
            study    = load_study(pjson)
            dap      = study["scans"][scan_idx].get("dap_value")
            if dap is None:
                continue
            dap   = np.asarray(dap, dtype=np.float64)
            issue, slope, intercept, good = _classify_and_fit_dap(dap)

            if issue:          # skip corrupted traces entirely
                continue

            ax_cum.plot(dap, color=color, lw=1, ls=ls,
                        label=f"{label} - {_hhmm(pjson)}")
            ax_delta.plot(np.diff(dap), color=color, lw=1, ls=ls)
            if slope is not None:
                n   = len(dap)
                fit = slope * np.arange(n) + intercept
                ax_cum.plot(np.where(good)[0], fit[good],
                            color=color, lw=2, ls=":", alpha=0.8)
                x_good = np.where(good)[0]
                ax_delta.hlines(slope, x_good[0], x_good[-1],
                                color=color, lw=2, ls=":", alpha=0.8)

        for ax, ylabel in [(ax_cum,   "Cumulative DAP"),
                           (ax_delta, "Per-sample DAP increment")]:
            ax.set_xlabel("DAP sample index")
            ax.set_ylabel(ylabel)
        _sort_legend(ax_cum)

    plt.suptitle("DAP values — cumulative (top) and per-sample increments (bottom)",
                 fontsize=11)
    plt.tight_layout()


def print_summary_table(all_scans):
    hdr = (f"{'kV':>4}  {'folder':<35}  {'roi_mean':>10}  "
           f"{'i0_dap':>10}  {'status'}")
    print("\n" + hdr)
    print("-" * len(hdr) + "-" * 12)
    for scan_idx in range(2):
        kv_str = "120" if scan_idx == 0 else " 80"
        for folder, _ in all_scans:
            scan_dir = os.path.join(_BASE, folder)
            pjson    = find_params_json(scan_dir)
            study    = load_study(pjson)
            params   = study["scans"][scan_idx]
            dap      = params.get("dap_value")

            # DAP status
            if dap is None:
                status = "NO DAP"
                i0_str = "—"
            else:
                issue, *_ = _classify_and_fit_dap(np.asarray(dap))
                if issue:
                    status = f"CORRUPTED ({issue})"
                    i0_str = "—"
                else:
                    status = "ok"
                    i0    = calc_i0_mean_from_dap(params, study.get("fov", ""))
                    i0_str = f"{i0:.1f}" if i0 is not None else "—"

            rows, cols = params["det_rows"], params["det_columns"]
            imgdir     = os.path.join(scan_dir, f"imgScan_{scan_idx}")
            roi_mean   = central_roi_mean(imgdir, rows, cols).mean()

            print(f"{kv_str:>4}  {folder:<35}  {roi_mean:>10.1f}  "
                  f"{i0_str:>10}  {status}")
    print()


def _list_sim_files(sim_dir):
    """
    Return sorted list of (index, rows, cols, path) for simulated projection
    files named like '0000_total_proj_512x512float.raw'.
    """
    pat = re.compile(r"^(\d+)_.*?(\d+)x(\d+)float\.raw$", re.IGNORECASE)
    entries = []
    for fname in os.listdir(sim_dir):
        m = pat.match(fname)
        if m:
            idx, rows, cols = int(m.group(1)), int(m.group(2)), int(m.group(3))
            entries.append((idx, rows, cols, os.path.join(sim_dir, fname)))
    entries.sort(key=lambda x: x[0])
    return entries


def _get_means(folder, scan_idx):
    scan_dir   = os.path.join(_BASE, folder)
    params     = load_study(find_params_json(scan_dir))["scans"][scan_idx]
    imgdir     = os.path.join(scan_dir, f"imgScan_{scan_idx}")
    return central_roi_mean(imgdir, params["det_rows"], params["det_columns"])


def plot_averages():
    _, axes = plt.subplots(1, 2, figsize=(15, 5), sharey=False)
    all_vals = []

    for scan_idx, ax in enumerate(axes):
        air_stack   = np.stack([_get_means(f, scan_idx) for f, _ in AIR_SCANS])
        table_stack = np.stack([_get_means(f, scan_idx) for f, _ in TABLE_SCANS])
        air_avg     = air_stack.mean(axis=0)
        table_avg   = table_stack.mean(axis=0)
        all_vals.extend([air_avg, table_avg])

        ax.plot(air_avg,   color="steelblue",   lw=1.5, label="Air avg (02, 05, 06)")
        ax.plot(table_avg, color="mediumpurple", lw=1.5, ls="--", label="Table avg (03, 04)")
        ax.set_title(KV_LABEL[scan_idx])
        ax.set_xlabel("Projection index")
        ax.set_ylabel("Mean raw pixel value (DN)")
        ax.legend(loc="lower right", fontsize=9)
        ax.grid(True, lw=0.4, alpha=0.6)

    y_min = min(v.min() for v in all_vals)
    y_max = max(v.max() for v in all_vals)
    margin = (y_max - y_min) * 0.05
    for ax in axes:
        ax.set_ylim(y_min - margin, y_max + margin)

    plt.suptitle("Average profiles — air (02,05,06) vs table (03,04)", fontsize=12)
    plt.tight_layout()


def plot_log_projection():
    """
    1. I0(i)      = mean of the 3 air-scan ROI values at projection i  (cached)
    2. img_avg(i) = pixel-wise mean of Table 1 and Table 2 full images at projection i
    3. proj(i)    = mean_ROI[ ln(I0(i)) − ln(img_avg(i)) ]
    """
    _, axes = plt.subplots(1, 2, figsize=(15, 5), sharey=False)
    all_proj = []

    for scan_idx, ax in enumerate(axes):
        # I0 per projection from the 3 air scans (scalar, cached)
        air_avg = np.mean([_get_means(f, scan_idx) for f, _ in AIR_SCANS], axis=0)

        # table scan metadata (image list + geometry)
        table_meta = []
        for folder, _ in TABLE_SCANS:
            sd     = os.path.join(_BASE, folder)
            study  = load_study(find_params_json(sd))
            params = study["scans"][scan_idx]
            imgdir = os.path.join(sd, f"imgScan_{scan_idx}")
            table_meta.append((list_images(imgdir), params["det_rows"], params["det_columns"]))

        images0, rows, cols = table_meta[0]
        n_proj = len(images0)
        c0, c1 = cols // 4, cols - cols // 4
        proj_roi = np.empty(n_proj, dtype=np.float64)

        out_dir = os.path.join(_BASE, f"imgScan_{scan_idx}_proj")
        os.makedirs(out_dir, exist_ok=True)

        print(f"  log-projection {KV_LABEL[scan_idx]}: {n_proj} projections ...", flush=True)
        for i in range(n_proj):
            imgs = [
                np.fromfile(images[i][1], dtype=np.uint16)
                  .reshape(r, c).astype(np.float32)
                for images, r, c in table_meta
            ]
            img_avg = np.mean(imgs, axis=0)
            np.clip(img_avg, 1, None, out=img_avg)
            log_proj    = np.log(float(air_avg[i])) - np.log(img_avg)
            proj_roi[i] = log_proj[:, c0:c1].mean()

            out_path = os.path.join(out_dir, f"{i:04d}_{cols}x{rows}float.raw")
            log_proj.astype(np.float32).tofile(out_path)

        print(f"  saved {n_proj} images → {out_dir}", flush=True)

        all_proj.append(proj_roi)
        color = "steelblue" if scan_idx == 0 else "tomato"
        ax.plot(proj_roi, color=color, lw=1, label="Experimental")
        ax.set_title(KV_LABEL[scan_idx])
        ax.set_xlabel("Projection index")
        ax.set_ylabel("Mean log-projection in ROI")
        ax.grid(True, lw=0.4, alpha=0.6)

        # ── Simulated overlay ─────────────────────────────────────────────────
        sim_dir = SIM_DIRS.get(scan_idx)
        if sim_dir and os.path.isdir(sim_dir):
            sim_files = _list_sim_files(sim_dir)
            if sim_files:
                _, s_rows, s_cols, _ = sim_files[0]
                sc0, sc1 = s_cols // 4, s_cols - s_cols // 4
                sim_roi = np.empty(len(sim_files), dtype=np.float64)
                print(f"  simulated {KV_LABEL[scan_idx]}: {len(sim_files)} projections "
                      f"({s_rows}×{s_cols}) ...", flush=True)
                for j, (_, r, c, path) in enumerate(sim_files):
                    img = np.fromfile(path, dtype=np.float32).reshape(r, c)
                    sim_roi[j] = img[:, sc0:sc1].mean()
                all_proj.append(sim_roi)
                ax.plot(sim_roi, color="red", lw=1, ls="--", label="Simulated")
                ax.legend(loc="lower right", fontsize=9)

    y_min = min(v.min() for v in all_proj)
    y_max = max(v.max() for v in all_proj)
    margin = (y_max - y_min) * 0.05
    for ax in axes:
        ax.set_ylim(y_min - margin, y_max + margin)

    plt.suptitle("Log-projection of averaged stretcher — ROI mean  "
                 "(I0 = air avg 02+05+06)", fontsize=12)
    plt.tight_layout()


def main():
    all_scans  = AIR_SCANS   + TABLE_SCANS
    all_colors = AIR_COLORS  + TABLE_COLORS
    all_ls     = ["-"] * len(AIR_SCANS) + ["--"] * len(TABLE_SCANS)
    print_summary_table(all_scans)
    plot_dap(all_scans, all_colors, all_ls)
    plot_averages()
    plot_log_projection()

    fig, axes = plt.subplots(1, 2, figsize=(15, 5), sharey=False)

    all_air_global   = []   # for lower limit: air scans only (excl. Air 1)
    all_table_global = []   # for upper limit: table scans included

    for scan_idx, ax in enumerate(axes):
        # ── air scans ────────────────────────────────────────────────────────
        for (folder, label), color in zip(AIR_SCANS, AIR_COLORS):
            scan_dir   = os.path.join(_BASE, folder)
            pjson      = find_params_json(scan_dir)
            study      = load_study(pjson)
            params     = study["scans"][scan_idx]
            rows, cols = params["det_rows"], params["det_columns"]
            imgdir     = os.path.join(scan_dir, f"imgScan_{scan_idx}")
            means      = central_roi_mean(imgdir, rows, cols)
            all_air_global.append(means)
            ax.plot(means, color=color, lw=1,
                    label=f"{label} - {_hhmm(pjson)}")

            # actual average — X marker
            ax.plot(len(means) // 2, means.mean(), "x", color=color,
                    markersize=10, markeredgewidth=2, zorder=6)

            # DAP-calculated I0 — square marker (only if DAP is not corrupted)
            dap = params.get("dap_value")
            dap_ok = dap is not None and _classify_and_fit_dap(np.asarray(dap))[0] is None
            if dap_ok:
                i0 = calc_i0_mean_from_dap(params, study.get("fov", ""))
                if i0 is not None:
                    ax.plot(len(means) // 2, i0, "s", color=color,
                            markersize=8, zorder=5)

        # ── table scans: plotted but y axis stays fixed ───────────────────────
        for (folder, label), color in zip(TABLE_SCANS, TABLE_COLORS):
            scan_dir   = os.path.join(_BASE, folder)
            pjson      = find_params_json(scan_dir)
            study      = load_study(pjson)
            params     = study["scans"][scan_idx]
            rows, cols = params["det_rows"], params["det_columns"]
            imgdir     = os.path.join(scan_dir, f"imgScan_{scan_idx}")
            means      = central_roi_mean(imgdir, rows, cols)
            all_table_global.append(means)
            ax.plot(means, color=color, lw=1, ls="--",
                    label=f"{label} - {_hhmm(pjson)}")

        ax.set_title(KV_LABEL[scan_idx])
        ax.set_xlabel("Projection index")
        ax.set_ylabel("Mean raw pixel value (DN)")
        _sort_legend(ax)
        ax.grid(True, lw=0.4, alpha=0.6)

    # lower limit: air scans only (excl. Air 1); upper limit: also includes table scans
    air_min = min(m.min() for m in all_air_global)
    air_max = max(m.max() for m in all_air_global + all_table_global)
    margin  = (air_max - air_min) * 0.05
    for ax in axes:
        ax.set_ylim(air_min - margin, air_max + margin)

    fig.suptitle("Air level — central half-width ROI  "
                 "(■ DAP-calc I0 · ✕ actual mean)", fontsize=12)
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()
