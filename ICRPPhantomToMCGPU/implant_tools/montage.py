#!/usr/bin/env python
"""Axial montage through a Z range of an MC-GPU label volume, axes in mm.
Used to locate the dental arch Z before placing implants.  Read-only."""
import argparse, numpy as np, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.colors import ListedColormap, BoundaryNorm
from pathlib import Path

NX = NY = 2134; NZ = 834; V = 0.3
OFF = (-320.10, -320.10, -125.10)
RGB = {0:(.06,.06,.09), 1:(.45,.33,.22), 2:(.83,.55,.52), 3:(.80,.68,.38),
       4:(.97,.95,.88), 5:(.95,.12,.12), 10:(.20,.82,.85), 11:(.16,.42,.58)}
_lut = np.zeros((256,3)); _lut[:] = (.85,0,.85)
for k,v in RGB.items(): _lut[k] = v
CMAP = ListedColormap(_lut); NORM = BoundaryNorm(np.arange(-.5,256.5), 256)
NVOX = (NX, NY, NZ)
mm2idx = lambda v,ax: max(0, min(NVOX[ax]-1, int((v - OFF[ax]) / V)))
idx2mm = lambda i,ax: i * V + OFF[ax] + V/2

ap = argparse.ArgumentParser()
ap.add_argument("raw"); ap.add_argument("out")
ap.add_argument("--z0", type=float, required=True)
ap.add_argument("--z1", type=float, required=True)
ap.add_argument("--step", type=float, default=5.0)
ap.add_argument("--win", type=float, nargs=4, metavar=("X0","X1","Y0","Y1"),
                help="explicit mm window; default = tissue bbox of first slice")
ap.add_argument("--cols", type=int, default=4)
ap.add_argument("--grid", type=float, default=10.0)
a = ap.parse_args()

vol = np.memmap(a.raw, dtype=np.uint8, mode="r", shape=(NZ, NY, NX))
zs = np.arange(a.z0, a.z1 + 1e-6, a.step)

if a.win:
    x0, x1, y0, y1 = a.win
else:
    sl = vol[mm2idx(zs[len(zs)//2], 2), :, :]
    m = (sl > 0) & (sl < 10)
    xi = np.where(m.any(axis=0))[0]; yi = np.where(m.any(axis=1))[0]
    x0, x1 = idx2mm(xi.min(),0)-10, idx2mm(xi.max(),0)+10
    y0, y1 = idx2mm(yi.min(),1)-10, idx2mm(yi.max(),1)+10

rows = int(np.ceil(len(zs) / a.cols))
fig, axes = plt.subplots(rows, a.cols, figsize=(4.4*a.cols, 4.4*rows))
axes = np.atleast_1d(axes).ravel()
for ax, z in zip(axes, zs):
    sl = vol[mm2idx(z,2), mm2idx(y0,1):mm2idx(y1,1), mm2idx(x0,0):mm2idx(x1,0)]
    ax.imshow(sl, cmap=CMAP, norm=NORM, origin="lower", extent=(x0,x1,y0,y1),
              interpolation="nearest", aspect="equal")
    ax.set_title(f"Z = {z:+.1f} mm", fontsize=9)
    ax.set_xticks(np.arange(np.ceil(x0/a.grid)*a.grid, x1+1e-6, a.grid))
    ax.set_yticks(np.arange(np.ceil(y0/a.grid)*a.grid, y1+1e-6, a.grid))
    ax.grid(True, color="#41d17a", lw=.4, alpha=.5)
    ax.tick_params(labelsize=6)
for ax in axes[len(zs):]: ax.axis("off")
fig.suptitle(f"{Path(a.raw).stem}   axial montage   "
             f"horiz = X (patient left +) · vert = Y (+Y posterior)", fontsize=10)
fig.tight_layout()
fig.savefig(a.out, dpi=100, facecolor="white")
print("wrote", a.out, " slices:", " ".join(f"{z:+.0f}" for z in zs))
