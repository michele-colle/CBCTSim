"""Dump a per-Z anatomical profile for one volume (stdin) to an .npz.
Streams once over the full Z range so the vertex (highest tissue slice) is
captured -- the arch is then located as a distance below the vertex, which is
independent of wherever the source series happened to be truncated.
Columns per slice: body, bone, cort, ant_bone_y, ant_skin_y, archW, arch_cx
"""
import sys, numpy as np
NX=NY=2134; V=0.3; OFFX=OFFY=-320.10; SL=NX*NY; NZ=834
f=sys.stdin.buffer; rows=[]; ks=[]
for k in range(NZ):
    b=f.read(SL)
    if len(b)<SL: break
    sl=np.frombuffer(b,dtype=np.uint8).reshape(NY,NX)
    body=(sl>0)&(sl<10)
    rowb=body.any(axis=1)
    nb=int(rowb.sum())
    if nb==0: continue
    bodyn=int(body.sum())
    bone=(sl==3)|(sl==4)
    boner=bone.any(axis=1)
    js=np.argmax(rowb)
    if not boner.any():
        rows.append((bodyn,0,0,np.nan,js*V+OFFY,0.,np.nan)); ks.append(k); continue
    jb=int(np.argmax(boner))
    band=bone[jb:jb+40,:]
    xi=np.where(band.any(axis=0))[0]
    archW=(xi.max()-xi.min())*V if len(xi) else 0.
    acx=((xi.min()+xi.max())/2*V+OFFX) if len(xi) else np.nan
    rows.append((bodyn,int(bone.sum()),int((sl==4).sum()),jb*V+OFFY,
                 js*V+OFFY,archW,acx)); ks.append(k)
np.savez_compressed(sys.argv[1], k=np.array(ks),
    prof=np.array(rows,dtype=np.float64),
    cols=np.array(["body","bone","cort","ant_bone_y","ant_skin_y","archW","arch_cx"]))
print(f"{sys.argv[1].split('/')[-1]}  slices={len(ks)}")
