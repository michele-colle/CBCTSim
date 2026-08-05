# Progress: dicom_to_mcgpu auto head/FOV placement + stretcher augmentation

Tracks implementation of `PLAN_dicom_to_mcgpu_auto_placement.md`, plus two
follow-up rounds: (1) minimal output / external flat output folder / basic
placement mode, (2) a separate external folder for `.in` files specifically
(course-correction: minimal mode turned out to still need a real `.in`, just
filed apart from the volume data). **Status: done, validated end-to-end
against real MRCP data.**

## Round 8: gantry tilt fixed (closes round 7's open bug)

`dicom_to_mcgpu.cpp` now honours the series' direction cosines, so tilted
acquisitions are no longer sheared. The change is small because the resampling
loop was already an inverse map: it takes each output voxel centre, subtracts
the corner and half-extent, applies `Rt` (the transpose of the augmentation
rotation), then divides by spacing. Only that last step was wrong -- dividing
by spacing assumes the index axes ARE the patient axes.

Forward model, with `u = index * spacing`:

```
p = R * D * (u - qc) + offset        =>        u = D^-1 * R^T * (p - offset) + qc
```

so the fix is one extra matrix folded into the existing transform,
`Minv = dcmDir.inv() * Rt`, precomputed once and used in place of `Rt`. No
runtime cost (same single matrix-vector product), and for an axis-aligned
series `D^-1` is the identity so `Minv == Rt` exactly.

**The trap: `D^-1`, NOT `D^T`.** The first implementation used the transpose,
which is correct only for a rotation. A gantry-tilted lattice is **sheared**,
not rotated: the slice planes tilt while the table still advances along patient
+Z, so `D`'s third column stays `(0,0,1)` while its second is
`(0, cos24, -sin24)` -- non-orthogonal, `det = 0.9135`. `ForceOrthogonalDirectionOff()`
(already in the reader) keeps it that way deliberately. Using `D^T` made case
108 visibly *worse* than before the fix; only a general inverse undoes a shear.
Beware that SimpleITK's default reader orthogonalises and reports `det = 1` for
the same series, which hides the shear entirely -- do not use it to reason about
this.

**Second trap, found by voxel-level checking: do NOT route the NRRD mask
through physical space.** An earlier revision mapped mask lookups
index -> mm -> index "properly" and lost ~15% of the masked volume on case 108.
Reason: the mask is stored with ORTHOGONALISED direction cosines (slice normal
in the 3rd column, since it was written through a normal ITK reader) while the
series is read with `ForceOrthogonalDirectionOff` and keeps the true sheared
column. The two frames disagree, so the round trip lands on the wrong voxels.
Masks are drawn on the series' own grid, so the correspondence is
index-to-index; the code now says so explicitly and warns if the mask grid size
differs from the series'.

Also added: `detectHeadHU` still scans the index grid, so on an oblique series
"tip Z" is measured along index k rather than true patient superior. That
shifts the placement anchor a few mm but does not deform anatomy; it now prints
a warning rather than being silent. Fixing it properly is left open.

### Validation (CQ500 teeth data)

| Test | Result |
|---|---|
| **Regression, tilt-free** (case 9, IOP identity, 0 deg) | `.raw` **md5-identical** to the pre-fix binary -- the 38 published phantoms are unaffected |
| **Geometry, tilted** (case 108, 24 deg) | tissue extent vs truth computed from the DICOM's own IPP/IOP: **y +0.31 mm, z +0.26 mm** (was **y +22.21 mm, z -26.74 mm**) |
| **Voxel-level** (case 108 mid-sagittal, 2.2 M voxels) | independent numpy reimplementation of the mapping agrees on **99.9992%** of labels, tissue Dice **0.999998**, 1 differing voxel |

The geometry test needed a taller test box (`vol_length_mm = 340`) and
`denoise = false`: un-shearing genuinely ENLARGES the anatomy's true z extent
(case 108: 175 mm as sampled on the index grid -> 202 mm in reality), so at the
production 250 mm box the corrected head touches the bottom face. Measuring in
the production box would have read that clipping as a geometry error.

**Consequence for the rebuilt phantoms:** tilted cases now reach the inferior
face of the 250 mm output volume. Box dimensions were left at
2134x2134x834 to keep every teeth phantom identical; the head TIP (the
FOV-anchored, superior end that matters for these studies) is correct, and the
loss is at the inferior edge.

### Rebuild + publish of the 28 pending teeth cases

**28/28 built, validated, uploaded.** The pending set was the 26 tilted cases
plus 190 and 460 (held back in round 7 for the split-archive reason), i.e.
every teeth case that had no `.tar.xz`.

**The jobs JSON was stale and had to be regenerated.** `batch_jobs_qureai_teeth.json`
still pointed 460 at its old `CT BONE` series, and 167/190/460 at their
pre-merge `qctNN/` paths -- the manifest had since moved all three to
`merged/` (and 460 to `PLAIN THIN` with a different mask). Rebuilt with
`make_qureai_jobs.py` into `batch_jobs_qureai_teeth_rebuild.json`, then
filtered to the cases lacking an archive. Using the old JSON would have
rebuilt 460 from the wrong series entirely.

Validation (per case, not sampled): each phantom's anterior-posterior tissue
extent vs the truth computed from that series' own IPP/IOP. y is the axis to
judge on -- gantry tilt shears in y-z, and unlike z the 640 mm box never clips
it.

| tilt | cases | worst y error |
|---|---|---|
| 13.5-24 deg | 6 | +0.31 mm |
| 7-11 deg | 12 | +0.33 mm |
| 4-6.5 deg | 8 | +0.35 mm |
| 0 deg (190, 460) | 2 | +0.43 mm |

All under half a millimetre against a 0.3 mm voxel; case 108 was +22.21 mm
before the fix. Independently confirmed from the DICOM headers that exactly 26
of the 28 are tilted, with the IOP column angle matching `GantryDetectorTilt`
to the decimal on every case, and that the tilted set matches round 7's list.

**A lesson about verifying a batch:** the first sweep of the batch log looked
clean and was worthless -- the run had been piped through `tail -60`, so the
saved log held only the last 60 lines (one case), and greps over it reported
"1 oblique series" for a 26-tilted-case batch. Never sweep a log that was
captured through `tail`/`head`. The numbers above come from re-deriving
everything from the DICOMs and the phantoms themselves.

Uploaded with `--list` (the folder mixes these 28 with the 38 from round 7):
**28 uploaded, 0 failed**, verified by NAME against the bucket listing rather
than by count, with gsutil's stderr kept. `gs://mcgpu-data-gcp/phantom/` went
77 -> 105 objects and now holds **all 66 teeth phantoms**. No `--reclaim`: the
28 `.raw` (~106 GB) are still on H:.

## Round 3: separate `.in` folder (`mcgpu_in_dir`)

User course-corrected round 2: minimal output (no `.in` at all) wasn't
actually wanted -- a real `.in` is still needed, just written to a SEPARATE
folder from the `.raw`/`.txt` (`H:\MICHELE_MCGPU\MCGPU_IN_FILES` for `.in`,
`H:\MICHELE_MCGPU\MCGPU_VOLUME_EXPORT` -- renamed from `MCGPU_EXPORT` -- for
the volume). New `mcgpu_in_dir` cfg key (`dicom_to_mcgpu.cpp`): when set,
`.in` file(s) are written there instead of alongside the `.raw`/`.txt`, flat,
with filenames prefixed by `phantom_name` (or the output-prefix stem) so a
shared folder across a whole batch doesn't collide (the previous
"CBCT.in"/"CBCT_<stem>.in" convention would have collided across
cases/positions in a flat shared folder). Batcher gets matching
`--in-dir` / `mcgpu_in_dir` JSON key, same per-case-override +
global-default pattern as `output_dir`.

**Bug caught during testing:** `render_check()` derived the `.raw` path from
`in_path.parent / geom["raw_name"]` -- correct when `.in` and `.raw` share a
folder (always true before `mcgpu_in_dir` existed), silently wrong once they
don't (`.raw` sits in `output_dir`, `.in` in `mcgpu_in_dir`). First test run
after adding `mcgpu_in_dir` produced 0 PNGs with "`.raw` not found" warnings
pointing at the WRONG (`.in`'s) folder. Fixed by threading the actual known
`raw_path_this_job` (already captured from `RAW_WRITTEN_MARKER`) into
`render_check()` explicitly instead of re-deriving it.

**Caveat documented, not fixed (by design):** the `.in` file's own `VOXEL
GEOMETRY FILE` line still reads `phantom/<raw-name>.raw` (relative) --
running MC-GPU from `mcgpu_in_dir` requires placing/symlinking the matching
`.raw` under `<mcgpu_in_dir>/phantom/` first. Positioning checks are
unaffected (they take the `.raw` path directly, not via that relative
reference).

Verified end-to-end with the exact requested paths
(`H:\MICHELE_MCGPU\MCGPU_VOLUME_EXPORT`, `H:\MICHELE_MCGPU\MCGPU_IN_FILES`,
both passed as Windows paths): `.raw`+`.txt` landed flat in the volume
folder, all 3 `.in` files (one per beam template) landed flat in the in-files
folder with correctly prefixed unique names, and positioning-check PNGs
rendered correctly (visually matches the known-good result) after the
`render_check` fix.

## Round 4: GradientHealth real-patient redo + kV-variant `.in` naming

Added `--in-template` (scan) to override `mcgpu_in_template` for a whole batch
(e.g. down to just `cbct_head_bar_template.in`, dropping bar_only/head_only),
and `--kv-variants` (run) which rewrites each generated `.in` into `_80kV`/
`_120kV` copies (kV embedded in the filename, original un-suffixed `.in`
removed) via a new `make_kv_variants()` helper reusing `expand_in_kv.py`'s
existing `set_spectrum`/`set_output_name` logic.

Used both to redo the real-patient GradientHealth batch (10 patients,
`F:\Michele_diskF\GradientHealth\download\testRAR-13MAY2026\dicomweb\export`,
old nested layout with `.nrrd` masks alongside), which had been run months
ago with the pre-auto-placement pipeline (manual offsets, no FOV anchoring).
Rescanned with `--basic-placement` (single centred auto-seated stretcher
position) + `--output-dir "H:\MICHELE_MCGPU\GRADIENTHEALTH_VOLUME_EXPORT"` +
`--in-dir "H:\MICHELE_MCGPU\GRADIENTHEALTH_IN_FILES"` + `--in-template`. All
10/10 succeeded; spot-checked 3 positioning-check PNGs (first, one middle,
last patient) -- all correctly FOV-anchored with stretcher seated behind, no
overlap, masks loaded and applied. Compressed (`upload_phantoms_to_gcp.sh`,
same as the MRCP batch) and uploaded to `gs://mcgpu-data-gcp/phantom/`,
confirmed present (22 archives total across both batches, MRCP + GradientHealth).

## Round 5: CQ500/qureai-headct batch (10 curated head CTs)

Same pipeline as round 4's GradientHealth batch, applied to the curated CQ500
selection in `/mnt/f/Michele_diskF/kaggle/qureai-headct/dataset_manifest.json`
(10 patients, `air > -300 HU` largest-component tissue masks in `masks/`).
Two things the GradientHealth path didn't have to handle:

1. **Layout doesn't match `scan`.** Series live under
   `qctNN/<PATIENT> <PATIENT>/<STUDY>/<series>`, masks sit together in
   `masks/` under their own naming, and the manifest already pairs
   image+mask per patient -- so `make_qureai_jobs.py` (new) reads the
   manifest directly and emits exactly what `scan --basic-placement
   --output-dir --in-dir --in-template` would have.
2. **Two cases ship a pre-built `.nrrd` volume, not a DICOM series**
   (`kind: "stitched"` -- CQ500-CT-136, CQ500-CT-172, joined from two partial
   acquisitions upstream). `dicom_to_mcgpu` reads only DICOM series
   (GDCMSeriesFileNames), so `nrrd_to_dicom_series.py` (new) converts them:
   geometry preserved exactly, HU written as signed int16 with no rescale so
   values round-trip bit-exactly and the companion mask NRRD still aligns.
   Each conversion is verified by reading the series back and comparing
   voxels + geometry against the source (both cases: 0 differing voxels).

**Real trap found while probing, before running:** `CQ500-CT-325`'s single
`CT PLAIN` folder holds TWO series -- the 35-slice thick reconstruction the
mask was built from, and a 30-slice gantry-tilted one. The binary's
`series_uid` default ("first series found") could take either, and the wrong
one silently mis-applies the mask. The batcher had no way to pass
`series_uid` per case (it wasn't in `OVERRIDE_KEYS`), so it was added
alongside `acquisition_number`, and `make_qureai_jobs.py` fills it in
automatically by matching each candidate series' slice count / origin /
spacing against the mask's own geometry (and hard-fails rather than guessing
if the match isn't unique).

**Caveat quantified (not a regression -- pre-existing, now documented in
README §10):** head detection runs on the RAW HU volume; `mask_nrrd` is only
applied later, in the resampling loop. So the CT table / head cradle counts
toward the detected AP half-depth, which inflates the auto-seated stretcher
distance by roughly the table's depth behind the head: ~20-40 mm across these
10 cases (measured raw-vs-masked bbox per case), vs ~80 mm on the
GradientHealth batch that was accepted in round 4. Fix if it matters is a
per-case negative `stretcher_cy_offset_mm`; left as-is here to keep this
batch identical in method to round 4.

Commands (after `nrrd_to_dicom_series.py --manifest ... --out-root
<dataset>/stitched_dicom`):

```bash
python3 make_qureai_jobs.py /mnt/f/Michele_diskF/kaggle/qureai-headct/dataset_manifest.json \
    -o batch_jobs_qureai.json \
    --output-dir "H:\MICHELE_MCGPU\QUREAI_VOLUME_EXPORT" \
    --in-dir     "H:\MICHELE_MCGPU\QUREAI_IN_FILES" \
    --in-template params/cbct_head_bar_template.in
python3 batch_dicom_to_mcgpu.py run batch_jobs_qureai.json --kv-variants \
    --png-dir positioning_checks_qureai
```

## Round 7: gantry tilt (OPEN BUG), upload/reclaim, split-archive merge

### ~~⚠ OPEN BUG~~ FIXED IN ROUND 8: `dicom_to_mcgpu` ignores gantry tilt -> sheared phantoms

Found by a user question about two TEETH phantoms with an "elongated cranium"
(108, 489) -- is it a patient feature or a data problem? It is neither: it is
**our** bug.

`dicom_to_mcgpu` places voxels at `(index + 0.5) * spacing` and never reads
`ImageOrientationPatient`. With a tilted gantry the slice planes are rotated
about X while the table advances along patient +Z, so treating the stack as
axial shears the anatomy by `tan(tilt)` of Y per unit Z.

Evidence (the DICOMs are flawless -- no duplicate positions, no gaps, uniform
steps, IPP advancing purely along +Z; only our interpretation is wrong):

| Patient | GantryDetectorTilt | IOP column vector | reported z spacing | 0.625/cos(tilt) |
|---|---|---|---|---|
| 108 | 24.0 deg | (0, 0.9135, -0.4067) = (0, cos24, -sin24) | 0.6842 mm | **0.6841** |
| 489 | 14.5 deg | (0, 0.9681, -0.2504) | 0.6456 mm | **0.6456** |
| 55  |  6.5 deg | (0, 0.9936, -0.1132) | 0.6290 mm | **0.6290** |
| 9   |  0.0 deg | (0, 1, 0) | 0.6250 mm | 0.6250 |

So the "odd" spacings across this dataset were never anomalies -- they are
exactly `nominal / cos(tilt)`. **26 of the 66 TEETH series are tilted (4-24
deg)**, up to 80 mm of skew over a 180 mm head:

```
>=15deg: 108(24) 411(20.5) 425(20) 468(15)
10-15  : 489(14.5) 90(13.5) 40(11) 159(11) 407(10.5) 402(10.5) 346(10.5)
4-10   : 384(9) 167(8.5) 149(8.5) 319(8) 60(7.5) 396(7.5) 196(7) 17(7)
         55(6.5) 383(6) 166(6) 162(6) 18(5.5) 121(5.5) 140(4)
```

Masks are NOT affected (they are applied in DICOM index space, so they stay
aligned with their own image); only the exported phantom geometry is wrong.
The 8 GOOD + 2 stitched cases and the whole GradientHealth/MRCP history are
tilt-free -- verified explicitly, nothing sheared has been published.

**Fix, not yet implemented** (the proper one, since tilted head CT is routine):
honour the direction cosines in `dicom_to_mcgpu.cpp` -- i.e. map output voxel
centres through the DICOM's full physical frame instead of `(idx+0.5)*spacing`
-- or pre-resample each series onto an axis-aligned grid. Then rebuild those 26.
Documented as a caveat in README §10.

### Publish + reclaim

38 of the 66 teeth phantoms (the tilt-free ones, minus 2 held back) compressed
and uploaded to `gs://mcgpu-data-gcp/phantom/`, **verified against the bucket
listing** (all 38 present; zero tilted cases published; bucket now holds 48
CQ500 + 22 MRCP/GradientHealth archives).

Then all 66 `.raw` were deleted from H: to reclaim **233 GiB** (295 GB -> 529 GB
free). The 38 uploaded ones were deleted only after proving recoverability
per file: local `.tar.xz` present, its md5 equal to the md5 GCS reports for the
object, and `xz -t` passing. The other 28 (26 tilted + 190/460) were deleted on
the user's instruction since they will be rebuilt anyway. `.txt` companions
(66) and the local `.tar.xz` (38, 151 MB) are kept; volumes are regenerable
from `params/generated_qureai_teeth/*.cfg`.

A caution for next time: verifying an upload with `gsutil ls ... 2>/dev/null`
turned an expired-credential error into an apparently empty bucket, i.e. a
false "upload failed". Never discard gsutil's stderr when using it as a check.

Both throwaway scripts this round needed have been **folded into
`upload_phantoms_to_gcp.sh`** rather than left in a scratchpad (README §11):
`--list FILE` (publish only the named phantoms -- essential when a batch folder
mixes publishable and unpublishable volumes, as here with the 26 sheared ones),
`--reclaim` / `--reclaim-only` (delete each `.raw` only after proving the
archive recoverable: bucket-md5 == local-archive-md5 plus `xz -t`), and
`--dry-run`. Default behaviour -- every `*.raw` in the given directories, no
deletion -- is unchanged. Verified end-to-end against a real bucket object.

### Split-archive merge (dataset-side fix)

Some CQ500 patients are split across two consecutive `qctNN` archives with the
SAME SeriesInstanceUID and DISJOINT slice positions -- each half a gappy
subsample, only the union complete. New `merge_split_series.py` (in the kaggle
dataset repo, not this one) builds a symlink-only merged view per affected
series, refuses to merge unless the parts agree on geometry, and verifies the
union is duplicate- and gap-free; `select_studies.py` consumes its
`merged_series.json` report and drops the halves from the candidate list.
**All 18 affected patients merged, all gap-free.** The 3 in TEETH were
re-masked: 167 (140 sl @ 1.263 mm -> 288 @ 0.632), 190 (253 sl with 35 missing
-> 288 @ 0.625), 460 (BONE 128 @ 1.25 -> PLAIN THIN 256 @ 0.625). Their
phantoms still need rebuilding (167 is also tilted).

## Round 6: CQ500 TEETH batch (66 partial-cranium cases)

The `kind: "teeth"` group of the same manifest (partial cranium, upper teeth
visible), after the dataset's own mask-QC pass settled which cases survive:
78 selected -> 12 discarded as too coarse in Z (>=3 mm at a 0.3 mm output
voxel) -> **66 built**. 22 masks had to be redone first (cradle/rail included);
see `qureai-headct/mask_qc_status.md` in the dataset for that history.

`make_qureai_jobs.py` gained `--kind` (build one batch per curated group) and
`--cfg-dir`; it also prints any `flags` the manifest carries per patient, so
QC caveats surface at job-generation time rather than being discovered later.

Same recipe as rounds 4/5, own folders per the per-batch convention:

```bash
python3 make_qureai_jobs.py .../dataset_manifest.json -o batch_jobs_qureai_teeth.json \
    --kind teeth --cfg-dir params/generated_qureai_teeth \
    --output-dir "H:\MICHELE_MCGPU\QUREAI_TEETH_VOLUME_EXPORT" \
    --in-dir     "H:\MICHELE_MCGPU\QUREAI_TEETH_IN_FILES" \
    --in-template params/cbct_head_bar_template.in
python3 batch_dicom_to_mcgpu.py run batch_jobs_qureai_teeth.json --kv-variants \
    --png-dir positioning_checks_qureai_teeth
```

**66/66 succeeded**, ~1.9 min/case (~2 h wall clock), 250 GB of `.raw`, 132
`.in` (80/120 kV), 132 positioning-check PNGs. No DICOM output
(`write_dicom=false`, as requested).

Validation approach: one case built and inspected first, then the whole log
swept numerically instead of eyeballing 66 renders -- output dims identical
(2134x2134x834) for all, head-tip target identical (75.909 mm, so FOV
anchoring worked in every case), tissue fraction in a tight 2.29-3.99 % band
with none near zero, AP half-depth 88.8-120.1 mm, stretcher seat 119.3-150.6 mm.
Renders then reviewed for the flagged cases plus the numeric extremes.

**`partial_no_head_top` cases confirmed visually (212, 321, 468):** placement
and stretcher seat are correct, but the crown is a FLAT CUT anchored near the
FOV top, with empty beam above where the vertex should be, and visible 5 mm
stair-stepping at the 0.3 mm voxel. 468 is the worst (5.176 mm: fragmented
cortical layer, patchy facial structures). Kept deliberately -- the finer
series for each costs 31-59 mm of Z coverage -- and flagged in the manifest so
downstream code knows the head-tip anchor is not real anatomy.

Known-imperfect inputs, documented not fixed (see the dataset's
`manual_list.txt` TODO block): 167, 460, 190 have their source series **split
across two qctNN archives**, so their phantoms are built from partial volumes.
190's 35 missing interior slices are invisible in the sagittal render (the
resampler interpolates across them), which is exactly why it went unnoticed.

## Follow-up round: minimal output, flat external folder, basic placement

Requested after the initial plan shipped: (1) output was "4 files", wanted
just `.raw` + `.txt`; (2) output folder should be settable to an external,
FLAT (no subfolder) location so DICOM input and MC-GPU output don't mix
(e.g. `H:\MICHELE_MCGPU\MCGPU_EXPORT`); (3) a "most basic" placement option
-- stretcher just behind the head, centred, minimal overlap -- as an
alternative to the augmentation grid.

**Real conflict surfaced and resolved** (asked the user, they picked
"decouple the renderer"): dropping to raw+txt-only removes the `.in` file,
but positioning-check PNGs (the main test) parse beam geometry FROM the
`.in`. Fix: `expand_in_kv.parse_split_geometry(txt_path, beam_template_path)`
reads voxel geometry from the `.txt` companion (same section format/marker a
`.in` carries -- `writeInfoFile()` already writes it identically) and beam
geometry from a separate, shared beam template file -- no per-run `.in`
needed at all, since beam geometry is identical across positions and across
the production templates. Verified: PNG rendered this way for MRCP-00F was
visually identical to the earlier `.in`-based render.

**Second bug caught during testing** (before it shipped): `auto_head_placement`
computed its FOV target from the FIRST entry of `mcgpu_in_template` -- so
clearing that key for minimal output ALSO silently broke FOV-anchored
placement (fell back to "margin below the output volume's own top face",
observed as head tip at Z=115.1mm instead of the correct ~76mm, with a
"could not parse beam geometry" warning). Fixed by adding a new, decoupled
`fov_template` cfg key (dicom_to_mcgpu.cpp) used only for the FOV
computation; `mcgpu_in_template` controls only which `.in` files get
written. The batcher auto-fills `fov_template` from `check_template` whenever
a case has `mcgpu_in_template=""` and no explicit `fov_template`, so this
can't silently regress again for anyone using `--minimal`.

New cfg keys (`dicom_to_mcgpu.cpp`): `output_dir` (flat output folder,
opt-in, backward compatible), `fov_template` (FOV source, decoupled from
`mcgpu_in_template`).

New batcher (`batch_dicom_to_mcgpu.py`) flags: `--output-dir`, `--minimal`,
`--basic-placement` (mutually exclusive with `--auto-placement`),
`--check-template`. New JSON keys: `output_dir`, `mcgpu_in_template` (`""` =
minimal), `fov_template`, `check_template` -- all per-case-overridable
top-level defaults, all backward compatible (absent everywhere = untouched,
existing JSONs unaffected).

Verified end-to-end against real MRCP-00F data: `scan --basic-placement
--output-dir "H:\MICHELE_MCGPU\MCGPU_EXPORT" --minimal` -> `run` produced
exactly 2 files (`.raw` + `.txt`) flat in the external folder, correct
FOV-anchored placement (`fov_template` auto-filled, confirmed via log line
and matching the earlier-validated -223.941mm corner value), and a
positioning-check PNG via the split-geometry path that visually matches the
known-good result.

Test data (real, on H: drive, not the repo disk):
- Input:  `H:\MICHELE_MCGPU\DICOM_EXPORT_MCRP` (10 MRCP tight-crop DICOM series,
  `/mnt/h/MICHELE_MCGPU/DICOM_EXPORT_MCRP` from WSL)
- Output: `H:\MICHELE_MCGPU\MCGPU_EXPORT` (`/mnt/h/MICHELE_MCGPU/MCGPU_EXPORT`)
- Main acceptance test: visual inspection of rendered `positioning_checks/*.png`
- All multi-GB test `.raw`/DICOM volumes were deleted from H: after each
  positioning-check PNG was rendered, to keep the drive from filling up
  (rerun any `params/generated*/*.cfg` to regenerate a volume if needed).

## Findings before coding

- Probed `MRCP-00F_dicom_749x538x1067` slice-by-slice (HU > -500 area per
  slice): body is truncated with **no air margin** at the k=0 (low-Z) end
  (already ~119k px of tissue at slice 0), rises through torso, narrows at
  the neck (~slice 600), widens again over the head (~slice 780-840), tapers
  to zero around slice ~1000, then **pure air** from slice ~1000 to 1066.
  Confirms: **head is at the high-k / high-IPP-Z end**, matching the plan's
  `head_at_max_z = true` default.

## Bug found + fixed during testing

First implementation used the DICOM's true ITK physical origin
(`dcm_orig_x/y/z`, from `huImage->GetOrigin()`) when converting detected
voxel indices to mm. That's the WRONG frame: the rest of `dicom_to_mcgpu.cpp`
(qcx/qcy/qcz, `dicom_corner_*_mm`, the resampling loop) all use a "local,
corner-at-zero" DICOM frame that ignores the true physical origin entirely.
Mixing the two shifted the detected head centre by a full DICOM half-extent
(~112 mm in X for MRCP-00F), placing the phantom almost entirely off to one
side of isocenter -- caught immediately because the first rendered
positioning-check PNG showed the stretcher in the right place but **no
phantom anatomy at all** in the central-sagittal slice. Fixed by dropping the
origin arguments from `detectHeadHU` and using `(idx+0.5)*spacing` (the same
convention as everywhere else in the file). Re-verified after the fix: XY
offsets came out near-zero (as expected, since the MRCP tight-crop already
centres the phantom in XY), and the rendered PNGs show the head correctly
placed. This is exactly why the plan called visual inspection the main test
rather than trusting the arithmetic alone.

## Steps

- [x] Explore H: drive test data, confirm head-end orientation empirically
- [x] `dicom_to_mcgpu.cpp`: port `parseBeamFOVHalfZ_mm`/`inSectionValues`
- [x] `dicom_to_mcgpu.cpp`: add `detectHeadHU` (HU-threshold head detection)
- [x] `dicom_to_mcgpu.cpp`: new cfg keys (`auto_head_placement`,
      `head_detect_thr_hu`, `head_at_max_z`, `head_region_mm`,
      `head_top_margin_mm`, `stretcher_auto`, `stretcher_cy_offset_mm`,
      `stretcher_gap_mm`)
- [x] `dicom_to_mcgpu.cpp`: wire auto placement into
      `dicom_center_offset_{x,y,z}_mm` + stretcher auto-seat into
      `stretcher.cy_mm`
- [x] Build `dicom_to_mcgpu`, fix compile errors (one scoping bug: `firstTmpl`
      referenced outside the block it was declared in -- fixed)
- [x] Regression: ran a manual-offset cfg (`dicom_corner_z_mm=-70`, absolute
      stretcher `cy_mm`) against MRCP-00F with both new flags left at their
      default (off) -- no "Head detected"/"Auto placement"/"Stretcher
      auto-seat" lines printed, `DICOM ctr offset` matched the manual value
      exactly. Confirms opt-in guard works, existing manual cfgs unaffected.
- [x] `batch_dicom_to_mcgpu.py`: port positioning-check integration
      (capture `.in`-written marker, call `render_check`; `--png-dir`/
      `--no-checks` flags on `run`)
- [x] `batch_dicom_to_mcgpu.py`: extend stretcher-slot schema for
      offset-based augmentation (`{"auto": true, "cy_offset_mm": ..}` slot
      form; `auto_head_placement` case/global toggle; `--auto-placement` on
      `scan` pre-fills offset-based slots)
- [x] `params/dicom_to_mcgpu_template.cfg`: documented new keys (commented
      out, off by default)
- [x] Ran against real MRCP DICOM exports on H: (MRCP-00F newborn, MRCP-15M
      adolescent, MRCP-01F via the batcher with 2 stretcher offsets x 3 beam
      templates), output to `H:\MICHELE_MCGPU\MCGPU_EXPORT`
- [x] **Visual inspection of positioning_checks/*.png** (main acceptance
      test) -- see results below
- [x] Update this file with final status

## Visual inspection results

| Phantom | What was checked | Result |
|---|---|---|
| MRCP-00F (newborn) | direct binary run, `auto_head_placement`+`stretcher_auto` | Head (proportionally large, correct for newborn) sits right at the FOV top with the expected ~10 mm margin below the beam edge; stretcher seated immediately behind the body, no overlap. |
| MRCP-15M (adolescent) | direct binary run, same cfg | Detailed skull/jaw/cervical-spine cross-section correctly anchored near the FOV top; stretcher correctly seated behind the neck. Confirms detection generalizes across very different head/body sizes. |
| MRCP-01F, via `batch_dicom_to_mcgpu.py run` | 2 stretcher offsets (0 mm, +25 mm) x 3 beam templates (head_bar/bar_only/head_only) = 6 PNGs, `denoise=true` path (median filter + despeckle ran before head detection) | Head placement identical and correct across all 3 templates (same beam geometry); stretcher visibly shifted ~25 mm further back (toward posterior) between the two offset variants -- confirms the augmentation knob works and the batcher's positioning-check integration (previously absent) now fires automatically after every successful build. |

PNGs are in `positioning_checks/`: `MRCP-00F_auto_test.png`,
`MRCP-15M_auto_test.png`, `MRCP-01F_A_CBCT_*.png` (offset 0),
`MRCP-01F_B_CBCT_*.png` (offset +25 mm).

## Numeric self-check

FOV half-height parsed from `params/cbct_head_bar_template.in` came out to
85.909 mm -- matches the handout's independently-verified reference value
(~85.9 mm) exactly, confirming the ported `parseBeamFOVHalfZ_mm` formula is
correct in its new home.

## Deviations from the plan

None functionally -- the plan's design (opt-in cfg keys, detection pass
after denoise/despeckle, limit+offset stretcher model, batcher schema
extension) was implemented as written. The only change was the internal fix
described above (coordinate frame for `detectHeadHU`), which is an
implementation detail, not a design change.

## Files changed

- `dicom_to_mcgpu.cpp` -- beam-FOV parser, `detectHeadHU`, new cfg keys,
  placement + stretcher auto-seat wiring.
- `batch_dicom_to_mcgpu.py` -- positioning-check integration, stretcher-slot
  schema extension, `--auto-placement` / `--png-dir` / `--no-checks` CLI
  flags.
- `params/dicom_to_mcgpu_template.cfg` -- documented new keys.
- `params/mrcp_auto_placement_test.cfg` -- new smoke-test example cfg
  (points at an MRCP tight-crop DICOM on H:, auto placement + stretcher auto
  enabled).
