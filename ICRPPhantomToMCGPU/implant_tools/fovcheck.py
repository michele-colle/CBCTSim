#!/usr/bin/env python
"""Preview a Z shift: render the central sagittal as it WOULD look after the
shift, with the CBCT FOV band and the phantom box edges drawn.

Landmarks are auto-detected from the volume:
  nose   = Z of the most anterior tissue voxel (nasal tip)
  vertex = highest tissue Z
The shift is expressed as "put the nose at Z = --nose-target".
"""
import argparse, numpy as np, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.colors import ListedColormap, BoundaryNorm
from pathlib import Path

NX = NY = 2134; NZ = 834; V = 0.3
OFF = (-320.10, -320.10, -125.10)
BOX_LO, BOX_HI = OFF[2], OFF[2] + NZ*V          # -125.1 .. +125.1
RGB = {0:(.06,.06,.09),1:(.45,.33,.22),2:(.83,.55,.52),3:(.80,.68,.38),
       4:(.97,.95,.88),5:(.95,.12,.12),10:(.20,.82,.85),11:(.16,.42,.58)}
_lut=np.zeros((256,3)); _lut[:]=(.85,0,.85)
for k,v in RGB.items(): _lut[k]=v
CMAP=ListedColormap(_lut); NORM=BoundaryNorm(np.arange(-.5,256.5),256)

ap=argparse.ArgumentParser()
ap.add_argument("raw"); ap.add_argument("out")
ap.add_argument("--fov-half-z", type=float, default=85.9)
ap.add_argument("--nose-target", type=float, default=0.0,
                help="Z to move the nasal tip to [mm]")
ap.add_argument("--as-is", action="store_true",
                help="render the volume EXACTLY as stored (shift 0). Use for QA of "
                     "a built volume -- otherwise the render is a preview of a "
                     "further shift, which is misleading when the detected nose "
                     "is not already at the target.")
a=ap.parse_args()

vol=np.memmap(a.raw,dtype=np.uint8,mode="r",shape=(NZ,NY,NX))

# ---- landmarks from the whole volume -------------------------------------
ant=np.full(NZ,np.nan); has=np.zeros(NZ,bool)
for k in range(NZ):
    body=(np.asarray(vol[k])>0)&(np.asarray(vol[k])<10)
    r=body.any(axis=1)
    if r.any():
        has[k]=True; ant[k]=np.argmax(r)*V+OFF[1]
zs=np.where(has)[0]
vertex=zs.max()*V+OFF[2]; floor=zs.min()*V+OFF[2]
nose_k=int(np.nanargmin(ant)); nose=nose_k*V+OFF[2]
shift=0.0 if a.as_is else a.nose_target-nose
print(f"vertex={vertex:+.1f}  floor={floor:+.1f}  nose(most anterior)={nose:+.1f} mm")
print(f"nose->{a.nose_target:+.1f} requires shift {shift:+.1f} mm")
print(f"after shift: vertex {vertex+shift:+.1f} (box top {BOX_HI:+.1f}"
      f"{', CLIPPED' if vertex+shift>BOX_HI else ''}), floor {floor+shift:+.1f}")
print(f"head_top_margin_mm equivalent = {a.fov_half_z-(vertex+shift):+.1f}")

# ---- sagittal, shifted in Z ---------------------------------------------
sl=np.ascontiguousarray(vol[:,:,NX//2])          # [Z, Y]
n=int(round(shift/V))
out=np.zeros_like(sl)
if n>=0:
    if n<NZ: out[n:,:]=sl[:NZ-n,:]
else:
    out[:NZ+n,:]=sl[-n:,:]
m=(out>0)&(out<10)
yi=np.where(m.any(axis=0))[0]
y0,y1=yi.min()*V+OFF[1]-15, yi.max()*V+OFF[1]+15

fig,ax=plt.subplots(figsize=(9,10))
ax.imshow(out[:,int((y0-OFF[1])/V):int((y1-OFF[1])/V)],cmap=CMAP,norm=NORM,
          origin="lower",extent=(y0,y1,BOX_LO,BOX_HI),interpolation="nearest",
          aspect="equal")
# FOV band
ax.axhspan(-a.fov_half_z,a.fov_half_z,color="#00e5ff",alpha=.13,zorder=3)
for z,lab in ((a.fov_half_z,"FOV top"),(-a.fov_half_z,"FOV bottom")):
    ax.axhline(z,color="#00e5ff",lw=1.8,zorder=4)
    ax.text(y1-2,z+2,f"{lab}  {z:+.1f}",color="#00b8d4",fontsize=8,ha="right",zorder=5)
ax.axhline(0,color="#00e5ff",lw=.8,ls="--",zorder=4)
ax.text(y1-2,2,"isocenter",color="#00b8d4",fontsize=8,ha="right",zorder=5)
for z,lab,c in ((BOX_HI,"box top","#ff5252"),(BOX_LO,"box bottom","#ff5252")):
    ax.axhline(z,color=c,lw=1.4,ls=":",zorder=4)
    ax.text(y0+2,z-6 if z>0 else z+3,f"{lab} {z:+.1f}",color=c,fontsize=8,zorder=5)
# nose marker (post-shift)
_nl = nose if a.as_is else a.nose_target
ax.axhline(_nl,color="#ffd54f",lw=1.2,zorder=4)
ax.text(y0+2,_nl+2,f"detected nose {_nl:+.1f}",color="#ffb300",fontsize=8,zorder=5)
ax.set_yticks(np.arange(-120,126,20)); ax.set_xticks(np.arange(np.ceil(y0/20)*20,y1,20))
ax.grid(True,color="#41d17a",lw=.35,alpha=.4)
ax.set_xlabel("Y  (+Y posterior)  [mm]"); ax.set_ylabel("Z  (+Z cranial)  [mm]")
ax.set_title(f"{Path(a.raw).stem}\nPREVIEW after Z shift {shift:+.1f} mm "
             f"(nose -> {a.nose_target:+.1f})   cyan = CBCT FOV, red dotted = phantom box",
             fontsize=9)
fig.tight_layout(); fig.savefig(a.out,dpi=115,facecolor="white")
print("wrote",a.out)
