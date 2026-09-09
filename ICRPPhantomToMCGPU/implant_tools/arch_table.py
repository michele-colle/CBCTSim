"""Offline arch-Z detector: works off the dumped per-slice profiles, so it can
be re-tuned instantly without touching the 3.8 GB volumes.

Anchor: the alveolar arch sits a roughly fixed distance below the VERTEX
(highest tissue slice), which is independent of where the source series was
truncated -- unlike the volume floor, which lands on the neck or on the
truncation cut.  Inside that window the best slice is the one whose anterior
bone both approaches the anterior skin and spans a dental-arch-like width.
"""
import numpy as np, glob, os
V=0.3; OFFZ=-125.10
LO, HI = 130.0, 185.0          # mm below vertex to search
rows=[]
for f in sorted(glob.glob(f"{os.path.dirname(__file__)}/prof/*.npz")):
    d=np.load(f, allow_pickle=True)
    k=d["k"]; P=d["prof"]; cols=list(d["cols"])
    body=P[:,cols.index("body")]; bone=P[:,cols.index("bone")]
    abY=P[:,cols.index("ant_bone_y")]; asY=P[:,cols.index("ant_skin_y")]
    aW=P[:,cols.index("archW")]; acx=P[:,cols.index("arch_cx")]
    z=k*V+OFFZ
    vtx=z.max(); flr=z.min()
    gap=abY-asY
    win=(z<=vtx-LO)&(z>=vtx-HI)
    # arch-likeness: bone present, dental-width arch, bone set back from the
    # skin by a real margin (>=4 mm rules out a truncation cut, <=32 mm rules
    # out deep structures like the vertebrae)
    good=win&(bone>4000)&(aW>=26)&(aW<=58)&(gap>=4)&(gap<=32)
    name=os.path.basename(f)[:-4]
    if not good.any():
        rows.append((name,vtx,flr,None,0,"NO_CANDIDATE")); continue
    # among candidates prefer the widest arch (the crest), tie-break lowest z
    idx=np.where(good)[0]
    best=idx[np.argmax(aW[idx]-0.15*np.abs(gap[idx]-14))]
    rows.append((name,vtx,flr,z[best],int(good.sum()),
                 f"archW={aW[best]:.1f} gap={gap[best]:.1f} cx={acx[best]:+.0f}"))
print(f"{'case':<24}{'vertex':>8}{'floor':>8}{'arch_z':>8}{'v-arch':>8}{'n':>4}  detail")
ok=0
for n,v,fl,az,c,det in rows:
    if az is None:
        print(f"{n[:24]:<24}{v:+8.1f}{fl:+8.1f}{'--':>8}{'--':>8}{c:4d}  {det}")
    else:
        ok+=1
        print(f"{n[:24]:<24}{v:+8.1f}{fl:+8.1f}{az:+8.1f}{v-az:8.1f}{c:4d}  {det}")
print(f"\n{ok}/{len(rows)} cases with an arch candidate")
d=[v-az for n,v,fl,az,c,_ in rows if az is not None]
print(f"vertex-to-arch distance: mean={np.mean(d):.1f} sd={np.std(d):.1f} "
      f"range=[{min(d):.1f},{max(d):.1f}] mm")
