#!/usr/bin/env python
"""Locate the nasal tip in a built MC-GPU volume (stdin), plus the anatomy
extent. Reliable here because the volume contains only the head -- unlike the
source series, where the chest/shoulder is more anterior than the face.
Prints: nose_z vertex floor  (mm, isocenter frame)
"""
import sys, numpy as np
NX=NY=2134; NZ=834; V=0.3; OFFY=-320.10; OFFZ=-125.10; SL=NX*NY
f=sys.stdin.buffer
ant=np.full(NZ,np.nan)
for k in range(NZ):
    b=f.read(SL)
    if len(b)<SL: break
    sl=np.frombuffer(b,dtype=np.uint8).reshape(NY,NX)
    body=(sl>0)&(sl<10)
    r=body.any(axis=1)
    if r.any(): ant[k]=np.argmax(r)*V+OFFY
ok=~np.isnan(ant)
if not ok.any(): print("NO_TISSUE"); sys.exit()
z=np.arange(NZ)*V+OFFZ
k=int(np.nanargmin(ant))
print(f"{z[k]:.1f} {z[ok].max():.1f} {z[ok].min():.1f} {ant[k]:.1f}")
