#!/usr/bin/env python3
r"""
stretcher_profile.py  -- visualise the stretcher axial cross-section (XY plane).

Cross-section shape (6 vertices, no rounded corners yet):

    <---------- top_w ---------->
    ____________________________________________   -+
    |                                          |    | straight_h
    |                                          |   -+
    /                                          \    | oblique_h  (= h - straight_h,
   /____________________________________________\   |   derived automatically)
    <---------- bot_w ---------->              ----+

Edit the PARAMETERS block with measured values, then run:
    python stretcher_profile.py

Parameters marked  *** MEASURE ***  are not in the .cfg file.
All other defaults come from:  params/dicom_to_mcgpu_template.cfg
"""

import numpy as np
import matplotlib.pyplot as plt

# ═══════════════════════════════════════════════════════════════════════════════
#  PARAMETERS  (all mm)
# ═══════════════════════════════════════════════════════════════════════════════

# Runtime position relative to isocenter (does not affect the shape)
cx_mm = -8.191      # from stretcher_cx_mm
cy_mm = 173.14      # from stretcher_cy_mm

# Outer profile
top_w      = 440.0  # outer width at the flat top               [mm]  ← stretcher_width_mm
bot_w      = 396.0  # outer width at the flat bottom            [mm]  *** MEASURE ***
h          =  57.0  # total outer height (carbon 2 + foam 36)   [mm]  ← sum of old thicknesses
straight_h =  20.0  # height of the vertical straight sides     [mm]  *** MEASURE ***
#                     oblique section height = h - straight_h  (derived)

# Shell
wall_t   =   1.5    # carbon wall thickness                     [mm]  ← stretcher_carbon_thickness_mm
chamfer  =   4.0    # size of the 45° chamfer at the top corners [mm]
corner_r =   5.0    # rounding radius applied to all corners     [mm]
fill     = "foam"   # interior material: "foam" or "air"

# ═══════════════════════════════════════════════════════════════════════════════


def inward_offset_polygon(vertices, d):
    """
    Offset a convex CCW polygon inward by distance d (sharp corners).
    Each inner vertex is the intersection of the two adjacent offset edges.
    """
    pts = np.array(vertices, dtype=float)
    n   = len(pts)

    # Inward normal of each edge = left normal of the CCW edge direction
    normals = []
    for i in range(n):
        edge = pts[(i + 1) % n] - pts[i]
        u    = edge / np.linalg.norm(edge)
        normals.append(np.array([-u[1], u[0]]))   # left normal

    inner = []
    for i in range(n):
        # Offset edge (i-1) and offset edge i; find their intersection
        prev = (i - 1) % n
        p1 = pts[prev] + d * normals[prev]
        d1 = pts[i] - pts[prev]

        p2 = pts[i] + d * normals[i]
        d2 = pts[(i + 1) % n] - pts[i]

        # Solve p1 + t*d1 = p2 + s*d2
        A   = np.array([[d1[0], -d2[0]], [d1[1], -d2[1]]])
        b   = p2 - p1
        det = A[0, 0] * A[1, 1] - A[0, 1] * A[1, 0]
        if abs(det) < 1e-10:          # parallel edges — keep offset point
            inner.append(p2.tolist())
        else:
            t = (b[0] * A[1, 1] - b[1] * A[0, 1]) / det
            inner.append((p1 + t * d1).tolist())

    return np.array(inner)


def round_polygon(vertices, r, n_arc=20):
    """Replace each vertex of a convex CCW polygon with a circular arc of radius r."""
    pts = np.array(vertices, dtype=float)
    n   = len(pts)
    result = []

    for i in range(n):
        p0 = pts[(i - 1) % n]
        p1 = pts[i]
        p2 = pts[(i + 1) % n]

        u_in  = (p1 - p0) / np.linalg.norm(p1 - p0)
        u_out = (p2 - p1) / np.linalg.norm(p2 - p1)

        cos_a    = np.clip(np.dot(u_in, u_out), -1.0, 1.0)
        half_ext = np.arccos(cos_a) / 2.0

        if half_ext < 1e-9 or r < 1e-9:
            result.append(p1.tolist())
            continue

        d    = r * np.tan(half_ext)
        tp1  = p1 - d * u_in
        tp2  = p1 + d * u_out

        perp  = np.array([-u_in[1], u_in[0]])          # left normal (inward for CCW)
        cross = u_in[0]*u_out[1] - u_in[1]*u_out[0]
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
    """Return outer and inner (N,2) CCW polygon arrays for the shell cross-section."""
    hw_t   = top_w / 2.0
    hw_b   = bot_w / 2.0
    ht     = h / 2.0
    kink_y = ht - straight_h     # y of the straight/oblique junction
    c      = chamfer

    # 8 vertices: chamfer replaces each top corner with a 45° diagonal segment
    outer = np.array([
        [-hw_b,     -ht    ],   # BL
        [ hw_b,     -ht    ],   # BR
        [ hw_t,      kink_y],   # kink-R  (bottom of right vertical)
        [ hw_t,      ht - c],   # chamfer-R bottom
        [ hw_t - c,  ht    ],   # chamfer-R top
        [-hw_t + c,  ht    ],   # chamfer-L top
        [-hw_t,      ht - c],   # chamfer-L bottom
        [-hw_t,      kink_y],   # kink-L  (bottom of left vertical)
    ])
    inner_sharp = inward_offset_polygon(outer, wall_t)
    outer = round_polygon(outer, corner_r)
    inner = round_polygon(inner_sharp, max(corner_r - wall_t, 0.0))

    return outer, inner


# ── Build profiles ────────────────────────────────────────────────────────────
outer, inner = shell_profile(top_w, bot_w, h, straight_h, wall_t, chamfer, corner_r)

oblique_h = h - straight_h
print(f"Derived oblique section height: {oblique_h:.1f} mm")

# ── Plot ──────────────────────────────────────────────────────────────────────
fig, ax = plt.subplots(figsize=(11, 5))

def closed(pts):
    return np.vstack([pts, pts[0]])

fill_color = "#d4c5a9" if fill == "foam" else "#dddddd"
ax.fill(outer[:, 0], outer[:, 1], color="#888888", label="carbon  (label 10)")
ax.fill(inner[:, 0], inner[:, 1], color=fill_color, label=f"{fill}  (label 11)")
ax.plot(closed(outer)[:, 0], closed(outer)[:, 1], "k-",  lw=1.5)
ax.plot(closed(inner)[:, 0], closed(inner)[:, 1], "k--", lw=1.0)

# ── Dimension annotations ─────────────────────────────────────────────────────
ht  = h / 2.0
pad = 18
kw_arr  = dict(arrowstyle="<->", color="steelblue", lw=1.2)
kw_txt  = dict(ha="center", color="steelblue", fontsize=9)

# top_w
ax.annotate("", xy=( top_w/2, ht + pad), xytext=(-top_w/2, ht + pad), arrowprops=kw_arr)
ax.text(0, ht + pad + 5, f"top_w = {top_w} mm", **kw_txt)

# bot_w
ax.annotate("", xy=( bot_w/2, -ht - pad), xytext=(-bot_w/2, -ht - pad), arrowprops=kw_arr)
ax.text(0, -ht - pad - 9, f"bot_w = {bot_w} mm", **kw_txt)

# total height h
ax.annotate("", xy=(top_w/2 + pad, ht), xytext=(top_w/2 + pad, -ht), arrowprops=kw_arr)
ax.text(top_w/2 + pad + 4, 0, f"h = {h} mm", va="center", ha="left", color="steelblue", fontsize=9)

# straight_h (right side)
kink_y = -ht + oblique_h
ax.annotate("", xy=(top_w/2 + pad*2.2, ht), xytext=(top_w/2 + pad*2.2, kink_y), arrowprops=kw_arr)
ax.text(top_w/2 + pad*2.2 + 4, (ht + kink_y)/2,
        f"straight_h\n= {straight_h} mm", va="center", ha="left", color="steelblue", fontsize=9)

# wall thickness indicator (top-right corner, horizontal)
ax.annotate("", xy=(top_w/2, ht - 5), xytext=(top_w/2 - wall_t, ht - 5),
            arrowprops=dict(arrowstyle="<->", color="firebrick", lw=1.2))
ax.text(top_w/2 - wall_t/2, ht - 10,
        f"wall_t = {wall_t} mm", ha="center", va="top", color="firebrick", fontsize=8)

ax.set_aspect("equal")
ax.grid(True, alpha=0.3)
ax.legend(loc="upper right", fontsize=9)
ax.set_xlabel("X [mm]")
ax.set_ylabel("Y [mm]  (+Y toward patient)")
ax.set_title(
    "Stretcher axial cross-section\n"
    f"cx = {cx_mm} mm, cy = {cy_mm} mm from isocenter"
)

plt.tight_layout()
plt.savefig("stretcher_profile.png", dpi=150)
plt.show()

# ── Summary ───────────────────────────────────────────────────────────────────
print("\nParameters in use:")
print(f"  cx_mm      = {cx_mm}")
print(f"  cy_mm      = {cy_mm}")
print(f"  top_w      = {top_w}   mm")
print(f"  bot_w      = {bot_w}   mm  *** verify ***")
print(f"  h          = {h}    mm")
print(f"  straight_h = {straight_h}   mm  *** verify ***")
print(f"  wall_t     = {wall_t}    mm")
print(f"  chamfer    = {chamfer}    mm")
print(f"  corner_r   = {corner_r}    mm")
print(f"  fill       = {fill}")
