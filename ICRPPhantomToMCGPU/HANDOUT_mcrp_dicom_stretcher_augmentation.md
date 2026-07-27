# Handout: retire `.in`-generation from `mcrp_to_vox`, port head/FOV placement
# into the `dicom_to_mcgpu` batcher, and use it for stretcher-position augmentation

**Purpose of this document:** a self-contained brief for a *dedicated* chat
session that will implement this. It captures the decision made, the reasoning
behind it, and exact code references so nothing from this session's placement
work gets lost or silently reverted.

**Status update (implemented since this doc was written):** the export side
(§2's goal — "mesh → tightly-cropped DICOM") is **done**. `mcrp_to_vox.cpp`
gained a new `tight_crop_dicom = true` mode (additive — the fixed-volume mode
and all placement functions in §2a/2b/2c are untouched, still dormant, still
waiting to be ported into `dicom_to_mcgpu` per this doc) plus
`params/mcrp_to_dicom_template.cfg` and `batch_mcrp_to_dicom.py`. It exports
ONLY a DICOM HU series per phantom, sized to that phantom's own captured
anatomy (head tip down to `vol_length_mm`, clamped to the phantom's own
extent if shorter) plus a flat `air_margin_mm` on every side — no
FOV-anchoring, no head-region XY centring, no overlays, no `.in`/`.raw`/`.txt`.
Verified on MRCP-00F/15F: e.g. 394×243×320 mm actual output vs. the old fixed
640×640×300 mm — most of the wasted air is gone.

**Still open / not started:** everything in §2b (automatic head/FOV placement
inside `dicom_to_mcgpu`), §2c (stretcher auto-seat port, optional), §2d
(positioning-check integration into `batch_dicom_to_mcgpu.py` — confirmed
still has zero coverage), §3/§4 (stretcher-position augmentation batch
loop), and the open questions in §5 (which now most directly concern how
`dicom_to_mcgpu` should place *this* tight-crop DICOM — see revised note in
§5.1 below).

---

## 1. The decision

`mcrp_to_vox`'s fixed-volume ("production") mode currently does three jobs:
1. voxelize the MRCP tetrahedral mesh into a **big, fixed, air-padded** volume
   (640×640×300 mm) with the phantom **placed and centred** inside it,
2. overlay a **stretcher shell** (rounded polygon, carbon/foam) at a computed
   position,
3. write an MC-GPU **`.in` file** referencing that volume.

Going forward, jobs 2 and 3 move to `dicom_to_mcgpu` / its batcher, which
already own mature, tested versions of both (real-patient placement, stretcher
overlay, multi-position batch driving). `mcrp_to_vox` shrinks to **just**:
voxelize the mesh → threshold to 5 labels → export as a **tightly-cropped**
DICOM HU series (minimal surrounding air, not the big fixed volume). That
DICOM is then fed into `dicom_to_mcgpu` as if it were a real patient series.

**Why:** stretcher position is a cheap data-augmentation axis (a 2D polygon
mask over a fixed anatomy), but re-deriving it inside `mcrp_to_vox` means
re-voxelizing the tetrahedral mesh from scratch per position — the expensive
part of the whole pipeline. `dicom_to_mcgpu` resamples/thresholds a DICOM
volume (OpenMP, Z-banded streaming) instead of testing millions of
tetrahedra against a voxel grid — that's the "faster pipeline" for generating
N stretcher-position variants from ONE voxelized anatomy.

## 2. What must NOT get lost

This session solved a real bug (head placed above the beam's reconstructed
FOV, ~64 mm of head invisible to any projection) and built the fix as three
pieces of logic inside `mcrp_to_vox.cpp`'s fixed-volume mode. All three need a
new home — **do not silently drop them** when `mcrp_to_vox` is trimmed down.

### 2a. Beam-FOV parsing (`mcrp_to_vox.cpp:110-165`)

```cpp
static std::vector<std::string> inSectionValues(...)   // mcrp_to_vox.cpp:110
static double parseBeamFOVHalfZ_mm(const std::string& tmplPath)  // mcrp_to_vox.cpp:138
```

Reads an MC-GPU `.in` template's `SECTION SOURCE` (source Y position → SAD)
and `SECTION IMAGE DETECTOR` (detector height, SDD) and returns the
half-height of the reconstructed FOV at isocenter, via similar triangles:

```
FOV_halfZ = (detector_height_cm / 2) * (SAD_cm / SDD_cm)
```

Mirrors the exact section-parsing convention `expand_in_kv.py`'s
`parse_in_geometry` already uses (same marker strings, same "value = text
before first `#`, skip blank/fully-commented lines" rule) — so this is not a
new convention, just a second reader of the same format. All three
production templates (`cbct_head_bar_template.in`, `cbct_head_only_template.in`,
`cbct_bar_only_template.in`) share identical beam geometry (SAD 57.8 cm, SDD
98.7 cm, 29.34 cm detector → FOV half-height ≈ 85.9 mm), so this was
verified against real values, not just theoretically.

### 2b. Head-tip Z-placement (`mcrp_to_vox.cpp:533-591`)

- XY: scan the top `head_region_mm` (default 100 mm) slab of the phantom for
  its XY bounding box; centre that box on the isocenter (`isoPx`/`isoPy`,
  `mcrp_to_vox.cpp:563-564`) — the head sits centred, not dragged off-centre
  by the shoulders.
- Z: **the actual fix.** The head tip (`bbMax.z()`) is anchored at
  `FOV_halfZ_mm - head_top_margin_mm` (default margin 10 mm) from isocenter —
  **not** at the volume's raw top face. `mcrp_to_vox.cpp:566-583`:
  ```cpp
  double headTipTargetZ_mm = halfZ / mm - head_top_margin_mm;   // fallback, no template
  if (!mcgpu_in_template.empty()) {
      const double fovHalfZ_mm = parseBeamFOVHalfZ_mm(firstTmpl);
      if (fovHalfZ_mm > 0.0)
          headTipTargetZ_mm = std::min(headTipTargetZ_mm, fovHalfZ_mm - head_top_margin_mm);
  }
  const double isoPz = bbMax.z() - headTipTargetZ_mm * mm;
  ```
  Verified visually (positioning-check renders, before/after) — anchoring to
  the volume top face left the top ~64 mm of head outside the beam; anchoring
  to the FOV fixed it, leaving a controlled air gap above the crown.

**Where this needs to go now:** `dicom_to_mcgpu.cpp` currently places any
DICOM series via **fully manual** offsets — `dicom_corner_x/y/z_mm` cfg keys
(`dicom_to_mcgpu.cpp:591-593, 684-686`), converted to a corner via
`dicom_corner_{x,y,z}_mm = dicom_center_offset_{x,y,z}_mm - half-extent`
(`dicom_to_mcgpu.cpp:970-972`). There is **no automatic head-detection or
FOV-anchoring** in `dicom_to_mcgpu` today — real patients are placed by a
human tuning `dicom_corner_z_mm` after looking at a DICOM viewer. For
MRCP-derived synthetic phantoms we *can* automate this (we know where the
head is, unlike a real patient scan that might be cropped differently). This
head-FOV-anchoring math needs to be **reimplemented as a new option** in
`dicom_to_mcgpu` (or as a small pre-processing script that computes
`dicom_corner_z_mm`/`dicom_center_offset_z_mm` before invoking it) —
it should NOT be assumed to just "fall out" of centring the tight-crop DICOM,
because centring ≠ FOV-anchoring unless the crop window itself is built with
the same asymmetric margin baked in (see open question in §5).

### 2c. Stretcher auto-seat (`mcrp_to_vox.cpp:606-618, 743-746`)

```cpp
const double halfDepthY = (hy1-hy0)/mm/2.0;   // head AP depth, from the same head-slab bbox
const double str_cy = stretcher_auto ? (halfDepthY + stretcher_gap_mm + ht) : stretcher.cy_mm;
```

Seats the shell's flat top just behind (**+Y = posterior**, same convention
as `dicom_to_mcgpu` and the `barella/` stretcher-fitting tools) the head's
own measured depth, `stretcher_gap_mm` (default 2 mm) further back. This is
what let one stretcher formula work correctly across all 12 phantoms (ages
0–15 + adult M/F) without per-phantom manual tuning — verified: computed
`cy` for MRCP_AF (132.1 mm) landed within 1.4 mm of the previously
hand-fitted production value (133.5 mm) used for the real AM/AF pipeline.

**This is optional to port** — `dicom_to_mcgpu`'s stretcher overlay already
exists and works (`dicom_to_mcgpu.cpp:617, 732-736, 997-1039`); it just
requires manual `stretcher_cx_mm`/`stretcher_cy_mm` per case (reasonable for
real patients, since there's no ground-truth head-depth to compute from).
For the **synthetic MRCP case specifically**, we *do* know the head extent
already (it's mesh data), so adding an MRCP-only "auto" mode to
`dicom_to_mcgpu` (or the pre-processing script that builds its cfg) removes
one manual-tuning step per phantom. Not required for correctness, just
convenience — decide in the dedicated chat whether it's worth doing now or
deferring.

### 2d. Positioning-check integration pattern (`batch_mcrp_to_vox.py:63-77, 200-209`)

**Confirmed: `batch_dicom_to_mcgpu.py` has ZERO positioning-check
integration today** (grepped for `render_positioning_check` / `expand_in_kv`
— no hits). This must be ported, or the new pipeline loses the exact
capability that caught the FOV bug in the first place.

The pattern to copy from `batch_mcrp_to_vox.py`:
```python
def render_check(in_path, png_dir, phantom, cache):
    from expand_in_kv import parse_in_geometry, render_positioning_check
    geom = parse_in_geometry(in_path)
    raw = in_path.parent / geom["raw_name"]
    render_positioning_check(in_path, raw, out_png, geom, cache)
```
Capture the `.in` path from the binary's stdout (marker string
`"MC-GPU .in file written:"`, same line `dicom_to_mcgpu` already prints —
verify the exact marker text is still emitted after any refactor), call
`render_check` after each successful build, write PNGs to
`positioning_checks/`. `render_positioning_check` itself
(`expand_in_kv.py:214`) is a general renderer (label volume + `.in` beam
geometry → sagittal PNG) — it does not need to change at all; only the
*driving* code (which script calls it, and when) needs to move/duplicate
into `batch_dicom_to_mcgpu.py`.

## 3. The stretcher-material-swap trick (already decided, document it)

For the **"no stretcher" augmentation**, the chosen mechanism is: do **not**
re-voxelize or even re-run `dicom_to_mcgpu`. Instead, take an already-built
`.in` (with the stretcher shell geometry baked into the `.raw` at some
position) and edit its `[SECTION MATERIAL FILE LIST]` so `voxelId=10`
(carbon) and `voxelId=11` (foam) both point at the air material instead of
`icrp_c.mcgpu`/`icrp_polyufoam.mcgpu`. The label volume's geometry is
untouched; the shell becomes physically air (invisible to the simulation),
zero recomputation cost.

**Scope of this trick — important:** it only produces the "with vs. without
stretcher **at this exact position**" pair from one `.raw`. It does **not**
help with different stretcher **positions** — moving the shell changes which
voxels carry labels 10/11, which is geometry, not material, so a new
`dicom_to_mcgpu` run (different `stretcher_cx_mm`/`stretcher_cy_mm`) is still
required per position. Position augmentation is what `dicom_to_mcgpu`'s
already-existing multi-case batch loop is for (see §4).

## 4. Existing batch machinery to reuse (already built, just needs a tweak)

`batch_dicom_to_mcgpu.py`'s `run` command already drives *N cases × M
stretcher positions* from one JSON file (`scan` pre-fills 3 slots per case,
labelled A/B/C by default — see `batch_dicom_to_mcgpu.py:53-61, 111-166`).
This is exactly the augmentation-multiplicity mechanism needed; it does not
need to be rebuilt, only extended:

- **Gap found while re-reading it for this handout:** `cmd_run`
  (`batch_dicom_to_mcgpu.py:219`) hardcodes `"stretcher_enable": "true"` for
  every generated job — there is currently no way to express a "no stretcher"
  entry in the per-case `stretchers` list. Either (a) add a sentinel (e.g.
  `{"cx_mm": null}` or a `"enabled": false` field) that `cmd_run` maps to
  `stretcher_enable = false`, or (b) skip this entirely and rely solely on
  the material-swap trick from §3 for the no-stretcher variant, treating it
  as a post-processing step over one already-generated `.in`/`.raw` pair
  rather than a distinct batch entry. Decide which in the dedicated chat —
  (b) is simpler and costs nothing extra, but (a) keeps every variant
  discoverable from the same JSON manifest.

## 5. Open questions for the dedicated chat

1. ~~Where does the "minimal surrounding air" tight-crop logic live?~~
   **Decided and implemented: option (a).** `mcrp_to_vox`'s new
   `tight_crop_dicom` mode (see status update above) crops to the tight
   bounding box of the captured Z-window (head tip down to `vol_length_mm`,
   clamped to the phantom's own extent if shorter) plus a flat
   `air_margin_mm` on every side — **no FOV-awareness at all**, no
   `.in`-template dependency. This is deliberately dumb/minimal so
   `mcrp_to_vox` truly stops caring about beam/MC-GPU templates.

   **This sharpens, rather than answers, the placement question**: since the
   exported DICOM's own centre is just the geometric centre of an arbitrary
   tight crop — not anything FOV-meaningful — `dicom_to_mcgpu`'s default
   **centred** placement (`dicom_corner_offset = 0,0,0`) will **not**
   reproduce the head-near-FOV-top result on its own. Per-phantom placement
   math is still needed somewhere downstream:
   - the crop is *not* symmetric around the head — there's `air_margin_mm`
     above the head tip but up to `vol_length_mm` of anatomy below, so the
     crop's own Z-centre is nowhere near the head:
     `dicom_center_offset_z_mm` (or `dicom_corner_z_mm`) must be computed
     per phantom from **both** the known crop geometry (fixed, from the
     export step: head tip is at `crop_half_Z - air_margin_mm` from the
     crop's own centre) **and** the target FOV half-height (from
     `parseBeamFOVHalfZ_mm`-style logic against the target `.in` template) —
     i.e. this computation now spans knowledge of *both* pipeline stages,
     and has to live in whatever glue script builds each phantom's
     `dicom_to_mcgpu` cfg (not purely inside either binary).
   - XY is simpler — the tight crop is already symmetric around the
     captured-window's own bbox centre (see `isoPx`/`isoPy` in the new mode),
     so centred placement (offset 0,0) should already put the head
     approximately at isocenter in X/Y. Worth confirming visually (a
     positioning check — §2d) rather than assuming.

2. **Does the tight-crop DICOM need the exact same voxel spacing as the final
   MC-GPU volume?** Recommended: yes. `dicom_to_mcgpu` resamples via
   trilinear interpolation (`dicom_to_mcgpu.cpp` main compute loop) — if the
   intermediate DICOM's spacing differs from the final `vol_vxy_mm`/
   `vol_vz_mm`, interpolation will blur the (currently discrete,
   one-HU-per-tissue) material boundaries baked in by `mcrp_to_vox`, subtly
   shifting label buckets between the mesh-native and DICOM-resampled paths.
   Exporting at the same spacing target from the start avoids this.

3. **Is `HUDicomExporter`'s output actually readable by `dicom_to_mcgpu`'s
   series reader as-is?** `mcrp_to_vox` already produces a DICOM series via
   `write_dicom = true` (`mcrp_to_vox.cpp:635-801`, uses
   `HUDicomExporter::Write`) for QA in 3D Slicer/ITK-SNAP — this is the part
   being **kept and made central**, not new work. But it's only been
   *visually* verified in an external viewer so far, never fed back into
   `dicom_to_mcgpu`'s own `itk::GDCMSeriesFileNames`/`ImageSeriesReader`
   pipeline (`dicom_to_mcgpu.cpp:779-849`). First thing to test in the
   dedicated session: point `dicom_to_mcgpu`'s `dicom_dir` at an
   `HUDicomExporter`-produced folder and confirm it loads cleanly (correct
   series UID discovery, spacing, HU values) before building anything else
   on top of it.

4. **Extend `batch_dicom_to_mcgpu.py`'s scan/run JSON for a "no stretcher"
   entry, or handle it purely via the §3 material-swap on an existing
   `.in`?** See §4.

## 6. Summary of concrete edits (once the above are decided)

- `mcrp_to_vox.cpp`: remove the `.in`-writing block from fixed-volume mode
  (currently `mcrp_to_vox.cpp:807-862`, ends right before the mode's
  `return 0;` at line 862) and the legacy-mode equivalent
  (`mcrp_to_vox.cpp` ~1488+, mirrors the same "MC-GPU .in file written"
  block). Replace the volume-sizing goal (fixed 640×640×300 mm, FOV-anchored)
  with a tight-crop-to-mesh-bbox goal per open question 1. Keep `write_dicom`
  path (§2's "kept" logic) — it becomes the primary output, not a QA
  side-channel.
- `dicom_to_mcgpu.cpp` and/or a new pre-processing script: add whatever
  §2b/§5-question-1 decides for automatic head/FOV placement of a
  known-synthetic DICOM source.
- `batch_dicom_to_mcgpu.py`: port the `render_check` pattern from
  `batch_mcrp_to_vox.py:63-77, 166-174, 200-209` (§2d); optionally extend the
  JSON schema for a no-stretcher entry (§4).
- New (or existing, extended) batcher: chain `mcrp_to_vox` (tight-crop DICOM
  export, once per phantom) → `dicom_to_mcgpu` (N stretcher-position variants
  + optional no-stretcher via material-swap, per phantom) → positioning
  checks for every variant.
- `MCRP_TO_MCGPU_EXPORT.md` (this repo's existing recap doc) will need a
  rewrite once the above lands — it currently documents the `mcrp_to_vox`
  fixed-volume `.in`-generating pipeline that this handout retires.

## 7. Reference file map

| File | Role today | Role after this change |
|---|---|---|
| `mcrp_to_vox.cpp` | mesh → fixed 640×640×300 mm 5-label volume + `.in` | mesh → tight-crop 5-label **DICOM** export only |
| `dicom_to_mcgpu.cpp` | real DICOM → placed/stretchered MC-GPU volume + `.in` | same, plus (maybe) automatic head/FOV placement for synthetic sources |
| `batch_mcrp_to_vox.py` | batch-builds all 12 phantoms + positioning checks | retired or trimmed to just drive the new lean `mcrp_to_vox` |
| `batch_dicom_to_mcgpu.py` | batch-builds real-patient cases × stretcher positions | gains positioning checks (§2d); becomes the driver for MRCP stretcher-position augmentation too |
| `expand_in_kv.py` | `render_positioning_check` (renderer, used by the mcrp batcher) | unchanged; called from `batch_dicom_to_mcgpu.py` too now |
| `MCRP_TO_MCGPU_EXPORT.md` | recap of the (soon retired) `mcrp_to_vox` production pipeline | needs a rewrite once this lands |
