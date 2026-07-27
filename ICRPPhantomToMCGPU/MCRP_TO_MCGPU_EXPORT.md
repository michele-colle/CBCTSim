# Exporting MC-GPU volumes from MRCP mesh phantoms

Recap of the production pipeline that turns an ICRP/MRCP tetrahedral mesh
phantom into an MC-GPU–ready voxel volume: fixed output geometry, head-tip
placement anchored to the CBCT beam's field of view, an auto-seated stretcher,
and a batcher that runs every phantom in `phantoms/` and drops a visual
positioning check for each one.

This is the **production / batch** workflow. For the older single-phantom,
ROI-based workflow (`x_start_mm`/`x_end_mm`, manual stretcher placement) see
`README_mcrp_to_vox.md` — both are handled by the same `mcrp_to_vox` binary,
selected by which cfg keys are set.

> **A third mode now exists**: `tight_crop_dicom = true` — a lean DICOM-only
> export (no `.in`/`.raw`, no overlays) sized to each phantom's own captured
> anatomy instead of a big fixed volume. It's the export half of a bigger
> planned change (retire `.in` generation here entirely; let `dicom_to_mcgpu`
> own placement + stretcher). See `params/mcrp_to_dicom_template.cfg`,
> `batch_mcrp_to_dicom.py`, and
> `HANDOUT_mcrp_dicom_stretcher_augmentation.md` for the full plan and status.
> Everything below this point still describes the original fixed-volume mode,
> unchanged and still fully functional.

## Pipeline at a glance

```
phantoms/<name>.{node,ele,material}   (ICRP/MRCP tetrahedral mesh, per age/sex)
        │
        ▼
mcrp_to_mcgpu_materials                 (once per phantom)
        │  -> data/mcgpu_mcrp_materials/<name>/MC-GPU_material_config.txt
        │     + Penelope .mat / MC-GPU .mcgpu material files
        ▼
mcrp_to_vox  (fixed-volume mode)        (once per phantom, via the batcher)
        │  -> output/<name>_vox_<Nx>x<Ny>x<Nz>/
        │        <name>_vox_..._<dims>.raw   (uint8 5-label volume)
        │        <same>.txt                  (MC-GPU geometry companion)
        │        CBCT.in                     (from mcgpu_in_template)
        ▼
batch_mcrp_to_vox.py                    (drives step 3 for every phantom)
        │  -> params/generated_mcrp/<name>.cfg
        │  -> positioning_checks/<name>_CBCT.png   (sagittal QA render)
        ▼
MC-GPU  (external)  — consumes the .raw + .in from output/
```

## 1. Prerequisite: material maps

`mcrp_to_vox` only needs the **sequential label→material config**, not a
per-organ voxel volume. Generate it once per phantom:

```bash
cmake --build build --target mcrp_to_mcgpu_materials -j$(nproc)
./run_all_mcgpu_materials.sh          # loops every phantom in phantoms/
```

This writes `data/mcgpu_mcrp_materials/<phantom>/MC-GPU_material_config.txt`
(and the underlying `.mat`/`.mcgpu` files). `batch_mcrp_to_vox.py` skips any
phantom missing this file, with a warning.

**Note on materials vs. geometry:** the `.in` template's
`[SECTION MATERIAL FILE LIST]` (e.g. `params/cbct_head_bar_template.in`) is
copied **verbatim** for every phantom — it isn't regenerated per phantom even
though per-phantom material densities differ (e.g. cortical bone density is
~1.5 for a newborn vs. ~1.9 for the adult female phantom). This pipeline
optimizes for correct **geometry** (placement, FOV, stretcher); some voxels
may end up bucketed as spongiosa vs. cortical slightly differently than a
fully phantom-specific material spectrum would give. Not a concern for
positioning/geometry validation.

## 2. `mcrp_to_vox` — fixed-volume (production) mode

Triggered automatically when `vol_width_mm`, `vol_height_mm`, `vol_length_mm`
are all set in the cfg (as opposed to the legacy `x_start_mm`/`x_end_mm`/...
ROI mode). Mirrors `dicom_to_mcgpu`'s output conventions.

**Coordinate system:** origin = isocenter = centre of the output volume, same
as `dicom_to_mcgpu`. Voxel `[i,j,k]` centre = `(idx+0.5)*voxelSize - N*voxelSize/2`.

**What it does, in order:**

1. **Fixed output volume.** One volume size for every phantom regardless of
   age (default 640×640×300 mm @ 0.3 mm ≈ 2134×2134×1000 voxels, ~4.5 GB).
   Newborn vs. adult phantoms differ only in how much real anatomy fills the
   volume below the head — the rest is air.
2. **XY centring.** The top `head_region_mm` (default 100 mm) slab of the
   phantom is scanned for its XY bounding box; its centre is placed at the
   isocenter, so the head sits centred like a real head CBCT (not dragged
   off-centre by the shoulders).
3. **Z placement — anchored to the beam's FOV, not the volume's top face.**
   The head tip (`bbMax.z()`) is pulled down until it sits `head_top_margin_mm`
   (default 10 mm) **inside the CBCT beam's reconstructed field of view**,
   leaving air above the crown. The FOV half-height is derived from the first
   `mcgpu_in_template`'s beam geometry:
   ```
   FOV_halfZ = (detector_height_cm / 2) * (SAD_cm / SDD_cm)
   ```
   parsed straight out of the `.in`'s `SECTION SOURCE` (source position →
   SAD) and `SECTION IMAGE DETECTOR` (image size, SDD) blocks — the exact
   fields `expand_in_kv.py`'s positioning-check renderer also reads. Falls
   back to `volume_half_height - head_top_margin_mm` if no template is given
   or its geometry can't be parsed.

   *Why this matters:* with a 300 mm volume and the production beam geometry
   (SAD 57.8 cm, SDD 98.7 cm, 29.34 cm detector → FOV half-height ≈ 86 mm),
   anchoring to the volume's top face put ~64 mm of head **above** the
   reconstructed FOV — invisible to any simulated projection. Anchoring to
   the FOV instead keeps the whole head (or "almost", per the margin) inside
   the beam.
4. **Lean voxelization.** A per-material HU look-up table (µ @ 60 keV via
   Geant4 → HU → threshold) is built once; each tetrahedron writes its label
   directly into a single `uint8` output buffer. No full µ/HU/DICOM volumes
   are held in memory (~4.5 GB, not ~40 GB) unless `write_dicom = true`.
5. **Overlays**, all in isocenter-relative mm (no volume padding needed —
   the fixed volume is already large enough):
   - **Implant cylinders** (label 5) — `implant_count` + `implant_<n>_*`.
   - **Crop cylinder** (zeros everything outside) — `crop_cylinder_*`.
   - **Stretcher shell** (carbon wall = 10, foam core = 11) — same rounded
     trapezoid profile as `dicom_to_mcgpu` (440/396 mm wide, 57 mm high,
     chamfered top corners, 5 mm rounding, 1.5 mm wall). With
     `stretcher_auto = true` (default) the shell is seated automatically
     just behind the head's posterior (**+Y**) surface, `stretcher_gap_mm`
     below it — the consolidated AM/AF convention — so it tracks head size
     across ages instead of using one fixed `cy` for every phantom.
6. **Outputs**: 5-label `.raw` + `.txt` (penEasy 2008 header) + one `.in`
   per `mcgpu_in_template` (`,`/`;`-separated). Optionally (`write_dicom =
   true`) an int16 HU `.raw` + a DICOM CT series for QA — see §5.

### Key cfg fields (fixed-volume mode)

| Key | Default | Meaning |
|---|---|---|
| `phantom_name` | — | must exist in `phantoms/` and have a material map |
| `output_dir` | `./output` | base folder every output subfolder is written under; quote if it contains spaces |
| `vol_vxy_mm` / `vol_vz_mm` | — | output voxel size, XY / Z [mm] |
| `vol_width_mm` / `vol_height_mm` / `vol_length_mm` | — | fixed output extent [mm]; setting all three enables this mode |
| `head_region_mm` | 100.0 | top slab used for XY centring + stretcher auto-seat |
| `head_top_margin_mm` | 10.0 | air gap kept between the head tip and the FOV top edge |
| `thr_air_fat` / `thr_fat_soft` / `thr_soft_spongiosa` / `thr_spongiosa_cort` | -500 / -50 / 200 / 800 | HU thresholds → labels 0–4 |
| `stretcher_enable` | false | turn the shell on |
| `stretcher_auto` | true | auto-seat behind the head vs. manual `stretcher_cy_mm` |
| `stretcher_cx_mm` | 0.0 | lateral offset of the shell centre |
| `stretcher_gap_mm` | 2.0 | gap between head posterior surface and shell (auto mode) |
| `crop_cylinder_*` | disabled | isocenter-relative crop cylinder |
| `implant_count`, `implant_<n>_*` | 0 | isocenter-relative implant cylinders |
| `mcgpu_in_template` | — | one or more `.in` templates (comma/`;`-separated); material list copied verbatim |
| `mcgpu_output_name`, `mcgpu_det_nx`, `mcgpu_det_nz` | — / 512 / 512 | written into each `.in`'s detector section |
| `write_dicom` | false | also emit an int16 HU `.raw` + DICOM series (QA; ~2× memory/disk) |
| `shift_x_mm` / `shift_y_mm` / `shift_z_mm` | 0.0 | fine offset added only to the `.txt`/`.in` geometry offset |

Full production template: **`params/mcrp_to_vox_template.cfg`**.

## 3. `batch_mcrp_to_vox.py` — batch driver

Runs `mcrp_to_vox` for every phantom found in `phantoms/` (or a subset),
writing one generated `.cfg` per phantom and rendering a positioning check
after each successful build.

```bash
# preview only — writes params/generated_mcrp/<phantom>.cfg, no run
python3 batch_mcrp_to_vox.py --dry-run

# build everything
python3 batch_mcrp_to_vox.py

# one or a few phantoms
python3 batch_mcrp_to_vox.py --only MRCP-15F MRCP_AM

# skip the PNG rendering step
python3 batch_mcrp_to_vox.py --no-checks

# redirect every phantom's output onto a different drive/folder
python3 batch_mcrp_to_vox.py --output-dir /mnt/f/Michele_diskF/mcrp_mcgpu_output
```

Flags: `--template` (default `params/mcrp_to_vox_template.cfg`), `--binary`
(default `build/mcrp_to_vox`), `--phantoms-dir`, `--material-dir`,
`--output-dir` (overrides `output_dir` for every phantom; unset = whatever the
template sets, `./output` by default),
`--cfg-out-dir` (default `params/generated_mcrp`), `--png-dir` (default
`positioning_checks`), `--stop-on-error`.

Per phantom the batcher overrides only `phantom_name` and
`mcgpu_output_name` — everything else (volume size, thresholds, stretcher,
template) comes from the shared template, so all phantoms get identical
geometry conventions. It skips phantoms without a material map (§1), and
writes `params/generated_mcrp/exported_raw.txt` listing every `.raw` produced.

## 4. Positioning checks

After each phantom builds, the batcher renders a **central-sagittal PNG**
into `positioning_checks/<phantom>_<in-stem>.png`: the label volume's
mid-sagittal slice (skull, soft tissue, stretcher) with the CBCT source,
beam edges, and detector overlaid at projection angle 0 — the same renderer
`expand_in_kv.py --render-checks` uses for the DICOM pipeline
(`render_positioning_check`, reused directly, not duplicated).

Use it to eyeball, per phantom:
- head centred left-right and anterior-posterior in the beam,
- the **whole head inside the beam edges**, with a bit of air above the
  crown (not clipped, not floating with excess headroom),
- the stretcher shell sitting directly behind (posterior to) the head,
  scaled to that phantom's head size.

If a check looks wrong, re-run just that phantom after adjusting
`head_top_margin_mm` / `head_region_mm` / `stretcher_gap_mm` in the template
— no need to touch the batcher or the binary.

## 5. QA: inspecting actual HU values

The mesh phantom assigns one discrete µ (hence one HU) per tissue **material**
— it's not a continuous CT. At 60 keV monoenergetic, cortical bone HU runs
much higher than clinical polychromatic CT (~1500–4700 vs. ~1000–2000), with
an empty gap between spongiosa and cortical HU — expected, not a bug, but
worth knowing before tuning `thr_spongiosa_cort`.

To inspect: set `write_dicom = true` for one run. This additionally writes
`<base>_HU60keV_<dims>.raw` (int16) and a DICOM CT series in
`HU60keV_dicom_<dims>/`, openable in any viewer (3D Slicer, ITK-SNAP) —
same overlay HU conventions as `dicom_to_mcgpu` (implant/carbon = 3000 HU,
foam = -100 HU, air/cropped = -1000 HU).

```ini
write_dicom = true    # add to a one-off cfg; ~2x memory & disk vs. label-only
```

## 6. Practical notes

- **Cost at full resolution.** 640×640×300 mm @ 0.3 mm ≈ 2134×2134×1000
  voxels ≈ 4.5 GB per `.raw`; ~55 GB for all 12 phantoms. Voxelization is
  single-phantom-serial (a few minutes each) — the Geant4/µ setup dominates
  short runs.
- **Iterate coarse.** Voxelization cost scales with tetrahedron-count ×
  voxel-count; drop `vol_vxy_mm`/`vol_vz_mm` to ~2 mm for a quick placement
  check (a few seconds) before committing to the full 0.3 mm run.
- **All three production `.in` templates share the same beam geometry**
  (`cbct_head_bar_template.in`, `cbct_head_only_template.in`,
  `cbct_bar_only_template.in`: SAD 57.8 cm, SDD 98.7 cm, 29.34 cm detector),
  so FOV-based head placement is consistent regardless of which one is
  listed first in `mcgpu_in_template`.
- **+Y = posterior** in the MRCP frame (same as `dicom_to_mcgpu` and the
  `barella/` stretcher-fitting tools) — the stretcher sits at positive `cy`,
  anterior structures at negative `cy`. Phantoms are already oriented
  correctly; no flips are needed.
- Legacy single-phantom ROI workflow (manual `x_start_mm`/`z_start_mm`
  slabs, `params/AF_head.cfg` etc.) still works unchanged — it's the other
  code path in the same binary, documented in `README_mcrp_to_vox.md`.

## 7. Related files

| File | Role |
|---|---|
| `mcrp_to_vox.cpp` | voxelizer; legacy ROI mode + fixed-volume production mode |
| `mcrp_to_mcgpu_materials.cpp` | per-phantom material map generator (§1) |
| `params/mcrp_to_vox_template.cfg` | production cfg template (§2) |
| `batch_mcrp_to_vox.py` | batch driver + positioning checks (§3–4) |
| `expand_in_kv.py` | positioning-check renderer (shared with the DICOM pipeline) |
| `dicom_to_mcgpu.cpp` | production DICOM→MC-GPU converter; output conventions mirrored here |
| `README_mcrp_to_vox.md` | legacy single-phantom ROI-mode guide |
