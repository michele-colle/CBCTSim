#!/usr/bin/env python
"""verify_imp.py <base.raw> <implanted.raw>

Gates an implant stamp, lifted verbatim from finish_case.sh so the batch driver
and the single-case driver enforce identical rules:

  - every changed voxel is label 5
  - carbon(10) / foam(11) counts EXACTLY unchanged (no implant may touch the
    stretcher shell)
  - total voxel count unchanged, and label5 count == changed count
  - air -> implant: a handful of voxels at a cylinder boundary is
    discretisation, not a floating implant.  <= max(20, 0.1% of the stamped
    volume) warns; anything more fails.

Exit 0 = VERIFY_OK, 9 = VERIFY_FAIL.
"""
import numpy as np, sys

NX = NY = 2134
NZ = 834

b = np.memmap(sys.argv[1], dtype=np.uint8, mode="r", shape=(NZ, NY, NX))
a = np.memmap(sys.argv[2], dtype=np.uint8, mode="r", shape=(NZ, NY, NX))
hb = np.zeros(256, np.int64); ha = np.zeros(256, np.int64)
nd = bad = 0
for k in range(NZ):
    sb = np.asarray(b[k]).ravel(); sa = np.asarray(a[k]).ravel()
    hb += np.bincount(sb, minlength=256); ha += np.bincount(sa, minlength=256)
    d = sb != sa
    if d.any():
        nd += int(d.sum()); bad += int((sa[d] != 5).sum())

errs = []; warn = []
if bad:
    errs.append(f"{bad} changed voxels are not label 5")
for lab, nm in ((10, "carbon"), (11, "foam")):
    if hb[lab] != ha[lab]:
        errs.append(f"{nm} count changed {hb[lab]}->{ha[lab]}")
dair = int(hb[0] - ha[0])
if dair:
    tol = max(20, int(0.001 * ha[5]))
    (warn if 0 < dair <= tol else errs).append(f"air->implant {dair} voxels (tol {tol})")
if hb.sum() != ha.sum():
    errs.append("total voxel count changed")
if ha[5] != nd:
    errs.append(f"label5={ha[5]} != changed={nd}")

for w in warn:
    print("VERIFY_WARN: " + w)
print(f"changed={nd} label5={ha[5]} air/carbon/foam preserved={not errs}")
if errs:
    print("VERIFY_FAIL: " + "; ".join(errs)); sys.exit(9)
print("VERIFY_OK")
