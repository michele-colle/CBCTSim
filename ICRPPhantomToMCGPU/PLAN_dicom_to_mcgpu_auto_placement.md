# `dicom_to_mcgpu`: automatic head/FOV placement + stretcher-position augmentation

## Context

`HANDOUT_mcrp_dicom_stretcher_augmentation.md` is **background only** — it
documents a prior session's fix (head placed above the CBCT beam's
reconstructed FOV, ~64 mm invisible to any projection) inside `mcrp_to_vox`,
and the reasoning that motivates wanting the same capability in
`dicom_to_mcgpu`. **`mcrp_to_vox` itself is out of scope for this plan** —
nothing here touches `mcrp_to_vox.cpp` or `batch_mcrp_to_vox.py`. The scope is
`dicom_to_mcgpu.cpp` and `batch_dicom_to_mcgpu.py` only: give `dicom_to_mcgpu`
its own automatic head-detection + FOV placement + stretcher auto-seat, and
make stretcher position a batchable augmentation axis, so it works on *any*
DICOM `dicom_to_mcgpu` is pointed at (real patient, or a DICOM series
exported by some other tool) — not a feature specific to one upstream
producer.

`dicom_to_mcgpu` currently places any DICOM series via **fully manual**
offsets (`dicom_corner_x/y/z_mm` cfg keys → `dicom_to_mcgpu.cpp:591-593,
684-686, 970-972`) — a human looks at a DICOM viewer and tunes
`dicom_corner_z_mm` by hand. There is no automatic head-detection or
FOV-anchoring today. `dicom_to_mcgpu` also already has a stretcher overlay
(`dicom_to_mcgpu.cpp:997-1058`) but it requires manually-measured absolute
`stretcher_cx_mm`/`stretcher_cy_mm` — there's no notion of "behind the head,
plus an augmentation offset."

## Decisions made this session

1. **Placement mechanism**: basic HU-threshold head detection, run directly
   on the DICOM volume `dicom_to_mcgpu` already loads. Threshold is a cfg key
   (default -500 HU, matching the existing `thr_air_fat` convention). Works
   on any head DICOM — no assumption about where it came from.
2. **Noise robustness**: reuse the noise-reduction pipeline already built
   into `dicom_to_mcgpu` for label segmentation — the median filter
   (`median_radius`, `dicom_to_mcgpu.cpp:860-875`) and the connected-component
   despeckle (`despeckleHuVolume`, `dicom_to_mcgpu.cpp:300-463`, invoked at
   `897-913`). Head detection must run **after** these (when `denoise=true`),
   on the same cleaned `huImage` buffer, so it isn't fooled by scan noise or
   small artifacts the segmentation path already knows how to remove — no new
   denoising code, just correct sequencing.
3. **No-stretcher augmentation**: material-swap trick only (handout §3 — edit
   `[SECTION MATERIAL FILE LIST]` on an already-built `.in` so carbon/foam
   read as air). No batch JSON schema change for this.
4. **Stretcher auto-seat**: driven by the *same* head-detection pass — the
   detected head sets a **minimum/limit** for the stretcher's Y position
   (must sit behind the head's measured posterior surface, `+Y` = posterior,
   existing convention). The augmentation axis is an **offset** added on top
   of that limit — varying the offset is what generates distinct
   stretcher-position variants for one placed volume.

## Design: `dicom_to_mcgpu.cpp`

New cfg keys, all **opt-in** (default off/unset so every existing
real-patient cfg — e.g. `params/dicom_to_mcgpu_template.cfg` — keeps working
unchanged):

| key | default | meaning |
|---|---|---|
| `auto_head_placement` | `false` | master switch for auto Z/XY placement |
| `head_detect_thr_hu` | `-500` | HU threshold, tissue vs. air (matches `thr_air_fat`) |
| `head_at_max_z` | `true` | which end of the loaded volume's own Z-index is "toward the head" (see orientation note below) |
| `head_region_mm` | `100.0` | top slab depth used for XY centering + AP-depth measurement |
| `head_top_margin_mm` | `10.0` | air gap kept between head tip and FOV top |
| `stretcher_auto` | `false` | when true, stretcher Y becomes limit + offset instead of absolute |
| `stretcher_cy_offset_mm` | `0.0` | augmentation knob, added on top of the auto-computed limit |
| `stretcher_gap_mm` | `2.0` | gap between head posterior surface and stretcher shell |

(`stretcher_cx_mm` is unaffected — already user-settable, no auto-component
needed; `stretcher_cy_mm` keeps its current absolute meaning when
`stretcher_auto=false`, so nothing breaks for existing real-patient cfgs.)

Implementation:

1. **Port the beam-FOV parser.** `parseBeamFOVHalfZ_mm` +
   `inSectionValues` don't exist in this file yet — add them (same
   `SECTION SOURCE` / `SECTION IMAGE DETECTOR` parsing convention
   `expand_in_kv.py`'s `parse_in_geometry` already uses). This file already
   parses `mcgpu_in_template` later for `.in`-writing, so the template path
   is available at the point placement is computed; reuse the first template
   in the (possibly `,`/`;`-separated) list, same as the existing `.in`-writing
   loop does.
   `FOV_halfZ = (detector_height_cm / 2) * (SAD_cm / SDD_cm)`.

2. **Head-detection pass**, run on `huImage`'s buffer (native DICOM
   resolution, *after* the existing median-filter/despeckle stages if
   `denoise=true`, *before* the trilinear resample into the output grid —
   `dicom_to_mcgpu.cpp:855-941` is roughly where this slots in, right after
   the "DICOM phys bbox" logging and before §3 "Compute output volume
   geometry"):
   - Scan slices from the `head_at_max_z`-designated end inward; for each
     slice, count voxels ≥ `head_detect_thr_hu`. The first slice whose count
     exceeds a small minimum-area floor (rejects couch/immobilization noise)
     is the head-tip slice.
   - Within the top `head_region_mm` slab from that tip, compute the XY
     bounding box of thresholded voxels — gives the XY centering target and
     the AP half-depth (Y extent) needed for the stretcher limit.
   - This is a straightforward O(N) full-volume scan, architecturally
     consistent with the existing `despeckleHuVolume` full-volume OpenMP
     passes — should be parallelized the same way (`#pragma omp parallel for`
     with reductions), not a serial add-on.

3. **Convert detection → placement**, when `auto_head_placement=true`:
   override `dicom_center_offset_{x,y,z}_mm` (computed in DICOM-local mm,
   then converted to the corner offset the existing code already derives at
   `dicom_to_mcgpu.cpp:970-972`) so that:
   - XY: the detected head slab's bbox centre lands at isocenter.
   - Z: the detected head tip lands at `FOV_halfZ_mm - head_top_margin_mm`
     from isocenter.
   Log clearly whenever auto values override a manually-set offset, so a
   user who sets both doesn't get silently surprised.

4. **Stretcher auto-seat**, when `stretcher_auto=true`: compute
   `cy_limit = halfDepthY + stretcher_gap_mm + ht` (`ht` = stretcher shell
   half-height = 28.5 mm, from the existing hardcoded profile at
   `dicom_to_mcgpu.cpp:1002-1011`) using the detected AP half-depth from step
   2, then `stretcher.cy_mm = cy_limit + stretcher_cy_offset_mm` before the
   existing stretcher-mask computation (`dicom_to_mcgpu.cpp:1012-1040`) runs
   — no changes needed to the mask computation itself, only to how
   `stretcher.cy_mm` is populated beforehand.

**Orientation note:** DICOM series don't have a universally-agreed "which end
is the head" — this varies by acquisition/vendor. `head_at_max_z` is a
config flag, not auto-detected orientation logic (out of scope: the ask was
for "a very basic head recognition technique", not full patient-orientation
inference). Document the assumption clearly in the cfg template; a user
feeding in a feet-first-ordered series just flips the flag.

## Design: `batch_dicom_to_mcgpu.py`

1. **Port positioning-check integration** (confirmed zero integration today
   — grepped for `render_positioning_check`/`expand_in_kv`, no hits). In
   `cmd_run`, also capture `"MC-GPU .in file written:"` from the subprocess
   stdout (currently only `RAW_WRITTEN_MARKER` is captured/used), then call
   a `render_check` helper (same pattern already proven in the sibling
   `mcrp_to_vox` batcher — reuses `expand_in_kv.parse_in_geometry` /
   `render_positioning_check`, which are already fully generic and need no
   changes). Write PNGs to `positioning_checks/`.
2. **Generalize the stretcher-slot schema** to support the new offset-based
   augmentation mode alongside the existing absolute mode: a per-case
   `stretchers` entry gains optional `auto: true` + `cy_offset_mm` (mapped to
   `stretcher_auto`/`stretcher_cy_offset_mm` overrides) as an alternative to
   the current `cx_mm`/`cy_mm` absolute pair. Add `auto_head_placement` as a
   case-level (or global-default) toggle plumbed the same way `binary`/
   `template`/`cwd` already are. Existing JSON files that only use
   `cx_mm`/`cy_mm` continue to work unchanged (absolute mode stays the
   default when `auto` isn't set).
3. `expand_in_kv.py`: no changes needed (already generic).

## Build / verification order

1. Add the beam-FOV parser + head-detection pass to `dicom_to_mcgpu.cpp`
   behind `auto_head_placement` (default off) — build, confirm it compiles
   and existing manual-offset cfgs are bit-for-bit unaffected (regression:
   run one existing real-patient cfg before/after, diff the `.raw`/`.in`
   output).
2. Point `auto_head_placement=true` at a real head DICOM series, inspect a
   positioning-check PNG (once wired, or manually via
   `expand_in_kv.render_positioning_check` in the interim) to confirm the
   head lands correctly relative to the FOV — no ground-truth number to
   regress against yet since this is new to `dicom_to_mcgpu`, so this is a
   visual sanity check, not a numeric regression.
3. Add `stretcher_auto`/`stretcher_cy_offset_mm`; verify visually the same
   way — shell seats behind the detected head, offset shifts it further back.
4. Wire the batch script changes; run one case through the new schema with
   2-3 stretcher offsets, inspect all resulting positioning-check PNGs.

## Files touched

- `dicom_to_mcgpu.cpp` — add beam-FOV parser, head detection, auto
  placement, stretcher auto-seat + offset.
- `batch_dicom_to_mcgpu.py` — port positioning checks, extend stretcher-slot
  schema for offset-based augmentation.
- `params/dicom_to_mcgpu_template.cfg` — document new keys (all commented
  out / off by default).
