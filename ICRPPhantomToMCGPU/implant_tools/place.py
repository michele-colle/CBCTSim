#!/usr/bin/env python
"""Propose dental implant cylinders along the alveolar arch of an MC-GPU
label volume, and report how much of each cylinder actually lands in bone.

Method
  1. take the bone mask (labels 3 spongiosa + 4 cortical) at the arch slice;
  2. keep the connected arch region around the alveolus;
  3. polar-sweep from the arch centroid, taking the mid-radius of the bone
     run on each ray -> the alveolar centreline;
  4. sample N points spread along that centreline; give each a realistic
     dental diameter/length, seeded per case so a rerun reproduces it;
  5. score every cylinder by the fraction of its voxels in bone.

Writes a JSON proposal.  Read-only w.r.t. the volume.
"""
import argparse, json, hashlib, numpy as np
from pathlib import Path

NX = NY = 2134; NZ = 834; V = 0.3
OFF = (-320.10, -320.10, -125.10)
DIAM = [3.3, 3.75, 4.1, 4.5]            # mm, standard dental implant diameters
LEN  = [8.0, 10.0, 11.5, 13.0]          # mm, standard lengths

mm2idx = lambda v, ax: int((v - OFF[ax]) / V)
idx2mm = lambda i, ax: i * V + OFF[ax] + V / 2

def arch_centreline(sl, x_win, y_win, n_ray=180):
    """sl = [Y, X] label slice.  Return (cx, cy) arch origin and a list of
    (x_mm, y_mm, theta) centreline points ordered along the arch."""
    i0, i1 = mm2idx(x_win[0], 0), mm2idx(x_win[1], 0)
    j0, j1 = mm2idx(y_win[0], 1), mm2idx(y_win[1], 1)
    sub = np.asarray(sl[j0:j1, i0:i1])
    bone = (sub == 3) | (sub == 4)
    if bone.sum() < 500: return None, []
    jj, ii = np.nonzero(bone)
    ox, oy = ii.mean(), jj.mean()                    # arch centroid (inside the U)
    pts = []
    for th in np.linspace(-np.pi, np.pi, n_ray, endpoint=False):
        dx, dy = np.cos(th), np.sin(th)
        rs = np.arange(2, 140, 0.5)                  # voxels
        xs = np.clip((ox + dx*rs).astype(int), 0, bone.shape[1]-1)
        ys = np.clip((oy + dy*rs).astype(int), 0, bone.shape[0]-1)
        hit = bone[ys, xs]
        if not hit.any(): continue
        # first contiguous bone run along the ray = the alveolar ridge
        k = np.argmax(hit); e = k
        while e + 1 < len(hit) and hit[e+1]: e += 1
        if (e - k) < 3: continue                     # too thin to hold an implant
        rmid = rs[(k + e) // 2]
        pts.append((idx2mm(i0 + ox + dx*rmid, 0),
                    idx2mm(j0 + oy + dy*rmid, 1), th, (e-k+1)*V))
    pts.sort(key=lambda p: p[2])
    return (idx2mm(i0+ox, 0), idx2mm(j0+oy, 1)), pts

def score(vol, imp):
    """Fraction of the cylinder's voxels that are bone / soft / air."""
    r, h = imp["radius_mm"], imp["height_mm"]
    k0, k1 = mm2idx(imp["cz_mm"]-h/2, 2), mm2idx(imp["cz_mm"]+h/2, 2)
    i0, i1 = mm2idx(imp["cx_mm"]-r, 0), mm2idx(imp["cx_mm"]+r, 0)+1
    j0, j1 = mm2idx(imp["cy_mm"]-r, 1), mm2idx(imp["cy_mm"]+r, 1)+1
    xs = (np.arange(i0, i1)*V + OFF[0] + V/2) - imp["cx_mm"]
    ys = (np.arange(j0, j1)*V + OFF[1] + V/2) - imp["cy_mm"]
    disc = (xs[None, :]**2 + ys[:, None]**2) <= r*r
    n = tot = 0; cnt = {"bone": 0, "soft": 0, "air": 0}
    for k in range(max(0,k0), min(NZ,k1)):
        patch = np.asarray(vol[k, j0:j1, i0:i1])[disc]
        cnt["bone"] += int(((patch==3)|(patch==4)).sum())
        cnt["soft"] += int(((patch==1)|(patch==2)).sum())
        cnt["air"]  += int((patch==0).sum())
        tot += patch.size
    return {k: round(v/max(tot,1), 3) for k, v in cnt.items()}, tot

def main():
    p = argparse.ArgumentParser()
    p.add_argument("raw"); p.add_argument("out_json")
    p.add_argument("--arch-z", type=float, required=True, help="alveolar crest Z [mm]")
    p.add_argument("--n", type=int, help="implant count (default: seeded 3-6)")
    p.add_argument("--x-win", type=float, nargs=2,
                   help="override arch search X window [mm]; default = auto")
    p.add_argument("--y-win", type=float, nargs=2,
                   help="override arch search Y window [mm]; default = auto")
    p.add_argument("--y-depth", type=float, default=55.0,
                   help="how far posterior of the anterior skin to search [mm]")
    p.add_argument("--up", type=float, default=1.5,
                   help="mm of implant below the crest (rest extends cranially)")
    p.add_argument("--min-bone", type=float, default=0.55)
    p.add_argument("--small", action="store_true",
                   help="force the smallest real fixture (3.3 mm x 8 mm) -- for "
                        "atrophic or paediatric jaws where full-size sites clip air")
    p.add_argument("--max-air", type=float, default=0.002,
                   help="reject a cylinder with more than this air fraction: an "
                        "implant hanging partly outside bone into air is "
                        "unphysical even if its bone fraction passes")
    a = p.parse_args()

    vol = np.memmap(a.raw, dtype=np.uint8, mode="r", shape=(NZ, NY, NX))
    case = Path(a.raw).stem
    rng = np.random.default_rng(
        int(hashlib.sha256(case.encode()).hexdigest()[:8], 16))

    # The head is not centred in every volume (GradientHealth cases are
    # laterally and AP offset), so the arch window is derived from this
    # slice's own tissue bbox unless it is explicitly overridden.
    sl = vol[mm2idx(a.arch_z, 2)]
    body = (np.asarray(sl) > 0) & (np.asarray(sl) < 10)
    if not body.any(): print("EMPTY SLICE"); Path(a.out_json).write_text("[]"); return
    xi = np.where(body.any(axis=0))[0]; yi = np.where(body.any(axis=1))[0]
    x_win = a.x_win or [idx2mm(xi.min(), 0), idx2mm(xi.max(), 0)]
    y_win = a.y_win or [idx2mm(yi.min(), 1), idx2mm(yi.min(), 1) + a.y_depth]
    print(f"  arch search window X[{x_win[0]:+.0f},{x_win[1]:+.0f}] "
          f"Y[{y_win[0]:+.0f},{y_win[1]:+.0f}] mm")
    origin, pts = arch_centreline(sl, x_win, y_win)
    if not pts:
        print("NO ARCH FOUND"); Path(a.out_json).write_text("[]"); return

    n = a.n or int(rng.integers(3, 7))
    # spread the sites over the anterior 2/3 of the swept arch, jittered
    sel = np.linspace(0.12, 0.88, n) + rng.normal(0, 0.025, n)
    sel = np.clip(sel, 0.02, 0.98)
    imps = []
    for f in sel:
        x, y, th, thick = pts[int(f*(len(pts)-1))]
        d, L = (3.3, 8.0) if a.small else (float(rng.choice(DIAM)),
                                            float(rng.choice(LEN)))
        imp = {"cx_mm": round(float(x), 3), "cy_mm": round(float(y), 3),
               "cz_mm": round(a.arch_z - a.up + L/2, 3),
               "radius_mm": d/2, "height_mm": L,
               "diam_mm": d, "ridge_thick_mm": round(float(thick), 1)}
        sc, tot = score(vol, imp)
        imp["frac"] = sc; imp["voxels"] = tot
        imp["ok"] = sc["bone"] >= a.min_bone and sc["air"] <= a.max_air
        imps.append(imp)

    Path(a.out_json).write_text(json.dumps(imps, indent=1))
    print(f"{case}  arch_z={a.arch_z:+.1f}  origin=({origin[0]:+.1f},{origin[1]:+.1f})"
          f"  ray_pts={len(pts)}  n={n}")
    for i, m in enumerate(imps):
        print(f"  imp{i}: ({m['cx_mm']:+7.1f},{m['cy_mm']:+7.1f},{m['cz_mm']:+7.1f}) "
              f"d={m['diam_mm']:.2f} L={m['height_mm']:.1f} ridge={m['ridge_thick_mm']:4.1f}mm"
              f"  bone={m['frac']['bone']:.2f} soft={m['frac']['soft']:.2f}"
              f" air={m['frac']['air']:.2f}  {'OK' if m['ok'] else 'REJECT'}")

main()
