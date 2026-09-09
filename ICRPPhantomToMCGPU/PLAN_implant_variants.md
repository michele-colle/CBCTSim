# PLAN — dental-implant variants from existing MC-GPU volumes

**Goal**: produce a new published set of 26 MC-GPU phantom volumes that are the
*existing* published volumes with dental implants (label 5, titanium) stamped
into the alveolar arch. No rebuild from DICOM: the base `.raw` is the input.

Status: planned, nothing built. Drafted 2026-09-07.

---

## 1. Why this is cheap

The published set is already implant-ready:

- every existing `.in` already declares `material/Pen_base/titanium.mcgpu
  density=4.54 voxelId=5` — no `.in` or material work at all;
- label 5 is already in the label scheme and the QA palette
  (`expand_in_kv.py:139`);
- publishing is `.raw`-only (`upload_phantoms_to_gcp.sh`), so no `.in` needs
  to be generated for this set — `.raw` + `.txt` companion is the whole
  deliverable.

Measured costs (this machine, 32 threads):

| Step | Cost |
|---|---|
| decompress an archive to disk | 8.4 s (1.5 s to /dev/null) |
| copy base 3.8 GB -> new name | ~10-20 s (local ext4) |
| stamp cylinders | seconds (only affected Z slices are touched) |
| **recompress `xz -T 32 -9e`** | **~68 s** <- dominates |
| round-trip verify (decompress + md5) | ~15 s |

=> ~2.5-3 min of compute per volume, ~75 min for all 26. Wall time is
dominated by the per-case visual check, not by compute.

---

## 2. Scope — exactly 26 volumes

Implants only make sense where the dental arch is actually in-frame and the
jaw can physically hold a fixture. The 66-case CQ500 **TEETH** batch is
**excluded** — its dental arch is only partial (crowns fall below the source
series cut; 25 of 66 cases have their lowest bone at a truncation plane).

### 2a. GradientHealth — 10, `.raw` already on disk
`GRADIENTHEALTH_VOLUME_EXPORT`, ages 41-89, all adult, no children:
`GRDN00MGH1ABQLE8` `GRDN02BKET588MC5` `GRDN0V840WXJ02B6` `GRDN0XOSMPPEAYBU`
`GRDN0YP5B56SGAJM` `GRDN17YG2OQIYVPO` `GRDN1P7W3AFP70R8` `GRDN1TQOUC2B1R2U`
`GRDN21I9AD975J98` `GRDN2SAZN503S7N0`

### 2b. CQ500 full-cranium — 10, `.raw` already on disk
`QUREAI_VOLUME_EXPORT`: CT-`127` `136` `172` `183` `241` `266` `298` `305`
`307` `325`. (`PatientAge` is stripped in CQ500; ages unverifiable per case.
CT-136 and CT-172 are stitched studies with no single source series.)

### 2c. MCRP — 6, archives only (need decompress)
Age >= 10 y per decision of 2026-09-07:
`MRCP-10F_dicom_1328x815x1067_A` `MRCP-10M_dicom_1317x828x1067_A`
`MRCP-15F_dicom_1316x812x1067_A` `MRCP-15M_dicom_1444x834x1067_A`
`MRCP_AF_dicom_1539x955x1067_A`  `MRCP_AM_dicom_1552x970x1067_A`

Excluded: 00F/00M/01F/01M/05F/05M. Measured mandibular wall thickness is
**3.6 mm** in the newborn vs **14.7 mm** in the adult male; the narrowest real
implant (3.3 mm) needs 5-6 mm of bone. They would be rejected by scoring
anyway. 10F/10M are **mixed dentition** — flag them and use the small end of
the size range (3.3 mm x 8 mm).

---

## 3. Naming and layout

`<case>_A_imp_<Nx>x<Ny>x<Nz>byte.raw` (+ `.txt`), keeping the letter field
meaning what it means today (stretcher position) and adding an orthogonal
token. Per the per-batch convention, one set of artifacts per source batch:

| Source batch | Output dir (H:) | Checks | Manifest |
|---|---|---|---|
| GradientHealth | `GRADIENTHEALTH_IMPLANT_VOLUME_EXPORT` | `positioning_checks_gradienthealth_implant/` + `_pending/` | `batch_jobs_gradienthealth_implant_exported_raw.txt` |
| CQ500 full-cranium | `QUREAI_IMPLANT_VOLUME_EXPORT` | `positioning_checks_qureai_implant/` + `_pending/` | `batch_jobs_qureai_implant_exported_raw.txt` |
| MCRP | `MCGPU_IMPLANT_VOLUME_EXPORT` | `positioning_checks_mcrp_implant/` + `_pending/` | `batch_jobs_mcrp_implant_exported_raw.txt` |

The accepted implant geometry per case is saved as JSON under
`params/generated_<batch>_implant/<case>.json` — that is the reproducibility
record for this set (it replaces the `.cfg` role, since `dicom_to_mcgpu` is
not re-run).

---

## 4. Tooling (built and tested, in `/home/colle/implant_work/`)

| Tool | Role |
|---|---|
| `view.py` | sagittal / axial / coronal, axes in isocenter mm, implant overlay (solid = intersects plane, dotted = off-plane) |
| `montage.py` | axial sweep over a Z range — used to pick the arch by eye |
| `place.py` | fits the alveolar arch centreline (polar sweep), samples N sites, scores each cylinder's bone/soft/air fractions |
| `stamp.py` | copies the base then patches only affected slices; reports what each implant replaced |
| `dump_profile.py` | one-pass per-slice profile -> `.npz`, so detector tuning never re-reads a 3.8 GB volume |
| `arch_table.py` / `arch_v2.py` | offline arch-Z candidates from the profiles |

Implant sizes: real dental hardware, seeded per case so a rerun reproduces it.
Diameters 3.3 / 3.75 / 4.1 / 4.5 mm, lengths 8 / 10 / 11.5 / 13 mm,
3-6 sites per case, jittered along the arch.

Profiles already dumped: all 20 on-disk cases + `MRCP_AM` + `MRCP-00M`.
**Still needed: 5 profiles** (`MRCP-10F` `10M` `15F` `15M` `AF`).

---

## 5. Per-case procedure

1. **Base available** — on disk for the 20; decompress for the 6 MCRP.
   *Never modify or delete a base.*
2. **Arch search window** = `[vertex-190, vertex-125]` mm, where vertex is the
   highest tissue slice. Centre the montage on the Rule-B candidate.
3. **Render axial montage** over that window (~10 panels, 6 mm step, XY window
   auto-derived from the slice's own tissue bbox). **Look at it. Pick arch Z.**
4. **`place.py`** at that Z -> per-site bone/soft/air fractions.
5. **Render QA overlay** (axial at implant Z + coronal + sagittal through one
   implant). **Look at it.** Accept, or re-place at a different Z / with
   manual coordinates.
6. **`stamp.py`** -> `<case>_A_imp_*.raw` + `.txt`.
7. **Verify** (see gates below). **Render final QA** of the stamped volume into
   `positioning_checks_*_implant/`; anything flagged also goes to `_pending/`.
8. **Compress** with the project's verbatim command:
   `tar --use-compress-program='xz -T 32 -9e -M 30G' -cf NAME.tar.xz NAME`
9. **Prove the archive** (round-trip md5 == raw md5, `xz -t` passes), append to
   the manifest, then delete the *implanted* `.raw`. Base untouched.

## 6. Acceptance gates

Numbers alone do not catch placement bugs — a render is looked at for every
case. In addition, hard gates:

- per accepted implant: `bone >= 0.55` **and** `air <= 0.05`
  (the air gate matters: a cylinder 35% in air passed the bone gate alone
  during testing on `MRCP_AM` and had to be rejected);
- `>= 2` accepted implants per case, else the case goes to `_pending/`;
- diff vs base: **every** differing voxel has new value 5;
- `air`, `carbon`(10) and `foam`(11) label counts **unchanged** — an implant
  must never land in air or in the stretcher;
- total voxel count unchanged;
- touched Z range confined to the implant span.

Reference run (CQ500-CT-100, TEETH, method validation only — not part of the
published set): 4 implants, 22 265 voxels, all new value 5, Z touched
[-89.7, -76.5] mm, air/carbon/foam unchanged, 94% of replaced voxels bone.

---

## 7. Disk

- Peak transient per case: base 3.8 GB + implanted 3.8 GB = **7.6 GB**.
- Work on local ext4 (`/`, 825 G free), not on H: — much faster than drvfs.
- Move only the finished `.tar.xz` + `.txt` to H: (220 G free).
- Total published payload: 26 archives, ~3-6 MB each.
- Do **not** reclaim the 20 base `.raw` on H: as part of this work — they are
  the input. (They remain separately reclaimable afterwards, ~71 GB.)

---

## 8. Risks / open items

| Risk | Handling |
|---|---|
| **Arch-Z auto-detection is unreliable** — the first rule was 24 mm off on the one visually confirmed case (picked the zygomatic level, which is *wider* than the alveolar crest). Rule B (lowest qualifying slice) landed 4.4 mm off, inside the crest band, but is validated on **one** case only. | Detector generates the search window only; the Z is picked by eye per case. |
| Ridge atrophy in elderly cases (`GRDN00MGH1ABQLE8` is 89 y) — thin alveolar bone, fewer viable sites. | Accept >= 2 sites; otherwise `_pending/`. |
| Laterally/AP-offset heads (all GradientHealth). | Fixed: arch window derived per case from the slice's tissue bbox (was hardcoded, would have missed every GRDN case). |
| Gantry-tilted / obliquely cut series (seen in the TEETH batch). | Visible in the montage; skip or re-place. Do not reason about tilt with SimpleITK's orthogonalised reader. |
| MCRP 10F/10M mixed dentition. | Smallest size class only; flag in the manifest. |
| **Publishing is blocked** — no GCP credentials on this machine. | Build + archive + verify locally; upload needs `gcloud auth login` by the user first. Never read a failed bucket call as "objects absent". |

## 9. Optional extension (not in scope)

The 60 MCRP `B-F` stretcher variants are archived but unpublished. Stretcher
inpainting never touches anatomy, so the *same* implant coordinates apply to
every letter of a given phantom — implants could be stamped into the in-scope
phantoms' B-F variants with no new placement work (30 more volumes for the 6
phantoms in scope).
