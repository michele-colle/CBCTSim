#!/usr/bin/env python
"""Stamp implant cylinders (label 5) into a COPY of an MC-GPU label volume.

Implants overwrite anatomy and cannot be undone, so this never edits the base:
it copies the base .raw to the target name, then patches only the affected Z
slices in place via a memmap.  Reports the label histogram delta, so what the
implants replaced is on the record.
"""
import argparse, json, shutil, numpy as np
from pathlib import Path

NX = NY = 2134; NZ = 834; V = 0.3
OFF = (-320.10, -320.10, -125.10)
mm2idx = lambda v, ax: int((v - OFF[ax]) / V)
NAMES = {0:"air",1:"fat",2:"soft",3:"spongiosa",4:"cortical",5:"implant",
         10:"carbon",11:"foam"}

ap = argparse.ArgumentParser()
ap.add_argument("base_raw"); ap.add_argument("out_raw"); ap.add_argument("implants_json")
ap.add_argument("--base-txt", help=".txt companion of the base, to clone")
a = ap.parse_args()

imps = [m for m in json.loads(Path(a.implants_json).read_text()) if m.get("ok", True)]
if not imps: raise SystemExit("no accepted implants in " + a.implants_json)

out = Path(a.out_raw)
if not out.exists():
    print(f"copying base -> {out.name}"); shutil.copyfile(a.base_raw, out)

vol = np.memmap(out, dtype=np.uint8, mode="r+", shape=(NZ, NY, NX))
replaced = {}
total = 0
for n, im in enumerate(imps):
    r, h = im["radius_mm"], im["height_mm"]
    k0, k1 = mm2idx(im["cz_mm"]-h/2, 2), mm2idx(im["cz_mm"]+h/2, 2)+1
    i0, i1 = mm2idx(im["cx_mm"]-r, 0), mm2idx(im["cx_mm"]+r, 0)+1
    j0, j1 = mm2idx(im["cy_mm"]-r, 1), mm2idx(im["cy_mm"]+r, 1)+1
    xs = (np.arange(i0, i1)*V + OFF[0] + V/2) - im["cx_mm"]
    ys = (np.arange(j0, j1)*V + OFF[1] + V/2) - im["cy_mm"]
    disc = (xs[None, :]**2 + ys[:, None]**2) <= r*r
    cnt = 0
    for k in range(max(0, k0), min(NZ, k1)):
        blk = vol[k, j0:j1, i0:i1]
        prev = blk[disc]
        for v, c in zip(*np.unique(prev, return_counts=True)):
            replaced[int(v)] = replaced.get(int(v), 0) + int(c)
        blk[disc] = 5
        vol[k, j0:j1, i0:i1] = blk
        cnt += int(disc.sum())
    total += cnt
    print(f"  imp{n}: d={im['diam_mm']:.2f} L={h:.1f} at "
          f"({im['cx_mm']:+.1f},{im['cy_mm']:+.1f},{im['cz_mm']:+.1f})  {cnt} voxels")
vol.flush(); del vol
print(f"stamped {len(imps)} implants, {total} voxels -> label 5")
print("replaced: " + ", ".join(f"{NAMES.get(k,k)}={v}"
                               for k, v in sorted(replaced.items())))
if a.base_txt:
    txt = Path(a.base_txt).read_text().replace(Path(a.base_raw).name, out.name)
    Path(str(out).replace(".raw", ".txt")).write_text(txt)
    print("wrote .txt companion")
