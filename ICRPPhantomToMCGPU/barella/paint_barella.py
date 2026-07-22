#!/usr/bin/env python3
r"""
paint_barella.py — interactively overlay a forward-projected stretcher onto the
measured sinogram and align it by hand.

Rationale
---------
With a phantom in the field of view an edge-detector cannot reliably tell the
stretcher panel edges apart from the phantom structure.  Instead we use the
KNOWN stretcher cross-section (stretcher_profile.py) as a rigid forward model:

  1. Build the row-averaged sinogram from the already-corrected projections
     (corrected_imgScan_<scan>/, produced by find_barella_auto.py).
  2. For a candidate centre (cx, cy) and in-plane rotation θ, forward-project the
     stretcher shell through a single axial row (Lambert–Beer line integral) at
     every gantry angle → a synthetic "stretcher-only" sinogram.
  3. Paint that synthetic sinogram over the measured one with a graduated colour
     map and drag cx / cy / θ with sliders until the painted edges sit on the
     real moving edges.

Only the centre (and optionally rotation) is a free parameter; the stretcher
shape and the scan geometry are fixed.

Usage
-----
    python paint_barella.py [--scan 0] [--params ...] [--base ...]

Requires the corrected_imgScan_<scan>/ directory to already exist
(run find_barella_auto.py first if it does not).
"""

import argparse
import glob
import os
import re

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from matplotlib.widgets import Slider, Button
from scipy.ndimage import gaussian_filter
from scipy.optimize import minimize

from cbct_utils import load_params
from find_barella_auto import fit_position, _project_mm

# ── Default paths (mirror find_barella_auto.py) ───────────────────────────────
_BASE   = ("/mnt/f/Michele_diskF/Test Fantoccio Zurigo-CBCTvsCT/CBCT_DE/"
            "Fantoccio_Head_17x17DE_Reg_Zurigo/Fantoccio_Head_17x17_Zurigo")
_PARAMS = os.path.join(_BASE, "19.1240516105915049.1331_Params.json")
#_BASE   = "/mnt/f/Michele_diskF/TestRAR__25_05_2026/04_ScanTable2"
#_PARAMS = os.path.join(_BASE, "19.1220805103146695.379_Params.json")

# ── Stretcher cross-section parameters (mm) — from stretcher_profile.py ────────
CX0        = -14.0     # initial centre X (lateral, mm from isocenter)
CY0        = 191.0     # initial centre Y (depth,   mm from isocenter)
THETA0_DEG = -0.75     # initial in-plane rotation (deg)
# Fine-tuning slider half-ranges (mm / deg): each slider spans init ± span with
# a small step.  do_fit() re-centres them on the fitted value, so click-to-fit
# still positions coarsely at any scale.  Widen these for a fresh coarse search.
CX_SPAN    = 50.0
CY_SPAN    = 50.0
TH_SPAN    = 5.0
CX_STEP    = 0.1
CY_STEP    = 0.1
TH_STEP    = 0.05

# ── Auto-refine (edge-strength) — the constraint is on the OUTPUT (edge column
#    position in the sinogram), not on the parameters.  The predicted edge may
#    move freely within ±REFINE_FLAT px of the seed track (the pose when the
#    button is pressed); beyond that a quadratic penalty rises steeply, so the
#    fit snaps to nearby ridge crests but cannot jump to the phantom's bands.
REFINE_FLAT  = 10.0   # px: flat (zero-cost) corridor half-width around the seed edge
REFINE_GAIN  = 6.0    # weight of the ridge-crest reward inside the corridor
REFINE_SIGMA = 4.0    # px: lateral blur of the ridge reward → smooth capture basin
REFINE_STEP  = (2.0, 2.0, 0.5)  # initial simplex step (cx mm, cy mm, θ deg)
TOP_W      = 440.0
BOT_W      = 396.0
H          = 57.0
STRAIGHT_H = 20.0
WALL_T     = 1.5
CHAMFER    = 4.0
CORNER_R   = 5.0

# ── Attenuation weights (relative; contrast is tunable with the slider) ───────
#   The shell is thin, so the carbon term is bright only where a ray grazes a
#   wall tangentially — i.e. exactly at the panel EDGES (the features to align).
#   The fill is kept faint so the painted body stays see-through.
MU_CARBON = 0.020      # 1/mm  (carbon shell, ~3000 HU)
MU_FILL   = 0.0005     # 1/mm  (foam / air fill)
OVERLAY_CMAP = "autumn"  # graduated colour for the painted stretcher


# ═════════════════════════════════════════════════════════════════════════════
#  Stretcher cross-section geometry (copied from stretcher_profile.py so that
#  importing this module does not trigger that script's plotting side effects)
# ═════════════════════════════════════════════════════════════════════════════

def inward_offset_polygon(vertices, d):
    pts = np.array(vertices, dtype=float)
    n   = len(pts)
    normals = []
    for i in range(n):
        edge = pts[(i + 1) % n] - pts[i]
        u    = edge / np.linalg.norm(edge)
        normals.append(np.array([-u[1], u[0]]))
    inner = []
    for i in range(n):
        prev = (i - 1) % n
        p1 = pts[prev] + d * normals[prev]
        d1 = pts[i] - pts[prev]
        p2 = pts[i] + d * normals[i]
        d2 = pts[(i + 1) % n] - pts[i]
        A   = np.array([[d1[0], -d2[0]], [d1[1], -d2[1]]])
        b   = p2 - p1
        det = A[0, 0] * A[1, 1] - A[0, 1] * A[1, 0]
        if abs(det) < 1e-10:
            inner.append(p2.tolist())
        else:
            t = (b[0] * A[1, 1] - b[1] * A[0, 1]) / det
            inner.append((p1 + t * d1).tolist())
    return np.array(inner)


def round_polygon(vertices, r, n_arc=20):
    pts = np.array(vertices, dtype=float)
    n   = len(pts)
    result = []
    for i in range(n):
        p0 = pts[(i - 1) % n]; p1 = pts[i]; p2 = pts[(i + 1) % n]
        u_in  = (p1 - p0) / np.linalg.norm(p1 - p0)
        u_out = (p2 - p1) / np.linalg.norm(p2 - p1)
        cos_a    = np.clip(np.dot(u_in, u_out), -1.0, 1.0)
        half_ext = np.arccos(cos_a) / 2.0
        if half_ext < 1e-9 or r < 1e-9:
            result.append(p1.tolist()); continue
        d    = r * np.tan(half_ext)
        tp1  = p1 - d * u_in
        tp2  = p1 + d * u_out
        perp  = np.array([-u_in[1], u_in[0]])
        cross = u_in[0] * u_out[1] - u_in[1] * u_out[0]
        if cross < 0:
            perp = -perp
        centre = tp1 + r * perp
        a1 = np.arctan2(tp1[1] - centre[1], tp1[0] - centre[0])
        a2 = np.arctan2(tp2[1] - centre[1], tp2[0] - centre[0])
        if a2 < a1:
            a2 += 2.0 * np.pi
        r_arc  = np.linalg.norm(tp1 - centre)
        angles = np.linspace(a1, a2, n_arc)
        arc    = centre + r_arc * np.column_stack([np.cos(angles), np.sin(angles)])
        result.extend(arc.tolist())
    return np.array(result)


def shell_profile(top_w, bot_w, h, straight_h, wall_t, chamfer, corner_r):
    """Outer and inner (N,2) CCW polygon arrays for the shell cross-section,
    centred on the shape centroid (origin), +Y toward the patient."""
    hw_t, hw_b = top_w / 2.0, bot_w / 2.0
    ht         = h / 2.0
    kink_y     = ht - straight_h
    c          = chamfer
    outer = np.array([
        [-hw_b,     -ht    ],
        [ hw_b,     -ht    ],
        [ hw_t,      kink_y],
        [ hw_t,      ht - c],
        [ hw_t - c,  ht    ],
        [-hw_t + c,  ht    ],
        [-hw_t,      ht - c],
        [-hw_t,      kink_y],
    ])
    inner_sharp = inward_offset_polygon(outer, wall_t)
    outer = round_polygon(outer, corner_r)
    inner = round_polygon(inner_sharp, max(corner_r - wall_t, 0.0))
    return outer, inner


# ═════════════════════════════════════════════════════════════════════════════
#  Sinogram from the already-corrected projections
# ═════════════════════════════════════════════════════════════════════════════

def load_corrected_sinogram(corr_dir):
    """Row-average every NNN_corrproj<cols>x<rows>float.raw in corr_dir.

    Returns (sino[n, cols], proj_idx[n], cols) where proj_idx is 0-based
    (img_num - 1), matching the scan geometry arrays.
    """
    pat = re.compile(r"(\d+)_corrproj(\d+)x(\d+)float\.raw$")
    files = []
    for f in glob.glob(os.path.join(corr_dir, "*_corrproj*float.raw")):
        m = pat.search(os.path.basename(f))
        if m:
            files.append((int(m.group(1)), int(m.group(2)), int(m.group(3)), f))
    if not files:
        raise FileNotFoundError(
            f"No corrected projections in {corr_dir} — run find_barella_auto.py first.")
    files.sort(key=lambda x: x[0])
    cols, rows = files[0][1], files[0][2]

    sino     = np.zeros((len(files), cols), dtype=np.float32)
    proj_idx = np.zeros(len(files), dtype=np.int64)
    for i, (num, c, r, path) in enumerate(files):
        img = np.fromfile(path, dtype=np.float32).reshape(r, c)
        sino[i]     = img.mean(axis=0)
        proj_idx[i] = num - 1
    return sino, proj_idx, cols


# ═════════════════════════════════════════════════════════════════════════════
#  Fan-beam single-row forward projection of a convex polygon
# ═════════════════════════════════════════════════════════════════════════════

def _convex_chord(S, d, verts):
    """
    Chord length (mm) of each fan-beam ray through a convex polygon.

    S     : (2,)   source position (rotated-object frame)
    d     : (U,2)  ray direction source→detector for each of U detector columns
    verts : (E,2)  convex polygon vertices (any winding)

    Returns (U,) chord lengths, 0 where a ray misses the polygon.  Uses
    Cyrus–Beck clipping: X(t)=S+t·d, t∈[0,1] spans source→detector.
    """
    edges = np.roll(verts, -1, axis=0) - verts          # (E,2)
    nrm   = np.stack([edges[:, 1], -edges[:, 0]], axis=1)  # candidate normals
    cen   = verts.mean(axis=0)
    # orient every normal outward (positive dot with vertex-minus-centroid)
    sign  = np.sign(np.einsum("ej,ej->e", nrm, verts - cen))
    sign[sign == 0] = 1.0
    nrm  *= sign[:, None]

    nd  = d @ nrm.T                                     # (U,E)  nₑ·d
    rhs = np.einsum("ej,ej->e", nrm, verts - S)         # (E,)   nₑ·(vₑ−S)
    with np.errstate(divide="ignore", invalid="ignore"):
        tb = rhs[None, :] / nd                          # (U,E)  boundary t
    neg = nd < 0
    pos = nd > 0
    lo = np.where(neg, tb, -np.inf).max(axis=1)
    hi = np.where(pos, tb,  np.inf).min(axis=1)
    lo = np.maximum(lo, 0.0)
    hi = np.minimum(hi, 1.0)
    outside = ((nd == 0) & (rhs[None, :] < 0)).any(axis=1)
    chord_t = np.clip(hi - lo, 0.0, None)
    chord_t[outside] = 0.0
    return chord_t * np.linalg.norm(d, axis=1)


class StretcherProjector:
    """Precompute per-projection ray geometry; project the shell for any (cx,cy,θ)."""

    def __init__(self, scan, proj_idx, cols, outer, inner, max_verts=40):
        pitch      = scan["det_column_pitch"]
        # Decimate the (heavily arc-sampled) convex polygons — corner rounding
        # is negligible for the projected shadow but dominates the per-update
        # cost.  Uniform subsampling keeps the polygon convex.
        self.outer = outer[:: max(1, len(outer) // max_verts)]
        self.inner = inner[:: max(1, len(inner) // max_verts)]
        self.u     = (np.arange(cols) - cols / 2.0) * pitch   # detector x (mm)
        self.th    = np.deg2rad(np.array(scan["angle_eff"]))[proj_idx]
        self.sod   = np.array(scan["sod"])[proj_idx]
        self.sid   = np.array(scan["sid"])[proj_idx]
        self.n     = len(proj_idx)
        self.cols  = cols
        # per-projection source position and ray directions (rotated-object frame)
        self.S = [np.array([-s, 0.0]) for s in self.sod]
        self.d = [np.stack([np.full(cols, sd), self.u], axis=1)
                  for sd in self.sid]                          # d = D − S = (sid, u)

    def project(self, cx, cy, theta_deg):
        """Return (n, cols) synthetic Lambert–Beer sinogram of the stretcher.

        Uses the same frame as find_barella_auto._project_mm: a world point is
        p = (depth, lateral) and projects to u = p_lateral·sid/(p_depth+sod).
        The shell shape is stored as (width_x, height_y); the wide axis is the
        LATERAL (detector) direction and the thin axis is DEPTH, so we map
        (width, height) → (depth, lateral) = (height, width) before placing the
        centre at (cy, cx) — i.e. cx is lateral, cy is depth (matching the fit).
        """
        thr = np.deg2rad(theta_deg)
        Rr  = np.array([[np.cos(thr), -np.sin(thr)],
                        [np.sin(thr),  np.cos(thr)]])
        om  = np.column_stack([self.outer[:, 1], self.outer[:, 0]])  # (depth, lateral)
        im  = np.column_stack([self.inner[:, 1], self.inner[:, 0]])
        out_local = om @ Rr.T + np.array([cy, cx])     # world frame (depth, lateral)
        in_local  = im @ Rr.T + np.array([cy, cx])

        sino = np.zeros((self.n, self.cols), dtype=np.float32)
        for i in range(self.n):
            c, s = np.cos(self.th[i]), np.sin(self.th[i])
            Rg   = np.array([[c, -s], [s, c]])
            vout = out_local @ Rg.T                            # → rotated frame
            vin  = in_local  @ Rg.T
            Lout = _convex_chord(self.S[i], self.d[i], vout)
            Lin  = _convex_chord(self.S[i], self.d[i], vin)
            sino[i] = MU_CARBON * (Lout - Lin) + MU_FILL * Lin
        return sino

    def edge_us(self, cx, cy, theta_deg):
        """Silhouette (outer shadow) edge positions in detector-x mm for every
        projection.  Returns (u_left[n], u_right[n]) = min/max projected u over
        all outer-polygon vertices — the true shadow boundary (the tangent
        vertex, which shifts with angle).  Fully vectorised for use in the
        auto-refine objective.
        """
        thr = np.deg2rad(theta_deg)
        Rr  = np.array([[np.cos(thr), -np.sin(thr)],
                        [np.sin(thr),  np.cos(thr)]])
        om  = np.column_stack([self.outer[:, 1], self.outer[:, 0]])   # (depth, lat)
        w   = om @ Rr.T + np.array([cy, cx])                          # (V,2) world
        wx, wy = w[:, 0][None, :], w[:, 1][None, :]                   # (1,V)
        c   = np.cos(self.th)[:, None]                                # (n,1)
        s   = np.sin(self.th)[:, None]
        P0  = c * wx - s * wy                                         # (n,V) depth'
        P1  = s * wx + c * wy                                         # (n,V) lateral'
        u   = P1 * self.sid[:, None] / (P0 + self.sod[:, None])       # (n,V)
        return u.min(axis=1), u.max(axis=1)


# ═════════════════════════════════════════════════════════════════════════════
#  Interactive figure
# ═════════════════════════════════════════════════════════════════════════════

def _overlay_rgba(ov, vmax, cmap):
    """Map a synthetic-shadow array to RGBA with opacity graduated by value.

    Colour encodes attenuation; alpha is proportional to it so the faint body is
    see-through (phantom shows behind) and the bright shell edges are opaque.
    """
    norm = np.clip(ov / max(vmax, 1e-9), 0.0, 1.0)
    rgba = cm.get_cmap(cmap)(norm)
    rgba[..., 3] = norm                         # graduated transparency
    return rgba


def main(params_path, base, scan_index):
    scan     = load_params(params_path, scan_index)
    corr_dir = os.path.join(base, f"corrected_imgScan_{scan_index}")
    sino, proj_idx, cols = load_corrected_sinogram(corr_dir)

    pitch     = scan["det_column_pitch"]
    x_lo      = -cols / 2.0 * pitch
    x_hi      =  cols / 2.0 * pitch
    n         = sino.shape[0]
    extent    = [x_lo, x_hi, n, 0]

    outer, inner = shell_profile(TOP_W, BOT_W, H, STRAIGHT_H,
                                 WALL_T, CHAMFER, CORNER_R)
    proj = StretcherProjector(scan, proj_idx, cols, outer, inner)

    # ── Ridge map for auto-refine: |∂/∂col| of a smoothed sinogram, normalised.
    #    High where the sinogram has a sharp lateral edge (stretcher/phantom).
    ridge = np.abs(np.gradient(gaussian_filter(sino, sigma=(1.0, 2.0)), axis=1))
    ridge = (ridge / (np.percentile(ridge, 99.5) or 1.0)).astype(np.float32)
    # laterally-blurred copy: gives the reward a smooth ~REFINE_SIGMA-wide basin so
    # the optimiser has a gradient toward the crest (raw ridge is too spiky).
    ridge_s = gaussian_filter(ridge, sigma=(1.0, REFINE_SIGMA)).astype(np.float32)
    _rows = np.arange(n)

    def _edge_cols(cx, cy, theta_deg):
        """Predicted left/right silhouette edge column (float) for every row."""
        uL, uR = proj.edge_us(cx, cy, theta_deg)
        return uL / pitch + cols / 2.0, uR / pitch + cols / 2.0

    def _ridge_reward(colf):
        """Ridge reward at each edge column via linear interpolation — SMOOTH in
        colf (no rounding), so sub-pixel pose changes change the objective."""
        c0    = np.floor(colf).astype(np.int64)
        frac  = colf - c0
        v     = (ridge_s[_rows, np.clip(c0,     0, cols - 1)] * (1.0 - frac) +
                 ridge_s[_rows, np.clip(c0 + 1, 0, cols - 1)] * frac)
        v[(colf < 0) | (colf >= cols)] = 0.0
        return v

    def _refine_cost(params, seedL, seedR):
        """Corridor objective (minimise): quadratic penalty once the edge leaves
        the ±REFINE_FLAT px band around the seed track, minus the ridge reward
        inside it.  Anchoring to the seed keeps the fit off the phantom bands."""
        cL, cR = _edge_cols(*params)
        cost   = 0.0
        for c, s in ((cL, seedL), (cR, seedR)):
            d    = np.abs(c - s)
            well = np.where(d <= REFINE_FLAT, 0.0, (d - REFINE_FLAT) ** 2)
            cost += float(well.sum() - REFINE_GAIN * _ridge_reward(c).sum())
        return cost

    fig, ax = plt.subplots(figsize=(14, 8))
    plt.subplots_adjust(left=0.08, right=0.82, bottom=0.30, top=0.94)

    # window/level defaults from the data percentiles
    p_lo, p_hi = np.percentile(sino, 1), np.percentile(sino, 99)
    wl0, ww0   = 0.5 * (p_lo + p_hi), max(p_hi - p_lo, 1e-3)
    s_span     = float(sino.max() - sino.min()) or 1.0

    im_bg = ax.imshow(sino, aspect="auto", cmap="gray",
                      vmin=wl0 - ww0 / 2, vmax=wl0 + ww0 / 2,
                      extent=extent, origin="upper")

    overlay0 = proj.project(CX0, CY0, THETA0_DEG)
    vmax0    = overlay0.max() or 1.0
    im_ov = ax.imshow(_overlay_rgba(overlay0, vmax0, OVERLAY_CMAP),
                      aspect="auto", extent=extent, origin="upper")
    ax.set_xlabel("Detector x (mm from isocenter)")
    ax.set_ylabel("Projection (row order)")
    ax.set_title("Drag cx / cy / θ until the painted stretcher sits on the moving edges")

    # ── Sliders (left col = model, extra rows = display) ──────────────────────
    ax_cx    = plt.axes([0.10, 0.24, 0.60, 0.025])
    ax_cy    = plt.axes([0.10, 0.205, 0.60, 0.025])
    ax_th    = plt.axes([0.10, 0.17, 0.60, 0.025])
    ax_int   = plt.axes([0.10, 0.135, 0.60, 0.025])
    ax_wl    = plt.axes([0.10, 0.10, 0.60, 0.025])
    ax_ww    = plt.axes([0.10, 0.065, 0.60, 0.025])
    ax_sharp = plt.axes([0.10, 0.03, 0.60, 0.025])
    s_cx    = Slider(ax_cx,  "cx (mm)", CX0 - CX_SPAN, CX0 + CX_SPAN,
                     valinit=CX0, valstep=CX_STEP)
    s_cy    = Slider(ax_cy,  "cy (mm)", CY0 - CY_SPAN, CY0 + CY_SPAN,
                     valinit=CY0, valstep=CY_STEP)
    s_th    = Slider(ax_th,  "θ (deg)", THETA0_DEG - TH_SPAN, THETA0_DEG + TH_SPAN,
                     valinit=THETA0_DEG, valstep=TH_STEP)
    s_int   = Slider(ax_int, "overlay contrast", 0.1, 5.0, valinit=1.0, valstep=0.05)
    s_wl    = Slider(ax_wl,  "WL (level)", float(sino.min()), float(sino.max()), valinit=wl0)
    s_ww    = Slider(ax_ww,  "WW (width)", 1e-3, 1.5 * s_span, valinit=ww0)
    s_sharp = Slider(ax_sharp, "sharpen", 0.0, 6.0, valinit=0.0, valstep=0.1)

    def _recenter(slider, val, span, step):
        """Re-centre a fine slider's range on val (keeps click-to-fit unclipped)."""
        slider.valmin, slider.valmax = val - span, val + span
        slider.valstep = step
        slider.ax.set_xlim(slider.valmin, slider.valmax)
        slider.set_val(float(np.clip(val, slider.valmin, slider.valmax)))

    # ── Overlay on/off button (hard hide — beats contrast=0's residual trace) ──
    ax_btn = plt.axes([0.845, 0.145, 0.12, 0.05])
    b_ov   = Button(ax_btn, "overlay: ON")

    # ── Auto-refine button: snap cx/cy/θ onto the ridge map from current pose ──
    ax_ref = plt.axes([0.845, 0.08, 0.12, 0.05])
    b_ref  = Button(ax_ref, "auto-refine")

    state = {"ov": overlay0, "vmax": vmax0, "sharp_cache": (None, sino)}

    def _display_data():
        amt = s_sharp.val
        cached_amt, cached = state["sharp_cache"]
        if amt == cached_amt:
            return cached
        disp = sino if amt <= 0 else sino + amt * (sino - gaussian_filter(sino, sigma=(0, 2)))
        state["sharp_cache"] = (amt, disp)
        return disp

    def update_bg(_=None):
        im_bg.set_data(_display_data())
        im_bg.set_clim(s_wl.val - s_ww.val / 2, s_wl.val + s_ww.val / 2)
        fig.canvas.draw_idle()

    def recompute(_=None):
        ov = proj.project(s_cx.val, s_cy.val, s_th.val)
        state["ov"]   = ov
        state["vmax"] = ov.max() or 1.0
        redraw()

    def redraw(_=None):
        # higher contrast → lower vmax → more of the faint body lights up
        vmax = state["vmax"] / s_int.val
        im_ov.set_data(_overlay_rgba(state["ov"], vmax, OVERLAY_CMAP))
        fig.canvas.draw_idle()

    def toggle_overlay(_=None):
        vis = not im_ov.get_visible()
        im_ov.set_visible(vis)
        b_ov.label.set_text(f"overlay: {'ON' if vis else 'OFF'}")
        fig.canvas.draw_idle()

    def auto_refine(_=None):
        # corridor anchored to the CURRENT pose (your visual match)
        x0 = np.array([s_cx.val, s_cy.val, s_th.val])
        seedL, seedR = _edge_cols(*x0)
        reward0 = float(_ridge_reward(seedL).sum() + _ridge_reward(seedR).sum())
        print("\n" + "-" * 60)
        print(f"AUTO-REFINE  seed cx={x0[0]:.2f} cy={x0[1]:.2f} θ={x0[2]:.3f}  "
              f"ridge-align={reward0:.1f}  (corridor ±{REFINE_FLAT:.0f} px)")
        # Nelder-Mead with a pose-scale initial simplex; the smooth reward gives
        # it a gradient even for sub-pixel moves (Powell stalled on the staircase).
        dc, dcy, dth = REFINE_STEP
        simplex = np.array([x0, x0 + [dc, 0, 0], x0 + [0, dcy, 0], x0 + [0, 0, dth]])
        res = minimize(_refine_cost, x0, args=(seedL, seedR), method="Nelder-Mead",
                       options=dict(initial_simplex=simplex, xatol=1e-3, fatol=1e-4))
        cx, cy, th = res.x
        cL, cR = _edge_cols(cx, cy, th)
        rewardf = float(_ridge_reward(cL).sum() + _ridge_reward(cR).sum())
        moved = np.abs(np.concatenate([cL - seedL, cR - seedR]))
        print(f"             fit  cx={cx:.2f} cy={cy:.2f} θ={th:.3f}  "
              f"ridge-align={rewardf:.1f}  ({'improved' if rewardf >= reward0 else 'no gain'})")
        print(f"             edge moved: mean={moved.mean():.1f} px  max={moved.max():.1f} px")
        print("-" * 60)
        _recenter(s_cx, cx, CX_SPAN, CX_STEP)
        _recenter(s_cy, cy, CY_SPAN, CY_STEP)
        _recenter(s_th, th, TH_SPAN, TH_STEP)

    for s in (s_cx, s_cy, s_th):
        s.on_changed(recompute)
    s_int.on_changed(redraw)
    for s in (s_wl, s_ww, s_sharp):
        s.on_changed(update_bg)
    b_ov.on_clicked(toggle_overlay)
    b_ref.on_clicked(auto_refine)

    # ── Click-to-fit: mark points on the true edges, least-squares fit cx/cy/θ ──
    #   Reuses find_barella_auto.fit_position (validated on the table dataset).
    #   Keys:  l = pick LEFT-edge points   r = pick RIGHT-edge points
    #          f = fit & update sliders    u = undo last point   c = clear all
    picks   = {"left": [], "right": []}          # each entry: (proj_idx, t_mm)
    markers = {"left": [], "right": []}          # plotted artists
    mode    = {"edge": "left"}

    def _refresh_title():
        ax.set_title(f"[pick mode: {mode['edge'].upper()} edge]   "
                     f"L={len(picks['left'])} pts  R={len(picks['right'])} pts   "
                     f"keys: l/r=edge  f=fit  u=undo  c=clear")
        fig.canvas.draw_idle()

    def on_click(ev):
        if ev.inaxes is not ax or ev.xdata is None:
            return
        row = int(np.clip(round(ev.ydata - 0.5), 0, n - 1))
        edge = mode["edge"]
        pidx_here = int(proj_idx[row])
        picks[edge].append((pidx_here, float(ev.xdata)))
        col = "deepskyblue" if edge == "left" else "gold"
        m, = ax.plot(ev.xdata, row + 0.5, "x", color=col, ms=8, mew=2, zorder=12)
        markers[edge].append(m)
        ang_here = np.degrees(proj.th[row])
        print(f"  + {edge:5s} pt #{len(picks[edge]):<2d}  row={row:<4d} "
              f"proj_idx={pidx_here:<4d} angle={ang_here:8.3f}°  x={ev.xdata:8.2f} mm")
        _refresh_title()

    def _print_points():
        angles = np.array(scan["angle_eff"])          # already in degrees
        for edge in ("left", "right"):
            print(f"  {edge.upper()} edge — {len(picks[edge])} points:")
            for k, (pi, t) in enumerate(picks[edge]):
                print(f"      #{k:<2d} proj_idx={pi:<4d} angle={angles[pi]:8.3f}°  x={t:8.2f} mm")

    def do_fit():
        print("\n" + "=" * 60)
        print("FIT requested.")
        _print_points()
        if len(picks["left"]) < 3 or len(picks["right"]) < 3:
            print(f"  ✗ Need ≥3 points on EACH edge "
                  f"(have L={len(picks['left'])}, R={len(picks['right'])}). Aborting.")
            print("=" * 60)
            return
        p_L = fit_position([(i, t) for i, t in picks["left"]],  scan)  # (depth, lat)
        p_R = fit_position([(i, t) for i, t in picks["right"]], scan)
        cx  = 0.5 * (p_L[1] + p_R[1])
        cy  = 0.5 * (p_L[0] + p_R[0])
        # board width axis tilt: right edge relative to centre is (-sinθ, cosθ)·w/2
        theta = -np.degrees(np.arctan2(p_R[0] - p_L[0], p_R[1] - p_L[1]))
        theta = (theta + 90) % 180 - 90          # wrap into (−90, 90]
        sep = abs(p_R[1] - p_L[1])
        # per-point residuals (measured x − predicted x at the fitted point)
        def _rms(p, pts):
            r = [t - _project_mm(p, np.deg2rad(scan["angle_eff"][i]),
                                 scan["sod"][i], scan["sid"][i]) for i, t in pts]
            return np.sqrt(np.mean(np.square(r))) if r else 0.0
        print(f"  left  edge point  (depth={p_L[0]:8.2f}, lateral={p_L[1]:8.2f}) mm   "
              f"RMS={_rms(p_L, picks['left']):.2f} mm")
        print(f"  right edge point  (depth={p_R[0]:8.2f}, lateral={p_R[1]:8.2f}) mm   "
              f"RMS={_rms(p_R, picks['right']):.2f} mm")
        print(f"  → CENTRE  cx={cx:8.3f} mm   cy={cy:8.3f} mm   θ={theta:7.3f}°   "
              f"edge separation={sep:.1f} mm  (expect ~{TOP_W:.0f})")
        if not (100 < cy < 250) or abs(sep - TOP_W) > 60:
            print("  ⚠ cy or separation looks off — points may be on the phantom, not the stretcher.")
        print("=" * 60)
        # re-centre the fine sliders on the fit so the result isn't clipped
        _recenter(s_cx, cx,    CX_SPAN, CX_STEP)
        _recenter(s_cy, cy,    CY_SPAN, CY_STEP)
        _recenter(s_th, theta, TH_SPAN, TH_STEP)

    def clear_picks():
        for e in ("left", "right"):
            for m in markers[e]:
                m.remove()
            markers[e].clear(); picks[e].clear()
        _refresh_title()

    def on_key(ev):
        if ev.key in ("l", "r"):
            mode["edge"] = "left" if ev.key == "l" else "right"
            _refresh_title()
        elif ev.key == "f":
            do_fit()
        elif ev.key == "c":
            clear_picks()
        elif ev.key == "u":
            e = mode["edge"]
            if picks[e]:
                picks[e].pop(); markers[e].pop().remove(); _refresh_title()

    # Free our shortcut keys from matplotlib's default keymaps (otherwise 'l'
    # toggles log-y — squashing the image — 'f' toggles fullscreen, 'c' = back).
    for _km in [k for k in plt.rcParams if k.startswith("keymap.")]:
        plt.rcParams[_km] = [x for x in plt.rcParams[_km]
                             if x not in ("l", "r", "f", "c", "u")]

    fig.canvas.mpl_connect("button_press_event", on_click)
    fig.canvas.mpl_connect("key_press_event", on_key)
    _refresh_title()

    print("Interactive stretcher overlay — close the window when aligned.")
    print(f"  start: cx={CX0}  cy={CY0}  θ={THETA0_DEG}°")
    print("  Click-to-fit: press 'l'/'r' to select left/right edge, click along it,")
    print("  then 'f' to fit.  'u' undo, 'c' clear.  Sliders still work for fine-tuning.")
    plt.show()
    print(f"\nFinal: cx={s_cx.val:.3f} mm   cy={s_cy.val:.3f} mm   θ={s_th.val:.3f}°")


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--params", default=_PARAMS)
    ap.add_argument("--base",   default=_BASE)
    ap.add_argument("--scan",   type=int, default=0)
    args = ap.parse_args()
    main(args.params, args.base, args.scan)
