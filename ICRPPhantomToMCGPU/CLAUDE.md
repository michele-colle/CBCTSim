# ICRPPhantomToMCGPU — CLAUDE.md

**Purpose of this folder: build MC-GPU phantom volumes (uint8 label `.raw` +
`.txt` geometry companion, optional `.in`) from anatomical sources, QA them
with sagittal positioning-check renders, then compress and publish them to
Google Cloud Storage and reclaim the working disk.**

Everything here serves that loop: converters (`mcrp_to_vox`,
`dicom_to_mcgpu`), Python batch drivers, per-batch job JSONs / cfgs /
positioning-check folders, and the two upload scripts.

## Layout and where things live

- **Git repo root is one level up**: `/home/colle/CBCTSim` (branch
  `celeritas-test`, remote-side main is `main`). This folder is tracked as
  `ICRPPhantomToMCGPU/`, so run `git` from `/home/colle/CBCTSim` and expect
  paths prefixed with `ICRPPhantomToMCGPU/`. `.gitignore` lives at the repo
  root and already excludes this folder's `data/`, `output/`, `phantoms/`.
- **Run binaries and Python from *this* folder** — cfgs use relative
  `params/...`, `phantom/...` paths that resolve from here.
- `data/`, `output/`, `phantoms/` are **symlinks** into
  `/mnt/h/MICHELE_CBCTSim_DATA/ICRPPhantomToMCGPU/` (H: drive, gitignored).
  `phantoms/` holds the ICRP/MRCP `.node/.ele/.material` meshes.
- **Production volumes and DICOM intermediates live on H:**
  `/mnt/h/MICHELE_MCGPU/<BATCH>_VOLUME_EXPORT` (+ `..._IN_FILES`).
  **Source clinical datasets live on F:** e.g.
  `/mnt/f/Michele_diskF/kaggle/qureai-headct/` (CQ500 + `dataset_manifest.json`),
  `/mnt/f/Michele_diskF/GradientHealth/...`.
- Disk is the standing constraint: H: is 932 G / ~348 G free (2026-09-09),
  F: 466 G / ~35 G free. One uncompressed volume is 3.80 GB.

## Build

```bash
cmake --build build --target dicom_to_mcgpu -j$(nproc)
```

Targets: `dicom_to_mcgpu`, `mcrp_to_vox`, `mcrp_to_mcgpu_materials`,
`hu_to_4labels`, `raw_converter`, `dicom_fitter`.

## The production volume (fixed for the whole published set)

| | |
|---|---|
| Matrix / voxel | 2134 × 2134 × 834, 0.3 mm isotropic (640.2 × 640.2 × 250.2 mm) |
| Format | uint8, Z-major (`index = k*Ny*Nx + j*Nx + i`), 3.80 GB; 2.7–5.5 MB compressed (~930:1) |
| Origin / axes | isocenter at volume centre; +Y posterior (stretcher side), +Z cranial |
| Labels | 0 air, 1 fat, 2 soft tissue, 3 spongiosa, 4 cortical, 5 implant, 10 stretcher carbon, 11 stretcher foam |
| Naming | `<phantom_name>_<PP>_<variation>[_imp[N]]_<Nx>x<Ny>x<Nz>byte.raw` + `.txt` — see below |

### Naming convention (current)

```
<phantom_name>_<PP>_<variation>[_imp[N]]_<Nx>x<Ny>x<Nz>byte.raw   (+ .txt)
```

- **`<PP>`** — two-digit **geometry tag**: the position of the anatomy inside
  the box for a *fixed* DICOM head placement. A different `PP` means the
  volume was rebuilt with a different placement, so volumes with different
  `PP` are **not** voxel-comparable. `01` = the original head-tip-anchored
  placement (`auto_head_placement`, `head_top_margin_mm = 10`) used by the
  whole published set; `02` = the nose-centred / dental framing of the
  implant lane (mid-face at the reconstructed FOV centre).
- **`<variation>`** — the stretcher-position letter, `A`–`F`, `A` = base.
  Same `PP`, different letter = same anatomy placement, stretcher moved.
- **`_imp[N]`** — optional implant tag, so implanted volumes are trivially
  selectable. Bare `_imp` = the first implant placement; a future alternative
  placement over the same base becomes `_imp2`, `_imp3`, …
- Every geometry ships **with and without** implants, so an artefact study
  always has a control at identical geometry.

The published set (158 objects, built before this convention) is named
`<phantom_name>_<letter>_<dims>.raw` with no `PP`; read those as `PP = 01`.

Build time ~1.9 min/volume. Keep the box dimensions identical across batches
— campaigns swap phantoms without touching the beam definition.

## Pipelines

1. **ICRP/MRCP mesh → phantom**: `mcrp_to_mcgpu_materials` (once per phantom)
   → `mcrp_to_vox` with `tight_crop_dicom = true` (`batch_mcrp_to_dicom.py`)
   → tight-crop DICOM in `/mnt/h/MICHELE_MCGPU/DICOM_EXPORT_MCRP` → fed to
   `dicom_to_mcgpu` like a real patient. See `MCRP_TO_MCGPU_EXPORT.md`,
   `README_mcrp_to_vox.md`. (`mcrp_to_vox`'s own fixed-volume `.in` mode
   still works but is superseded — `dicom_to_mcgpu` owns placement now.)
2. **DICOM (real patient or synthetic) → phantom**: `dicom_to_mcgpu` +
   `batch_dicom_to_mcgpu.py scan|run` (N cases × M stretcher positions),
   with `auto_head_placement` (FOV-anchored head tip) and auto-seated
   stretcher. See `README_dicom_to_mcgpu.md` — that file is the primary
   reference for cfg keys, outputs, and batch flags.
3. **Non-DICOM volumes** (`.nrrd/.nii/.mha`) → `nrrd_to_dicom_series.py`
   first (`dicom_to_mcgpu` only reads DICOM series); geometry preserved,
   HU bit-exact, verified by read-back.
4. **Stretcher-position augmentation without rebuilding**:
   `inpaint_stretcher_variants.py` zeroes labels 10/11 in an existing base
   volume and re-stamps the shell at sampled positions (variants B–F).
5. **QA (the real acceptance test)**: `expand_in_kv.py`'s
   `render_positioning_check` sagittal PNG — head inside the FOV with a small
   air gap, stretcher directly behind the body, no overlap. Numbers alone do
   not catch placement bugs; always look at a render before trusting a batch.

Per-batch convention (follow it for every new batch): its own
`batch_jobs_<name>.json`, `params/generated_<name>/*.cfg`,
`--output-dir`/`--in-dir` on H:, `positioning_checks_<name>/` (plus a
`_pending/` folder holding only the renders that need eyes), and a
`batch_jobs_<name>_exported_raw.txt` manifest of what was written.

## Data produced so far (as of 2026-09-09)

`gs://mcgpu-data-gcp/phantom/` held **158 `.tar.xz` objects** when last
verified by listing (2026-09-08). The "98 volumes" figure below counts only
the four batches in the table and is *not* the bucket total — treat the
table as the record of what each batch contributed, not as an inventory of
the bucket.

| Batch | Where (H:) | Volumes | On-disk state |
|---|---|---|---|
| ICRP/MRCP mesh, base `A` | `MCGPU_EXPORT` | 12 published | `.raw` reclaimed; 72 `.tar.xz` + 72 `.txt` kept |
| ICRP/MRCP support variants `B–F` | `MCGPU_EXPORT` | 60 **archived, NOT uploaded** | archives only (`params/generated_mcrp_inpaint/exported_archives.txt`) |
| GradientHealth, full cranium | `GRADIENTHEALTH_VOLUME_EXPORT` | 10 published | 10 `.raw` **still on disk** (~38 GB reclaimable) |
| CQ500 curated, full cranium | `QUREAI_VOLUME_EXPORT` | 10 published | 10 `.raw` **still on disk** (~38 GB reclaimable) |
| CQ500 TEETH, partial cranium | `QUREAI_TEETH_VOLUME_EXPORT` | 66 published (all 66 after the round-8 tilt rebuild) | `.raw` reclaimed, 99 GiB freed; 66 `.tar.xz` + 66 `.txt` kept |

**427 GiB** of working disk has been reclaimed overall (332 GiB up to
2026-09-07, plus 95 GiB on 2026-09-08: the 20 published GradientHealth/CQ500
`.raw`, 4 MRCP `*_vox_*x1000` intermediates and 21 older `output/`
intermediates — all compressed and proven before deletion). Volumes are regenerable
from `params/generated*/*.cfg`; local `.tar.xz` are the fast restore path and
are always kept.

`MCGPU_PHANTOM_PIPELINE_REPORT.html` / `.pdf` (2026-08-31, untracked) is the
current external-facing summary of exactly this set.

**Staged next, converted but nothing built yet:**
- `/mnt/h/MICHELE_MCGPU/TOTALSEG_DICOM` — 226 TotalSegmentator v2.0.1 cases
  as CT DICOM series, 1.5 mm isotropic, filed by most distal anatomy: head
  194, knee 22, lower_leg 5, foot_ankle 5 (thighs deliberately excluded).
  `INDEX.csv` carries study type, age/sex, shape, obliquity. Obliquity there
  is a **rigid rotation, not gantry shear**.
- `/mnt/h/MICHELE_MCGPU/VSD_dicom` — 56 single-leg (mid-thigh→toe) DICOM
  series (28 of the 29 human VSD cases × 2 sides; `z036` excluded, see
  below), table removed, written by `extract_leg_dicom.py`. Details, per-case
  QA stats and dataset gotchas found along the way: `VSD_LEG_EXTRACTION.md`
  + `vsd_leg_extraction.csv`; central-sagittal QA renders in
  `VSD_dicom_sagittal_checks/` (`make_sagittal_check_pngs.py`). The 20 `z*`
  cases' crops are stored top/bottom-inverted relative to the rest — the
  pipeline derives the foot direction per case rather than applying the
  supplied `Transform-Upside-Down.h5` (that transform turned out to have its
  own metadata bugs — see `VSD_LEG_EXTRACTION.md`). `z036`'s `Femur_L/R`
  segmentation doesn't cleanly separate the femur from the rest of the
  pelvis, breaking the mid-thigh landmark; excluded from the pool rather than
  shipped broken.
- `/mnt/h/MICHELE_MCGPU/MCRP_dicom` — 48 single-limb (2 arms + 2 legs) DICOM
  series across all 12 MRCP mesh phantoms (00F/M, 01F/M, 05F/M, 10F/M,
  15F/M, AF, AM), written by `extract_mcrp_limb_dicom.py`. Computes each
  limb's crop box straight from the tetrahedral mesh (`.node`/`.ele`/
  `.material` — NOT from `DICOM_EXPORT_MCRP`, which is head-only,
  `vol_length_mm=300`) and drives `mcrp_to_vox`'s legacy ROI mode
  (`write_dicom=true`) once per limb; no thresholding/table-removal needed
  since these are clean synthetic phantoms with air backgrounds. Needs
  `LD_LIBRARY_PATH=/opt/Geant4/lib` to run the binary (not set by default in
  this shell). Left/right resolved per phantom via `Kidney_left`/
  `Kidney_right` centroid X, not assumed. Central-sagittal QA renders in
  `MCRP_dicom_sagittal_checks/`.

**Open work lane — dental implants in the CBCT FOV:** 26 **nose-centred base
volumes** are built, verified and archived in
`/mnt/h/MICHELE_MCGPU/IMPLANT_V2_BASE` (10 GradientHealth + 10 CQ500
full-cranium + 6 MRCP >=10 y), but **nothing is published and no implants are
stamped yet**. These are re-placed so the mid-face sits at the *reconstructed
FOV* centre rather than the head tip — the earlier attempt stamped implants
into the published `_A` bases and every implant fell outside the FOV, so that
set was deleted from GCS. Status, per-case margins, the placement rule and the
gotchas: [DENTAL_IMPLANT_LANE_STATUS.md](DENTAL_IMPLANT_LANE_STATUS.md);
tooling in [implant_tools/](implant_tools/). One case
(`GRDN1P7W3AFP70R8`) is still unresolved — read §7 before touching this lane.

**Open work lane — mcgpu volumes for arms and legs:** both `VSD_dicom` (legs)
and `MCRP_dicom` (arms + legs) above are DICOM-extraction-complete and ready
to feed into `dicom_to_mcgpu`, but **nothing has been run through it yet** —
no placement convention, output box size, or stretcher decision exists for a
limb (the current placement logic is head/CBCT-specific). Start here:
[LIMB_DICOM_HANDOFF.md](LIMB_DICOM_HANDOFF.md) (full manifest of all 104
series + open questions to resolve before batching).

## Compression + upload to GCP

Account `michele.colle@rartech.it`, project `cbct-simulation-gpu`.

> **There are NO usable GCP credentials on this machine.** Any `gsutil`/
> `gcloud` call that touches the bucket (upload, listing, md5 verification,
> `--reclaim`) will fail with *Reauthentication required*. Credentials cannot
> be obtained non-interactively: **ask the user to authenticate via OAuth**
> (`gcloud auth login`) and wait for them to confirm before running anything
> that needs the bucket. Do not attempt to work around it, and do not read a
> failed bucket call as "the objects aren't there".

The known-good compression command, used verbatim by both scripts:

```bash
tar --use-compress-program='xz -T 32 -9e -M 30G' -cf NAME.tar.xz NAME
```

**Phantoms → `gs://mcgpu-data-gcp/phantom/`** — one `.tar.xz` per `.raw`
(raw only, no `.txt`, no `.in`), one at a time so each compression gets all
32 threads. Every run is logged to `logs/upload_phantoms_to_gcp_<ts>.log`.

```bash
./upload_phantoms_to_gcp.sh /mnt/h/MICHELE_MCGPU/QUREAI_TEETH_VOLUME_EXPORT
./upload_phantoms_to_gcp.sh --list clean.txt --reclaim <dir>
./upload_phantoms_to_gcp.sh --list clean.txt --reclaim-only <dir>
./upload_phantoms_to_gcp.sh --list clean.txt --reclaim --dry-run <dir>
```

`--list FILE` = publish only the named `.raw` (bare names resolve against
`<dir>`, `#` comments ok) — **required whenever a folder mixes publishable
and unpublishable volumes**, which is the normal case. `--reclaim` deletes a
`.raw` only after proving recoverability: local archive exists, its md5
equals the md5 GCS reports for the uploaded object, and `xz -t` passes.
Never gate deletion on an exit status.

**MC-GPU output folders → `gs://mcgpu-data-gcp/output/`** — whole folder,
`.raw` + `.txt`, `*.in` excluded:

```bash
./compress_and_upload_to_gcp.sh <folder1> [folder2 ...]
```

## Hard-won gotchas — do not relearn these

- **No GCP credentials on this machine — ask the user to OAuth in.** Verified
  2026-09-07: `gsutil ls gs://mcgpu-data-gcp/` returns *Reauthentication
  required*. Bucket work is blocked until the user authenticates
  interactively; ask them, don't retry. Never use
  `gsutil ls ... 2>/dev/null`: an expired credential then looks exactly like
  an empty bucket, i.e. a false "upload failed". `gcs_md5_hex()` in
  `upload_phantoms_to_gcp.sh` still swallows stderr, so an auth failure there
  is indistinguishable from a missing object (it fails safe — keeps the
  `.raw` — but diagnoses terribly). Check
  `gcloud config get-value account` before blaming the bucket.
- **Verify uploads by NAME against the bucket listing, not by object count.**
- **Never sweep a batch log that was captured through `tail`/`head`** — a
  26-tilted-case batch once reported "1 oblique series" because only the last
  60 lines had been saved.
- **Gantry tilt is handled (round 8) — with `D^-1`, not `D^T`.** A tilted
  lattice is *sheared*, not rotated (`det = 0.9135` at 24°); only a general
  inverse undoes it. SimpleITK's default reader orthogonalises and reports
  `det = 1`, hiding the shear — don't reason about tilt with it. NRRD masks
  are looked up **index-to-index**, deliberately: routing them through
  physical space loses ~15% of the masked volume.
- **`mask_nrrd` is applied after head detection**, so the couch/cradle
  inflates the detected AP depth and the stretcher seats 20–40 mm (CQ500) to
  ~80 mm (GradientHealth) too far posterior. Compensate per case with a
  negative `stretcher_cy_offset_mm` when tight contact matters.
- **`detectHeadHU` still scans the index grid** — on an oblique series "tip Z"
  is a few mm off (anatomy is not deformed). Warns; not fixed.
- **`head_at_max_z` is a config flag, not orientation detection** — flip it
  for feet-first series.
- **The reconstructed FOV is much smaller than the box.**
  `fovHalfZ = (Dz/2)*(SAD/SDD) = (29.34/2)*(57.8/98.70) = 85.9 mm`, while the
  box half-height is 125.1 mm. `auto_head_placement` anchors the **head tip**
  at `fovHalfZ - head_top_margin_mm`, which puts the dentition below the beam:
  an entire 26-volume implant set had to be scrapped for this. Changing
  `head_top_margin_mm` by D shifts all anatomy by -D, so re-place relative to
  a known build rather than solving from scratch.
- **Many clinical head CTs here are DEFACED** (anonymised by shaving the face
  off), so **the nose is not a usable landmark** — the anterior contour shows a
  flat plateau and the most-anterior voxel becomes the glabella or a surviving
  lip. Confirmed on `CQ500-CT-298`, `GRDN02BKET588MC5`, `GRDN1P7W3AFP70R8`.
  Anchor on the dental arch instead. Also: "most anterior voxel" is the
  shoulder when the series includes the chest, and the chin on some patients
  (`GRDN1TQOUC2B1R2U`). Sanity-check vertex-to-nose = 120-145 mm, and detect
  landmarks in the **built volume**, never in the source series.
- **A QA render must show the volume as stored.** `implant_tools/fovcheck.py`
  needs `--as-is`; without it the render is a *preview of a further shift* and
  silently shows the wrong geometry for any volume whose detected landmark
  isn't already on target. This caused two false "bad placement" reports.
- **Never `pkill -f <pattern>` when the pattern matches your own command
  line** — it kills the shell running it (exit 144) and every later line in
  that call silently never runs.
- **Regenerate a stale jobs JSON rather than reusing it** — a stale one once
  pointed a case at the wrong source series entirely.
- Build one case and look at its render before launching a full batch.

## Doc map

| File | What it covers |
|---|---|
| `README_dicom_to_mcgpu.md` | primary reference: cfg keys, placement, stretcher, outputs, batching, §11 publishing/reclaiming |
| `MCRP_TO_MCGPU_EXPORT.md` | mesh → voxel production pipeline |
| `README_mcrp_to_vox.md` | older single-phantom ROI workflow |
| `PROGRESS_dicom_to_mcgpu_auto_placement.md` | round-by-round history + validation numbers (round 8 = tilt fix) |
| `PLAN_...` / `HANDOUT_...` | design rationale for auto-placement and the `.in`-retirement decision |
| `VSD_FULLBODY_INVENTORY.md`, `TOTALSEGMENTATOR_SELECTION.md` | inventories of the two staged datasets |
| `VSD_LEG_EXTRACTION.md` | VSD leg-DICOM extraction method + dataset bugs found/fixed |
| `LIMB_DICOM_HANDOFF.md` | **open work**: manifest + next steps for building limb MC-GPU volumes |
| `DENTAL_IMPLANT_LANE_STATUS.md` | **open work**: nose-centred re-placement + dental-implant set, per-case margins, gotchas |
| `implant_tools/README.md` | implant placement/stamping tools and their method notes |
| `PLAN_implant_variants.md`, `IMPLANT_RUN_RESULTS.md` | original implant design, and the scrapped round-8 set |
| `MCGPU_PHANTOM_PIPELINE_REPORT.html/.pdf` | external-facing summary of the published set |
