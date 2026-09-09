#!/usr/bin/env python
"""Render sagittal / axial / coronal views of an MC-GPU uint8 label volume,
axes labelled in isocenter mm, so implant coordinates can be read straight
off the picture.  Read-only: memmaps the .raw, never writes to it.

Optionally overlays proposed implant cylinders (--implants JSON) as outlines,
so a placement can be checked before it is stamped.
"""
import argparse, json, numpy as np, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.colors import ListedColormap, BoundaryNorm
from matplotlib.patches import Circle, Rectangle
from pathlib import Path

NX = NY = 2134; NZ = 834; V = 0.3
OFF = (-320.10, -320.10, -125.10)          # mm, voxel-grid origin (low edge)
NVOX = (NX, NY, NZ)

RGB = {0:(.06,.06,.09), 1:(.45,.33,.22), 2:(.83,.55,.52), 3:(.80,.68,.38),
       4:(.97,.95,.88), 5:(.95,.12,.12), 10:(.20,.82,.85), 11:(.16,.42,.58)}
_lut = np.zeros((256, 3)); _lut[:] = (.85, 0, .85)
for k, v in RGB.items(): _lut[k] = v
CMAP = ListedColormap(_lut); NORM = BoundaryNorm(np.arange(-.5, 256.5), 256)

def mm2idx(v, ax): return max(0, min(NVOX[ax] - 1, int((v - OFF[ax]) / V)))
def idx2mm(i, ax): return i * V + OFF[ax]            # low edge of voxel i

def window(lo_mm, hi_mm, ax):
    """Clamp an mm window to the grid; return (i0, i1, lo_mm, hi_mm) that are
    mutually consistent, so imshow's extent always matches the cropped array."""
    i0, i1 = mm2idx(lo_mm, ax), mm2idx(hi_mm, ax) + 1
    i1 = max(i1, i0 + 1)
    return i0, i1, idx2mm(i0, ax), idx2mm(i1, ax)

def load(path): return np.memmap(path, dtype=np.uint8, mode="r", shape=(NZ, NY, NX))

def bbox_mm(mask1d, ax, pad):
    idx = np.where(mask1d)[0]
    return idx.min() * V + OFF[ax] - pad, idx.max() * V + OFF[ax] + pad

def panel(ax, img, ext, title, grid_mm):
    ax.imshow(img, cmap=CMAP, norm=NORM, origin="lower", extent=ext,
              interpolation="nearest", aspect="equal")
    ax.set_title(title, fontsize=9)
    ax.set_xticks(np.arange(np.ceil(ext[0]/grid_mm)*grid_mm, ext[1]+1e-6, grid_mm))
    ax.set_yticks(np.arange(np.ceil(ext[2]/grid_mm)*grid_mm, ext[3]+1e-6, grid_mm))
    ax.grid(True, color="#41d17a", lw=.4, alpha=.55)
    ax.tick_params(labelsize=6)

def main():
    p = argparse.ArgumentParser()
    p.add_argument("raw"); p.add_argument("out")
    p.add_argument("--sag-x", type=float, default=0.0)
    p.add_argument("--axial-z", type=float)
    p.add_argument("--cor-y", type=float)
    p.add_argument("--pad", type=float, default=15.0)
    p.add_argument("--grid", type=float, default=20.0)
    p.add_argument("--implants", help="JSON list of implant dicts to outline")
    a = p.parse_args()

    vol = load(a.raw)
    imps = json.loads(Path(a.implants).read_text()) if a.implants else []
    if isinstance(imps, dict): imps = imps.get("implants", [])
    views = []

    def crop2(sl, ax_h, ax_v, lab_h, lab_v, title):
        """sl indexed [v, h]; crop to tissue bbox and return (img, extent)."""
        m = (sl > 0) & (sl < 10)
        h0, h1 = bbox_mm(m.any(axis=0), ax_h, a.pad)
        v0, v1 = bbox_mm(m.any(axis=1), ax_v, a.pad)
        i0, i1, h0, h1 = window(h0, h1, ax_h)
        j0, j1, v0, v1 = window(v0, v1, ax_v)
        return np.ascontiguousarray(sl[j0:j1, i0:i1]), (h0, h1, v0, v1), title

    # sagittal [Z, Y] at fixed X
    sl = vol[:, :, mm2idx(a.sag_x, 0)]
    views.append(crop2(sl, 1, 2, "Y", "Z", f"SAGITTAL  X={a.sag_x:+.0f} mm"
                 "\nhoriz = Y (+Y posterior) · vert = Z (+Z cranial)") + ("sag",))
    # axial [Y, X] at fixed Z
    if a.axial_z is not None:
        sl = vol[mm2idx(a.axial_z, 2), :, :]
        views.append(crop2(sl, 0, 1, "X", "Y", f"AXIAL  Z={a.axial_z:+.1f} mm"
                     "\nhoriz = X (patient left +) · vert = Y (+Y posterior)") + ("ax",))
    # coronal [Z, X] at fixed Y
    if a.cor_y is not None:
        sl = vol[:, mm2idx(a.cor_y, 1), :]
        views.append(crop2(sl, 0, 2, "X", "Z", f"CORONAL  Y={a.cor_y:+.0f} mm"
                     "\nhoriz = X · vert = Z (+Z cranial)") + ("cor",))

    fig, axes = plt.subplots(1, len(views), figsize=(7.5*len(views), 8.5))
    axes = np.atleast_1d(axes)
    for ax, (img, ext, title, kind) in zip(axes, views):
        panel(ax, img, ext, title, a.grid)
        for im in imps:                       # outline implants
            r, h = im["radius_mm"], im["height_mm"]
            cx, cy, cz = im["cx_mm"], im["cy_mm"], im["cz_mm"]
            # solid = this implant actually intersects the displayed plane;
            # dashed = drawn for context only, its centre is off-plane.
            if kind == "ax":
                d = abs(cz - a.axial_z) <= h/2
                sty = dict(ec="#00e5ff", lw=1.2) if d else dict(ec="#00e5ff",
                            lw=.7, ls=":", alpha=.5)
                ax.add_patch(Circle((cx, cy), r, fill=False, **sty))
            elif kind == "sag":
                d = abs(cx - a.sag_x) <= r
                sty = dict(ec="#00e5ff", lw=1.2) if d else dict(ec="#00e5ff",
                            lw=.7, ls=":", alpha=.5)
                ax.add_patch(Rectangle((cy-r, cz-h/2), 2*r, h, fill=False, **sty))
            else:
                d = abs(cy - a.cor_y) <= r
                sty = dict(ec="#00e5ff", lw=1.2) if d else dict(ec="#00e5ff",
                            lw=.7, ls=":", alpha=.5)
                ax.add_patch(Rectangle((cx-r, cz-h/2), 2*r, h, fill=False, **sty))
    fig.suptitle(Path(a.raw).stem, fontsize=10)
    fig.tight_layout(); fig.savefig(a.out, dpi=110, facecolor="white")
    print("wrote", a.out, " panels:", len(views))

main()
