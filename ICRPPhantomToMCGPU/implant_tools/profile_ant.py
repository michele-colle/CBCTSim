import sys, numpy as np
NX=NY=2134;NZ=834;V=0.3;OFFY=-320.10;OFFZ=-125.10;SL=NX*NY
f=sys.stdin.buffer; ant=np.full(NZ,np.nan)
for k in range(NZ):
    b=f.read(SL)
    if len(b)<SL: break
    sl=np.frombuffer(b,dtype=np.uint8).reshape(NY,NX)
    # midsagittal band only: the face profile, not the shoulders
    band=sl[:,NX//2-25:NX//2+26]
    body=(band>0)&(band<10)
    r=body.any(axis=1)
    if r.any(): ant[k]=np.argmax(r)*V+OFFY
z=np.arange(NZ)*V+OFFZ
ok=~np.isnan(ant)
print("  z_mm  ant_Y   (midsagittal +-7.5mm band)")
for k in range(0,NZ,17):
    if ok[k]: print(f"  {z[k]:+7.1f} {ant[k]:+7.1f}")
print(f"\nGLOBAL most-anterior: z={z[np.nanargmin(ant)]:+.1f}  ant_Y={np.nanmin(ant):+.1f}")
