# mcrp_to_vox — How-To Guide

Voxelizes an MRCP tetrahedral mesh phantom (ICRP) into an MC-GPU–compatible
uint8 label volume, using the same output conventions as the production
`dicom_to_mcgpu` tool.

**Inputs**
- Tetrahedral phantom: `./phantoms/<phantom_name>.{node,ele,material}`
- Material map: `./data/mcgpu_mcrp_materials/<phantom_name>/`

**Main output** — a label `.raw` + companion `.txt` + MC-GPU `.in` file(s):

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

Labels 0–4 come from HU thresholding of the voxelized phantom materials
(μ computed at 60 keV via Geant4, converted to HU).

---

## 1. Build

```bash
cmake --build build --target mcrp_to_vox -j$(nproc)
```

## 2. Run

Always run **from the repo root** (paths `./phantoms` and `./data` are relative):

```bash
./run_mcrp_to_vox.sh params/AF_head.cfg        # via helper script
./build/mcrp_to_vox params/AF_head.cfg         # or call the binary directly
```

The binary takes one argument: a `.cfg` parameter file.
(Legacy positional arguments also exist, but the cfg file is the recommended way.)

## 3. Minimal config example

Save as e.g. `params/my_run.cfg`:

```ini
# ── Phantom & resolution ─────────────────────────────────────────
phantom_name   = MRCP_AF        # must exist in ./phantoms and ./data/mcgpu_mcrp_materials
voxel_size_mm  = 1.0

# ── ROI [mm from phantom bounding-box corner]; -1 = full extent ──
x_start_mm = 0.0
x_end_mm   = -1
y_start_mm = 0.0
y_end_mm   = -1
z_start_mm = 1412.0             # e.g. head only
z_end_mm   = -1

# ── Stretcher (optional) ─────────────────────────────────────────
stretcher_enable = true
stretcher_cx_mm  = 0.0          # polygon centre, X from isocenter [mm]
stretcher_cy_mm  = 133.5        # polygon centre, Y from isocenter [mm]
```

Run it:

```bash
./run_mcrp_to_vox.sh params/my_run.cfg
```

## 4. Full config reference

```ini
# ── Phantom ──────────────────────────────────────────────────────
phantom_name   = MRCP_AF
voxel_size_mm  = 0.5

# ── ROI [mm from bbMin corner]; -1 = full extent ─────────────────
x_start_mm / x_end_mm
y_start_mm / y_end_mm
z_start_mm / z_end_mm

# ── HU thresholds [HU] (defaults shown) ──────────────────────────
thr_air_fat        = -500
thr_fat_soft       =  -50
thr_soft_spongiosa =  200
thr_spongiosa_cort =  800

# ── Geometry fine-offset [mm] (added to the .txt/.in offset only) ─
shift_x_mm = 0.0
shift_y_mm = 0.0
shift_z_mm = 0.0

# ── Implant cylinders (optional; axis along Z) ────────────────────
# Coordinates relative to the isocenter (= centre of the ROI) [mm].
implant_count        = 1
implant_0_cx_mm      = -24.0
implant_0_cy_mm      = -35.0
implant_0_cz_mm      = -55.25
implant_0_radius_mm  =   2.5
implant_0_height_mm  =  22.0
# ... implant_1_*, implant_2_*, ...

# ── Crop cylinder (optional; zeros everything outside) ────────────
crop_cylinder_enable    = false
crop_cylinder_radius_mm = 86.0
crop_cylinder_cx_mm     = 254.0     # absolute coords from bbMin corner
crop_cylinder_cy_mm     = 86.0

# ── Stretcher shell (optional; full Z extent) ─────────────────────
# Same rounded-polygon profile as dicom_to_mcgpu: 440/396 mm trapezoid,
# 57 mm high, 1.5 mm carbon wall (10) around a foam core (11).
# cx/cy: polygon centre from isocenter [mm]. The volume is auto-padded
# if the stretcher falls outside the ROI (shift is updated accordingly).
stretcher_enable = true
stretcher_cx_mm  = 0.0
stretcher_cy_mm  = 133.5

# ── MC-GPU .in generation (optional) ─────────────────────────────
# One or more templates separated by commas; one .in is written per
# template. Omit to skip .in generation entirely.
mcgpu_in_template  = params/cbct_head_bar_template.in, params/cbct_bar_only_template.in
mcgpu_output_name  = results/my_run/
mcgpu_det_nx       = 512
mcgpu_det_nz       = 512
```

Lines starting with `#` are comments; everything after a `#` on a line is ignored.

## 5. Outputs

Written to `./output/<phantom>_<cfgname>_vox_<Nx>x<Ny>x<Nz>/`
(e.g. `output/MRCP_AF_AF_head_vox_374x422x437/`):

| File | Description |
|------|-------------|
| `<cfgname>_vox_5labels_<dims>.raw` | label volume (no extras) |
| `<cfgname>_vox_implant_stretcher_labels_<dims>.raw` | label volume with extras (name reflects enabled features) |
| `<same>.txt` | MC-GPU geometry companion (offset/dims/voxel size) |
| `CBCT.in` / `CBCT_<template-stem>.in` | one per template, if `mcgpu_in_template` set |
| `..._mu60keV_<dims>.raw` | float32 μ map @ 60 keV [cm⁻¹] |
| `..._HU60keV_<dims>.raw` | int16 HU map @ 60 keV |
| `..._HU60keV_dicom_<dims>/` | HU map exported as a DICOM CT series (QA) |
| `..._mcgpu_section.txt` | `[SECTION MATERIAL FILE LIST]` sidecar from the material map |

**Raw format:** uint8, Z-major order (`index = k*Ny*Nx + j*Nx + i`),
dimensions in the filename. Read e.g. in Python:

```python
import numpy as np
nx, ny, nz = 374, 422, 437   # from the file name
vol = np.fromfile("output/MRCP_AF_AF_head_vox_374x422x437/AF_head_vox_5labels_374x422x437.raw",
                  dtype=np.uint8).reshape(nz, ny, nx)
```

## 6. Practical tips

- **Start coarse.** Voxelization tests every tetrahedron against the ROI grid;
  halving the voxel size multiplies runtime by ~8. Do a 5 mm / small-slab test
  first (see `params/smoke_stretcher.cfg`), inspect the result, then launch the
  fine full run.
- **Isocenter convention.** Implant and stretcher coordinates are relative to
  the isocenter = centre of the selected ROI (same convention as
  `dicom_to_mcgpu` and the stretcher-fitting tools in `barella/`).
  +Y points toward the patient's back (posterior) — the stretcher sits at
  positive `cy`, anterior structures (e.g. dental implants) at negative `cy`.
- **Coordinate cross-check.** The exported HU DICOM series can be opened in any
  viewer (3D Slicer, ITK-SNAP) to verify ROI placement, implants and the
  stretcher before running MC-GPU.
- Ready-made configs: `params/AF_head.cfg`, `params/AF_head_neck.cfg`,
  `params/AF_full.cfg`.

## 7. Related tools

- `mcrp_to_mcgpu_materials` — generates the per-organ `.mcgpu` material files
  and `MC-GPU_material_config.txt` referenced by the `.in` templates
  (run it once per phantom, output lands in `data/mcgpu_mcrp_materials/`).
- `dicom_to_mcgpu` — the production DICOM→MC-GPU converter whose output
  conventions this tool mirrors.
