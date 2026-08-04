# dicom_to_mcgpu — How-To Guide

Reads a DICOM CT series (real patient, or a synthetic tight-crop export from
another tool), places it into a fixed MC-GPU output volume — automatically or
via manual offsets — adds implant / crop-cylinder / stretcher overlays, and
writes:

- an MC-GPU uint8 label `.raw` + companion `.txt`
- an MC-GPU `.in` file (if a template is given — omit `mcgpu_in_template` for
  a minimal raw+txt-only output; positioning checks still work, see §8)
- a DICOM output folder with HU values (if `write_dicom` is enabled)

Output normally lands in a `<output_prefix>_vox_<dims>/` subfolder next to
the input; set `output_dir` to instead write the `.raw`/`.txt` directly
(flat, no subfolder) into an external folder, and `mcgpu_in_dir` to write the
`.in` file(s) into a SEPARATE external folder — keeps large MC-GPU volumes,
small `.in` files, and the DICOM input tree from all mixing together (see
§6/§7). Ready-to-use example pair:

```ini
output_dir   = H:\MICHELE_MCGPU\MCGPU_VOLUME_EXPORT
mcgpu_in_dir = H:\MICHELE_MCGPU\MCGPU_IN_FILES
```

(Windows paths are converted automatically — see the note at the end of §6.)

**Label scheme:**

| Label | Meaning |
|-------|---------|
| 0 | air |
| 1 | fat |
| 2 | soft tissue |
| 3 | bone spongiosa |
| 4 | bone cortical |
| 5 | implant (optional cylinders) |
| 10 | stretcher carbon wall (optional) |
| 11 | stretcher foam core (optional) |

**Coordinate system:** origin = isocenter = centre of the output volume.
Voxel `[i,j,k]` centre = `((i+0.5)*vxy - Nx*vxy/2, ..., ...)`. `+Y` = patient
posterior (the stretcher side).

---

## 1. Build

```bash
cmake --build build --target dicom_to_mcgpu -j$(nproc)
```

## 2. Run

Always run **from the repo root** (relative paths in the cfg — `params/...`,
`phantom/...` — resolve from there):

```bash
./build/dicom_to_mcgpu params/my_run.cfg
./run_dicom_to_mcgpu.sh params/my_run.cfg     # helper: also translates Windows (F:\...) paths
```

The binary takes one argument: a `.cfg` parameter file (or, legacy form,
`dicom_to_mcgpu <dicom_dir> [output_prefix]` with everything else at
defaults).

## 3. Minimal config example

```ini
dicom_dir     = "/mnt/f/some/patient/series"
output_prefix = "/mnt/f/some/patient/MCGPU"

vol_vxy_mm = 0.3
vol_vz_mm  = 0.3
vol_width_mm  = 640.0
vol_height_mm = 640.0
vol_length_mm = 250.0

dicom_corner_z_mm = -70.0    # manually measured in a DICOM viewer

stretcher_enable = true
stretcher_cx_mm  = 0.0
stretcher_cy_mm  = 133.5
```

## 4. Placement: manual vs. automatic

### Manual (default; how real-patient cases are normally placed)

A human looks at the DICOM in a viewer and sets:

```ini
# Centre-to-centre offset of the DICOM volume from the output volume's centre
# (isocenter), in mm.  (0,0,0) = DICOM centred.  Despite the cfg key names
# ("dicom_corner_*"), these are CENTRE offsets, not raw corner coordinates —
# the binary derives the actual corner internally (offset - half-extent), so
# the same value places every series identically regardless of its physical size.
dicom_corner_x_mm = 0.0
dicom_corner_y_mm = 0.0
dicom_corner_z_mm = -70.0

# Optional 3D rotation around the DICOM's own centre (XYZ intrinsic Euler, deg)
dicom_rot_x_deg = 0.0
dicom_rot_y_deg = 0.0
dicom_rot_z_deg = 0.0
```

### Automatic (opt-in; works on any head DICOM, real or synthetic)

Instead of a human measuring `dicom_corner_z_mm`, a basic HU-threshold scan
finds the head tip and auto-anchors it against the CBCT beam's reconstructed
FOV (parsed from `mcgpu_in_template`):

```ini
auto_head_placement = true
head_detect_thr_hu  = -500    # tissue vs. air; matches thr_air_fat
head_at_max_z       = true    # which end of the loaded volume is "toward the head"
head_region_mm      = 100.0   # top slab used for XY centring + AP-depth measurement
head_top_margin_mm  = 10.0    # air gap kept between head tip and the FOV top
```

How it works: scans slices from the `head_at_max_z`-designated end inward for
the first slice whose thresholded-voxel count exceeds a small noise floor —
that's the head tip, regardless of the torso/neck/head area profile further
in. Within the following `head_region_mm` slab (moving into the body), it
takes the XY bounding box for centring. Runs on the already-loaded (and, if
`denoise=true`, already median-filtered/despeckled) HU buffer, so it isn't
fooled by scan noise. When enabled, it **overrides** `dicom_corner_*_mm`
(a log line shows the values actually used) — set it and leave the manual
offsets alone.

`head_at_max_z` matters: DICOM Z order isn't universal. If the detected
placement looks wrong in a positioning-check PNG (head at the wrong end, or
plumb air where the head should be), flip this flag.

**`fov_template` (decoupled from `mcgpu_in_template`):** the FOV half-height
used above comes from the FIRST entry of `mcgpu_in_template` by default — but
if you want a minimal run (`mcgpu_in_template` empty, no `.in` written; see
the top of this doc and §6), there'd be no template left to compute the FOV
from, and placement would silently degrade to "margin below the *output
volume's own* top face" instead of the actual beam FOV. Set `fov_template`
explicitly to keep FOV-anchored placement correct even with no `.in` output:

```ini
mcgpu_in_template =                              # empty -> no .in written
fov_template      = params/cbct_head_bar_template.in   # FOV source only
```

`batch_dicom_to_mcgpu.py run --minimal` sets this automatically (§9).

## 5. Stretcher: manual vs. auto-seat + augmentation

### Manual (absolute; default)

```ini
stretcher_enable = true
stretcher_cx_mm  = 0.0
stretcher_cy_mm  = 133.5      # measured directly, isocenter-relative
```

### Auto-seat + offset augmentation (opt-in)

Uses the **same** head-detection pass as automatic placement (so
`auto_head_placement` doesn't need to be on too, but does need a head to
detect — either works standalone):

```ini
stretcher_enable       = true
stretcher_auto         = true
stretcher_cx_mm        = 0.0     # lateral position stays direct either way
stretcher_gap_mm       = 2.0     # gap behind the detected head's posterior surface
stretcher_cy_offset_mm = 0.0     # <-- the augmentation knob
```

`stretcher_cy_mm` is ignored when `stretcher_auto=true`. Instead the shell's Y
position becomes `(detected head AP half-depth) + stretcher_gap_mm +
28.5 (shell half-height) + stretcher_cy_offset_mm`. Varying
`stretcher_cy_offset_mm` across otherwise-identical runs is how you generate
distinct stretcher-position variants for augmentation without re-measuring
anything — see the batch driver below.

**No-stretcher variant:** don't re-run the binary. Take an already-built
`.in` and edit its `[SECTION MATERIAL FILE LIST]` so `voxelId=10` (carbon)
and `voxelId=11` (foam) both point at the air material. The label geometry
is untouched; the shell becomes physically air. This only works for "with
vs. without stretcher at this exact position" — a different stretcher
*position* still needs its own run (it's geometry, not material).

## 6. Full config reference

```ini
# ── Input ─────────────────────────────────────────────────────────────────
dicom_dir          = path/to/series      # Windows (F:\...) or Linux form
output_prefix       = path/to/MCGPU       # output folder derives from this
                                           # (ignored for the FOLDER when
                                           # output_dir is set -- see below)
output_dir          =                     # optional: write .raw/.txt flat into this
                                           # folder instead of "<output_prefix>_vox_<dims>/"
mcgpu_in_dir        =                     # optional: write .in file(s) into a
                                           # SEPARATE flat folder (see §4/§7's caveat
                                           # about the .in's own phantom/ reference)
series_uid          =                     # empty = first series found
acquisition_number  = -1                  # -1 = auto; see note below

# ── Output volume geometry (0/omitted -> derive from DICOM) ──────────────
vol_vxy_mm = 0.3
vol_vz_mm  = 0.3
vol_width_mm  = 640.0
vol_height_mm = 640.0
vol_length_mm = 250.0

# ── Placement (§4) ────────────────────────────────────────────────────────
dicom_corner_x_mm / dicom_corner_y_mm / dicom_corner_z_mm   # manual
dicom_rot_x_deg / dicom_rot_y_deg / dicom_rot_z_deg
auto_head_placement / head_detect_thr_hu / head_at_max_z /
  head_region_mm / head_top_margin_mm                        # automatic
fov_template =    # optional; FOV source for auto placement, decoupled from
                  # mcgpu_in_template -- set when mcgpu_in_template is empty

# ── HU thresholds [HU] ────────────────────────────────────────────────────
thr_air_fat        = -500
thr_fat_soft       =  -50
thr_soft_spongiosa =  200
thr_spongiosa_cort =  800

# ── Noise-robust classification (optional) ───────────────────────────────
denoise                = false   # master switch for both stages below
median_radius          = 0       # 3D median filter radius [vox]; 0 = off
despeckle_min_size     = 0       # min connected-component size [vox]; 0 = off
despeckle_connectivity = 1       # 1 = 6-neigh., 2/3 = 26-neigh.
despeckle_labels       = 0 3 4   # labels to treat (air, spongiosa, cortical)

# ── Geometry fine-offset [mm] (added only to the .txt/.in offset) ───────
shift_x_mm / shift_y_mm / shift_z_mm = 0.0

# ── Crop cylinder (axis along Z, isocenter coords) ───────────────────────
crop_cylinder_enable    = false
crop_cylinder_radius_mm = 100.0
crop_cylinder_cx_mm / crop_cylinder_cy_mm = 0.0

# ── Implant cylinders (isocenter coords, axis along Z) ───────────────────
implant_count = 0
implant_0_cx_mm / _cy_mm / _cz_mm / _radius_mm / _height_mm = ...
# (implant_1_*, implant_2_*, ... ; or the legacy single-implant
#  implant_cx_mm/.../implant_radius_mm keys when implant_count is unset)

# ── Stretcher (§5) ────────────────────────────────────────────────────────
stretcher_enable / stretcher_cx_mm / stretcher_cy_mm            # manual
stretcher_auto / stretcher_gap_mm / stretcher_cy_offset_mm      # auto-seat

# ── MC-GPU simulation template(s) ────────────────────────────────────────
mcgpu_in_template = params/cbct_head_bar_template.in   # ','/';' for several
                                                        # empty -> no .in written (minimal output)
mcgpu_output_name = results/
mcgpu_det_nx = 512
mcgpu_det_nz = 512
phantom_name =            # sets the .raw basename; empty = legacy descriptive name

# ── DICOM output (HU values) ──────────────────────────────────────────────
write_dicom = true    # NOTE: defaults to true; the shipped template sets it
                       # false — set explicitly, don't rely on the default.
                       # Output folder: <output_prefix>_vox_<dims>_dicom/

# ── Segmentation mask (NRRD, optional) ────────────────────────────────────
mask_nrrd =    # voxels where mask==0 are forced to air; same physical space as the DICOM
```

`acquisition_number`: some DICOM exports bundle multiple acquisitions of the
same anatomy into one series (duplicate slice positions, which otherwise
z-squash into a bogus half-spacing volume). `-1` auto-picks the acquisition
with the most slices; set an explicit number to override.

**Windows paths are handled automatically.** `dicom_dir`, `output_prefix`,
`output_dir`, `mcgpu_in_dir` and `mask_nrrd` all go through the same
`toLinuxPath()` conversion (`X:\foo\bar` → `/mnt/x/foo/bar`) the binary
already applies to `dicom_dir` — paste a Windows path directly into any of
them, no manual translation needed. Verified: `output_dir =
H:\MICHELE_MCGPU\MCGPU_VOLUME_EXPORT` and `mcgpu_in_dir =
H:\MICHELE_MCGPU\MCGPU_IN_FILES` both resolved correctly end-to-end (files
landed in `/mnt/h/MICHELE_MCGPU/...`).

## 7. Outputs

Written to `<output_prefix>_vox_<Nx>x<Ny>x<Nz>/` — or, if `output_dir` is
set, directly (flat, no subfolder) into `output_dir`:

| File | Description |
|------|-------------|
| `<phantom_name or descriptive>_<dims>byte.raw` (or `..._5labels_<dims>.raw`) | uint8 label volume |
| `<same>.txt` | MC-GPU geometry companion (offset/dims/voxel size) |
| `CBCT.in` / `CBCT_<template-stem>.in` | one per `mcgpu_in_template`, omitted entirely if it's empty (minimal output) |
| `<output_prefix>_vox_<dims>_dicom/` (or `<output_dir>/<prefix-stem>_dicom/` when flat) | HU DICOM series, if `write_dicom=true` |

**`mcgpu_in_dir` (separate `.in` folder):** when set, the `.in` file(s) go
here instead, flat, with filenames prefixed by `phantom_name` (or the
output-prefix stem) for uniqueness across a whole batch sharing this folder
— e.g. `MRCP-00F_A_CBCT_cbct_head_bar_template.in`. **Caveat:** the `.in`'s
own `VOXEL GEOMETRY FILE` line still reads `phantom/<raw-name>.raw`
(relative) — to actually run MC-GPU from `mcgpu_in_dir`, place or symlink the
matching `.raw` under `<mcgpu_in_dir>/phantom/` first; this only affects
running MC-GPU itself, not positioning checks (§8), which don't care where
the `.raw` lives.

Minimal output (`mcgpu_in_template` empty, no `mcgpu_in_dir` either) is just
the `.raw` + `.txt` — 2
files. Filenames stay unique in a shared flat `output_dir` because
`phantom_name` (always set by the batcher, per case+stretcher-position) drives
the basename, not `output_prefix`.

**Raw format:** uint8, Z-major order (`index = k*Ny*Nx + j*Nx + i`).

## 8. Positioning checks — the main correctness test

Numbers alone don't catch placement bugs (a coordinate-frame mixup can place
the phantom entirely outside the visible/beam region while every computed
offset still "looks" plausible in the log). Always render and **look at** a
positioning-check PNG before trusting a new run:

```python
from expand_in_kv import parse_in_geometry, render_positioning_check
from pathlib import Path

in_path = Path("<output_dir>/CBCT.in")
geom = parse_in_geometry(in_path)
raw = in_path.parent / geom["raw_name"]
render_positioning_check(in_path, raw, Path("positioning_checks/check.png"), geom, {})
```

This draws the central-sagittal slice (label volume) with the CBCT beam
(source, detector, FOV edges) overlaid at projection angle 0. Check: the head
sits just inside the FOV with a small air gap at the top (not clipped, not
floating with a huge unused gap), and the stretcher sits directly behind the
body with no overlap. `batch_dicom_to_mcgpu.py run` renders one of these
automatically for every job (see below) — no manual step needed there.

**Minimal output (no `.in`)?** Positioning checks still work.
`parse_split_geometry(txt_path, beam_template_path)` reads voxel geometry
from the `.txt` companion (same section format a `.in` carries) and beam
geometry from a separate, shared beam template — no per-run `.in` needed at
all, since beam geometry is identical across positions and across the
production templates:

```python
from expand_in_kv import parse_split_geometry, render_positioning_check
geom = parse_split_geometry("<output_dir>/case_A_2134x2134x834byte.txt",
                            "params/cbct_head_bar_template.in")
render_positioning_check(raw_path, raw_path, Path("positioning_checks/check.png"), geom, {})
```

`batch_dicom_to_mcgpu.py run` does this automatically whenever a job produced
no `.in` (see `--minimal` / `--check-template` below).

## 9. Batch driver: `batch_dicom_to_mcgpu.py`

Drives *N cases × M stretcher positions* from one JSON file.

```bash
# 1. generate a job skeleton from an export folder
#    (folder layout: export/<PATIENT_ID>/<STUDY>/<series>/, optional *.nrrd)
python3 batch_dicom_to_mcgpu.py scan "F:\...\export" -o batch_jobs.json

#    --auto-placement pre-fills offset-based stretcher slots and turns on
#    auto_head_placement for every case, instead of the absolute cx/cy pairs
#    you'd otherwise measure by hand. Default grid: 3 lateral (cx) positions
#    in [-30, 30] mm x 3 depth (cy_offset) positions in [0, 60] mm beyond the
#    auto-detected head-clearance limit = 9 positions/case; tune with
#    --stretcher-{cx,cy}-count / --stretcher-cx-{min,max} / --stretcher-cy-max-offset:
python3 batch_dicom_to_mcgpu.py scan "F:\...\export" -o batch_jobs.json --auto-placement
python3 batch_dicom_to_mcgpu.py scan "F:\...\export" -o batch_jobs.json --auto-placement \
    --stretcher-cx-count 2 --stretcher-cx-min -20 --stretcher-cx-max 20 \
    --stretcher-cy-count 4 --stretcher-cy-max-offset 45

#    --basic-placement: a SINGLE stretcher position instead of a grid --
#    centred (cx=0), auto-seated at the closest legal position directly
#    behind the detected head (cy_offset=0). Mutually exclusive with
#    --auto-placement.
python3 batch_dicom_to_mcgpu.py scan "F:\...\export" -o batch_jobs.json --basic-placement

#    --output-dir / --in-dir: write .raw/.txt and .in into two SEPARATE flat
#    external folders instead of one subfolder next to the DICOM input (keeps
#    large volumes, small .in files, and DICOM input from all mixing).
#    Ready-to-paste example pair (recommended default going forward):
python3 batch_dicom_to_mcgpu.py scan "F:\...\export" -o batch_jobs.json --basic-placement \
    --output-dir "H:\MICHELE_MCGPU\MCGPU_VOLUME_EXPORT" \
    --in-dir "H:\MICHELE_MCGPU\MCGPU_IN_FILES"

#    --minimal: suppress .in generation ENTIRELY (raw+txt only, no .in at
#    all, not even in --in-dir) -- positioning checks still render, via a
#    shared beam template instead of a per-run .in (§8). Rarely what you
#    want; --in-dir (above) is the usual choice when you want the .in kept
#    separate rather than not generated.
python3 batch_dicom_to_mcgpu.py scan "F:\...\export" -o batch_jobs.json --basic-placement \
    --output-dir "H:\MICHELE_MCGPU\MCGPU_VOLUME_EXPORT" --minimal

# 2. edit batch_jobs.json (absolute mode: fill in cx_mm/cy_mm per case)

# 3. run everything
python3 batch_dicom_to_mcgpu.py run batch_jobs.json
python3 batch_dicom_to_mcgpu.py run batch_jobs.json --dry-run     # cfg files only
python3 batch_dicom_to_mcgpu.py run batch_jobs.json --only PATIENT_ID
python3 batch_dicom_to_mcgpu.py run batch_jobs.json --no-checks   # skip PNG rendering
python3 batch_dicom_to_mcgpu.py run batch_jobs.json --png-dir positioning_checks
python3 batch_dicom_to_mcgpu.py run batch_jobs.json --check-template params/cbct_head_bar_template.in
```

Each `stretchers` entry in the JSON is one of two forms:

```json
{"cx_mm": 0.0, "cy_mm": 133.5}                              // absolute (manual)
{"cx_mm": 0.0, "auto": true, "cy_offset_mm": 0.0}           // auto-seat + augmentation
```

Top-level JSON keys with per-case override + global-default semantics
(a per-case key of the same name wins; absent everywhere -> untouched,
existing JSON files behave exactly as before):

| key | effect |
|---|---|
| `auto_head_placement` | enables automatic Z/XY placement (needed for `"auto": true` slots too) |
| `output_dir` | flat external folder for `.raw`/`.txt` (§6/§7) |
| `mcgpu_in_dir` | flat external folder for `.in` file(s), separate from `output_dir` (§6/§7 caveat about the `phantom/` reference) |
| `mcgpu_in_template` | `""` (e.g. via `--minimal`) suppresses `.in` generation entirely; unset = template's own value |
| `fov_template` | FOV source for placement, independent of `mcgpu_in_template`; `run` auto-fills this from `check_template` when a case has `mcgpu_in_template=""` and no explicit `fov_template`, so minimal output never silently loses FOV-anchored placement |
| `check_template` (or `--check-template`) | beam template used to render positioning checks for minimal-output jobs (default: `params/cbct_head_bar_template.in`) |

Per-case (not top-level) input-selection keys, emitted only when present —
for folders where "first series found" / auto acquisition pick isn't the
series the curated mask belongs to:

| key | effect |
|---|---|
| `series_uid` | forces `series_uid` in the cfg (a folder holding two series, e.g. a thin + a gantry-tilted reconstruction under one name) |
| `acquisition_number` | forces `acquisition_number` (bundled duplicate slice positions; see §6) |

After each successful build, `.in` file(s) are used to render positioning
checks if any were written (one per `.in`, so several `mcgpu_in_template`
entries produce several PNGs); otherwise (minimal output) one PNG is
rendered from the `.txt` + `check_template` instead. Either way, PNGs land in
`positioning_checks/` (see §8).

## 10. Practical tips

- **Disk space.** Output volumes are large (a 640×640×250mm volume at
  0.3mm is several GB). If redirecting `output_prefix` to a shared/limited
  drive, clean up `.raw`/DICOM volumes once their positioning-check PNGs are
  rendered and reviewed — the PNGs are the lasting artifact, the big volumes
  are regenerable from the `.cfg`.
- **⚠ GANTRY TILT IS IGNORED — sheared phantoms, silently.** The binary builds
  voxel positions as `(index + 0.5) * spacing` and never reads
  `ImageOrientationPatient` (tag 0020|0037). Head CT is routinely acquired with
  a tilted gantry: the slice planes are then rotated about X while the table
  still advances along patient +Z, so treating the stack as axial **shears the
  anatomy** by `tan(tilt)` of Y per unit Z — the head comes out visibly
  elongated/skewed in a sagittal view. Nothing warns you; the DICOM is
  perfectly valid and every computed offset looks plausible.
  Two giveaways, both cheap to check before a batch:
  - `GantryDetectorTilt` (0018|1120) is non-zero;
  - the reported slice spacing is `nominal / cos(tilt)` rather than the nominal
    value (e.g. 0.625 mm nominal → 0.684 mm at 24°), i.e. an oddly precise
    non-round spacing.
  Measured on the CQ500 TEETH batch: **26 of 66 series were tilted, 4°–24°**,
  giving up to 80 mm of skew across a 180 mm head. Until the binary honours the
  direction cosines, either exclude tilted series or resample them onto an
  axis-aligned grid first (SimpleITK `Resample` with the identity direction).
- **`head_at_max_z` is a config flag, not orientation detection.** Basic
  HU-threshold head detection, not patient-orientation inference — if a
  series is scanned feet-first (or otherwise Z-reversed relative to what's
  expected), flip this flag rather than expecting it to be auto-detected.
- **`denoise` runs before head detection.** If both are enabled, the median
  filter + despeckle pass completes first, so head detection isn't fooled by
  scan noise or small artifacts — no need to denoise separately beforehand.
- **`mask_nrrd` does NOT run before head detection.** The mask is applied
  during the resampling loop, so head detection sees the raw HU volume —
  including the CT table / head cradle, which real-patient series contain and
  the mask exists to remove. Consequence: the detected AP half-depth is
  inflated by roughly the table's own depth behind the head, and since the
  head is XY-centred on the same bbox, the stretcher ends up seated that far
  behind the (masked, table-free) occiput instead of `stretcher_gap_mm`.
  Measured: ~20–40 mm on the CQ500 batch, ~80 mm on the GradientHealth batch.
  Compensate per case with a negative `stretcher_cy_offset_mm` if a tight
  head-to-stretcher contact matters for the study.
- Start with one case and inspect its positioning-check PNG before launching
  a full batch — the same mixup-prone step (placement) is exactly what
  silently produces a physically-wrong-but-non-crashing volume otherwise.

## 11. Publishing phantoms and reclaiming disk

`upload_phantoms_to_gcp.sh` compresses each `.raw` into its own `.tar.xz`
(raw only — no `.txt`, no `.in`) and uploads it to
`gs://mcgpu-data-gcp/phantom/`. Default behaviour is unchanged: every `*.raw`
directly inside each given directory.

```bash
# whole folder (the original behaviour)
./upload_phantoms_to_gcp.sh /mnt/h/MICHELE_MCGPU/QUREAI_TEETH_VOLUME_EXPORT

# only the phantoms named in a list, then reclaim their disk space
./upload_phantoms_to_gcp.sh --list clean.txt --reclaim <dir>

# reclaim after an earlier upload (no compression, no upload)
./upload_phantoms_to_gcp.sh --list clean.txt --reclaim-only <dir>

# see what either would do, touching nothing
./upload_phantoms_to_gcp.sh --list clean.txt --reclaim --dry-run <dir>
```

| flag | effect |
|---|---|
| `--list FILE` | process only the `.raw` names in FILE (one per line, bare names resolved against `<dir>`, `#` comments ok) |
| `--reclaim` | after a successful upload, verify recoverability and DELETE the local `.raw` |
| `--reclaim-only` | skip compress+upload; only verify and delete (for an earlier batch) |
| `--dry-run` | report only |

**Why `--list` exists:** a batch folder routinely holds a mix of phantoms you
want published and phantoms you don't — the CQ500 TEETH batch had 26 of 66
gantry-sheared (§10), which must not be published. Globbing the folder would
have uploaded them.

**Deletion is gated on proof, never on the upload's exit code.** A `.raw` is
removed only when all three hold: the local `.tar.xz` exists; its md5 equals
the md5 GCS reports for the uploaded object (so the bucket copy is
byte-identical to the local archive); and `xz -t` passes. That guarantees two
independent copies before anything is deleted. Anything failing a check is
skipped and reported, never deleted. Local archives are always kept — they are
small (~4 MB for a 3.8 GB volume) and are the fast restore path; a volume is
also regenerable from its `params/generated*/*.cfg`.

**When verifying by hand, don't discard gsutil's stderr.** `gsutil ls
gs://... 2>/dev/null | wc -l` reports 0 both for "bucket empty" and for
"credentials expired" — which reads as a failed upload when the upload was
fine. Let stderr through, or check the exit code.

## 12. Related tools

- `expand_in_kv.py` — `parse_in_geometry` / `render_positioning_check`, the
  general .in + label-volume → sagittal-PNG renderer used above (also
  supports expanding a batch of `.in` files into numbered kV variants).
- `nrrd_to_dicom_series.py` — converts a single-file volume (`.nrrd`/`.nii`/
  `.mha`) into a CT DICOM series so `dicom_to_mcgpu` (which only reads DICOM
  series) can take it. Geometry is preserved exactly and HU round-trips
  bit-exactly (signed int16, no rescale), so a companion mask NRRD in the same
  physical space still aligns; `--manifest`/`--out-root` converts every
  non-DICOM entry of a dataset manifest in one go, each verified by read-back.
- `make_qureai_jobs.py` — builds a `batch_dicom_to_mcgpu.py` jobs JSON from
  the CQ500/qureai-headct `dataset_manifest.json` (image + mask are listed
  there, so no folder-tree `scan` is needed), including automatic
  `series_uid` disambiguation when a folder bundles several series.
  `--kind teeth|single_series|stitched` builds one batch per curated group
  (give each its own `--output-dir`/`--in-dir`/`--cfg-dir`/`--png-dir` so a new
  batch never lands among already-reviewed files), and any per-patient `flags`
  in the manifest are printed at generation time — e.g. `partial_no_head_top`,
  which means the vertex is cropped, so `auto_head_placement` anchors the FOV
  on a flat cut surface instead of real anatomy.
- `mcrp_to_vox` / `README_mcrp_to_vox.md` — voxelizes an MRCP tetrahedral
  mesh phantom into a tight-crop DICOM series that can be fed into
  `dicom_to_mcgpu` as if it were a real patient (this is what
  `auto_head_placement` was generalized to support, alongside real patients).
- `PLAN_dicom_to_mcgpu_auto_placement.md` /
  `PROGRESS_dicom_to_mcgpu_auto_placement.md` — design rationale and
  validation history for §4/§5/§9's automatic-placement features.
