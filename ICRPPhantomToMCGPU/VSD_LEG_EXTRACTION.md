# VSD single-leg DICOM pool — `VSD_dicom`

56 single-leg CT DICOM series (mid-thigh → toe tip, one per side, table
removed) extracted from the VSD Full Body dataset (Zenodo 8302449) — see
[VSD_FULLBODY_INVENTORY.md](VSD_FULLBODY_INVENTORY.md) for the source
dataset itself. Written by [extract_leg_dicom.py](extract_leg_dicom.py).
Per-case stats: [vsd_leg_extraction.csv](vsd_leg_extraction.csv). Central-
sagittal QA renders for every series: [make_sagittal_check_pngs.py](make_sagittal_check_pngs.py)
→ `/mnt/h/MICHELE_MCGPU/VSD_dicom_sagittal_checks/`.

Output: `/mnt/h/MICHELE_MCGPU/VSD_dicom/<case>_<Left|Right>Leg_MidThigh-ToToe/`
— 28 of the 29 VSD human cases (all except `Phantom001`, the calibration
object, and `z036`, excluded — see below) × 2 sides = 56 series.

## Why this needed its own pipeline

VSD never ships a single volume spanning mid-thigh to toe tip. Every case
gives a `Pelvis-Thighs` crop (has the femurs) and a `Shanks-Feet` crop (has
the foot bones) separately, overlapping around the knee — never combined.
Three case layouts exist and all had to be handled uniformly:

- **Same-file crops** (`002`, `006`, `014`, `019`, `023`, all 20 `z*` cases):
  `*-Pelvis-Thighs.nrrd` / `*-Shanks-Feet.nrrd` plus matching
  `*_Segmentation.seg.nrrd` label maps.
- **Split raw series, no crops** (`010`, `015`, `016`, `017`): two full
  512×512-FOV series per case, distinguished only by which one has a
  `Pelvis-Thighs_Segmentation` vs `Shanks-Feet_Segmentation` companion.
- **Crop-only, no full series** (`019`): only the two small ROI crops exist.

## Method, per leg

1. Locate the case's "upper" (Pelvis-Thighs-like) and "lower"
   (Shanks-Feet-like) volume + segmentation pair (`find_case_volumes`).
2. Read each segment's name → label value straight out of the `.seg.nrrd`
   text header (label numbering is **not** consistent across cases).
3. Mid-thigh Z = midpoint of `Femur_<side>`'s bounding box (upper volume).
   Ankle Z = `Talus_<side>` bbox center (lower volume) — used only to learn
   which Z direction is "toward the foot" for *this case's* convention (see
   below). Toe tip = the `Phalanges_<side>` bbox extreme farthest from the
   ankle in that direction, padded 10 mm.
4. X/Y crop box = union of this leg's bone labels in both volumes, padded
   40 mm, clamped to stop 15 mm short of the *other* leg's nearest bone bbox.
5. Resample both source volumes onto one output grid spanning that box
   (validity masks track where each source actually has data; the overlap
   between the two crops is split at its physical midpoint between the
   thigh and toe landmarks).
6. Threshold at −300 HU, keep only the **largest connected component**, blank
   everything else to air (−1024 HU). This is what removes the scan
   table/stretcher — it shows up as a separate, thinner component spanning
   the full crop length and width, distinct from the leg.
7. Write the result as a DICOM series via `nrrd_to_dicom_series.convert()`,
   voxel-exact read-back verified against the intermediate volume.

## Data-quality issues found and worked around

These are dataset quirks, not extraction bugs — each was confirmed by
inspecting the raw source files directly (see conversation history for the
verification steps), not assumed:

- **z\* crops are stored top/bottom-inverted** relative to every other case
  (confirmed on `z001`: `Shanks-Feet` crop's own Z origin sits *above* its
  `Pelvis-Thighs` crop's, the reverse of every non-`z*` case). Rather than
  apply the supplied `Transform-Upside-Down.h5`, the pipeline never assumes
  a fixed sign convention: it derives "which Z direction is toward the foot"
  per case from the ankle-vs-mid-thigh relationship (step 3 above), so it's
  correct either way.
- **`z001`'s `Patella_L`/`Patella_R` labels are swapped** relative to
  `Femur_L`/`Femur_R` (`Patella_R`'s bbox sits where `Femur_L` is, and vice
  versa) — a labeling bug in the source segmentation. Fixed generally: every
  per-leg label (not just Patella) is resolved by spatial proximity to the
  femur anchor, never trusted by name alone (`resolve_bbox`).
- **`z050`'s segmentation is a 4-D labelmap** (two overlapping label
  "layers", since some of its segments spatially overlap — Slicer can't
  encode that in one 3-D layer). `bbox_phys` now reads the segment's `Layer`
  attribute and indexes the right slice of the 4th axis.
- **`z066`'s `Phalanges_L`/`Phalanges_R` are empty segments** (declared in
  the header, zero voxels painted — matches its documented missing `.ply`
  mesh in VSD_FULLBODY_INVENTORY.md). Falls back to `Metatarsals` as the
  toe-tip landmark, with 40 mm extra margin so the real (unsegmented) toe
  tissue still visible in the raw CT gets captured by the HU threshold.
- **`z066`'s own `Shanks-Feet.nrrd` crop is truncated mid-bone**, independent
  of the segmentation issue above — its last slice still shows dense tissue
  (~2366 HU) with no tapering into air, meaning the original ROI box was cut
  short of the true toe tips. This is a limit of the source file, not
  recoverable by widening the margin (confirmed: nothing exists past the
  crop's own edge). `z066`'s feet are slightly shorter than anatomically
  complete as a result.
- **`z036`'s `Femur_L`/`Femur_R` segmentation doesn't cleanly separate the
  femur from the rest of the pelvic bone** — confirmed by sampling the raw
  HU values under each label: genuine bone density (mean ~760 HU, up to 1651)
  spans essentially the *entire* Pelvis-Thighs crop height (506 mm for
  `Femur_L`, vs. ~400–450 mm for a real femur), and `Femur_L`/`Femur_R`'s X
  ranges overlap each other. The "mid-thigh" landmark this pipeline derives
  from that label's midpoint therefore lands nowhere near the real mid-thigh,
  and the resulting crop is ~80% empty air (confirmed on both sides via a
  coronal MIP). **`z036` is excluded from the pool** (58 → 56) rather than
  shipped as a broken series — no reliable per-case landmark could be
  recovered from this segmentation without hand-tuning.
  A first attempt at an automated guard (rejecting an implausibly wide/tall
  `Femur_*` bbox) was tried and reverted: femur-bbox width is a *continuum*
  across the 29 cases (79–312 mm, no natural gap), and a threshold tight
  enough to catch `z036` also rejected several cases later confirmed correct
  by MIP (`z050`, `z063`, `z046`, `z056`, `z061`, `z064`) — so this is
  presently a manually-confirmed exclusion, not an automated one.
- **`z063`'s *raw* `Pelvis-Thighs` volume itself ships a mirrored header**
  (direction `(-1,0,0, 0,1,0, 0,0,-1)`, origin negated on X/Z too — a full
  point-reflection through the world origin), unlike every other case where
  only a *segmentation* ever carried that bug. Its paired `Shanks-Feet`
  volume is a normal identity-direction file, so the two ended up in
  inconsistent coordinate frames: the femur-based anchor (from the mirrored,
  uncorrected upper volume) didn't line up with the lower volume's own bone
  positions, and `resolve_bbox` picked the *same* (right-side) foot/shank
  bones for both the left- and right-leg queries — no error, just a silently
  wrong ~24mm-wide, badly mispositioned left-leg crop (this is what showed up
  as "z063 looks odd" on visual review, despite a deceptively high 100%
  leg-fraction score — a narrow, wrong-location crop can still be internally
  "pure"). Fixed generally: any raw volume (not just segmentations) with a
  pure diagonal ±1 direction now has the reflection undone
  (`normalize_mirror_direction`) before anything else touches it.

## Result quality

Mean **97.4%** of thresholded voxels kept in the largest connected component
across the 56 legs (i.e., ≤3% residual non-leg material on average). All 56
passed the read-back geometry/voxel verification — including `z063`, whose
verification had passed *before* its coordinate-frame fix too (a reminder
that voxel/geometry read-back checks the DICOM matches its own intermediate
volume, not that the crop is anatomically sound; see `z063`'s bug above).

| Range | Count | Cases |
|---|---|---|
| ≥ 95% | 49 | — |
| 90–95% | 5 | `z009/R` 91.4%, `z019/L` 94.2%, `z050/L` 94.9%, `z057/R` 90.8%, `z064/L` 94.4% |
| < 90% | 2 | `z023/R` 89.7%, `z057/L` 88.6% |

`z023/R` and `z057/L` were both individually checked with a coronal +
sagittal MIP and are **visually complete, correct legs** — mid-thigh through
knee, tibia/fibula, ankle, full foot, no gaps. Their lower percentage just
means a bit of extra bone (beyond the femur shaft itself) stayed attached to
the `Femur_*` label and rode along in the largest connected component; it
doesn't mean anatomy is missing. `z057` additionally has a high component
count (2039/1658 vs. 2–800 elsewhere), consistent with more image
noise/fragmentation in that source volume, but the leg itself renders clean.

## Regenerating

```bash
python3 extract_leg_dicom.py \
  --root /mnt/h/MICHELE_MCGPU/zenodo_VSDFullBody/zenodo_8302449 \
  --out-root /mnt/h/MICHELE_MCGPU/VSD_dicom \
  --force
```

`--only <case> [<case> ...]` restricts to specific cases; `--sides L` or
`--sides R` restricts to one side. `z036` (and `Phantom001`) are hardcoded
into `EXCLUDE_CASES` so a plain re-run never regenerates the broken output.
